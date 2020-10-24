#pragma once

#include "common.h"
#include "fastinterp/fastinterp_snippet.h"

namespace llvm {
class Value;
class BasicBlock;
};

namespace PochiVM
{

class AstNodeBase;
class FastInterpBoilerplateInstance;

class DestructorIREmitter
{
public:
    virtual ~DestructorIREmitter() {}
    virtual bool HasNontrivialDestructor() = 0;
    virtual void EmitDestructorIR() = 0;
};

class ScopedVariableManager
{
public:
    enum OperationMode
    {
        LLVM,
        FASTINTERP,
        INVALID
    };

    ScopedVariableManager()
        : m_operationMode(OperationMode::INVALID)
        , m_numNontrivialDestructorObjects(0)
        , m_llvmEhCurExceptionObject(nullptr)
        , m_llvmEhCurExceptionType(nullptr)
        , m_llvmCurrentEHCatchBlock(nullptr)
        , m_fiCurrentEHCatchBlock(nullptr)
        , m_llvmDtorTreeBlockOrdinal(0)
    { }

    // The SetOperationMode() / ClearOperationMode() is expected to be called on a per-function basis
    //
    void SetOperationMode(OperationMode operationMode)
    {
        TestAssert(operationMode != OperationMode::INVALID && m_operationMode == OperationMode::INVALID);
        m_operationMode = operationMode;
        m_numNontrivialDestructorObjects = 0;
        m_llvmBreakStmtTarget.clear();
        m_llvmContinueStmtTarget.clear();
        m_fiBreakStmtTarget.clear();
        m_fiContinueStmtTarget.clear();
        m_scopeStack.clear();
        m_llvmExceptionDtorTree.clear();
        m_fiExceptionDtorTree.clear();
        m_llvmEhCurExceptionObject = nullptr;
        m_llvmEhCurExceptionType = nullptr;
        m_llvmCurrentEHCatchBlock = nullptr;
        m_fiCurrentEHCatchBlock = nullptr;
        m_llvmDtorTreeBlockOrdinal = 0;
    }

    void ClearOperationMode()
    {
        TestAssert(m_operationMode != OperationMode::INVALID);
        m_operationMode = OperationMode::INVALID;
    }

    void AssertInCleanState()
    {
        TestAssert(m_operationMode != OperationMode::INVALID);
        TestAssert(m_numNontrivialDestructorObjects == 0);
        TestAssert(m_scopeStack.size() == 0);
        if (m_operationMode == OperationMode::LLVM)
        {
            TestAssert(m_llvmBreakStmtTarget.size() == 0);
            TestAssert(m_llvmContinueStmtTarget.size() == 0);
            TestAssert(m_llvmExceptionDtorTree.size() == 0);
        }
        else
        {
            TestAssert(m_fiBreakStmtTarget.size() == 0);
            TestAssert(m_fiContinueStmtTarget.size() == 0);
            TestAssert(m_fiExceptionDtorTree.size() == 0);
        }
    }

    void PushObject(DestructorIREmitter* obj)
    {
        TestAssert(m_scopeStack.size() > 0);
        m_scopeStack.back().second.push_back(obj);
        if (obj->HasNontrivialDestructor())
        {
            m_numNontrivialDestructorObjects++;
        }
    }

    AstNodeBase* GetCurrentScope() const
    {
        TestAssert(m_scopeStack.size() > 0);
        return m_scopeStack.back().first;
    }

    size_t GetNumObjectsInCurrentScope() const
    {
        TestAssert(m_scopeStack.size() > 0);
        return m_scopeStack.back().second.size();
    }

    size_t GetNumNontrivialDestructorObjects() const
    {
        return m_numNontrivialDestructorObjects;
    }

    const std::vector<DestructorIREmitter*>& GetObjectsInCurrentScope() const
    {
        TestAssert(m_scopeStack.size() > 0);
        return m_scopeStack.back().second;
    }

    void PushScope(AstNodeBase* scope)
    {
        m_scopeStack.push_back(std::make_pair(scope, std::vector<DestructorIREmitter*>()));
    }

    void PopScope(AstNodeBase* TESTBUILD_ONLY(expectedScope))
    {
        TestAssert(m_scopeStack.size() > 0 && m_scopeStack.back().first == expectedScope);
        TestAssertImp(m_operationMode == OperationMode::LLVM, m_llvmExceptionDtorTree.size() <= m_scopeStack.size());
        TestAssertImp(m_operationMode == OperationMode::FASTINTERP, m_fiExceptionDtorTree.size() <= m_scopeStack.size());
        const std::vector<DestructorIREmitter*>& vars = m_scopeStack.back().second;
        for (auto it = vars.begin(); it != vars.end(); it++)
        {
            if ((*it)->HasNontrivialDestructor())
            {
                TestAssert(m_numNontrivialDestructorObjects > 0);
                m_numNontrivialDestructorObjects--;
            }
        }
        m_scopeStack.pop_back();
        if (m_operationMode == OperationMode::LLVM)
        {
            if (m_llvmExceptionDtorTree.size() > m_scopeStack.size())
            {
                m_llvmExceptionDtorTree.pop_back();
            }
        }
        else
        {
            if (m_fiExceptionDtorTree.size() > m_scopeStack.size())
            {
                m_fiExceptionDtorTree.pop_back();
            }
        }
    }

    void PushLLVMBreakAndContinueTarget(llvm::BasicBlock* breakTarget,
                                        AstNodeBase* breakTargetScope,
                                        llvm::BasicBlock* continueTarget,
                                        AstNodeBase* continueTargetScope)
    {
        TestAssert(m_operationMode == OperationMode::LLVM);
        m_llvmBreakStmtTarget.push_back(std::make_pair(breakTarget, breakTargetScope));
        m_llvmContinueStmtTarget.push_back(std::make_pair(continueTarget, continueTargetScope));
    }

    void PushFIBreakAndContinueTarget(FastInterpBoilerplateInstance* breakTarget,
                                      AstNodeBase* breakTargetScope,
                                      FastInterpBoilerplateInstance* continueTarget,
                                      AstNodeBase* continueTargetScope)
    {
        TestAssert(m_operationMode == OperationMode::FASTINTERP);
        m_fiBreakStmtTarget.push_back(std::make_pair(breakTarget, breakTargetScope));
        m_fiContinueStmtTarget.push_back(std::make_pair(continueTarget, continueTargetScope));
    }

    void PopBreakAndContinueTarget(OperationMode TESTBUILD_ONLY(expectedOperationMode))
    {
        TestAssert(m_operationMode == expectedOperationMode);
        if (m_operationMode == OperationMode::LLVM)
        {
            TestAssert(m_llvmBreakStmtTarget.size() > 0);
            m_llvmBreakStmtTarget.pop_back();
            TestAssert(m_llvmContinueStmtTarget.size() > 0);
            m_llvmContinueStmtTarget.pop_back();
        }
        else
        {
            TestAssert(m_fiBreakStmtTarget.size() > 0);
            m_fiBreakStmtTarget.pop_back();
            TestAssert(m_fiContinueStmtTarget.size() > 0);
            m_fiContinueStmtTarget.pop_back();
        }
    }

    std::pair<llvm::BasicBlock* /*branchTarget*/, AstNodeBase* /*scope*/> GetLLVMBreakTarget() const
    {
        TestAssert(m_operationMode == OperationMode::LLVM && m_llvmBreakStmtTarget.size() > 0);
        return m_llvmBreakStmtTarget.back();
    }

    std::pair<FastInterpBoilerplateInstance* /*branchTarget*/, AstNodeBase* /*scope*/> GetFIBreakTarget() const
    {
        TestAssert(m_operationMode == OperationMode::FASTINTERP && m_fiBreakStmtTarget.size() > 0);
        return m_fiBreakStmtTarget.back();
    }

    std::pair<llvm::BasicBlock* /*branchTarget*/, AstNodeBase* /*scope*/> GetLLVMContinueTarget() const
    {
        TestAssert(m_operationMode == OperationMode::LLVM && m_llvmContinueStmtTarget.size() > 0);
        return m_llvmContinueStmtTarget.back();
    }

    std::pair<FastInterpBoilerplateInstance* /*branchTarget*/, AstNodeBase* /*scope*/> GetFIContinueTarget() const
    {
        TestAssert(m_operationMode == OperationMode::FASTINTERP && m_fiContinueStmtTarget.size() > 0);
        return m_fiContinueStmtTarget.back();
    }

    llvm::Value* GetLLVMExceptionObjectAlloca() const
    {
        TestAssert(m_operationMode == OperationMode::LLVM);
        TestAssert(m_llvmEhCurExceptionObject != nullptr);
        return m_llvmEhCurExceptionObject;
    }

    llvm::Value* GetLLVMExceptionTypeAlloca() const
    {
        TestAssert(m_operationMode == OperationMode::LLVM);
        TestAssert(m_llvmEhCurExceptionType != nullptr);
        return m_llvmEhCurExceptionType;
    }

    // Emit IR that calls destructors in reverse order of declaration for all variables declared,
    // until scope 'boundaryScope' (inclusive). When boundaryScope == nullptr, destructs everything.
    //
    // The IR is emitted at the current program position.
    //
    void EmitIRDestructAllVariablesUntilScope(AstNodeBase* boundaryScope);

    // Emit IR for exception handler cleanup logic
    // One is supposed to branch to the logic after catching the exception with
    // 'landingpad {i8*, i32} cleanup [catch ...] instruction.
    // At the end of cleanup logic, control is transferred to 'catch' blocks
    // (or 'resume' if there is no catch block).
    //
    llvm::BasicBlock* WARN_UNUSED EmitIRExceptionHandlerCleanupLogic();

    // Same as EmitIRDestructAllVariablesUntilScope, except that it emits FastInterp code
    //
    FastInterpSnippet WARN_UNUSED FIGenerateDestructorSequenceUntilScope(AstNodeBase* boundaryScope);

    // Generate the fastinterp entry point that shall be branched to if an exception happened at current program position.
    //
    FastInterpBoilerplateInstance* WARN_UNUSED FIGenerateEHEntryPointForCurrentPosition();

private:
    llvm::BasicBlock* CreateEmptyDtorTreeBlock();

    OperationMode m_operationMode;
    size_t m_numNontrivialDestructorObjects;

    // Current break/continue target
    // The llvm::BasicBlock is the LLVM block target we should branch to.
    // The AstNodeBase* is the variable scope corresponding to the target (we branch to the end of the scope),
    // which may be a AstScope or a AstForLoop. All variables in the scope stack until 'scope' (inclusive)
    // should be destructed before branching to 'branchTarget' when the break/continue statement is executed.
    //
    std::vector<std::pair<llvm::BasicBlock* /*branchTarget*/, AstNodeBase* /*scope*/>> m_llvmBreakStmtTarget;
    std::vector<std::pair<llvm::BasicBlock* /*branchTarget*/, AstNodeBase* /*scope*/>> m_llvmContinueStmtTarget;

    // Same as above, except used by FastInterp
    //
    std::vector<std::pair<FastInterpBoilerplateInstance* /*branchTarget*/, AstNodeBase* /*scope*/>> m_fiBreakStmtTarget;
    std::vector<std::pair<FastInterpBoilerplateInstance* /*branchTarget*/, AstNodeBase* /*scope*/>> m_fiContinueStmtTarget;

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
    std::vector<std::pair<int /*boundary*/,  llvm::BasicBlock* /*branchTarget*/>> m_llvmExceptionDtorTree;

    std::vector<std::pair<int /*boundary*/,  FastInterpBoilerplateInstance* /*branchTarget*/>> m_fiExceptionDtorTree;

    // An alloca of { i8*, i32 } holding the return value of personalityFn
    //
    llvm::Value* m_llvmEhCurExceptionObject;
    llvm::Value* m_llvmEhCurExceptionType;
    // The generated catch-block for the current inner-most try-block we are inside, or nullptr if there isn't any
    //
    llvm::BasicBlock* m_llvmCurrentEHCatchBlock;
    FastInterpBoilerplateInstance* m_fiCurrentEHCatchBlock;

    // Ordinal used by dtor tree blocks
    //
    uint32_t m_llvmDtorTreeBlockOrdinal;
};

}   // namespace PochiVM
