#pragma once

#include "common.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Constants.h"

namespace Ast
{

class AstFunction;

}   // namespace Ast;

struct LLVMCodegenContext
{
    LLVMCodegenContext()
        : m_llvmContext()
        , m_builder(m_llvmContext)
        , m_module()
        , m_namedValues()
        , m_curFunction(nullptr)
    { }

    Ast::AstFunction* GetCurFunction() const
    {
        assert(m_curFunction != nullptr);
        return m_curFunction;
    }

    llvm::LLVMContext m_llvmContext;
    llvm::IRBuilder<> m_builder;
    std::unique_ptr<llvm::Module> m_module;
    std::map<std::string, llvm::Value *> m_namedValues;
    // The current function being codegen'ed
    //
    Ast::AstFunction* m_curFunction;
};

extern thread_local LLVMCodegenContext* thread_llvmContext;

class AutoThreadLLVMCodegenContext
{
public:
    AutoThreadLLVMCodegenContext()
    {
        TestAssert(thread_llvmContext == nullptr);
        m_contextPtr = new LLVMCodegenContext();
        ReleaseAssert(m_contextPtr != nullptr);
        thread_llvmContext = m_contextPtr;
    }

    ~AutoThreadLLVMCodegenContext()
    {
        TestAssert(thread_llvmContext == m_contextPtr);
        delete m_contextPtr;
        thread_llvmContext = nullptr;
    }

private:
    LLVMCodegenContext* m_contextPtr;
};
#define AutoLLVMCodegenContext(...) static_assert(false, "Wrong use of 'auto'-pattern!");
