#pragma once

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

#include "pochivm/common.h"
#include "gtest/gtest.h"
#include "pochivm/codegen_context.hpp"
#include "pochivm/pochivm.h"

// Whether we are in update-expected-output mode
//
extern bool g_is_update_expected_mode;

// a bunch of utility helpers for tests

// return if string 'a' starts with 'b'
//
inline bool WARN_UNUSED StartsWith(const char* a, const char* b)
{
    size_t la = strlen(a);
    size_t lb = strlen(b);
    CHECK(la >= lb);
    return strncmp(a, b, lb) == 0;
}

namespace {

const char* const x_expected_output_dir = "test_expected_output";

inline std::string GetExpectedOutputFileName(const std::string& suffix)
{
    const char* namePart1 = ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
    const char* namePart2 = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    return std::string(x_expected_output_dir) + std::string("/") +
           std::string(namePart1) + std::string(".") + std::string(namePart2) +
           (suffix == "" ? "" : std::string(".") + suffix) + ".expected";
}

inline void CreateExpectedOutputFolderIfNeeded()
{
    struct stat st;
    if (stat(x_expected_output_dir, &st) == -1)
    {
        int ret = mkdir(x_expected_output_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (ret != 0)
        {
            printf("Creating expected output file directory failed with errno=%d (%s)\n", errno, strerror(errno));
            ReleaseAssert(false);
        }
    }
    ReleaseAssert(stat(x_expected_output_dir, &st) == 0);
}

inline size_t GetFileSize(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0)
    {
        return static_cast<size_t>(st.st_size);
    }
    else
    {
        printf("Failed to access expected output file %s, errno=%d (%s)\n", filename, errno, strerror(errno));
        ReleaseAssert(false);
    }
}

inline void TrimTrailingWhitespaces(std::string& s, const char* t = " \t\n\r\f\v")
{
    size_t pos = s.find_last_not_of(t);
    if (pos == std::string::npos)
    {
        s = "";
    }
    else
    {
        s.erase(pos + 1);
    }
}

inline void NO_RETURN DumpExpectedAndActualAndFail(const std::string& actual, const std::string& expected)
{
    printf("!!! TEST FAILED !!!\n");
    printf("=============== Expected Output ===============\n");
    for (size_t i = 0; i < expected.length(); i++)
    {
        printf("%c", expected[i]);
    }
    printf("\n");
    printf("================ Actual Output ================\n");
    for (size_t i = 0; i < actual.length(); i++)
    {
        printf("%c", actual[i]);
    }
    printf("\n");
    printf("===============================================\n");
    ReleaseAssert(false);
}

}   // anonymous namespace

// Check the output against the expected output file of this test
//
inline void AssertIsExpectedOutput(std::string out, std::string suffix = "")
{
    if (g_is_update_expected_mode)
    {
        CreateExpectedOutputFolderIfNeeded();
        std::string filename = GetExpectedOutputFileName(suffix);
        FILE* pFile = fopen(filename.c_str(), "w");
        if (pFile == nullptr)
        {
            printf("Failed to open file %s for write, errno=%d (%s)\n", filename.c_str(), errno, strerror(errno));
            ReleaseAssert(false);
        }
        ReleaseAssert(fwrite(out.data(), sizeof(char), out.length(), pFile) == out.length());
        fclose(pFile);
        printf("[UPDATE_EXPECTED_OUTPUT] Succesfully updated expected output file.\n");
    }
    else
    {
        std::string filename = GetExpectedOutputFileName(suffix);
        FILE* pFile = fopen(filename.c_str(), "r");
        if (pFile == nullptr)
        {
            printf("!!! TEST CONFIGURATION ISSUE !!!\n");
            printf("=============== Expected Output ===============\n");
            printf("[ERROR] Failed to open expected output file %s, errno=%d (%s)\n", filename.c_str(), errno, strerror(errno));
            printf("================ Actual Output ================\n");
            for (size_t i = 0; i < out.length(); i++)
            {
                printf("%c", out[i]);
            }
            printf("\n");
            printf("===============================================\n");
            ReleaseAssert(false);
        }
        Auto(fclose(pFile));
        size_t fileSize = GetFileSize(filename.c_str());
        std::string expected(fileSize, ' ');
        ReleaseAssert(fread(expected.data(), sizeof(char), fileSize, pFile) == fileSize);

        TrimTrailingWhitespaces(expected);
        TrimTrailingWhitespaces(out);

        if (expected != out)
        {
            DumpExpectedAndActualAndFail(out, expected);
        }
    }
}

// This class has been so rotten.. Don't use it for anything but test purpose
// WARNING: especially do not use this class for perf benchmark.
// It does not set llvm::CodeGenOpt::Level correctly, so the assembly instructions emitted by LLVM is very poor quality.
//
class SimpleJIT
{
public:
    SimpleJIT()
        : m_jit(nullptr)
        , m_astModule(nullptr)
        , m_allowResolveSymbolInHostProcess(false)
    { }

    // JIT the given module. Transfers ownership of the llvm module.
    // The previous JIT'd module, if exists, is thrown away
    //
    void SetModule(PochiVM::AstModule* module)
    {
        llvm::ExitOnError exitOnErr;
        std::unique_ptr<llvm::orc::LLJIT>&& jit = GetJIT();
        llvm::orc::ThreadSafeModule M = module->GetThreadSafeModule();
        exitOnErr(jit->addIRModule(std::move(M)));
        m_jit.reset(jit.release());
        m_astModule = module;
    }

    void SetNonAstModule(std::unique_ptr<llvm::orc::ThreadSafeModule> module)
    {
        llvm::ExitOnError exitOnErr;
        std::unique_ptr<llvm::orc::LLJIT> jit = GetJIT();
        exitOnErr(jit->addIRModule(std::move(*module.release())));
        m_jit.reset(jit.release());
        m_astModule = nullptr;
    }

    std::unique_ptr<llvm::orc::LLJIT> GetJIT()
    {
        llvm::ExitOnError exitOnErr;
        std::unique_ptr<llvm::orc::LLJIT> jit = exitOnErr(llvm::orc::LLJITBuilder().create());
        if (m_allowResolveSymbolInHostProcess)
        {
            char Prefix = llvm::EngineBuilder().selectTarget()->createDataLayout().getGlobalPrefix();
            std::unique_ptr<llvm::orc::DynamicLibrarySearchGenerator> R =
                    exitOnErr(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(Prefix));
            ReleaseAssert(R != nullptr);
            jit->getMainJITDylib().addGenerator(std::move(R));
        }
        return jit;
    }

    void SetAllowResolveSymbolInHostProcess(bool value)
    {
        m_allowResolveSymbolInHostProcess = value;
    }

    // Get a callable to the function name
    // FnPrototype may be a C-style pointer or std::function object.
    // Fires an assert if the function prototype does not match the actual type of the function
    //
    template<typename FnPrototype>
    FnPrototype GetFunction(const std::string& fnName)
    {
        ReleaseAssert(m_jit != nullptr && m_astModule != nullptr);
        ReleaseAssert(m_astModule->CheckFunctionExistsAndPrototypeMatches<FnPrototype>(fnName));
        llvm::ExitOnError exitOnErr;
        auto sym = exitOnErr(m_jit->lookup(fnName));
        return PochiVM::AstTypeHelper::function_addr_to_callable<FnPrototype>::get(
                reinterpret_cast<void*>(sym.getAddress()));
    }

    template<typename FnPrototype>
    FnPrototype GetFunctionNonAst(const std::string& fnName)
    {
        ReleaseAssert(m_jit != nullptr);
        llvm::ExitOnError exitOnErr;
        auto sym = exitOnErr(m_jit->lookup(fnName));
        return PochiVM::AstTypeHelper::function_addr_to_callable<FnPrototype>::get(
                reinterpret_cast<void*>(sym.getAddress()));
    }

    std::unique_ptr<llvm::orc::LLJIT> m_jit;
    PochiVM::AstModule* m_astModule;
    bool m_allowResolveSymbolInHostProcess;
};

class TestJitHelper
{
public:
    TestJitHelper() {}

    void Init(int optLevel)
    {
        using namespace PochiVM;
        TestAssert(0 <= optLevel && optLevel <= 3);
        if (optLevel > 0)
        {
            thread_pochiVMContext->m_curModule->OptimizeIR(optLevel);
        }

        llvm::ExitOnError exitOnErr;
        llvm::orc::JITTargetMachineBuilder jtmb = exitOnErr(llvm::orc::JITTargetMachineBuilder::detectHost());
        if (optLevel == 0)
        {
            jtmb.setCodeGenOptLevel(llvm::CodeGenOpt::None);
        }
        else if (optLevel == 1)
        {
            jtmb.setCodeGenOptLevel(llvm::CodeGenOpt::Less);
        }
        else if (optLevel == 2)
        {
            jtmb.setCodeGenOptLevel(llvm::CodeGenOpt::Default);
        }
        else if (optLevel == 3)
        {
            jtmb.setCodeGenOptLevel(llvm::CodeGenOpt::Aggressive);
        }
        jtmb.setCodeModel(llvm::CodeModel::Small);

        m_jit = exitOnErr(llvm::orc::LLJITBuilder().setJITTargetMachineBuilder(jtmb).create());

        {
            char Prefix = llvm::EngineBuilder().selectTarget()->createDataLayout().getGlobalPrefix();
            std::unique_ptr<llvm::orc::DynamicLibrarySearchGenerator> R =
                    exitOnErr(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(Prefix));
            ReleaseAssert(R != nullptr);
            m_jit->getMainJITDylib().addGenerator(std::move(R));
        }

        llvm::orc::ThreadSafeModule M = thread_pochiVMContext->m_curModule->GetThreadSafeModule();
        exitOnErr(m_jit->addIRModule(std::move(M)));
        m_astModule = thread_pochiVMContext->m_curModule;
    }

    template<typename FnPrototype>
    FnPrototype GetFunction(const std::string& fnName)
    {
        ReleaseAssert(m_jit != nullptr && m_astModule != nullptr);
        ReleaseAssert(m_astModule->CheckFunctionExistsAndPrototypeMatches<FnPrototype>(fnName));
        llvm::ExitOnError exitOnErr;
        auto sym = exitOnErr(m_jit->lookup(fnName));
        return PochiVM::AstTypeHelper::function_addr_to_callable<FnPrototype>::get(
                reinterpret_cast<void*>(sym.getAddress()));
    }

    std::unique_ptr<llvm::orc::LLJIT> m_jit;
    PochiVM::AstModule* m_astModule;
};
