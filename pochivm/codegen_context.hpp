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
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/Passes/PassBuilder.h"

namespace PochiVM
{

class DestructorIREmitter;
class AstNodeBase;
class AstVariable;
class AstFunction;

struct LLVMCodegenContext
{
    // Optimization level used for compiling LLVM IR.
    //
    static const llvm::PassBuilder::OptimizationLevel
            x_optimizationLevel = llvm::PassBuilder::OptimizationLevel::O2;

    LLVMCodegenContext()
        : m_llvmContext(nullptr)
        , m_builder(nullptr)
        , m_module(nullptr)
        , m_dummyBlock(nullptr)
        , m_isCursorAtDummyBlock(false)
        , m_curFunction(nullptr)
        , m_breakStmtTarget()
        , m_continueStmtTarget()
    {
        m_passBuilder.registerModuleAnalyses(m_MAM);
        m_passBuilder.registerCGSCCAnalyses(m_CGAM);
        m_passBuilder.registerFunctionAnalyses(m_FAM);
        m_passBuilder.registerLoopAnalyses(m_LAM);
        m_passBuilder.crossRegisterProxies(m_LAM, m_FAM, m_CGAM, m_MAM);

        m_MPM = m_passBuilder.buildPerModuleDefaultPipeline(x_optimizationLevel);
    }

    AstFunction* GetCurFunction() const
    {
        assert(m_curFunction != nullptr);
        return m_curFunction;
    }

    void SetInsertPointToDummyBlock()
    {
        assert(m_builder != nullptr);
        m_builder->SetInsertPoint(m_dummyBlock);
        m_isCursorAtDummyBlock = true;
    }

    void RunOptimizationPass(llvm::Module* module)
    {
        TestAssert(module != nullptr);
        m_MPM.run(*module, m_MAM);
    }

    void SetupModule(llvm::LLVMContext* llvmContext, llvm::IRBuilder<>* builder, llvm::Module* module)
    {
        TestAssert(m_llvmContext == nullptr && m_builder == nullptr && m_module == nullptr);
        TestAssert(llvmContext != nullptr && builder != nullptr && module != nullptr);
        m_llvmContext = llvmContext;
        m_builder = builder;
        m_module = module;
    }

    void ClearModule()
    {
        m_llvmContext = nullptr;
        m_builder = nullptr;
        m_module = nullptr;
    }

    void PopVariableScope(AstNodeBase* TESTBUILD_ONLY(expectedScope))
    {
        TestAssert(m_scopeStack.size() > 0 && m_scopeStack.back().first == expectedScope);
        TestAssert(m_exceptionDtorTree.size() <= m_scopeStack.size());
        m_scopeStack.pop_back();
        if (m_exceptionDtorTree.size() > m_scopeStack.size())
        {
            m_exceptionDtorTree.pop_back();
        }
    }

    llvm::LLVMContext* m_llvmContext;
    llvm::IRBuilder<>* m_builder;
    llvm::Module* m_module;

    llvm::PassBuilder m_passBuilder;
    llvm::LoopAnalysisManager m_LAM;
    llvm::FunctionAnalysisManager m_FAM;
    llvm::CGSCCAnalysisManager m_CGAM;
    llvm::ModuleAnalysisManager m_MAM;
    llvm::ModulePassManager m_MPM;

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
    AstFunction* m_curFunction;

    // Current break/continue target
    // The llvm::BasicBlock is the LLVM block target we should branch to.
    // The AstNodeBase* is the variable scope corresponding to the target (we branch to the end of the scope),
    // which may be a AstScope or a AstForLoop. All variables in the scope stack until 'scope' (inclusive)
    // should be destructed before branching to 'branchTarget' when the break/continue statement is executed.
    //
    std::vector<std::pair<llvm::BasicBlock* /*branchTarget*/, AstNodeBase* /*scope*/>> m_breakStmtTarget;
    std::vector<std::pair<llvm::BasicBlock* /*branchTarget*/, AstNodeBase* /*scope*/>> m_continueStmtTarget;

    // Current stack of variable scopes and declared variables in each scope
    //
    std::vector<std::pair<AstNodeBase* /*scope*/, std::vector<DestructorIREmitter*>>> m_scopeStack;
    // Current "stack" of destructor blocks for handling exceptions
    // It is actually a tree, with each node branching to its parent in the end,
    // but it's sufficient to only keep track of the current "dtor position" in each scope stack.
    // Example: { ctor1; E; ctor2; { ctor3; E; ctor4; } { ctor5; E; ctor6; } ctor7; E; ctor8; }
    // would have a tree structure of                /- dtor3 <- dtor4
    //                                dtor1 <- dtor2 -- dtor5 <- dtor6
    //                                               \- dtor7 <- dtor8
    // and each exception 'E' would just branch to the corresponding dtor node in the tree.
    //
    // m_exceptionDtorTree[i] would store a 'boundary' and a 'branchTarget', which means destructors
    // for the the first 'boundary' objects constructed in scope stack 'i', and any alive objects
    // in earlier scopes, would be called when branching to 'branchTarget'.
    //
    std::vector<std::pair<int /*boundary*/,  llvm::BasicBlock* /*branchTarget*/>> m_exceptionDtorTree;
    // An alloca of { i8*, i32 } holding the return value of personalityFn
    //
    llvm::Value* m_ehCurExceptionObject;
    llvm::Value* m_ehCurExceptionType;
    // The generated catch-block for the current inner-most try-block we are inside, or nullptr if there isn't any
    //
    llvm::BasicBlock* m_currentEHCatchBlock;
    // Ordinal used by dtor tree blocks
    //
    uint32_t m_dtorTreeBlockOrdinal;
    // Ordinal used by landingpad blocks
    //
    uint32_t m_landingPadBlockOrdinal;
    // The personalityFn in this module ("__gxx_personality_v0")
    //
    llvm::Constant* m_personalityFn;
};

inline thread_local LLVMCodegenContext* thread_llvmContext = nullptr;

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

}   // namespace PochiVM
