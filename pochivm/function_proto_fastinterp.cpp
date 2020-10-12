#include "fastinterp_ast_helper.hpp"
#include "function_proto.h"
#include "codegen_context.hpp"

namespace PochiVM
{

FastInterpSnippet WARN_UNUSED AstDeclareVariable::PrepareForFastInterp(FISpillLocation TESTBUILD_ONLY(spillLoc))
{
    TestAssert(spillLoc.IsNoSpill());

    // First of all, allocate storage space in stack frame
    //
    {
        uint64_t offset = thread_pochiVMContext->m_fastInterpStackFrameManager->PushLocalVar(m_variable->GetTypeId().RemovePointer());
        m_variable->SetFastInterpOffset(offset);
    }

    FastInterpSnippet result;
    if (m_assignExpr != nullptr)
    {
        result = m_assignExpr->PrepareForFastInterp(x_FINoSpill);
    }
    else if (m_callExpr != nullptr)
    {
        if (!m_isCtor)
        {
            m_callExpr->SetFastInterpSretVariable(m_variable);
            result = m_callExpr->PrepareForFastInterp(x_FINoSpill);
        }
        else
        {
            result = m_callExpr->PrepareForFastInterp(x_FINoSpill);
        }
    }
    else
    {
        // Uninitialized primitive-typed variable, it is an no-op
        //
        result = FastInterpSnippet();
    }

    // After constructor is called, push variable to scope stack
    // Unlike LLVM mode (which only pushes CPP class that actually have destructors)
    // we need to push everything, since we need to reduce stack frame size when it is destructed.
    //
    TestAssert(thread_llvmContext->m_scopeStack.size() > 0);
    thread_llvmContext->m_scopeStack.back().second.push_back(m_variable);
    return result;
}

FastInterpSnippet WARN_UNUSED AstReturnStmt::PrepareForFastInterp(FISpillLocation TESTBUILD_ONLY(spillLoc))
{
    TestAssert(spillLoc.IsNoSpill());

    bool isNoExcept = true /*TODO FIXME*/;

    // Case 1: return void
    //
    {
        if (m_retVal == nullptr)
        {
            FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                        FastInterpBoilerplateLibrary<FIOutlinedReturnImpl>::SelectBoilerplateBluePrint(
                            TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                            isNoExcept,
                            false /*exceptionThrown*/,
                            thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral(),
                            thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat()));
            return FastInterpSnippet {
                inst, nullptr
            };
        }
    }

    // Case 2: matches operand shape
    // FIInlinedReturnImpl
    //
    {
        AstFIOperandShape osc = AstFIOperandShape::TryMatch(m_retVal);
        if (osc.MatchOK())
        {
            FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                        FastInterpBoilerplateLibrary<FIInlinedReturnImpl>::SelectBoilerplateBluePrint(
                            m_retVal->GetTypeId().GetOneLevelPtrFastInterpTypeId(),
                            osc.m_indexType,
                            isNoExcept,
                            osc.m_kind));
            osc.PopulatePlaceholder(inst, 0, 1);
            return FastInterpSnippet {
                inst, nullptr
            };
        }
    }

    // Case 3: default case, outlined operand
    // FIOutlinedReturnImpl
    //
    {
        thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(m_retVal->GetTypeId());
        FastInterpSnippet operand = m_retVal->PrepareForFastInterp(x_FINoSpill);
        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIOutlinedReturnImpl>::SelectBoilerplateBluePrint(
                        m_retVal->GetTypeId().GetOneLevelPtrFastInterpTypeId(),
                        isNoExcept,
                        false /*exceptionThrown*/,
                        thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral(),
                        thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat()));
        return operand.AddContinuation(FastInterpSnippet { inst, nullptr });
    }
}

}   // namespace PochiVM
