#include <iomanip>
#include <sstream>

#include "pochivm/common.h"
#include "pochivm/pochivm_reflection_helper.h"
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

    ParsedFnTypeNamesInfo(PochiVM::ReflectionHelper::RawFnTypeNamesInfo* src)
    {
        m_numArgs = src->m_numArgs;
        m_origRet = ParseTypeName(src->m_originalRetAndArgTypenames[0]);
        m_ret = ParseTypeName(src->m_apiRetAndArgTypenames[0].first);
        m_isRetApiVar = src->m_apiRetAndArgTypenames[0].second;
        for (size_t i = 1; i <= src->m_numArgs; i++)
        {
            m_origParams.push_back(ParseTypeName(src->m_originalRetAndArgTypenames[i]));
            m_params.push_back(ParseTypeName(src->m_apiRetAndArgTypenames[i].first));
            m_isParamsApiVar.push_back(src->m_apiRetAndArgTypenames[i].second);
        }
        m_fnType = src->m_fnType;
        if (m_fnType == PochiVM::ReflectionHelper::FunctionType::NonStaticMemberFn)
        {
            std::string fnName = ParseValueName(src->m_fnName);
            std::string className = ParseTypeName(src->m_classTypename);
            ReleaseAssert(fnName.length() > className.length() + 2);
            ReleaseAssert(fnName.substr(0, className.length()) == className);
            ReleaseAssert(fnName[className.length()] == ':');
            ReleaseAssert(fnName[className.length() + 1] == ':');
            m_prefix = className;
            m_functionName = fnName.substr(className.length() + 2);
        }
        else if (m_fnType == PochiVM::ReflectionHelper::FunctionType::Constructor ||
                 m_fnType == PochiVM::ReflectionHelper::FunctionType::Destructor)
        {
            std::string className = ParseTypeName(src->m_classTypename);
            m_prefix = className;
            m_functionName = "";
        }
        else
        {
            std::string fnName = ParseValueName(src->m_fnName);
            int k = static_cast<int>(fnName.length()) - 1;
            while (k >= 0 && fnName[static_cast<size_t>(k)] != ':')
            {
                k--;
            }
            m_functionName = fnName.substr(static_cast<size_t>(k + 1));
            ReleaseAssert(k == -1 || k >= 1);
            if (k == -1)
            {
                ReleaseAssert(m_fnType == PochiVM::ReflectionHelper::FunctionType::FreeFn);
                m_prefix = "";
            }
            else
            {
                ReleaseAssert(fnName[static_cast<size_t>(k - 1)] == ':');
                m_prefix = fnName.substr(0, static_cast<size_t>(k - 1));
            }
        }
        m_isConst = src->m_isConst;
        m_isNoExcept = src->m_isNoExcept;

        m_isUsingWrapper = src->m_isUsingWrapper;
        m_isWrapperUsingSret = src->m_isWrapperUsingSret;
        m_wrapperFnAddress = src->m_wrapperFnAddress;
    }

    void RecordMangledSymbolName(const std::string& mangledSymbolName)
    {
        m_mangledSymbolName = mangledSymbolName;
    }

    void RecordWrapperFnMangledSymbolName(const std::string& mangledSymbolName)
    {
        m_wrapperFnMangledSymbolName = mangledSymbolName;
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
            if (i + 1 <= lastPos - 1)
            {
                m_templateParams.push_back(demangledSymbolName.substr(i + 1, lastPos - i - 1));
            }
            else
            {
                // This is a variadic-number templated function which took no template parameters
                // e.g. the prototype is template<typename... Arg> f(), and the instantiation is f<>
                //
                ReleaseAssert(m_templateParams.size() == 0);
            }
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

    // Find out the class name from m_prefix
    //
    std::string FindClassName()
    {
        ReleaseAssert(m_fnType == PochiVM::ReflectionHelper::FunctionType::StaticMemberFn ||
                      m_fnType == PochiVM::ReflectionHelper::FunctionType::NonStaticMemberFn);
        ReleaseAssert(m_prefix.length() > 0);
        size_t i = m_prefix.length() - 1;
        while (i > 0 && isWhitespace(m_prefix[i])) { i--; }
        ReleaseAssert(i > 0);
        size_t rightBoundary = i;
        if (m_prefix[i] == '>')
        {
            // find out the matching '<', logic similar to FindTemplateParameters
            //
            int bracketDepth = 1;
            i--;
            while (i > 0)
            {
                if (m_prefix[i] == ')')
                {
                    // We hit a right parenthesis, temporarily switch to parenthesis matching mode.
                    // Find the left parenthesis, ignore anything (including brackets) in between.
                    //
                    int parenthesisDepth = 0;
                    while (i > 0)
                    {
                        if (m_prefix[i] == ')')
                        {
                            parenthesisDepth++;
                        }
                        else if (m_prefix[i] == '(')
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
                    ReleaseAssert(m_prefix[i] == '(');
                    ReleaseAssert(i > 0);
                }
                else if (m_prefix[i] == '>')
                {
                    bracketDepth++;
                }
                else if (m_prefix[i] == '<')
                {
                    bracketDepth--;
                    if (bracketDepth == 0)
                    {
                        break;
                    }
                }
                i--;
            }
            ReleaseAssert(bracketDepth == 0);
            ReleaseAssert(m_prefix[i] == '<');
            ReleaseAssert(i > 0);
            i--;
        }
        // Now we are at the class name (without template part),
        // scan left to find the '::' or a whitespace
        //
        while (i > 0 && m_prefix[i] != ':' && !isWhitespace(m_prefix[i])) { i--; }
        if (m_prefix[i] == ':' || isWhitespace(m_prefix[i]))
        {
            if (m_prefix[i] == ':')
            {
                ReleaseAssert(i > 0 && m_prefix[i-1] == ':');
            }
            i++;
        }
        // The class name is [i, rightBoundary]
        //
        return m_prefix.substr(i, rightBoundary - i + 1);
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

    PochiVM::ReflectionHelper::FunctionType m_fnType;
    size_t m_numArgs;
    // typenames of parameters, before and after transform
    //
    std::vector<std::string> m_origParams;
    std::vector<std::string> m_params;
    std::vector<bool> m_isParamsApiVar;
    // typename of return value, before and after transform
    //
    std::string m_origRet;
    std::string m_ret;
    bool m_isRetApiVar;

    // The prefix before the function
    // For member function this is namespace + class name,
    // for free function this is only namespace
    // for constructor/destructor this is the class name (namespace + class name)
    //
    std::string m_prefix;
    // name of function
    //
    std::string m_functionName;
    // If the function is a templated function, the template parameters
    //
    std::vector<std::string> m_templateParams;
    std::string m_mangledSymbolName;
    bool m_isConst;
    bool m_isNoExcept;

    bool m_isUsingWrapper;
    bool m_isWrapperUsingSret;
    void* m_wrapperFnAddress;
    std::string m_wrapperFnMangledSymbolName;
};

static std::map<void*, ParsedFnTypeNamesInfo> g_symbolAddrToTypeData;

}   // anonymous namespace

void PochiVM::__pochivm_report_info__(PochiVM::ReflectionHelper::RawFnTypeNamesInfo* info)
{
    if (!g_symbolAddrToTypeData.count(info->m_fnAddress))
    {
        g_symbolAddrToTypeData[info->m_fnAddress] = ParsedFnTypeNamesInfo(info);
    }
    else
    {
        if (info->m_fnType != PochiVM::ReflectionHelper::FunctionType::Destructor)
        {
            fprintf(stderr, "[WARNING] function %s::%s appeared to be registered more than once, ignored multiple occurrance.\n",
                    (info->m_classTypename != nullptr ? info->m_classTypename : ""), info->m_fnName);
        }
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

static bool CmpParsedFnTypeNamesInfo(const ParsedFnTypeNamesInfo& a, const ParsedFnTypeNamesInfo& b)
{
    if (a.m_prefix != b.m_prefix) { return a.m_prefix < b.m_prefix; }
    if (a.m_fnType != b.m_fnType) { return a.m_fnType < b.m_fnType; }
    if (a.m_functionName != b.m_functionName) { return a.m_functionName < b.m_functionName; }
    if (a.m_params != b.m_params) { return a.m_params < b.m_params; }
    if (a.m_isParamsApiVar != b.m_isParamsApiVar) { return a.m_isParamsApiVar < b.m_isParamsApiVar; }
    if (a.m_ret != b.m_ret) { return a.m_ret < b.m_ret; }
    if (a.m_isRetApiVar != b.m_isRetApiVar) { return a.m_isRetApiVar < b.m_isRetApiVar; }
    if (a.m_templateParams.size() != b.m_templateParams.size()) { return a.m_templateParams.size() < b.m_templateParams.size(); }
    if (a.m_templateParams != b.m_templateParams) { return a.m_templateParams < b.m_templateParams; }
    // order below doesn't matter
    //
    if (a.m_numArgs != b.m_numArgs) { return a.m_numArgs < b.m_numArgs; }
    if (a.m_origRet != b.m_origRet) { return a.m_origRet < b.m_origRet; }
    if (a.m_origParams != b.m_origParams) { return a.m_origParams < b.m_origParams; }
    if (a.m_mangledSymbolName != b.m_mangledSymbolName) { return a.m_mangledSymbolName < b.m_mangledSymbolName; }
    if (a.m_isConst != b.m_isConst) { return a.m_isConst < b.m_isConst; }
    if (a.m_isNoExcept != b.m_isNoExcept) { return a.m_isNoExcept < b.m_isNoExcept; }
    if (a.m_isUsingWrapper != b.m_isUsingWrapper) { return a.m_isUsingWrapper < b.m_isUsingWrapper; }
    if (a.m_isWrapperUsingSret != b.m_isWrapperUsingSret) { return a.m_isWrapperUsingSret < b.m_isWrapperUsingSret; }
    if (a.m_wrapperFnAddress != b.m_wrapperFnAddress) { return a.m_wrapperFnAddress < b.m_wrapperFnAddress; }
    return a.m_wrapperFnMangledSymbolName < b.m_wrapperFnMangledSymbolName;
}

static void PrintFnParams(FILE* fp, const ParsedFnTypeNamesInfo& info, bool doNotPrintVarName = false, size_t firstParam = 0)
{
    ReleaseAssert(info.m_params.size() >= firstParam);
    fprintf(fp, "(%s", (info.m_params.size() == firstParam ? ")" : "\n"));
    for (size_t i = firstParam; i < info.m_params.size(); i++)
    {
        fprintf(fp, "        %s<%s>", (info.m_isParamsApiVar[i] ? "Reference" : "Value"), info.m_params[i].c_str());
        if (!doNotPrintVarName)
        {
            fprintf(fp, " __pochivm_%d", static_cast<int>(i - firstParam));
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

static int g_curUniqueFunctionOrdinal = 0;

static void PrintFnCallBody(FILE* fp, const ParsedFnTypeNamesInfo& info)
{
    ReleaseAssert(info.m_fnType != PochiVM::ReflectionHelper::FunctionType::Constructor &&
                  info.m_fnType != PochiVM::ReflectionHelper::FunctionType::Destructor);
    fprintf(fp, "{\n");
    int numParams = static_cast<int>(info.m_params.size());
    if (info.m_fnType == PochiVM::ReflectionHelper::FunctionType::NonStaticMemberFn)
    {
        numParams++;
    }
    fprintf(fp, "    static constexpr TypeId __pochivm_cpp_fn_params[%d] = {", numParams);
    if (info.m_fnType == PochiVM::ReflectionHelper::FunctionType::NonStaticMemberFn)
    {
        fprintf(fp, "\n        TypeId::Get<typename std::add_pointer<%s>::type>()", info.m_prefix.c_str());
        if (info.m_params.size() > 0)
        {
            fprintf(fp, ",");
        }
        else
        {
            fprintf(fp, "\n    ");
        }
    }
    for (size_t i = 0; i < info.m_params.size(); i++)
    {
        fprintf(fp, "\n        TypeId::Get<%s%s%s>()",
                (info.m_isParamsApiVar[i] ? "typename std::add_pointer<" : ""),
                info.m_params[i].c_str(),
                (info.m_isParamsApiVar[i] ? ">::type" : ""));
        if (i != info.m_params.size() - 1)
        {
            fprintf(fp, ",");
        }
        else
        {
            fprintf(fp, "\n    ");
        }
    }
    fprintf(fp, "};\n");
    fprintf(fp, "    static constexpr TypeId __pochivm_cpp_fn_ret = TypeId::Get<%s%s%s>();\n",
            (info.m_isRetApiVar ? "typename std::add_pointer<" : ""),
            info.m_ret.c_str(),
            (info.m_isRetApiVar ? ">::type" : ""));

    if (info.m_fnType == PochiVM::ReflectionHelper::FunctionType::NonStaticMemberFn)
    {
        fprintf(fp, "    using __pochivm_func_t = %s(%s::*)(", info.m_origRet.c_str(), info.m_prefix.c_str());
    }
    else
    {
        fprintf(fp, "    using __pochivm_func_t = %s(*)(", info.m_origRet.c_str());
    }
    for (size_t i = 0; i < info.m_params.size(); i++)
    {
        fprintf(fp, "\n        %s", info.m_origParams[i].c_str());
        if (i != info.m_params.size() - 1)
        {
            fprintf(fp, ",");
        }
        else
        {
            fprintf(fp, "\n    ");
        }
    }
    fprintf(fp, ")%s%s;\n", (info.m_isConst ? " const" : ""), (info.m_isNoExcept ? " noexcept" : ""));

    fprintf(fp, "    using __pochivm_wrapper_generator_t = ReflectionHelper::function_wrapper_helper<\n");
    fprintf(fp, "            static_cast<__pochivm_func_t>(\n");
    fprintf(fp, "                    ");
    if (info.m_prefix == "")
    {
        fprintf(fp, "&::%s", info.m_functionName.c_str());
    }
    else
    {
        fprintf(fp, "&::%s::%s", info.m_prefix.c_str(), info.m_functionName.c_str());
    }
    if (info.m_templateParams.size() > 0)
    {
        PrintFnTemplateParams(fp, info);
    }
    fprintf(fp, "\n");
    fprintf(fp, "            )\n");
    fprintf(fp, "        >;\n");
    fprintf(fp, "    using __pochivm_wrapper_fn_t = typename __pochivm_wrapper_generator_t::WrapperFnPtrType;\n");

    bool isUsingSret = info.m_isUsingWrapper && info.m_isWrapperUsingSret;
    size_t offset = 0;
    fprintf(fp, "    using __pochivm_wrapper_fn_typeinfo = AstTypeHelper::function_type_helper<__pochivm_wrapper_fn_t>;\n");

    if (isUsingSret)
    {
        ReleaseAssert(!info.m_isRetApiVar);
        fprintf(fp, "    static_assert(TypeId::Get<typename ReflectionHelper::recursive_remove_cv<typename __pochivm_wrapper_fn_typeinfo::ReturnType>::type>() ==\n");
        fprintf(fp, "                  TypeId::Get<void>(), \"unexpected return type\");\n");

        fprintf(fp, "    static_assert(TypeId::Get<typename ReflectionHelper::recursive_remove_cv<typename __pochivm_wrapper_fn_typeinfo::ArgType<%d>>::type>() ==\n",
                static_cast<int>(offset));
        fprintf(fp, "                  __pochivm_cpp_fn_ret.AddPointer(), \"unexpected param type (sret)\");\n");
        offset++;
    }
    else
    {
        fprintf(fp, "    static_assert(TypeId::Get<typename ReflectionHelper::recursive_remove_cv<typename __pochivm_wrapper_fn_typeinfo::ReturnType>::type>() ==\n");
        fprintf(fp, "                  __pochivm_cpp_fn_ret, \"unexpected return type\");\n");
    }

    for (int i = 0; i < numParams; i++)
    {
        fprintf(fp, "    static_assert(TypeId::Get<typename ReflectionHelper::recursive_remove_cv<typename __pochivm_wrapper_fn_typeinfo::ArgType<%d>>::type>() ==\n",
                static_cast<int>(offset));
        fprintf(fp, "                  __pochivm_cpp_fn_params[%d], ", i);
        if (info.m_fnType == PochiVM::ReflectionHelper::FunctionType::NonStaticMemberFn)
        {
            if (i == 0)
            {
                fprintf(fp, "\"unexpected param type ('this' pointer)\");\n");
            }
            else
            {
                fprintf(fp, "\"unexpected param type (param %d)\");\n", i - 1);
            }
        }
        else
        {
            fprintf(fp, "\"unexpected param type (param %d)\");\n", i);
        }
        offset++;
    }
    fprintf(fp, "    static_assert(__pochivm_wrapper_fn_typeinfo::numArgs == %d, \"unexpected number of arguments\");\n", static_cast<int>(offset));

    fprintf(fp, "    static constexpr CppFunctionMetadata __pochivm_cpp_fn_metadata = {\n");
    std::string varname = std::string("__pochivm_internal_bc_") + GetUniqueSymbolHash(info.m_mangledSymbolName);
    fprintf(fp, "        &%s,\n", varname.c_str());
    fprintf(fp, "        __pochivm_cpp_fn_params,\n");
    fprintf(fp, "        %d /*numParams*/,\n", numParams);
    fprintf(fp, "        __pochivm_cpp_fn_ret /*returnType*/,\n");
    fprintf(fp, "        %s /*isUsingSret*/,\n", (isUsingSret ? "true" : "false"));
    fprintf(fp, "        AstTypeHelper::interp_call_cpp_fn_helper<__pochivm_wrapper_generator_t::wrapperFn>::interpFn /*interpFn*/,\n");
    fprintf(fp, "        %d /*uniqueFunctionOrdinal*/,\n", g_curUniqueFunctionOrdinal);
    g_curUniqueFunctionOrdinal++;
    fprintf(fp, "    };\n");

    fprintf(fp, "    return %s<%s>(new AstCallExpr(\n",
            (info.m_isRetApiVar ? "Reference" : "Value"), info.m_ret.c_str());
    fprintf(fp, "            &__pochivm_cpp_fn_metadata,\n");
    fprintf(fp, "            std::vector<AstNodeBase*>{");
    if (info.m_fnType == PochiVM::ReflectionHelper::FunctionType::NonStaticMemberFn)
    {
        fprintf(fp, "\n                m_refPtr");
        if (info.m_params.size() > 0)
        {
            fprintf(fp, ",");
        }
        else
        {
            fprintf(fp, "\n            ");
        }
    }
    for (size_t i = 0; i < info.m_params.size(); i++)
    {
        fprintf(fp, "\n                __pochivm_%d.%s",
                static_cast<int>(i), (info.m_isParamsApiVar[i] ? "m_refPtr" : "m_ptr"));
        if (i != info.m_params.size() - 1)
        {
            fprintf(fp, ",");
        }
        else
        {
            fprintf(fp, "\n            ");
        }
    }
    fprintf(fp, "}\n");
    fprintf(fp, "    ));\n");
    fprintf(fp, "}\n\n");
}

static void PrintConstructorFnCallBody(FILE* fp, const ParsedFnTypeNamesInfo& info)
{
    ReleaseAssert(info.m_fnType == PochiVM::ReflectionHelper::FunctionType::Constructor);
    fprintf(fp, "{\n");
    ReleaseAssert(info.m_params.size() > 0);
    int numCtorParams = static_cast<int>(info.m_params.size()) - 1;
    fprintf(fp, "    static constexpr TypeId __pochivm_cpp_fn_params[%d] = {", static_cast<int>(info.m_params.size()));
    for (size_t i = 0; i < info.m_params.size(); i++)
    {
        fprintf(fp, "\n        TypeId::Get<%s%s%s>()",
                (info.m_isParamsApiVar[i] ? "typename std::add_pointer<" : ""),
                info.m_params[i].c_str(),
                (info.m_isParamsApiVar[i] ? ">::type" : ""));
        if (i != info.m_params.size() - 1)
        {
            fprintf(fp, ",");
        }
        else
        {
            fprintf(fp, "\n    ");
        }
    }
    fprintf(fp, "};\n");
    fprintf(fp, "    static constexpr TypeId __pochivm_cpp_fn_ret = TypeId::Get<%s%s%s>();\n",
            (info.m_isRetApiVar ? "typename std::add_pointer<" : ""),
            info.m_ret.c_str(),
            (info.m_isRetApiVar ? ">::type" : ""));
    fprintf(fp, "    using __pochivm_wrapper_t = ReflectionHelper::constructor_wrapper_helper<");
    for (size_t i = 0; i < info.m_params.size(); i++)
    {
        fprintf(fp, "\n        %s%s%s",
                (i == 0 ? "typename std::remove_pointer<" : ""),
                info.m_origParams[i].c_str(),
                (i == 0 ? ">::type" : ""));
        if (i != info.m_params.size() - 1)
        {
            fprintf(fp, ",");
        }
        else
        {
            fprintf(fp, "\n    ");
        }
    }
    fprintf(fp, ">;\n");

    fprintf(fp, "    using __pochivm_wrapper_fn_typeinfo = AstTypeHelper::function_type_helper<__pochivm_wrapper_t::WrapperFnPtrType>;\n");

    for (int i = 0; i < static_cast<int>(info.m_params.size()); i++)
    {
        fprintf(fp, "    static_assert(TypeId::Get<typename ReflectionHelper::recursive_remove_cv<typename __pochivm_wrapper_fn_typeinfo::ArgType<%d>>::type>() ==\n",
                i);
        fprintf(fp, "                  __pochivm_cpp_fn_params[%d], ", i);
        fprintf(fp, "\"unexpected param type (constructor param %d)\");\n", i);
    }
    fprintf(fp, "    static_assert(__pochivm_cpp_fn_ret == TypeId::Get<void>(), \"unexpected return type\");\n");
    fprintf(fp, "    static_assert(__pochivm_wrapper_fn_typeinfo::numArgs == %d + 1, \"unexpected number of arguments\");\n", numCtorParams);

    fprintf(fp, "    static constexpr CppFunctionMetadata __pochivm_cpp_fn_metadata = {\n");
    std::string varname = std::string("__pochivm_internal_bc_") + GetUniqueSymbolHash(info.m_mangledSymbolName);
    fprintf(fp, "        &%s,\n", varname.c_str());
    fprintf(fp, "        __pochivm_cpp_fn_params,\n");
    fprintf(fp, "        %d /*numParams*/,\n", static_cast<int>(info.m_params.size()));
    fprintf(fp, "        __pochivm_cpp_fn_ret /*returnType*/,\n");
    fprintf(fp, "        false /*isUsingSret*/,\n");
    fprintf(fp, "        AstTypeHelper::interp_call_cpp_fn_helper<__pochivm_wrapper_t::wrapperFn>::interpFn /*interpFn*/,\n");
    fprintf(fp, "        %d /*uniqueFunctionOrdinal*/,\n", g_curUniqueFunctionOrdinal);
    g_curUniqueFunctionOrdinal++;
    fprintf(fp, "    };\n");

    fprintf(fp, "    m_constructorMd = &__pochivm_cpp_fn_metadata;\n");
    fprintf(fp, "    m_params = std::vector<AstNodeBase*>{");
    for (size_t i = 0; i < static_cast<size_t>(numCtorParams); i++)
    {
        fprintf(fp, "\n        __pochivm_%d.%s",
                static_cast<int>(i), (info.m_isParamsApiVar[i + 1] ? "m_refPtr" : "m_ptr"));
        if (i != static_cast<size_t>(numCtorParams) - 1)
        {
            fprintf(fp, ",");
        }
        else
        {
            fprintf(fp, "\n    ");
        }
    }
    fprintf(fp, "};\n");
    fprintf(fp, "}\n\n");
}

static void PrintBoilerplateMethods(FILE* fp, const std::string& className)
{
    fprintf(fp, "public:\n");
    fprintf(fp, "Reference<%s>(AstNodeBase* refPtr) : m_refPtr(refPtr) {\n", className.c_str());
    // The 'ReleaseAssert' actually evaluates to constant expression.
    // Ideally we want to delete the constructor in that case, but for now go simple.
    //
    fprintf(fp, "    ReleaseAssert((AstTypeHelper::is_cpp_class_type<%s>::value));\n", className.c_str());
    fprintf(fp, "    TestAssert((m_refPtr->GetTypeId() == TypeId::Get<%s>().AddPointer()));\n", className.c_str());
    fprintf(fp, "}\n");
    fprintf(fp, "protected:\n");
    fprintf(fp, "// Used by Variable<> class\n");
    fprintf(fp, "Reference<%s>(AstNodeBase* refPtr, bool /*unused*/) : Reference<%s>(refPtr) {} \n", className.c_str(), className.c_str());
    fprintf(fp, "public:\n");
    fprintf(fp, "AstNodeBase* m_refPtr;\n\n");
    fprintf(fp, "Value<%s*> Addr() const {\n", className.c_str());
    fprintf(fp, "    return Value<%s*>(m_refPtr);\n", className.c_str());
    fprintf(fp, "}\n\n");
}

static void GenerateCppRuntimeHeaderFile(const std::string& generatedFileFolder,
                                         Module* module)
{
    std::map<std::string /*llvmTypeName*/, std::string /*cppTypeName*/> typeNameMap;

    // Check if a function is using sret (struct return) attribute
    //
    std::function<bool(Function*)> isUsingSRet = [](Function* fn) -> bool
    {
        return (fn->getReturnType()->isVoidTy() &&
                fn->getFunctionType()->getNumParams() >= 1 &&
                fn->hasParamAttribute(0 /*paramOrd*/, Attribute::AttrKind::StructRet));
    };

    // Generate mapping between CPP type name and LLVM type name for composite types that we encountered
    //
    std::function<void(Function*, Type*, std::string&, bool)> matchTypes =
            [&typeNameMap](Function* func, Type* type, const std::string& stringType, bool isVar)
    {
        int numPointers = 0;
        while (type->isPointerTy())
        {
            numPointers++;
            type = type->getPointerElementType();
        }
        if (isVar)
        {
            ReleaseAssert(numPointers > 0);
            numPointers--;
        }
        // Lockdown function pointer type arguments for now
        //
        if (type->isFunctionTy())
        {
            fprintf(stderr, "[ERROR] Calling function with function-pointer-typed parameter or return value "
                            "is not supported yet. Offending function: %s\n",
                    func->getName().str().c_str());
            abort();
        }
        // Lockdown array type for now, ask user to change to pointer type
        //
        if (type->isArrayTy())
        {
            fprintf(stderr, "[ERROR] Calling function with array-typed parameter or return value "
                            "is not supported yet. Change it to pointer-type to workaround. Offending function: %s\n",
                    func->getName().str().c_str());
            abort();
        }
        // Lockdown SIMD vector type
        //
        if (type->isVectorTy())
        {
            fprintf(stderr, "[ERROR] Calling function with SIMD-vector-typed parameter or return value "
                            "is not supported. Offending function: %s\n",
                    func->getName().str().c_str());
            abort();
        }
        if (type->isStructTy())
        {
            StructType* st = dyn_cast<StructType>(type);
            ReleaseAssert(st != nullptr);
            if (!st->hasName())
            {
                fprintf(stderr, "[INTERNAL ERROR] Encountered a struct type with no name. Offending function: %s\n",
                        stringType.c_str());
                abort();
            }
            std::string structName = st->getName().str();
            // Attempt to remove pointers from 'stringtype'
            // Since the base type is a struct and we know there isn't any cv-qualifiers,
            // removing pointer should be simply removing the rightmost '*'
            //
            std::string rawType = stringType;
            for (int i = 0; i < numPointers; i++)
            {
                size_t k = rawType.length() - 1;
                while (k > 0 && isWhitespace(rawType[k]))
                {
                    k--;
                }
                if (rawType[k] != '*')
                {
                    fprintf(stderr, "[INTERNAL ERROR] Failed to get base type from type '%s' "
                                    "(attempting to remove %d layers of pointers). "
                                    "Offending function: %s, llvm struct name: %s\n",
                            stringType.c_str(), numPointers, func->getName().str().c_str(), structName.c_str());
                    abort();
                }
                ReleaseAssert(k > 0);
                rawType = rawType.substr(0, k);
            }
            {
                // Remove leading and trailing whitespaces so they print better
                //
                size_t head = 0;
                while (head < rawType.size() && isWhitespace(rawType[head])) { head++; }
                ReleaseAssert(head < rawType.size());
                size_t tail = rawType.size() - 1;
                while (head < tail && isWhitespace(rawType[tail])) { tail--; }
                rawType = rawType.substr(head, tail - head + 1);
            }
            // TODO: generate static_assert to assert all CppName are equivalent for the same llvmTypeName?
            //
            if (!typeNameMap.count(structName))
            {
                typeNameMap[structName] = rawType;
            }
        }
    };

    // Extract type information
    //
    for (auto it = g_symbolAddrToTypeData.begin(); it != g_symbolAddrToTypeData.end(); it++)
    {
        ParsedFnTypeNamesInfo& info = it->second;
        std::string targetFnSymbolName;
        if (info.m_isUsingWrapper)
        {
            targetFnSymbolName = info.m_wrapperFnMangledSymbolName;
        }
        else
        {
            targetFnSymbolName = info.m_mangledSymbolName;
        }
        Function* fn = module->getFunction(targetFnSymbolName);
        ReleaseAssert(fn != nullptr);
        FunctionType* fnType = fn->getFunctionType();
        // Lockdown variadic function
        //
        if (fn->isVarArg())
        {
            fprintf(stderr, "[ERROR] Calling variadic function from generated code is not supported yet. "
                            "Offending function: %s\n",
                    fn->getName().str().c_str());
            abort();
        }

        ReleaseAssert(!isUsingSRet(fn));

        bool isSretFunction = info.m_isUsingWrapper && info.m_isWrapperUsingSret;

        // Match the return type
        // We need to special check for 'sret' struct-return parameter
        //
        unsigned int firstArgOrd = 0;
        Type* returnType = fnType->getReturnType();
        bool isRetApiVar = info.m_isRetApiVar;
        if (isSretFunction)
        {
            ReleaseAssert(!info.m_isRetApiVar);
            isRetApiVar = true;
            returnType = fnType->getParamType(firstArgOrd);
            ReleaseAssert(returnType->isPointerTy());
            firstArgOrd += 1;
        }
        matchTypes(fn, returnType, info.m_ret, isRetApiVar);

        // If it is a class member function, the first parameter is 'this' pointer
        //
        if (info.m_fnType == PochiVM::ReflectionHelper::FunctionType::NonStaticMemberFn)
        {
            ReleaseAssert(fnType->getNumParams() > firstArgOrd);
            Type* thisPtr = fnType->getParamType(firstArgOrd);
            ReleaseAssert(thisPtr->isPointerTy());
            std::string className = info.FindClassName();
            matchTypes(fn, thisPtr, className, true /*isVar*/);
            firstArgOrd += 1;
        }

        // Match each of the remaining parameters
        //
        ReleaseAssert(info.m_params.size() == fnType->getNumParams() - firstArgOrd);
        for (size_t i = 0; i < info.m_params.size(); i++)
        {
            matchTypes(fn, fnType->getParamType(static_cast<unsigned int>(firstArgOrd + i)),
                       info.m_params[i], info.m_isParamsApiVar[i]);
        }
    }

    std::function<std::string(const std::string&)> quoteString = [](const std::string& s)
    {
        std::ostringstream ss;
        ss << std::quoted(s);
        return ss.str();
    };

    // generate Ast syntax header
    //
    std::set<std::string> allDefaultConstructibleClasses;
    {
        std::string filename = generatedFileFolder + "/pochivm_runtime_headers.generated.h";
        FILE* fp = fopen(filename.c_str(), "w");
        if (fp == nullptr)
        {
            fprintf(stderr, "Failed to open file '%s' for write, errno = %d (%s)\n",
                    filename.c_str(), errno, strerror(errno));
            abort();
        }

        fprintf(fp, "// GENERATED FILE, DO NOT EDIT!\n//\n\n");
        fprintf(fp, "#pragma once\n");
        fprintf(fp, "#include \"runtime/pochivm_runtime_headers.h\"\n");
        fprintf(fp, "#include \"pochivm_runtime_cpp_types.generated.h\"\n");
        fprintf(fp, "#include \"pochivm/function_proto.h\"\n");
        fprintf(fp, "#include \"pochivm/api_base.h\"\n");
        fprintf(fp, "#include \"pochivm/api_function_proto.h\"\n\n");
        fprintf(fp, "namespace PochiVM {\n\n");

        // generate all Value<> prototypes
        //
        {
            // Track which types we have not printed the instantiated class.
            // If a class does not have any registered method, it will not be printed in the following loop.
            // We will need to print them in the end.
            //
            std::set<std::string> unprinted;

            // forward declare all Reference<> classes
            //
            for (auto it = typeNameMap.begin(); it != typeNameMap.end(); it++)
            {
                const std::string& cppTypeName = it->second;
                fprintf(fp, "template<> class Reference<%s>;\n", cppTypeName.c_str());
            }
            fprintf(fp, "\n");

            // print all Value<> classes
            //
            for (auto it = typeNameMap.begin(); it != typeNameMap.end(); it++)
            {
                const std::string& cppTypeName = it->second;
                fprintf(fp, "template<> class Value<%s>\n{\npublic:\n", cppTypeName.c_str());
                fprintf(fp, "    Value<%s>(AstNodeBase* ptr) : m_ptr(ptr) {\n", cppTypeName.c_str());
                fprintf(fp, "        ReleaseAssert((AstTypeHelper::is_cpp_class_type<%s>::value));\n", cppTypeName.c_str());
                fprintf(fp, "        TestAssert((m_ptr->GetTypeId() == TypeId::Get<%s>()));\n", cppTypeName.c_str());
                fprintf(fp, "    }\n");
                fprintf(fp, "    AstNodeBase* m_ptr;\n};\n\n");
                unprinted.insert(cppTypeName);
            }
            fprintf(fp, "\n");

            // Print prototypes for classes with registered methods
            //
            fprintf(fp, "// Class member functions prototype\n//\n\n");
            std::map<std::string /*class*/, std::vector<ParsedFnTypeNamesInfo> > fns;

            for (auto it = g_symbolAddrToTypeData.begin(); it != g_symbolAddrToTypeData.end(); it++)
            {
                ParsedFnTypeNamesInfo& info = it->second;
                if (info.m_fnType != PochiVM::ReflectionHelper::FunctionType::FreeFn &&
                    info.m_fnType != PochiVM::ReflectionHelper::FunctionType::Constructor &&
                    info.m_fnType != PochiVM::ReflectionHelper::FunctionType::Destructor)
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

                fprintf(fp, "template<> class Reference<%s>\n{\n", className.c_str());
                PrintBoilerplateMethods(fp, className);

                if (unprinted.count(className))
                {
                    unprinted.erase(unprinted.find(className));
                }

                size_t start = 0;
                while (start < data.size())
                {
                    size_t end = start;
                    while (end < data.size() &&
                           data[start].m_fnType == data[end].m_fnType &&
                           data[start].m_functionName == data[end].m_functionName &&
                           data[start].m_params == data[end].m_params &&
                           data[start].m_isParamsApiVar == data[end].m_isParamsApiVar &&
                           data[start].m_ret == data[end].m_ret &&
                           data[start].m_isRetApiVar == data[end].m_isRetApiVar &&
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
                            fprintf(fp, "%s%s<%s> %s",
                                    (info.m_fnType == PochiVM::ReflectionHelper::FunctionType::StaticMemberFn ? "static " : ""),
                                    (info.m_isRetApiVar ? "Reference" : "Value"),
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
                            fprintf(fp, "%s%s<%s> %s",
                                    (data[start].m_fnType == PochiVM::ReflectionHelper::FunctionType::StaticMemberFn ? "static " : ""),
                                    (data[start].m_isRetApiVar ? "Reference" : "Value"),
                                    data[start].m_ret.c_str(), data[start].m_functionName.c_str());
                            PrintFnParams(fp, data[start], true /*doNotPrintVarName*/);
                            fprintf(fp, " = delete;\n");
                        }
                        fprintf(fp, "\n");
                    }
                    start = end;
                }

                fprintf(fp, "}; // class Reference<%s>\n\n", className.c_str());
            }

            // Print the yet unprinted classes.
            // Those are classes with no special methods to call. Just print the common boilerplate part.
            //
            for (const std::string& className : unprinted)
            {
                fprintf(fp, "template<> class Reference<%s>\n{\n", className.c_str());
                PrintBoilerplateMethods(fp, className);
                fprintf(fp, "};\n\n");
            }
        }

        // generate all free functions
        //
        {
            fprintf(fp, "namespace CallFreeFn {\n\n");

            std::map<std::string /*ns*/, std::vector<ParsedFnTypeNamesInfo> > freeFns;

            for (auto it = g_symbolAddrToTypeData.begin(); it != g_symbolAddrToTypeData.end(); it++)
            {
                ParsedFnTypeNamesInfo& info = it->second;
                if (info.m_fnType == PochiVM::ReflectionHelper::FunctionType::FreeFn)
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
                           data[start].m_isParamsApiVar == data[end].m_isParamsApiVar &&
                           data[start].m_ret == data[end].m_ret &&
                           data[start].m_isRetApiVar == data[end].m_isRetApiVar &&
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
                            fprintf(fp, "inline %s<%s> %s", (info.m_isRetApiVar ? "Reference" : "Value"),
                                    info.m_ret.c_str(), info.m_functionName.c_str());
                            PrintFnParams(fp, info);
                            fprintf(fp, "\n");
                            PrintFnCallBody(fp, info);
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
                            fprintf(fp, "%s<%s> %s", (data[start].m_isRetApiVar ? "Reference" : "Value"),
                                    data[start].m_ret.c_str(), data[start].m_functionName.c_str());
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
                            fprintf(fp, "inline %s<%s> %s", (info.m_isRetApiVar ? "Reference" : "Value"),
                                    info.m_ret.c_str(), info.m_functionName.c_str());
                            PrintFnTemplateParams(fp, info);
                            PrintFnParams(fp, info);
                            fprintf(fp, "\n");
                            PrintFnCallBody(fp, info);
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

        // generate class member functions (implementation)
        //
        {
            fprintf(fp, "// Class member functions implementation\n//\n\n");
            std::map<std::string /*class*/, std::vector<ParsedFnTypeNamesInfo> > fns;

            for (auto it = g_symbolAddrToTypeData.begin(); it != g_symbolAddrToTypeData.end(); it++)
            {
                ParsedFnTypeNamesInfo& info = it->second;
                if (info.m_fnType != PochiVM::ReflectionHelper::FunctionType::FreeFn &&
                    info.m_fnType != PochiVM::ReflectionHelper::FunctionType::Constructor &&
                    info.m_fnType != PochiVM::ReflectionHelper::FunctionType::Destructor)
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
                    fprintf(fp, "inline %s<%s> Reference<%s>::%s",
                            (info.m_isRetApiVar ? "Reference" : "Value"),
                            info.m_ret.c_str(), info.m_prefix.c_str(), info.m_functionName.c_str());
                    if (tplSize > 0)
                    {
                        PrintFnTemplateParams(fp, info);
                    }
                    PrintFnParams(fp, info);
                    fprintf(fp, "\n");
                    PrintFnCallBody(fp, info);
                }
            }
        }

        // generate all constructors
        //
        {
            fprintf(fp, "// Constructors\n//\n");

            fprintf(fp, "template<typename T>\n");
            fprintf(fp, "class Constructor {\n");
            fprintf(fp, "    static_assert(sizeof(T) == 0, \"unknown class, register the constructor in pochivm_register_runtime.cpp?\");\n");
            fprintf(fp, "};\n\n");

            std::map<std::string /*class*/, std::vector<ParsedFnTypeNamesInfo> > fns;

            for (auto it = g_symbolAddrToTypeData.begin(); it != g_symbolAddrToTypeData.end(); it++)
            {
                ParsedFnTypeNamesInfo& info = it->second;
                if (info.m_fnType == PochiVM::ReflectionHelper::FunctionType::Constructor)
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

                bool isDefaultConstructible = false;
                fprintf(fp, "template<> class Constructor<%s> : public ConstructorParamInfo\n{\npublic:\n\n", className.c_str());
                for (size_t k = 0; k < data.size(); k++)
                {
                    ParsedFnTypeNamesInfo& info = data[k];
                    ReleaseAssert(info.m_params.size() > 0);
                    if (info.m_params.size() == 1)
                    {
                        isDefaultConstructible = true;
                    }
                    fprintf(fp, "// Original parameters:\n");
                    for (const std::string& param : info.m_origParams)
                    {
                        fprintf(fp, "//          %s\n", param.c_str());
                    }
                    fprintf(fp, "//\n");
                    ReleaseAssert(info.m_templateParams.size() == 0);
                    fprintf(fp, "Constructor");
                    // The first parameter is 'ClassName*', we should not print it.
                    //
                    PrintFnParams(fp, info, false /*doNotPrintVarName*/, 1 /*firstParam*/);
                    fprintf(fp, "\n");
                    PrintConstructorFnCallBody(fp, info);
                }
                fprintf(fp, "};\n\n");

                if (isDefaultConstructible)
                {
                    ReleaseAssert(!allDefaultConstructibleClasses.count(className));
                    allDefaultConstructibleClasses.insert(className);
                }
            }
        }

        fprintf(fp, "\n} // namespace PochiVM\n\n");
        fclose(fp);
    }

    // generate typename info (may use AstTypeHelper and other helper classes)
    //
    {
        std::string filename = generatedFileFolder + "/pochivm_runtime_cpp_typeinfo.generated.h";
        FILE* fp = fopen(filename.c_str(), "w");
        if (fp == nullptr)
        {
            fprintf(stderr, "Failed to open file '%s' for write, errno = %d (%s)\n",
                    filename.c_str(), errno, strerror(errno));
            abort();
        }

        fprintf(fp, "// GENERATED FILE, DO NOT EDIT!\n//\n\n");
        fprintf(fp, "#pragma once\n");
        fprintf(fp, "#include \"pochivm/ast_type_helper.h\"\n\n");
        fprintf(fp, "namespace PochiVM {\n\n");

        // generate all destructors
        //
        {
            fprintf(fp, "template<typename T>\nclass DestructorCppFnMetadata\n{\npublic:\n");
            fprintf(fp, "    static constexpr const CppFunctionMetadata* value = nullptr;\n");
            fprintf(fp, "};\n\n");

            std::map<std::string /*class*/, ParsedFnTypeNamesInfo > fns;

            for (auto it = g_symbolAddrToTypeData.begin(); it != g_symbolAddrToTypeData.end(); it++)
            {
                ParsedFnTypeNamesInfo& info = it->second;
                if (info.m_fnType == PochiVM::ReflectionHelper::FunctionType::Destructor)
                {
                    if (fns.find(info.m_prefix) != fns.end())
                    {
                        fprintf(stderr, "Multiple destructors with different function addresses is registered! Offending class: %s\n",
                                info.m_prefix.c_str());
                        abort();
                    }
                    fns[info.m_prefix] = info;
                }
            }

            for (auto it = fns.begin(); it != fns.end(); it++)
            {
                std::string className = it->first;
                ParsedFnTypeNamesInfo& info = it->second;
                fprintf(fp, "template<> class DestructorCppFnMetadata<%s> {\nprivate:\n", className.c_str());
                ReleaseAssert(info.m_params.size() == 1);
                ReleaseAssert(info.m_templateParams.size() == 0);
                ReleaseAssert(!info.m_isParamsApiVar[0]);
                ReleaseAssert(!info.m_isRetApiVar);
                fprintf(fp, "    using __pochivm_classname = %s;\n", className.c_str());
                fprintf(fp, "    static constexpr TypeId __pochivm_cpp_fn_params[1] = { TypeId::Get<%s>() };\n",
                        info.m_params[0].c_str());
                fprintf(fp, "    static constexpr TypeId __pochivm_cpp_fn_ret = TypeId::Get<%s>();\n",
                        info.m_ret.c_str());
                fprintf(fp, "    static_assert(__pochivm_cpp_fn_params[0] == TypeId::Get<__pochivm_classname>().AddPointer(), \"unexpected param type\");\n");
                fprintf(fp, "    static_assert(__pochivm_cpp_fn_ret == TypeId::Get<void>(), \"unexpected return type\");\n");

                fprintf(fp, "    using __pochivm_wrapper_t = ReflectionHelper::destructor_wrapper_helper<__pochivm_classname>;\n");
                fprintf(fp, "    static constexpr CppFunctionMetadata __pochivm_cpp_fn_metadata = {\n");
                std::string varname = std::string("__pochivm_internal_bc_") + GetUniqueSymbolHash(info.m_mangledSymbolName);
                fprintf(fp, "        &%s,\n", varname.c_str());
                fprintf(fp, "        __pochivm_cpp_fn_params,\n");
                fprintf(fp, "        1 /*numParams*/,\n");
                fprintf(fp, "        __pochivm_cpp_fn_ret /*returnType*/,\n");
                fprintf(fp, "        false /*isUsingSret*/,\n");
                fprintf(fp, "        AstTypeHelper::interp_call_cpp_fn_helper<__pochivm_wrapper_t::wrapperFn>::interpFn /*interpFn*/,\n");
                fprintf(fp, "        %d /*uniqueFunctionOrdinal*/,\n", g_curUniqueFunctionOrdinal);
                g_curUniqueFunctionOrdinal++;
                fprintf(fp, "    };\n");
                fprintf(fp, "public:\n");
                fprintf(fp, "    static constexpr const CppFunctionMetadata* value = &__pochivm_cpp_fn_metadata;\n");
                fprintf(fp, "};\n\n");
            }

            fprintf(fp, "template<typename T>\n");
            fprintf(fp, "struct is_destructor_registered : std::integral_constant<bool,\n");
            fprintf(fp, "        (DestructorCppFnMetadata<T>::value != nullptr)\n");
            fprintf(fp, "> {};\n\n");
        }

        fprintf(fp, "\n} // namespace PochiVM\n\n");
        fclose(fp);
    }

    // generate typename map (this file is very low-level and included BY ast_type_helper.h,
    // so it only has access to a very limited set of utilities)
    //
    {
        std::string filename = generatedFileFolder + "/pochivm_runtime_cpp_types.generated.h";
        FILE* fp = fopen(filename.c_str(), "w");
        if (fp == nullptr)
        {
            fprintf(stderr, "Failed to open file '%s' for write, errno = %d (%s)\n",
                    filename.c_str(), errno, strerror(errno));
            abort();
        }

        fprintf(fp, "// GENERATED FILE, DO NOT EDIT!\n//\n\n");
        fprintf(fp, "#pragma once\n");
        fprintf(fp, "#include \"pochivm/common.h\"\n");
        fprintf(fp, "#include \"pochivm/pochivm_reflection_helper.h\"\n");
        fprintf(fp, "#include \"runtime/pochivm_runtime_headers.h\"\n");
        fprintf(fp, "#include \"pochivm_runtime_library_bitcodes.generated.h\"\n\n");
        fprintf(fp, "namespace PochiVM {\n\n");
        fprintf(fp, "namespace AstTypeHelper {\n\n");

        fprintf(fp, "template<typename T>\n");
        fprintf(fp, "struct cpp_type_ordinal {\n");
        fprintf(fp, "    static const uint64_t ordinal = %d;\n", static_cast<int>(typeNameMap.size()));
        fprintf(fp, "};\n\n");

        std::vector<std::string> llvmTypeNames, cppTypeNames;
        int ordinal = 0;
        for (auto it = typeNameMap.begin(); it != typeNameMap.end(); it++)
        {
            const std::string& llvmTypeName = it->first;
            const std::string& cppTypeName = it->second;
            llvmTypeNames.push_back(llvmTypeName);
            cppTypeNames.push_back(cppTypeName);
            fprintf(fp, "template<>\n");
            fprintf(fp, "struct cpp_type_ordinal<%s> {\n", cppTypeName.c_str());
            fprintf(fp, "    static const uint64_t ordinal = %d;\n", ordinal);
            fprintf(fp, "};\n\n");
            ordinal++;
        }

        fprintf(fp, "const char* const AstCppTypePrintName[%d] = {\n", ordinal + 1);
        for (const std::string& cppTypeName : cppTypeNames)
        {
            fprintf(fp, "    %s,\n", quoteString(cppTypeName).c_str());
        }
        fprintf(fp, "    \"(Unknown CPP type)\"\n");
        fprintf(fp, "};\n\n");

        fprintf(fp, "const char* const AstCppTypeLLVMTypeName[%d] = {\n", ordinal + 1);
        for (const std::string& llvmTypeName : llvmTypeNames)
        {
            fprintf(fp, "    %s,\n", quoteString(llvmTypeName).c_str());
        }
        fprintf(fp, "    nullptr\n");
        fprintf(fp, "};\n\n");

        if (cppTypeNames.size() > 0)
        {
            fprintf(fp, "#define FOR_EACH_CPP_CLASS_TYPE \\\n");
            for (size_t i = 0; i < cppTypeNames.size(); i++)
            {
                fprintf(fp, "    F(%s) ", cppTypeNames[i].c_str());
                if (i == cppTypeNames.size() - 1)
                {
                    fprintf(fp, "\n\n");
                }
                else
                {
                    fprintf(fp, "\\\n");
                }
            }

            // Just for use in AstTypeLabelEnum
            // Bothe cppTypeName and llvmTypeName are too wild to directly used as enum label
            //
            fprintf(fp, "#define CPP_CLASS_ENUM_TYPE_LIST \\\n");
            for (size_t i = 0; i < cppTypeNames.size(); i++)
            {
                fprintf(fp, "  , CPP_TYPE_%d ", static_cast<int>(i));
                if (i == cppTypeNames.size() - 1)
                {
                    fprintf(fp, "\n\n");
                }
                else
                {
                    fprintf(fp, "\\\n");
                }
            }
        }
        else
        {
            fprintf(fp, "#define FOR_EACH_CPP_CLASS_TYPE \n\n");
            fprintf(fp, "#define CPP_CLASS_ENUM_TYPE_LIST \n\n");
        }

        fprintf(fp, "const static int x_num_cpp_class_types = %d;\n\n", ordinal);
        fprintf(fp, "const static int x_num_cpp_functions = %d;\n\n", g_curUniqueFunctionOrdinal);

        fprintf(fp, "template<typename T> struct is_default_ctor_registered : std::false_type {};\n");
        for (const std::string& className : allDefaultConstructibleClasses)
        {
            fprintf(fp, "template<> struct is_default_ctor_registered<%s> : std::true_type {};\n", className.c_str());
        }
        fprintf(fp, "\n");

        fprintf(fp, "\n} // namespace AstTypeHelper\n\n");
        fprintf(fp, "\n} // namespace PochiVM\n\n");
        fclose(fp);
    }
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

    // --dump-list [bc_file] [symbol_list_output] [needed_symbols_info_output] [generated_file_folder]
    //    Used on '__pochivm_runtime__.bc'. In addition to '--dump', it attempts to
    //    dump the type information of the symbols described in pochivm_register_runtime.cpp
    //    output the IR information of those symbols to 'needed_symbols_info_output',
    //    and generate the C++ header files into 'generated_file_folder'.
    //

    ReleaseAssert(argc >= 2);
    std::string param1 = argv[1];
    ReleaseAssert(param1 == "--dump" || param1 == "--dump-list");

    if (param1 == "--dump") { ReleaseAssert(argc == 4); } else { ReleaseAssert(argc == 6); }

    bool isDumpList = (param1 == "--dump-list");

    std::string bcFileName = argv[2];
    std::string strippedBcFileName = bcFileName + ".stripped.bc";

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

    {
        int fd = creat(strippedBcFileName.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (fd == -1)
        {
            fprintf(stderr, "Failed to open file '%s' for write, errno = %d (%s)\n",
                    strippedBcFileName.c_str(), errno, strerror(errno));
            abort();
        }
        raw_fd_ostream fdStream(fd, true /*shouldClose*/);
        WriteBitcodeToFile(*module.get(), fdStream);
        if (fdStream.has_error())
        {
            std::error_code ec = fdStream.error();
            fprintf(stderr, "Writing to file '%s' failed with errno = %d (%s)\n",
                    strippedBcFileName.c_str(), ec.value(), ec.message().c_str());
            abort();
        }
        /* fd closed when fdStream is destructed here */
    }

    {
        // Load the stripped module from file, just for sanity
        //
        std::unique_ptr<LLVMContext> newContext(new LLVMContext);
        std::unique_ptr<Module> newModule = parseIRFile(strippedBcFileName, llvmErr, *newContext.get());

        if (newModule == nullptr)
        {
            fprintf(stderr, "[ERROR] An error occurred while parsing IR file '%s', detail:\n", strippedBcFileName.c_str());
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
            if (it->second.m_fnType != PochiVM::ReflectionHelper::FunctionType::Constructor &&
                it->second.m_fnType != PochiVM::ReflectionHelper::FunctionType::Destructor)
            {
                // For constructors, the symbol is a generated wrapper function, not the original prototype
                // The original prototype must have no template parameters since our API dit not support
                // specifying them (for constructors, we do not support templated-constructors),
                // or is not possible (for destructors, which always have a fixed prototype)
                //
                it->second.FindTemplateParameters(symbolPair.second /*demangled*/);
            }
            it->second.RecordMangledSymbolName(symbolPair.first /*mangled*/);
            if (it->second.m_fnType != PochiVM::ReflectionHelper::FunctionType::Constructor &&
                it->second.m_fnType != PochiVM::ReflectionHelper::FunctionType::Destructor)
            {
                uintptr_t wrapperAddr = reinterpret_cast<uintptr_t>(it->second.m_wrapperFnAddress);
                ReleaseAssert(addrToSymbol.count(wrapperAddr));
                symbolPair = addrToSymbol[wrapperAddr];
                it->second.RecordWrapperFnMangledSymbolName(symbolPair.first /*mangled*/);
            }
            else
            {
                ReleaseAssert(it->second.m_wrapperFnAddress == nullptr);
            }
        }

        // Write 'needed_symbols_info_output'
        //
        {
            std::set<std::string> symbolsOut;
            for (auto it = g_symbolAddrToTypeData.begin(); it != g_symbolAddrToTypeData.end(); it++)
            {
                ParsedFnTypeNamesInfo& info = it->second;
                std::string name = info.m_mangledSymbolName;
                // If the function requires calling the wrapper, we need to extract the wrapper IR as well
                //
                if (info.m_isUsingWrapper)
                {
                    ReleaseAssert(!symbolsOut.count(name));
                    std::string tmp = std::string("*") + name;
                    ReleaseAssert(!symbolsOut.count(tmp));
                    symbolsOut.insert(tmp);
                    name = info.m_wrapperFnMangledSymbolName + std::string(",") + name;
                }
                ReleaseAssert(!symbolsOut.count(name));
                symbolsOut.insert(name);
            }

            std::string neededSymbolOutputFile = std::string(argv[4]);
            WriteSymbolListFileOrDie(neededSymbolOutputFile, symbolsOut);
        }

        {
            // Load the optimized module, to generate C++ header file
            //
            std::unique_ptr<LLVMContext> newContext(new LLVMContext);
            std::unique_ptr<Module> newModule = parseIRFile(strippedBcFileName, llvmErr, *newContext.get());

            if (newModule == nullptr)
            {
                fprintf(stderr, "[ERROR] An error occurred while parsing IR file '%s', detail:\n", strippedBcFileName.c_str());
                llvmErr.print(argv[0], errs());
                abort();
            }
            module = std::move(newModule);
            context = std::move(newContext);
        }

        // Generate the C++ header file
        //
        if (isDumpList)
        {
            std::string generatedFileFolder = std::string(argv[5]);
            GenerateCppRuntimeHeaderFile(generatedFileFolder, module.get());
        }
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
