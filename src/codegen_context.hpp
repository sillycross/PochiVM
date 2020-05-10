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
        , m_module(nullptr)
        , m_dummyBlock(nullptr)
        , m_isCursorAtDummyBlock(false)
        , m_curFunction(nullptr)
    { }

    Ast::AstFunction* GetCurFunction() const
    {
        assert(m_curFunction != nullptr);
        return m_curFunction;
    }

    void SetInsertPointToDummyBlock()
    {
        m_builder.SetInsertPoint(m_dummyBlock);
        m_isCursorAtDummyBlock = true;
    }

    llvm::LLVMContext m_llvmContext;
    llvm::IRBuilder<> m_builder;
    llvm::Module* m_module;

    // After we codegen a control-flow redirection statement (break/continue/return),
    // we redirect m_builder to m_dummyBlock and set m_isCursorAtDummyBlock to true,
    // since any statement immediately after those statements are not reachable.
    //
    // This is required, since LLVM requires each basic block to contain exactly one terminator
    // statement at the end (which we have generated when we codegen break/continue/return),
    // so it is disallowed to further emit any statement into current basic block.
    //
    // A well-formed program should not contain unreachable code (this is enforced in Validate()),
    // so nothing should be outputted into m_dummyBlock in the end, which we assert.
    //
    llvm::BasicBlock* m_dummyBlock;
    bool m_isCursorAtDummyBlock;

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
