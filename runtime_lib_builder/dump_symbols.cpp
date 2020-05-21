#include "src/common.h"
#include "src/pochivm_reflection_helper.h"
#include "runtime_lib_builder/symbol_list_util.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Mangler.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/DebugInfo.h"

using namespace llvm;
using namespace llvm::orc;

// The pre-processing system is naive right now.
// It generates 2^n prototypes for a template function with n parameters.
// While all the negative effect is that it slows down C++ code compile speed,
// set a limit for now. Raise it if necessary.
//
const static int MAX_TEMPLATE_PARAMS_FOR_TEMPLATED_FN = 5;

namespace {

static bool isWhitespace(char c)
{
    const char* whitespaces = " \t\n\r\f\v";
    for (size_t i = 0; i < strlen(whitespaces); i++)
    {
        if (whitespaces[i] == c)
        {
            return true;
        }
    }
    return false;
}

struct ParsedFnTypeNamesInfo
{
    ParsedFnTypeNamesInfo() {}

    ParsedFnTypeNamesInfo(Ast::ReflectionHelper::RawFnTypeNamesInfo* src)
    {
        m_numArgs = src->m_numArgs;
        m_origRet = ParseTypeName(src->m_originalRetAndArgTypenames[0]);
        m_ret = ParseTypeName(src->m_transformedRetAndArgTypenames[0]);
        for (size_t i = 1; i <= src->m_numArgs; i++)
        {
            m_origParams.push_back(ParseTypeName(src->m_originalRetAndArgTypenames[i]));
            m_params.push_back(ParseTypeName(src->m_transformedRetAndArgTypenames[i]));
        }
        m_fnType = src->m_fnType;
        std::string fnName = ParseValueName(src->m_fnName);
        if (m_fnType == Ast::ReflectionHelper::FunctionType::NonStaticMemberFn)
        {
            std::string className = ParseTypeName(src->m_classTypename);
            ReleaseAssert(fnName.length() > className.length() + 2);
            ReleaseAssert(fnName.substr(0, className.length()) == className);
            ReleaseAssert(fnName[className.length()] == ':');
            ReleaseAssert(fnName[className.length() + 1] == ':');
            m_prefix = className;
            m_functionName = fnName.substr(className.length() + 2);
        }
        else
        {
            int k = static_cast<int>(fnName.length()) - 1;
            while (k >= 0 && fnName[static_cast<size_t>(k)] != ':')
            {
                k--;
            }
            m_functionName = fnName.substr(static_cast<size_t>(k + 1));
            ReleaseAssert(k == -1 || k >= 1);
            if (k == -1)
            {
                ReleaseAssert(m_fnType == Ast::ReflectionHelper::FunctionType::FreeFn);
                m_prefix = "";
            }
            else
            {
                ReleaseAssert(fnName[static_cast<size_t>(k - 1)] == ':');
                m_prefix = fnName.substr(0, static_cast<size_t>(k - 1));
            }
        }
    }

    void RecordMangledSymbolName(const std::string& mangledSymbolName)
    {
        m_mangledSymbolName = mangledSymbolName;
    }

    // For some reason, __PRETTY_FUNCTION__ (which is how we get all the type data here)
    // does not include the template values for templated function/methods.
    // We patch those part using demangledSymbolName.
    //
    void FindTemplateParameters(const std::string& demangledSymbolName)
    {
        // demangled symbol name looks like this:
        //     int A<int, 1>::foo<double>(double) const
        // We start from rightmost and find the matching left parenthesis,
        // then find the matching left bracket
        //
        size_t i = demangledSymbolName.length() - 1;
        while (i > 0 && demangledSymbolName[i] != ')')
        {
            i--;
        }
        ReleaseAssert(i > 0 && demangledSymbolName[i] == ')');
        int parenthesisDepth = 0;
        while (i > 0)
        {
            if (demangledSymbolName[i] == ')')
            {
                parenthesisDepth++;
            }
            else if (demangledSymbolName[i] == '(')
            {
                parenthesisDepth--;
                if (parenthesisDepth == 0)
                {
                    break;
                }
            }
            i--;
        }
        ReleaseAssert(parenthesisDepth == 0);
        ReleaseAssert(demangledSymbolName[i] == '(');
        ReleaseAssert(i > 0);
        i--;
        while (i > 0 && isWhitespace(demangledSymbolName[i]))
        {
            i--;
        }
        ReleaseAssert(i > 0);
        m_templateParams.clear();
        if (demangledSymbolName[i] == '>')
        {
            // There are template parameters, parse them out
            //
            int bracketDepth = 1;
            size_t lastPos = i;
            i--;
            while (i > 0)
            {
                if (demangledSymbolName[i] == ')')
                {
                    // We hit a right parenthesis, temporarily switch to parenthesis matching mode.
                    // Find the left parenthesis, ignore anything (including brackets) in between.
                    //
                    parenthesisDepth = 0;
                    while (i > 0)
                    {
                        if (demangledSymbolName[i] == ')')
                        {
                            parenthesisDepth++;
                        }
                        else if (demangledSymbolName[i] == '(')
                        {
                            parenthesisDepth--;
                            if (parenthesisDepth == 0)
                            {
                                break;
                            }
                        }
                        i--;
                    }
                    ReleaseAssert(parenthesisDepth == 0);
                    ReleaseAssert(demangledSymbolName[i] == '(');
                    ReleaseAssert(i > 0);
                }
                else if (demangledSymbolName[i] == '>')
                {
                    bracketDepth++;
                }
                else if (demangledSymbolName[i] == '<')
                {
                    bracketDepth--;
                    if (bracketDepth == 0)
                    {
                        break;
                    }
                }
                else if (demangledSymbolName[i] == ',' && bracketDepth == 1)
                {
                    // We found a template parameter at [i+1, lastPos-1]
                    //
                    ReleaseAssert(i + 1 <= lastPos - 1);
                    m_templateParams.push_back(demangledSymbolName.substr(i + 1, lastPos - i - 1));
                    lastPos = i;
                }
                i--;
            }
            ReleaseAssert(bracketDepth == 0);
            ReleaseAssert(demangledSymbolName[i] == '<');
            ReleaseAssert(i > 0);
            // push the last template parameter
            //
            ReleaseAssert(i + 1 <= lastPos - 1);
            m_templateParams.push_back(demangledSymbolName.substr(i + 1, lastPos - i - 1));
            std::reverse(m_templateParams.begin(), m_templateParams.end());
            // Remove leading and trailing whitespace, just for sanity
            //
            for (size_t k = 0; k < m_templateParams.size(); k++)
            {
                std::string& s = m_templateParams[k];
                size_t head = 0;
                while (head < s.length() && isWhitespace(s[head])) { head++; }
                ReleaseAssert(head < s.length() && !isWhitespace(s[head]));
                size_t tail = s.length() - 1;
                while (tail > head && isWhitespace(s[tail])) { tail--; }
                ReleaseAssert(head <= tail && !isWhitespace(s[tail]));
                m_templateParams[k] = s.substr(head, tail - head + 1);
            }
        }
    }

    static std::string ParseNameInternal(const char* data)
    {
        int bracketDepth = 1;
        size_t i = 0;
        size_t len = strlen(data);
        while (i < len)
        {
            if (data[i] == ']')
            {
                bracketDepth--;
                if (bracketDepth == 0)
                {
                    break;
                }
            }
            else if (data[i] == '[')
            {
                bracketDepth++;
            }
            i++;
        }
        ReleaseAssert(bracketDepth == 0 && i < len);
        return std::string(data, i);
    }

    static std::string ParseTypeName(const char* data)
    {
        static const char* prefix = "const char *__pochivm_stringify_type__() [T = ";
        size_t len = strlen(data);
        size_t prefixLen = strlen(prefix);
        ReleaseAssert(len > prefixLen);
        ReleaseAssert(strncmp(data, prefix, prefixLen) == 0);
        return ParseNameInternal(data + prefixLen);
    }

    static std::string ParseValueName(const char* data)
    {
        static const char* prefix = "const char *__pochivm_stringify_value__() [v = &";
        size_t len = strlen(data);
        size_t prefixLen = strlen(prefix);
        ReleaseAssert(len > prefixLen);
        ReleaseAssert(strncmp(data, prefix, prefixLen) == 0);
        return ParseNameInternal(data + prefixLen);
    }

    Ast::ReflectionHelper::FunctionType m_fnType;
    size_t m_numArgs;
    // typenames of parameters, before and after transform
    //
    std::vector<std::string> m_origParams;
    std::vector<std::string> m_params;
    // typename of return value, before and after transform
    //
    std::string m_origRet;
    std::string m_ret;

    // The prefix before the function
    // For member function this is namespace + class name,
    // for free function this is only namespace
    //
    std::string m_prefix;
    // name of function
    //
    std::string m_functionName;
    // If the function is a templated function, the template parameters
    //
    std::vector<std::string> m_templateParams;
    std::string m_mangledSymbolName;
};

static std::map<void*, ParsedFnTypeNamesInfo> g_symbolAddrToTypeData;

}   // anonymous namespace

void Ast::__pochivm_report_info__(Ast::ReflectionHelper::RawFnTypeNamesInfo* info)
{
    if (!g_symbolAddrToTypeData.count(info->m_fnAddress))
    {
        g_symbolAddrToTypeData[info->m_fnAddress] = ParsedFnTypeNamesInfo(info);
    }
    else
    {
        fprintf(stderr, "[WARNING] function %s::%s appeared to be registered more than once, ignored multiple occurrance.\n",
                (info->m_classTypename != nullptr ? info->m_classTypename : ""), info->m_fnName);
    }
}

namespace {

// A fake symbol resolver that returns fake (but unique) addresses for each queried symbol
// Of course the program crashes if the symbols are called, but if they only care about the addresses
// things should be fine.
//
class FakeSymbolResolver : public JITDylib::DefinitionGenerator
{
public:
    FakeSymbolResolver()
        : m_lock()
        , m_curAddr(0)
        , m_addrEnd(0)
        , m_pastAddrs()
    { }

    ~FakeSymbolResolver() override
    {
        for (uintptr_t addr : m_pastAddrs)
        {
            int r = munmap(reinterpret_cast<void*>(addr), x_length);
            ReleaseAssert(r == 0);
        }
    }

    virtual Error tryToGenerate(LookupKind /*K*/,
                                JITDylib &JD,
                                JITDylibLookupFlags /*JDLookupFlags*/,
                                const SymbolLookupSet &symbols) override
    {
        // no idea what the thread safety requirement is... so just to be safe
        //
        std::lock_guard<std::mutex> guard(m_lock);

        SymbolMap newSymbols;

        for (auto& kv : symbols)
        {
            const SymbolStringPtr& name = kv.first;

            if ((*name).empty())
                continue;

            uintptr_t addr = GetNextAddress();
            newSymbols[name] = JITEvaluatedSymbol(
                    static_cast<JITTargetAddress>(addr),
                    JITSymbolFlags::Exported);
            // std::string strname((*name).data(), (*name).size());
            // fprintf(stderr, "Assigned symbol %s to address %llu\n",
            //         strname.c_str(), static_cast<unsigned long long>(addr));
        }

        if (newSymbols.empty())
            return Error::success();

        return JD.define(absoluteSymbols(std::move(newSymbols)));
    }

private:
    uintptr_t GetNextAddress()
    {
        if (m_curAddr == 0 || m_curAddr >= m_addrEnd)
        {
            void* r = mmap(nullptr, x_length, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1 /*fd*/, 0 /*offset*/);
            ReleaseAssert(r != reinterpret_cast<void*>(-1));
            m_curAddr = reinterpret_cast<uintptr_t>(r);
            m_addrEnd = m_curAddr + x_length;
            m_pastAddrs.push_back(m_curAddr);
        }

        ReleaseAssert(m_curAddr != 0 && m_curAddr < m_addrEnd);
        uintptr_t r = m_curAddr;
        m_curAddr += 32;
        return r;
    }

    static const size_t x_length = 1024 * 1024;

    std::mutex m_lock;
    uintptr_t m_curAddr;
    uintptr_t m_addrEnd;
    std::vector<uintptr_t> m_pastAddrs;
};

static void ReadCppFiltOutput(const std::string& file, std::vector<std::string>& symbols /*out*/)
{
    FILE* fp = fopen(file.c_str(), "r");
    if (fp == nullptr)
    {
        fprintf(stderr, "Failed to open file '%s', errno = %d (%s)\n", file.c_str(), errno, strerror(errno));
        abort();
    }

    symbols.clear();
    char* buf = nullptr;
    size_t line_buf_size = 0;
    bool firstLine = true;  // firstLine is 'n', the number of lines follows
    while (true)
    {
        ssize_t line_size = getline(&buf, &line_buf_size, fp);
        if (line_size < 0)
        {
            ReleaseAssert(!firstLine);
            break;
        }
        if (firstLine)
        {
            firstLine = false;
            continue;
        }

        ssize_t i = 0;
        while (i < line_size && isWhitespace(buf[i])) { i++; }
        if (i == line_size)
        {
            continue;
        }
        symbols.push_back(std::string(buf + i, static_cast<size_t>(line_size - i)));
    }
    free(buf);
    fclose(fp);
}

// DEVNOTE:
//    It is stupid that clang++ --save-temps outputs unoptimized IR file
//    For now, we will optimize it and save to xx.optimized.bc
//
__attribute__((used))
static void RunLLVMOptimizePass(Module* module)
{
    static const llvm::PassBuilder::OptimizationLevel
            optLevel = llvm::PassBuilder::OptimizationLevel::O3;

    llvm::PassBuilder m_passBuilder;
    llvm::LoopAnalysisManager m_LAM;
    llvm::FunctionAnalysisManager m_FAM;
    llvm::CGSCCAnalysisManager m_CGAM;
    llvm::ModuleAnalysisManager m_MAM;
    llvm::ModulePassManager m_MPM;

    m_passBuilder.registerModuleAnalyses(m_MAM);
    m_passBuilder.registerCGSCCAnalyses(m_CGAM);
    m_passBuilder.registerFunctionAnalyses(m_FAM);
    m_passBuilder.registerLoopAnalyses(m_LAM);
    m_passBuilder.crossRegisterProxies(m_LAM, m_FAM, m_CGAM, m_MAM);

    m_MPM = m_passBuilder.buildPerModuleDefaultPipeline(optLevel);

    ReleaseAssert(module != nullptr);
    m_MPM.run(*module, m_MAM);
}

static void RunLLVMOptimizePassIfNotDebug(Module* module)
{
#ifdef NDEBUG
    RunLLVMOptimizePass(module);
#endif
    std::ignore = module;
}

static bool CmpParsedFnTypeNamesInfo(const ParsedFnTypeNamesInfo& a, const ParsedFnTypeNamesInfo& b)
{
    if (a.m_prefix != b.m_prefix) { return a.m_prefix < b.m_prefix; }
    if (a.m_fnType != b.m_fnType) { return a.m_fnType < b.m_fnType; }
    if (a.m_functionName != b.m_functionName) { return a.m_functionName < b.m_functionName; }
    if (a.m_params != b.m_params) { return a.m_params < b.m_params; }
    if (a.m_ret != b.m_ret) { return a.m_ret < b.m_ret; }
    if (a.m_templateParams.size() != b.m_templateParams.size()) { return a.m_templateParams.size() < b.m_templateParams.size(); }
    if (a.m_templateParams != b.m_templateParams) { return a.m_templateParams < b.m_templateParams; }
    // order below doesn't matter
    //
    if (a.m_numArgs != b.m_numArgs) { return a.m_numArgs < b.m_numArgs; }
    if (a.m_origRet != b.m_origRet) { return a.m_origRet < b.m_origRet; }
    if (a.m_origParams != b.m_origParams) { return a.m_origParams < b.m_origParams; }
    return a.m_mangledSymbolName < b.m_mangledSymbolName;
}

static void PrintFnParams(FILE* fp, const ParsedFnTypeNamesInfo& info, bool doNotPrintVarName = false)
{
    fprintf(fp, "(%s", (info.m_params.size() == 0 ? ")" : "\n"));
    for (size_t i = 0; i < info.m_params.size(); i++)
    {
        fprintf(fp, "        Value<%s>", info.m_params[i].c_str());
        if (!doNotPrintVarName)
        {
            fprintf(fp, " __pochivm_%d", static_cast<int>(i));
        }
        fprintf(fp, "%s", (i == info.m_params.size() - 1) ? ")" : ",\n");
    }
}

static void PrintFnTemplateParams(FILE* fp, const ParsedFnTypeNamesInfo& info)
{
    fprintf(fp, "<");
    for (size_t i = 0; i < info.m_templateParams.size(); i++)
    {
        fprintf(fp, "%s", info.m_templateParams[i].c_str());
        if (i != info.m_templateParams.size() - 1)
        {
            fprintf(fp, ", ");
        }
        else
        {
            fprintf(fp, ">");
        }
    }
}

static void GenerateCppRuntimeHeaderFile(const std::string& filename)
{
    FILE* fp = fopen(filename.c_str(), "w");
    if (fp == nullptr)
    {
        fprintf(stderr, "Failed to open file '%s' for write, errno = %d (%s)\n",
                filename.c_str(), errno, strerror(errno));
        abort();
    }

    fprintf(fp, "// GENERATED FILE, DO NOT EDIT!\n//\n\n");
    fprintf(fp, "#include \"runtime/pochivm_runtime_headers.h\"\n\n");

    fprintf(fp, "namespace Ast {\n\n");

    // generate all free functions
    //
    {
        fprintf(fp, "namespace CallFreeFn {\n\n");

        std::map<std::string /*ns*/, std::vector<ParsedFnTypeNamesInfo> > freeFns;

        for (auto it = g_symbolAddrToTypeData.begin(); it != g_symbolAddrToTypeData.end(); it++)
        {
            ParsedFnTypeNamesInfo& info = it->second;
            if (info.m_fnType == Ast::ReflectionHelper::FunctionType::FreeFn)
            {
                freeFns[info.m_prefix].push_back(info);
            }
        }

        for (auto it = freeFns.begin(); it != freeFns.end(); it++)
        {
            ReleaseAssert(it->second.size() > 0);
            std::string nsPrefix = it->first;
            std::vector<ParsedFnTypeNamesInfo>& data = it->second;
            std::sort(data.begin(), data.end(), CmpParsedFnTypeNamesInfo);

            if (nsPrefix != "")
            {
                fprintf(fp, "namespace %s {\n\n", nsPrefix.c_str());
            }

            size_t start = 0;
            while (start < data.size())
            {
                size_t end = start;
                while (end < data.size() &&
                       data[start].m_functionName == data[end].m_functionName &&
                       data[start].m_params == data[end].m_params &&
                       data[start].m_ret == data[end].m_ret &&
                       data[start].m_templateParams.size() == data[end].m_templateParams.size())
                {
                    end++;
                }
                if (data[start].m_templateParams.size() > MAX_TEMPLATE_PARAMS_FOR_TEMPLATED_FN)
                {
                    fprintf(stderr, "[Soft Lockdown] Function %s::%s contains more than %d template parameters. "
                                    "Adjust MAX_TEMPLATE_PARAMS_FOR_TEMPLATED_FN in dump_symbols.cpp if necessary.",
                            data[start].m_prefix.c_str(), data[start].m_functionName.c_str(),
                            static_cast<int>(MAX_TEMPLATE_PARAMS_FOR_TEMPLATED_FN));
                    abort();
                }
                size_t tplSize = data[start].m_templateParams.size();
                if (tplSize == 0)
                {
                    for (size_t k = start; k < end; k++)
                    {
                        ParsedFnTypeNamesInfo& info = data[k];
                        fprintf(fp, "// Original parameter and return types:\n");
                        for (const std::string& param : info.m_origParams)
                        {
                            fprintf(fp, "//          %s\n", param.c_str());
                        }
                        fprintf(fp, "// Returns: %s\n", info.m_origRet.c_str());
                        fprintf(fp, "//\n");
                        fprintf(fp, "inline Value<%s> %s", info.m_ret.c_str(), info.m_functionName.c_str());
                        PrintFnParams(fp, info);
                        fprintf(fp, "\n{\n");
                        fprintf(fp, "    static const char* __pochivm_sym = \"%s\";\n", info.m_mangledSymbolName.c_str());
                        fprintf(fp, "}\n\n");
                    }
                }
                else
                {
                    fprintf(fp, "// Template declaration header for templated function %s\n",
                            data[start].m_functionName.c_str());
                    fprintf(fp, "// # of template parameters: %d\n", static_cast<int>(tplSize));
                    fprintf(fp, "// Parameter types and return type:\n");
                    for (const std::string& param : data[start].m_params)
                    {
                        fprintf(fp, "//          %s\n", param.c_str());
                    }
                    fprintf(fp, "// Returns: %s\n", data[start].m_ret.c_str());
                    fprintf(fp, "//\n");

                    for (size_t bitmask = 0; bitmask < (static_cast<size_t>(1) << tplSize); bitmask++)
                    {
                        fprintf(fp, "template<");
                        for (size_t i = 0; i < tplSize; i++)
                        {
                            if (bitmask & (static_cast<size_t>(1) << i))
                            {
                                fprintf(fp, "typename");
                            }
                            else
                            {
                                fprintf(fp, "auto");
                            }
                            if (i != tplSize - 1)
                            {
                                fprintf(fp, ", ");
                            }
                            else
                            {
                                fprintf(fp, ">\n");
                            }
                        }
                        fprintf(fp, "Value<%s> %s", data[start].m_ret.c_str(), data[start].m_functionName.c_str());
                        PrintFnParams(fp, data[start], true /*doNotPrintVarName*/);
                        fprintf(fp, " = delete;\n");
                    }
                    fprintf(fp, "\n\n");

                    for (size_t k = start; k < end; k++)
                    {
                        ParsedFnTypeNamesInfo& info = data[k];
                        fprintf(fp, "// Original parameter and return types:\n");
                        for (const std::string& param : info.m_origParams)
                        {
                            fprintf(fp, "//          %s\n", param.c_str());
                        }
                        fprintf(fp, "// Returns: %s\n", info.m_origRet.c_str());
                        fprintf(fp, "//\n");
                        fprintf(fp, "template<>\n");
                        fprintf(fp, "inline Value<%s> %s", info.m_ret.c_str(), info.m_functionName.c_str());
                        PrintFnTemplateParams(fp, data[start]);
                        PrintFnParams(fp, data[start]);
                        fprintf(fp, "\n{\n");
                        fprintf(fp, "    static const char* __pochivm_sym = \"%s\";\n", info.m_mangledSymbolName.c_str());
                        fprintf(fp, "}\n\n");
                    }
                }
                start = end;
            }
            if (nsPrefix != "")
            {
                fprintf(fp, "\n} // namespace %s\n\n", nsPrefix.c_str());
            }
        }
    }
    fprintf(fp, "\n} // namespace CallFreeFn\n\n");

    // generate class member functions
    //
    {
        fprintf(fp, "// Class member functions\n//\n\n");
        std::map<std::string /*class*/, std::vector<ParsedFnTypeNamesInfo> > fns;

        for (auto it = g_symbolAddrToTypeData.begin(); it != g_symbolAddrToTypeData.end(); it++)
        {
            ParsedFnTypeNamesInfo& info = it->second;
            if (info.m_fnType != Ast::ReflectionHelper::FunctionType::FreeFn)
            {
                fns[info.m_prefix].push_back(info);
            }
        }

        for (auto it = fns.begin(); it != fns.end(); it++)
        {
            ReleaseAssert(it->second.size() > 0);
            std::string className = it->first;
            std::vector<ParsedFnTypeNamesInfo>& data = it->second;
            std::sort(data.begin(), data.end(), CmpParsedFnTypeNamesInfo);

            fprintf(fp, "template<> class Value<%s>\n{\npublic:\n", className.c_str());

            size_t start = 0;
            while (start < data.size())
            {
                size_t end = start;
                while (end < data.size() &&
                       data[start].m_fnType == data[end].m_fnType &&
                       data[start].m_functionName == data[end].m_functionName &&
                       data[start].m_params == data[end].m_params &&
                       data[start].m_ret == data[end].m_ret &&
                       data[start].m_templateParams.size() == data[end].m_templateParams.size())
                {
                    end++;
                }
                if (data[start].m_templateParams.size() > MAX_TEMPLATE_PARAMS_FOR_TEMPLATED_FN)
                {
                    fprintf(stderr, "[Soft Lockdown] Function %s::%s contains more than %d template parameters. "
                                    "Adjust MAX_TEMPLATE_PARAMS_FOR_TEMPLATED_FN in dump_symbols.cpp if necessary.",
                            data[start].m_prefix.c_str(), data[start].m_functionName.c_str(),
                            static_cast<int>(MAX_TEMPLATE_PARAMS_FOR_TEMPLATED_FN));
                    abort();
                }
                size_t tplSize = data[start].m_templateParams.size();
                if (tplSize == 0)
                {
                    for (size_t k = start; k < end; k++)
                    {
                        ParsedFnTypeNamesInfo& info = data[k];
                        fprintf(fp, "// Original parameter and return types:\n");
                        for (const std::string& param : info.m_origParams)
                        {
                            fprintf(fp, "//          %s\n", param.c_str());
                        }
                        fprintf(fp, "// Returns: %s\n", info.m_origRet.c_str());
                        fprintf(fp, "//\n");
                        fprintf(fp, "%sValue<%s> %s",
                                (info.m_fnType == Ast::ReflectionHelper::FunctionType::StaticMemberFn ? "static " : ""),
                                info.m_ret.c_str(), info.m_functionName.c_str());
                        PrintFnParams(fp, info);
                        fprintf(fp, ";\n");
                    }
                }
                else
                {
                    fprintf(fp, "// Template declaration header for templated function %s\n",
                            data[start].m_functionName.c_str());
                    fprintf(fp, "// # of template parameters: %d\n", static_cast<int>(tplSize));
                    fprintf(fp, "// Parameter types and return type:\n");
                    for (const std::string& param : data[start].m_params)
                    {
                        fprintf(fp, "//          %s\n", param.c_str());
                    }
                    fprintf(fp, "// Returns: %s\n", data[start].m_ret.c_str());
                    fprintf(fp, "//\n");

                    for (size_t bitmask = 0; bitmask < (static_cast<size_t>(1) << tplSize); bitmask++)
                    {
                        fprintf(fp, "template<");
                        for (size_t i = 0; i < tplSize; i++)
                        {
                            if (bitmask & (static_cast<size_t>(1) << i))
                            {
                                fprintf(fp, "typename");
                            }
                            else
                            {
                                fprintf(fp, "auto");
                            }
                            if (i != tplSize - 1)
                            {
                                fprintf(fp, ", ");
                            }
                            else
                            {
                                fprintf(fp, ">\n");
                            }
                        }
                        fprintf(fp, "%sValue<%s> %s",
                                (data[start].m_fnType == Ast::ReflectionHelper::FunctionType::StaticMemberFn ? "static " : ""),
                                data[start].m_ret.c_str(), data[start].m_functionName.c_str());
                        PrintFnParams(fp, data[start], true /*doNotPrintVarName*/);
                        fprintf(fp, " = delete;\n");
                    }
                    fprintf(fp, "\n");
                }
                start = end;
            }

            fprintf(fp, "}; // class Value<%s>\n\n", className.c_str());

            for (size_t k = 0; k < data.size(); k++)
            {
                ParsedFnTypeNamesInfo& info = data[k];
                fprintf(fp, "// Original parameter and return types:\n");
                for (const std::string& param : info.m_origParams)
                {
                    fprintf(fp, "//          %s\n", param.c_str());
                }
                fprintf(fp, "// Returns: %s\n", info.m_origRet.c_str());
                fprintf(fp, "//\n");
                size_t tplSize = info.m_templateParams.size();
                if (tplSize > 0)
                {
                    fprintf(fp, "template<>\n");
                }
                fprintf(fp, "inline Value<%s> Value<%s>::%s",
                        info.m_ret.c_str(), info.m_prefix.c_str(), info.m_functionName.c_str());
                if (tplSize > 0)
                {
                    PrintFnTemplateParams(fp, info);
                }
                PrintFnParams(fp, info);
                fprintf(fp, "\n{\n");
                fprintf(fp, "    static const char* __pochivm_sym = \"%s\";\n", info.m_mangledSymbolName.c_str());
                fprintf(fp, "}\n\n");
            }
        }
    }

    fprintf(fp, "\n} // namespace Ast\n\n");
    fclose(fp);
}

}   // anonymous namespace

int main(int argc, char** argv)
{
    InitLLVM _init_llvm_(argc, argv);

    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();

    // --dump [bc_file] [symbol_list_output]
    //    Used on a bc file. Dumps all the global symbols it defined to symbol_list_output
    //

    // --dump-list [bc_file] [symbol_list_output] [needed_symbols_info_output] [generated .h output]
    //    Used on '__pochivm_runtime__.bc'. In addition to '--dump', it attempts to
    //    match the symbols described in runtime_ops.txt and the function stubs generated from generate_stub.cpp,
    //    and output the IR information of those symbols to 'needed_symbols_info_output'.
    //

    ReleaseAssert(argc >= 2);
    std::string param1 = argv[1];
    ReleaseAssert(param1 == "--dump" || param1 == "--dump-list");

    if (param1 == "--dump") { ReleaseAssert(argc == 4); } else { ReleaseAssert(argc == 6); }

    bool isDumpList = (param1 == "--dump-list");

    std::string bcFileName = argv[2];
    ReleaseAssert(bcFileName.substr(bcFileName.length() - 3, 3) == ".bc");
    std::string optimizedBcFileName = bcFileName.substr(0, bcFileName.length() - 3) + ".optimized.bc";

    SMDiagnostic llvmErr;
    std::unique_ptr<LLVMContext> context(new LLVMContext);
    std::unique_ptr<Module> module = parseIRFile(bcFileName, llvmErr, *context.get());

    if (module == nullptr)
    {
        fprintf(stderr, "[ERROR] An error occurred while parsing IR file '%s', detail:\n", bcFileName.c_str());
        llvmErr.print(argv[0], errs());
        abort();
    }

    // Strip debug info if requested
    // DEVNOTE: if someday we fixed the CMake issue and removed the optimize pass below,
    // we still need to make sure the module passed to update_symbol_matches.cpp has debug info stripped.
    //
    if (!OPTION_KEEP_DEBUG_INFO)
    {
        StripDebugInfo(*module.get());
    }

    // DEVNOTE: clang++ --save-temps outputs unoptimized IR file (even if -O3 is specified)...
    // For now, we will just run optimize and save it to xxx.optimized.bc.
    //
    RunLLVMOptimizePassIfNotDebug(module.get());

    {
        int fd = creat(optimizedBcFileName.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (fd == -1)
        {
            fprintf(stderr, "Failed to open file '%s' for write, errno = %d (%s)\n",
                    optimizedBcFileName.c_str(), errno, strerror(errno));
            abort();
        }
        raw_fd_ostream fdStream(fd, true /*shouldClose*/);
        WriteBitcodeToFile(*module.get(), fdStream);
        if (fdStream.has_error())
        {
            std::error_code ec = fdStream.error();
            fprintf(stderr, "Writing to file '%s' failed with errno = %d (%s)\n",
                    optimizedBcFileName.c_str(), ec.value(), ec.message().c_str());
            abort();
        }
        /* fd closed when fdStream is destructed here */
    }

    {
        // Load the optimized module from file, just for sanity
        //
        std::unique_ptr<LLVMContext> newContext(new LLVMContext);
        std::unique_ptr<Module> newModule = parseIRFile(optimizedBcFileName, llvmErr, *newContext.get());

        if (newModule == nullptr)
        {
            fprintf(stderr, "[ERROR] An error occurred while parsing IR file '%s', detail:\n", optimizedBcFileName.c_str());
            llvmErr.print(argv[0], errs());
            abort();
        }
        module = std::move(newModule);
        context = std::move(newContext);
    }

    // Iterate the IR file and pick up all the function name symbols
    //
    std::vector<std::string> allDefinitions, allDeclarations;
    for (Function& f : module->functions())
    {
        // Ignore private linkage functions (internal/private).
        // Weak/LinkOnce should not be possible for C++ (should always be the ODR variant)
        // Appending/common linkage is not possible for functions.
        //
        if (f.getLinkage() == GlobalValue::LinkageTypes::LinkOnceODRLinkage ||
            f.getLinkage() == GlobalValue::LinkageTypes::WeakODRLinkage ||
            f.getLinkage() == GlobalValue::LinkageTypes::ExternalLinkage ||
            f.getLinkage() == GlobalValue::LinkageTypes::ExternalWeakLinkage ||
            f.getLinkage() == GlobalValue::LinkageTypes::AvailableExternallyLinkage)
        {
            std::string name = f.getName();
            // Empty functions are declarations
            //
            if (!f.empty())
            {
                allDefinitions.push_back(name);
            }
            allDeclarations.push_back(name);
        }
        else if (f.getLinkage() == GlobalValue::LinkageTypes::WeakAnyLinkage ||
                 f.getLinkage() == GlobalValue::LinkageTypes::LinkOnceAnyLinkage ||
                 f.getLinkage() == GlobalValue::LinkageTypes::AppendingLinkage ||
                 f.getLinkage() == GlobalValue::LinkageTypes::CommonLinkage)
        {
            fprintf(stderr, "[WARNING] bitcode '%s' function '%s' has unexpected linkage type %d, function ignored.\n",
                    bcFileName.c_str(), f.getName().str().c_str(), static_cast<int>(f.getLinkage()));
        }
        else
        {
            ReleaseAssert(f.getLinkage() == GlobalValue::LinkageTypes::PrivateLinkage ||
                          f.getLinkage() == GlobalValue::LinkageTypes::InternalLinkage);
        }
    }

    std::string symbolListOutputFilename = std::string(argv[3]);
    std::string symbolListOutputFilenameTmp = symbolListOutputFilename + std::string(".tmp");
    std::string symbolMatchesOutputFile = symbolListOutputFilename + ".matches";

    // Clear *.syms.matches file, it is no longer valid after the IR changes
    //
    {
        FILE* fp = fopen(symbolMatchesOutputFile.c_str(), "w");
        if (fp == nullptr)
        {
            fprintf(stderr, "Failed to open file '%s' for write, errno = %d (%s)\n",
                    symbolListOutputFilenameTmp.c_str(), errno, strerror(errno));
            abort();
        }
        fclose(fp);
    }

    // Write symbol list file, we should only write defintions
    //
    {
        std::set<std::string> symbolsOut;
        for (const std::string& symbol : allDefinitions)
        {
            ReleaseAssert(!symbolsOut.count(symbol));
            symbolsOut.insert(symbol);
        }
        WriteSymbolListFileOrDie(symbolListOutputFilenameTmp, symbolsOut);
    }

    // Beyond this point the IR module is gone: we move it to the JIT
    //
    ExitOnError exitOnError;
    ThreadSafeModule tsm(std::move(module), std::move(context));

    if (isDumpList)
    {
        // JIT execution function __pochivm_register_runtime_library__ to figure out
        // which functions are needed by the runtime library, and their detailed type information
        //
        std::unique_ptr<LLJIT> J = exitOnError(LLJITBuilder().create());

        char Prefix = EngineBuilder().selectTarget()->createDataLayout().getGlobalPrefix();
        std::unique_ptr<DynamicLibrarySearchGenerator> R = exitOnError(DynamicLibrarySearchGenerator::GetForCurrentProcess(Prefix));
        ReleaseAssert(R != nullptr);

        // For undefined symbol in the IR file, first try to resolve in host process (it
        // contains symbols from system library and our hook __pochivm_report_info__).
        // If failed, it should be a symbol implemented in other CPP files,
        // and we should only care about its address (and never actually need to invoke it),
        // so just resolve it to an unique address using our FakeSymbolResolver.
        //
        J->getMainJITDylib().addGenerator(std::move(R));
        J->getMainJITDylib().addGenerator(std::unique_ptr<FakeSymbolResolver>(new FakeSymbolResolver()));

        exitOnError(J->addIRModule(std::move(tsm)));

        auto entryPointSym = exitOnError(J->lookup("__pochivm_register_runtime_library__"));
        using _FnPrototype = void(*)(void);
        _FnPrototype entryPoint = reinterpret_cast<_FnPrototype>(entryPointSym.getAddress());
        entryPoint();

        // map from mangled symbol name to demangled name
        //
        std::map<std::string, std::string> demangledSymbols;

        {
            std::string symbolListOutputFileCppFilt = symbolListOutputFilename + std::string(".cppfilt");
            std::string symbolListinputFileCppFilt = symbolListOutputFilename + std::string(".cppfilt.input");

            std::set<std::string> orig;
            for (const std::string& symbol : allDeclarations)
            {
                ReleaseAssert(!orig.count(symbol));
                orig.insert(symbol);
            }

            WriteSymbolListFileOrDie(symbolListinputFileCppFilt, orig);

            // Generate the corresponding list of demangled names
            //
            {
                std::string cmd = std::string("c++filt -n < ") + symbolListinputFileCppFilt + " > " + symbolListOutputFileCppFilt;
                int r = system(cmd.c_str());
                if (r != 0)
                {
                    fprintf(stderr, "Command '%s' failed with return code %d\n", cmd.c_str(), r);
                    abort();
                }
            }

            std::vector<std::string> demangled;
            ReadCppFiltOutput(symbolListOutputFileCppFilt, demangled /*out*/);
            ReleaseAssert(orig.size() == demangled.size());
            size_t index = 0;
            for (const std::string& v : orig)
            {
                demangledSymbols[v] = demangled[index++];
            }
            ReleaseAssert(demangledSymbols.size() == allDeclarations.size());
        }

        // Match the type defintion with the symbol name by symbol address.
        //
        std::map<uintptr_t, std::pair< std::string /*mangled*/, std::string /*demangled*/> > addrToSymbol;

        for (std::string& symbol : allDeclarations)
        {
            auto sym = exitOnError(J->lookup(symbol));
            uintptr_t addr = sym.getAddress();
            // Craziness: we cannot assert(!addrToSymbol.count(addr))....
            // Sometimes two symbols with different names can actually
            // resolve to the same address. For example, symbol '_ZNSaIcED1Ev'
            // and symbol '_ZNSaIcED2Ev' (the 3rd character to the right differs)
            // turns out to have the same demangled name 'std::allocator<char>::~allocator()'
            // and have the same address pointer in host space. I have no idea why
            // this could happen, but this should never happen to the functions registered
            // for the runtime library. In the worst case that it did somehow happened,
            // we will trace a warning (in our __pochivm_report_info__ hook) and all
            // but the first function prototype would be ignored with nothing else bad happening
            // (other than when the user attempted to use the ignored prototype he gets a compile
            // error), so it should be safe.
            //
            addrToSymbol[addr] = std::make_pair(symbol, demangledSymbols[symbol]);
        }

        for (auto it = g_symbolAddrToTypeData.begin(); it != g_symbolAddrToTypeData.end(); it++)
        {
            uintptr_t addr = reinterpret_cast<uintptr_t>(it->first);
            ReleaseAssert(addrToSymbol.count(addr));
            std::pair<std::string, std::string>& symbolPair = addrToSymbol[addr];
            it->second.FindTemplateParameters(symbolPair.second /*demangled*/);
            it->second.RecordMangledSymbolName(symbolPair.first /*mangled*/);
        }

        // Write 'needed_symbols_info_output'
        //
        {
            std::set<std::string> symbolsOut;
            for (auto it = g_symbolAddrToTypeData.begin(); it != g_symbolAddrToTypeData.end(); it++)
            {
                std::string name = it->second.m_mangledSymbolName;
                ReleaseAssert(!symbolsOut.count(name));
                symbolsOut.insert(name);
            }

            std::string neededSymbolOutputFile = std::string(argv[4]);
            WriteSymbolListFileOrDie(neededSymbolOutputFile, symbolsOut);
        }

        std::string generatedHeaderOutput = std::string(argv[5]);
        GenerateCppRuntimeHeaderFile(generatedHeaderOutput);
    }

    // We wrote to a tmp file and rename it here in the end, to give all or nothing guarantee:
    // either the output file timestamp is unchanged, or the output file is updated with content integrity guaranteed.
    // This prevents us from breaking Makefile timestamp system if we are interrupted in the middle.
    //
    int r = rename(symbolListOutputFilenameTmp.c_str(), symbolListOutputFilename.c_str());
    ReleaseAssert(r == 0 || r == -1);
    if (r == -1)
    {
        fprintf(stderr, "Failed to rename file '%s' into '%s', errno = %d (%s)\n",
                symbolListOutputFilenameTmp.c_str(), symbolListOutputFilename.c_str(), errno, strerror(errno));
        abort();
    }
    return 0;
}