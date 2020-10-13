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

FastInterpSnippet WARN_UNUSED AstCallExpr::PrepareForFastInterp(FISpillLocation spillLoc)
{
    ReleaseAssert(!m_isCppFunction); // TODO handle cpp functions later
    AstFunction* callee = thread_pochiVMContext->m_curModule->GetAstFunction(m_fnName);
    TestAssert(callee != nullptr);
    
    // We do not know the stack frame size right now.
    // We will fix it in the end after all functions are compiled,
    // at that time all stack frame sizes are known.
    //
    FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallExprImpl>::SelectBoilerplateBluePrint(
                    callee->GetReturnType().GetOneLevelPtrFastInterpTypeId(),
                    !spillLoc.IsNoSpill(),
                    callee->GetIsNoExcept(),
                    static_cast<FIStackframeSizeCategory>(0)));
    spillLoc.PopulatePlaceholderIfSpill(inst, 0);
    m_fastInterpSpillLoc = spillLoc;
    m_fastInterpInst = inst;
    thread_pochiVMContext->m_fastInterpFnCallFixList.push_back(std::make_pair(callee, this));

    FastInterpSnippet callOp { nullptr, nullptr };

    // A function call invalidates all registers, so it is not beneficial to keep things in register
    // (it has to be pushed to the stack and restored). Force spill everything to memory.
    // The in-register stuff should consist of only the new stack frame pointer.
    //
    thread_pochiVMContext->m_fastInterpStackFrameManager->ForceSpillAll();
    thread_pochiVMContext->m_fastInterpStackFrameManager->PushTemp(TypeId::Get<uint64_t>());

    // Evaluate each parameter
    //
    bool isNewSfSpilled = false;
    for (size_t index = 0; index < m_params.size(); index++)
    {
        // Evaluate parameter
        //
        {
            thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(m_params[index]->GetTypeId());
            FastInterpSnippet snippet = m_params[index]->PrepareForFastInterp(x_FINoSpill);

            // If evaluating the parameter causes newStackFrame to be spilled to memory,
            // we need to spill it before transfering control to match the expectation.
            //
            if (!isNewSfSpilled)
            {
                FISpillLocation newsfSpillLoc = thread_pochiVMContext->m_fastInterpStackFrameManager->PeekTopTemp(TypeId::Get<uint64_t>());
                if (!newsfSpillLoc.IsNoSpill())
                {
                    isNewSfSpilled = true;
                    FastInterpBoilerplateInstance* spillOp = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                                FastInterpBoilerplateLibrary<FICallExprSpillStackAddrImpl>::SelectBoilerplateBluePrint(
                                    false /*isFakeSpillForNoParamsCase*/));
                    callOp = callOp.AddContinuation(spillOp);
                }
            }
            callOp = callOp.AddContinuation(snippet);
        }

        // Populate the evaluated parameter into new stack frame
        //
        {
            FastInterpBoilerplateInstance* fillParamOp;
            if (!isNewSfSpilled)
            {
                fillParamOp = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                            FastInterpBoilerplateLibrary<FICallExprStoreParamImpl>::SelectBoilerplateBluePrint(
                                m_params[index]->GetTypeId().GetOneLevelPtrFastInterpTypeId(),
                                index + 1 < m_params.size() /*hasMore*/));
            }
            else
            {
                FISpillLocation newsfSpillLoc = thread_pochiVMContext->m_fastInterpStackFrameManager->PeekTopTemp(TypeId::Get<uint64_t>());
                fillParamOp = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                            FastInterpBoilerplateLibrary<FICallExprStoreParamNewSfSpilledImpl>::SelectBoilerplateBluePrint(
                                m_params[index]->GetTypeId().GetOneLevelPtrFastInterpTypeId(),
                                index + 1 < m_params.size() /*hasMore*/));
                newsfSpillLoc.PopulatePlaceholderIfSpill(fillParamOp, 1);
            }
            fillParamOp->PopulateConstantPlaceholder<uint64_t>(0, index * 8 + 8);
            callOp = callOp.AddContinuation(fillParamOp);
        }
    }

    {
        FISpillLocation newsfSpillLoc = thread_pochiVMContext->m_fastInterpStackFrameManager->PopTemp(TypeId::Get<uint64_t>());
        TestAssertIff(isNewSfSpilled, !newsfSpillLoc.IsNoSpill());
        std::ignore = newsfSpillLoc;
    }

    if (m_params.size() == 0)
    {
        // The 'FICallExprImpl' operator is calling its continuation using arguments 'oldsf, newsf',
        // and expects the last function parameter to remove the 'oldsf'.
        // We have no function parameters, so remove the 'oldsf' arguments right now.
        //
        FastInterpBoilerplateInstance* spillOp = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FICallExprSpillStackAddrImpl>::SelectBoilerplateBluePrint(
                        true /*isFakeSpillForNoParamsCase*/));
        callOp = callOp.AddContinuation(spillOp);
    }

    // After all parameters are populated, transfer control to new function
    //
    TestAssert(callOp.m_tail != nullptr);
    thread_pochiVMContext->m_fastInterpEngine->PopulateBoilerplateFnPtrPlaceholderAsFunctionEntryPoint(
                callOp.m_tail, callee, 0 /*ordinal*/);

    // Entry from current function to callOp
    //
    TestAssert(callOp.m_entry != nullptr);
    inst->PopulateBoilerplateFnPtrPlaceholder(1, callOp.m_entry);

    // After call is complete, if the callee is not noexcept, we need to check for exception
    //
    TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral() == static_cast<FINumOpaqueIntegralParams>(0));
    TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat() == static_cast<FINumOpaqueFloatingParams>(0));
    FastInterpSnippet result { inst, inst };
    if (!callee->GetIsNoExcept())
    {
        if (!callee->GetReturnType().IsVoid() && spillLoc.IsNoSpill())
        {
            thread_pochiVMContext->m_fastInterpStackFrameManager->PushTemp(callee->GetReturnType());
        }
        FastInterpBoilerplateInstance* checkExnOp = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FICallExprCheckExceptionImpl>::SelectBoilerplateBluePrint(
                        thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral(),
                        thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat()));
        if (!callee->GetReturnType().IsVoid() && spillLoc.IsNoSpill())
        {
            FISpillLocation tmp = thread_pochiVMContext->m_fastInterpStackFrameManager->PopTemp(callee->GetReturnType());
            TestAssert(tmp.IsNoSpill());
            std::ignore = tmp;
        }
        // TODO: populate information about exception handler
        //
        result = result.AddContinuation(checkExnOp);
    }
    return result;
}

}   // namespace PochiVM
