#include "ast_catch_throw.h"
#include "fastinterp_ast_helper.hpp"
#include "exception_helper.h"
#include "codegen_context.hpp"

namespace PochiVM
{

void AstThrowStmt::FastInterpSetupSpillLocation()
{
    thread_pochiVMContext->m_fastInterpStackFrameManager->AssertNoTemp();
    if (m_isLValueObject)
    {
        // The address of the LValue object is the second parameter to the copy-constructor callExpr
        // We will only evaluate 'expr', not the 'callExpr'
        // This is really poorly designed, but just leave it as it is for now.
        //
        AstCallExpr* callExpr = assert_cast<AstCallExpr*>(m_operand);
        AstNodeBase* expr = callExpr->GetParams()[1];
        expr->FastInterpSetupSpillLocation();
    }
    else
    {
        m_operand->FastInterpSetupSpillLocation();
    }
}

FastInterpSnippet WARN_UNUSED AstThrowStmt::PrepareForFastInterp(FISpillLocation TESTBUILD_ONLY(spillLoc))
{
    TestAssert(spillLoc.IsNoSpill());
    thread_pochiVMContext->m_fastInterpStackFrameManager->AssertNoTemp();

    // Important to declare the variable before the AutoScope: it must outlive the AutoScope
    //
    AstVariable variable(m_exceptionTypeId.AddPointer(), thread_llvmContext->m_curFunction, 1000000000 /*varSuffix*/);

    // Create a new scope, which will contain the exception variable temporary
    //
    AutoScopedVariableManagerScope asvms(nullptr /*scope*/);

    void* cppHelper = GetFastInterpThrowHelper(m_exceptionTypeId);
    TestAssert(cppHelper != nullptr);

    if (m_isLValueObject)
    {
        // The address of the LValue object is the second parameter to the copy-constructor callExpr
        //
        AstCallExpr* callExpr = assert_cast<AstCallExpr*>(m_operand);
        AstNodeBase* expr = callExpr->GetParams()[1];
        TestAssert(expr->GetTypeId().IsPointerType() && expr->GetTypeId() == m_exceptionTypeId.AddPointer());
        TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->CanReserveWithoutSpill(expr->GetTypeId()));
        FastInterpSnippet snippet = expr->PrepareForFastInterp(x_FINoSpill);
        TestAssert(!snippet.IsEmpty() && !snippet.IsUncontinuable());

        FastInterpBoilerplateInstance* enterCppInst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FICallExprEnterCppFnImpl>::SelectBoilerplateBluePrint(
                        TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                        true /*isNoExcept*/));
        enterCppInst->PopulateCppFnPtrPlaceholder(0, cppHelper);

        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIThrowExceptionImpl>::SelectBoilerplateBluePrint(
                        true /*isQuickAccess*/));
        inst->PopulateBoilerplateFnPtrPlaceholder(1, enterCppInst);
        snippet = snippet.AddContinuation(inst);

        FastInterpBoilerplateInstance* cleanup = thread_pochiVMContext->m_scopedVariableManager.FIGenerateEHEntryPointForCurrentPosition();
        inst->PopulateBoilerplateFnPtrPlaceholder(0, cleanup);

        // This is weird. Despite 'throw' never returns, the caller expects it to have a continuation,
        // since unlike break/continue/return, we haven't locked down unreachable code after throw.
        // So just create a dummy noop operator in the end as workaround. Same for the other branch.
        // TODO: lockdown unreachable code after throw, like we did for break/continue/return
        //
        return FastInterpSnippet {
            snippet.m_entry, FIGetNoopBoilerplate()
        };
    }
    else
    {
        // Allocate space to store the temporary exception object.
        // It will be copy-constructed to the real exception object by the CPP helper.
        // This is allowed by C++ standard: C++17 guaranteed copy-elision does not apply to throw
        //
        uint64_t offset = thread_pochiVMContext->m_fastInterpStackFrameManager->PushLocalVar(variable.GetTypeId().RemovePointer());
        variable.SetFastInterpOffset(offset);

        FastInterpSnippet snippet;
        if (m_isCtor)
        {
            TestAssert(m_operand->GetAstNodeType() == AstNodeType::AstCallExpr);
            AstCallExpr* callExpr = assert_cast<AstCallExpr*>(m_operand);
            TestAssert(callExpr->GetTypeId().IsVoid());
            TestAssert(callExpr->GetParams().size() > 0 && callExpr->GetParams()[0]->GetAstNodeType() == AstNodeType::AstExceptionAddressPlaceholder);
            AstExceptionAddressPlaceholder* placeholder = assert_cast<AstExceptionAddressPlaceholder*>(callExpr->GetParams()[0]);
            placeholder->SetFastInterpValue(offset);
            snippet = callExpr->PrepareForFastInterp(x_FINoSpill);
        }
        else if (m_exceptionTypeId.IsCppClassType())
        {
            // The only way to get a RValue CPP type is from the return value of a callExpr
            //
            TestAssert(m_operand->GetAstNodeType() == AstNodeType::AstCallExpr);
            AstCallExpr* callExpr = assert_cast<AstCallExpr*>(m_operand);
            TestAssert(callExpr->IsCppFunction() && callExpr->GetCppFunctionMetadata()->m_isUsingSret);
            callExpr->SetFastInterpSretVariable(&variable);
            snippet = callExpr->PrepareForFastInterp(x_FINoSpill);
        }
        else
        {
            TestAssert(m_operand->GetTypeId().IsPrimitiveType() || m_operand->GetTypeId().IsPointerType());
            FISpillLocation loc;
            loc.SetSpillLocation(static_cast<uint32_t>(offset));
            snippet = m_operand->PrepareForFastInterp(loc);
        }

        // Now the exception object is constructed, and shall be destructed after the exception is thrown
        //
        thread_pochiVMContext->m_scopedVariableManager.PushObject(&variable);

        FastInterpBoilerplateInstance* enterCppInst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FICallExprEnterCppFnImpl>::SelectBoilerplateBluePrint(
                        TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                        true /*isNoExcept*/));
        enterCppInst->PopulateCppFnPtrPlaceholder(0, cppHelper);

        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIThrowExceptionImpl>::SelectBoilerplateBluePrint(
                        false /*isQuickAccess*/));
        inst->PopulateConstantPlaceholder<uint64_t>(0, offset);
        inst->PopulateBoilerplateFnPtrPlaceholder(1, enterCppInst);
        snippet = snippet.AddContinuation(inst);

        // Important to call FIGenerateEHEntryPointForCurrentPosition after pushing variable to scopeVariableManager,
        // since it must be destructed after exception is thrown
        //
        FastInterpBoilerplateInstance* cleanup = thread_pochiVMContext->m_scopedVariableManager.FIGenerateEHEntryPointForCurrentPosition();
        inst->PopulateBoilerplateFnPtrPlaceholder(0, cleanup);

        thread_pochiVMContext->m_fastInterpStackFrameManager->PopLocalVar(variable.GetTypeId().RemovePointer());

        return FastInterpSnippet {
            snippet.m_entry, FIGetNoopBoilerplate()
        };
    }
}

}   // namespace PochiVM
