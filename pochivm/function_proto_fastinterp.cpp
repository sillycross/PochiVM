#include "fastinterp_ast_helper.hpp"
#include "function_proto.h"
#include "codegen_context.hpp"
#include "destructor_helper.h"

namespace PochiVM
{

void AstDeclareVariable::FastInterpSetupSpillLocation()
{
    if (m_assignExpr != nullptr)
    {
        m_assignExpr->FastInterpSetupSpillLocation();
    }
    else if (m_callExpr != nullptr)
    {
        m_callExpr->FastInterpSetupSpillLocation();
    }
}

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

void AstReturnStmt::FastInterpSetupSpillLocation()
{
    if (m_retVal != nullptr)
    {
        thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(m_retVal->GetTypeId());
        m_retVal->FastInterpSetupSpillLocation();
    }
}

FastInterpSnippet WARN_UNUSED AstReturnStmt::PrepareForFastInterp(FISpillLocation TESTBUILD_ONLY(spillLoc))
{
    TestAssert(spillLoc.IsNoSpill());
    thread_pochiVMContext->m_fastInterpStackFrameManager->AssertNoTemp();

    bool isNoExcept = thread_llvmContext->m_curFunction->GetIsNoExcept();

    FastInterpSnippet dtors = FIGenerateDestructorSequenceUntilScope(nullptr /*everything*/);

    // Case 1: return void
    //
    if (m_retVal == nullptr)
    {
        TestAssert(thread_llvmContext->m_curFunction->GetReturnType().IsVoid());
        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIOutlinedReturnImpl>::SelectBoilerplateBluePrint(
                        TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                        isNoExcept,
                        false /*exceptionThrown*/,
                        thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral(),
                        thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat()));
        FastInterpSnippet snippet = dtors.AddContinuation(inst);
        return FastInterpSnippet {
            snippet.m_entry, nullptr
        };
    }

    TestAssert(thread_llvmContext->m_curFunction->GetReturnType() == m_retVal->GetTypeId() && !m_retVal->GetTypeId().IsVoid());

    if (dtors.IsEmpty())
    {
        // In the case that we have no dtors to run, we can directly return, without spilling to memory
        //

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
            TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->CanReserveWithoutSpill(m_retVal->GetTypeId()));
            FastInterpSnippet operand = m_retVal->PrepareForFastInterp(x_FINoSpill);
            FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                        FastInterpBoilerplateLibrary<FIOutlinedReturnImpl>::SelectBoilerplateBluePrint(
                            m_retVal->GetTypeId().GetOneLevelPtrFastInterpTypeId(),
                            isNoExcept,
                            false /*exceptionThrown*/,
                            thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral(),
                            thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat()));
            FastInterpSnippet snippet = operand.AddContinuation(inst);
            return FastInterpSnippet {
                snippet.m_entry, nullptr
            };
        }
    }
    else
    {
        // Otherwise, the return statement must be evaluated BEFORE the dtors
        // So we must force spill the return statement to a memory location
        //
        uint64_t offset = thread_pochiVMContext->m_fastInterpStackFrameManager->PushLocalVar(m_retVal->GetTypeId());
        FISpillLocation sloc;
        sloc.SetSpillLocation(static_cast<uint32_t>(offset));

        TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->CanReserveWithoutSpill(m_retVal->GetTypeId()));
        FastInterpSnippet operand = m_retVal->PrepareForFastInterp(sloc);

        FastInterpSnippet snippet = operand.AddContinuation(dtors);

        FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FIReturnSpilledLocationImpl>::SelectBoilerplateBluePrint(
                        m_retVal->GetTypeId().GetOneLevelPtrFastInterpTypeId(),
                        isNoExcept));
        inst->PopulateConstantPlaceholder<uint64_t>(0, offset);
        snippet = snippet.AddContinuation(inst);

        thread_pochiVMContext->m_fastInterpStackFrameManager->PopLocalVar(m_retVal->GetTypeId());

        return FastInterpSnippet {
            snippet.m_entry, nullptr
        };
    }
}

void AstCallExpr::FastInterpSetupSpillLocation()
{
    thread_pochiVMContext->m_fastInterpStackFrameManager->ForceSpillAll();
    thread_pochiVMContext->m_fastInterpStackFrameManager->PushTemp(TypeId::Get<uint64_t>());

    bool isNewSfSpilled = false;
    m_fastInterpSpillNewSfAddrAt = static_cast<uint32_t>(m_params.size());
    for (size_t index = 0; index < m_params.size(); index++)
    {
        thread_pochiVMContext->m_fastInterpStackFrameManager->ReserveTemp(m_params[index]->GetTypeId());
        m_params[index]->FastInterpSetupSpillLocation();

        if (!isNewSfSpilled)
        {
            FISpillLocation newsfSpillLoc = thread_pochiVMContext->m_fastInterpStackFrameManager->PeekTopTemp(TypeId::Get<uint64_t>());
            if (!newsfSpillLoc.IsNoSpill())
            {
                m_fastInterpSpillNewSfAddrAt = static_cast<uint32_t>(index);
                isNewSfSpilled = true;
            }
        }
    }

    FISpillLocation newsfSpillLoc = thread_pochiVMContext->m_fastInterpStackFrameManager->PopTemp(TypeId::Get<uint64_t>());
    TestAssertIff(isNewSfSpilled, !newsfSpillLoc.IsNoSpill());
    std::ignore = newsfSpillLoc;
}

FastInterpSnippet WARN_UNUSED AstCallExpr::PrepareForFastInterp(FISpillLocation spillLoc)
{
    AstFunction* astCallee = nullptr;
    void* cppInterpCallee = nullptr;
    bool isCalleeNoExcept;
    TypeId calleeReturnType;
    size_t trueNumParams;
    FastInterpBoilerplateInstance* inst;
    if (!m_isCppFunction)
    {
        astCallee = thread_pochiVMContext->m_curModule->GetAstFunction(m_fnName);
        TestAssert(astCallee != nullptr);
        isCalleeNoExcept = astCallee->GetIsNoExcept();
        calleeReturnType = astCallee->GetReturnType();
        trueNumParams = astCallee->GetNumParams();
        // For generated function, we do not know the stack frame size right now.
        // We will fix it in the end after all functions are compiled,
        // at that time all stack frame sizes are known.
        //
        inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallExprImpl>::SelectBoilerplateBluePrint(
                    calleeReturnType.GetOneLevelPtrFastInterpTypeId(),
                    !spillLoc.IsNoSpill(),
                    isCalleeNoExcept,
                    static_cast<FIStackframeSizeCategory>(0)));
        spillLoc.PopulatePlaceholderIfSpill(inst, 0);
        m_fastInterpSpillLoc = spillLoc;
        m_fastInterpInst = inst;
        thread_pochiVMContext->m_fastInterpFnCallFixList.push_back(std::make_pair(astCallee, this));
    }
    else
    {
        // For C++ function, the stack frame size is fixed: it is always 8 * (2 * #params + 1) bytes
        //
        trueNumParams = m_cppFunctionMd->m_numParams + (m_cppFunctionMd->m_isUsingSret ? 1 : 0);
        FastInterpCppFunctionInfo info = m_cppFunctionMd->m_getFastInterpFn();
        isCalleeNoExcept = info.m_isNoExcept;
        calleeReturnType = info.m_returnType;
        cppInterpCallee = info.m_interpImpl;

        TestAssert(isCalleeNoExcept == m_cppFunctionMd->m_isNoExcept);
        TestAssertImp(m_cppFunctionMd->m_isUsingSret, calleeReturnType.IsVoid());
        TestAssertImp(!m_cppFunctionMd->m_isUsingSret, calleeReturnType == m_cppFunctionMd->m_returnType);
        size_t stackFrameSize = 8 * (2 * trueNumParams + 1);
        inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICallExprImpl>::SelectBoilerplateBluePrint(
                    calleeReturnType.GetOneLevelPtrFastInterpTypeId(),
                    !spillLoc.IsNoSpill(),
                    isCalleeNoExcept,
                    FIStackframeSizeCategoryHelper::SelectCategory(stackFrameSize)));
        spillLoc.PopulatePlaceholderIfSpill(inst, 0);
    }

    FastInterpSnippet callOp { nullptr, nullptr };

    // A function call invalidates all registers, so it is not beneficial to keep things in register
    // (it has to be pushed to the stack and restored). Force spill everything to memory.
    // The in-register stuff should consist of only the new stack frame pointer.
    //
    thread_pochiVMContext->m_fastInterpStackFrameManager->ForceSpillAll();
    thread_pochiVMContext->m_fastInterpStackFrameManager->PushTemp(TypeId::Get<uint64_t>());

    // Populate sret parameter
    //
    if (m_isCppFunction && m_cppFunctionMd->m_isUsingSret)
    {
        TestAssert(m_fastInterpSretVar != nullptr);
        TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->CanReserveWithoutSpill(m_fastInterpSretVar->GetTypeId()));
        FastInterpSnippet snippet = m_fastInterpSretVar->PrepareForFastInterp(x_FINoSpill);
        FISpillLocation newsfSpillLoc = thread_pochiVMContext->m_fastInterpStackFrameManager->PeekTopTemp(TypeId::Get<uint64_t>());
        TestAssert(newsfSpillLoc.IsNoSpill());

        FastInterpBoilerplateInstance* fillParamOp = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FICallExprStoreParamImpl>::SelectBoilerplateBluePrint(
                        m_fastInterpSretVar->GetTypeId().GetOneLevelPtrFastInterpTypeId(),
                        trueNumParams > 1 /*hasMore*/));
        fillParamOp->PopulateConstantPlaceholder<uint64_t>(0, 8 /*offset*/);
        callOp = callOp.AddContinuation(snippet).AddContinuation(fillParamOp);
    }

    // Evaluate each parameter
    //
    TestAssert(0 <= m_fastInterpSpillNewSfAddrAt && m_fastInterpSpillNewSfAddrAt <= static_cast<uint32_t>(m_params.size()));
    for (size_t index = 0; index < m_params.size(); index++)
    {
        if (m_fastInterpSpillNewSfAddrAt == static_cast<uint32_t>(index))
        {
            TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral() == static_cast<FINumOpaqueIntegralParams>(1));
            TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat() == static_cast<FINumOpaqueFloatingParams>(0));
            thread_pochiVMContext->m_fastInterpStackFrameManager->ForceSpillAll();
        }

        // Evaluate parameter
        //
        TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->CanReserveWithoutSpill(m_params[index]->GetTypeId()));
        FastInterpSnippet snippet = m_params[index]->PrepareForFastInterp(x_FINoSpill);

        FISpillLocation newsfSpillLoc = thread_pochiVMContext->m_fastInterpStackFrameManager->PeekTopTemp(TypeId::Get<uint64_t>());
        TestAssertIff(newsfSpillLoc.IsNoSpill(), static_cast<uint32_t>(index) < m_fastInterpSpillNewSfAddrAt);

        // If evaluating the parameter causes newStackFrame to be spilled to memory,
        // we need to spill it before transfering control to match the expectation.
        //
        if (m_fastInterpSpillNewSfAddrAt == static_cast<uint32_t>(index))
        {
            FastInterpBoilerplateInstance* spillOp = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                        FastInterpBoilerplateLibrary<FICallExprSpillStackAddrImpl>::SelectBoilerplateBluePrint(
                            false /*isFakeSpillForNoParamsCase*/));
            newsfSpillLoc.PopulatePlaceholderIfSpill(spillOp, 0);
            callOp = callOp.AddContinuation(spillOp);
        }

        callOp = callOp.AddContinuation(snippet);

        // Populate the evaluated parameter into new stack frame
        // TODO: handle RValueToConstPrimitiveReference
        //
        {
            FastInterpBoilerplateInstance* fillParamOp;
            if (newsfSpillLoc.IsNoSpill())
            {
                fillParamOp = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                            FastInterpBoilerplateLibrary<FICallExprStoreParamImpl>::SelectBoilerplateBluePrint(
                                m_params[index]->GetTypeId().GetOneLevelPtrFastInterpTypeId(),
                                index + 1 < m_params.size() /*hasMore*/));
            }
            else
            {
                fillParamOp = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                            FastInterpBoilerplateLibrary<FICallExprStoreParamNewSfSpilledImpl>::SelectBoilerplateBluePrint(
                                m_params[index]->GetTypeId().GetOneLevelPtrFastInterpTypeId(),
                                index + 1 < m_params.size() /*hasMore*/));
                newsfSpillLoc.PopulatePlaceholderIfSpill(fillParamOp, 1);
            }
            uint64_t offset = index * 8 + 8;
            if (m_isCppFunction && m_cppFunctionMd->m_isUsingSret)
            {
                offset += 8;
            }
            fillParamOp->PopulateConstantPlaceholder<uint64_t>(0, offset);
            callOp = callOp.AddContinuation(fillParamOp);
        }
    }

    {
        FISpillLocation newsfSpillLoc = thread_pochiVMContext->m_fastInterpStackFrameManager->PopTemp(TypeId::Get<uint64_t>());
        TestAssertIff(m_fastInterpSpillNewSfAddrAt == static_cast<uint32_t>(m_params.size()), newsfSpillLoc.IsNoSpill());
        std::ignore = newsfSpillLoc;
    }

    if (trueNumParams == 0)
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
    if (!m_isCppFunction)
    {
        // For generated function, we just attach the entry point of the function as our continuation
        //
        thread_pochiVMContext->m_fastInterpEngine->PopulateBoilerplateFnPtrPlaceholderAsFunctionEntryPoint(
                    callOp.m_tail, astCallee, 0 /*ordinal*/);
        callOp.m_tail = nullptr;
    }
    else
    {
        // For CPP function, we need another operator which transfers control to the interp entry point
        //
        FastInterpBoilerplateInstance* entryInst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FICallExprEnterCppFnImpl>::SelectBoilerplateBluePrint(
                        calleeReturnType.GetDefaultFastInterpTypeId(),
                        isCalleeNoExcept));
        entryInst->PopulateCppFnPtrPlaceholder(0, reinterpret_cast<void(*)(void) noexcept>(cppInterpCallee));
        callOp = callOp.AddContinuation(entryInst);
        callOp.m_tail = nullptr;
    }

    // Entry from current function to callOp
    //
    TestAssert(callOp.m_entry != nullptr);
    inst->PopulateBoilerplateFnPtrPlaceholder(1, callOp.m_entry);

    // After call is complete, if the callee is not noexcept, we need to check for exception
    //
    TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral() == static_cast<FINumOpaqueIntegralParams>(0));
    TestAssert(thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat() == static_cast<FINumOpaqueFloatingParams>(0));
    FastInterpSnippet result { inst, inst };
    if (!isCalleeNoExcept)
    {
        if (!calleeReturnType.IsVoid() && spillLoc.IsNoSpill())
        {
            thread_pochiVMContext->m_fastInterpStackFrameManager->PushTemp(calleeReturnType);
        }
        FastInterpBoilerplateInstance* checkExnOp = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FICallExprCheckExceptionImpl>::SelectBoilerplateBluePrint(
                        thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillIntegral(),
                        thread_pochiVMContext->m_fastInterpStackFrameManager->GetNumNoSpillFloat()));
        // TODO: FIXME
        // Program position indicator
        //
        checkExnOp->PopulateConstantPlaceholder<uint64_t>(0, 123);
        // C++ soft exception emulator
        //
        checkExnOp->PopulateCppFnPtrPlaceholder(0, reinterpret_cast<void(*)(uintptr_t, uintptr_t) noexcept>(0x123));
        // Catch clause
        //
        {
            FastInterpBoilerplateInstance* trap = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                        FastInterpBoilerplateLibrary<FIAbortTrapImpl>::SelectBoilerplateBluePrint(true /*dummy*/));
            checkExnOp->PopulateBoilerplateFnPtrPlaceholder(1, trap);
        }

        if (!calleeReturnType.IsVoid() && spillLoc.IsNoSpill())
        {
            FISpillLocation tmp = thread_pochiVMContext->m_fastInterpStackFrameManager->PopTemp(calleeReturnType);
            TestAssert(tmp.IsNoSpill());
            std::ignore = tmp;
        }
        // TODO: populate information about exception handler
        //
        result = result.AddContinuation(checkExnOp);
    }
    return result;
}

void AstFunction::PrepareForFastInterp()
{
    TestAssert(thread_llvmContext->m_curFunction == nullptr);
    thread_llvmContext->m_scopeStack.clear();
    thread_pochiVMContext->m_fastInterpStackFrameManager->Reset(static_cast<uint32_t>(m_params.size() + 1) * 8);
    thread_llvmContext->m_curFunction = this;
    thread_llvmContext->m_fiBreakStmtTarget.clear();
    thread_llvmContext->m_fiContinueStmtTarget.clear();

    for (size_t index = 0; index < m_params.size(); index++)
    {
        m_params[index]->SetFastInterpOffset((index + 1) * 8);
    }

    m_body->FastInterpSetupSpillLocation();
    thread_pochiVMContext->m_fastInterpStackFrameManager->AssertEmpty();

    FastInterpSnippet body = m_body->PrepareForFastInterp(x_FINoSpill);
    if (body.m_tail != nullptr)
    {
        // We may reach end of control flow without an explicit return.
        // If the function returns void, we are good: it's just a normal return.
        // Otherwise, it means the function has a return value but user did not set it,
        // we fire an assert at execution time if the control flow actually hit the branch.
        //
        if (GetReturnType().IsVoid())
        {
            FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                        FastInterpBoilerplateLibrary<FIOutlinedReturnImpl>::SelectBoilerplateBluePrint(
                            TypeId::Get<void>().GetDefaultFastInterpTypeId(),
                            GetIsNoExcept(),
                            false /*exceptionThrown*/,
                            FIOpaqueParamsHelper::GetMaxOIP(),
                            FIOpaqueParamsHelper::GetMaxOFP()));
            body = body.AddContinuation(FastInterpSnippet { inst, nullptr });
        }
        else
        {
            FastInterpBoilerplateInstance* inst = thread_pochiVMContext->m_fastInterpEngine->InstantiateBoilerplate(
                        FastInterpBoilerplateLibrary<FIAbortTrapImpl>::SelectBoilerplateBluePrint(true));
            body = body.AddContinuation(FastInterpSnippet { inst, nullptr });
        }
    }

    // Align the entry point of the function to 16 bytes
    //
    TestAssert(!body.IsEmpty());
    body.m_entry->SetAlignmentLog2(4);

    thread_pochiVMContext->m_fastInterpEngine->RegisterGeneratedFunctionEntryPoint(this, body.m_entry);

    TestAssert(m_fastInterpStackFrameSize == static_cast<uint32_t>(-1));
    m_fastInterpStackFrameSize = thread_pochiVMContext->m_fastInterpStackFrameManager->GetFinalStackFrameSize();

    thread_pochiVMContext->m_fastInterpStackFrameManager->AssertEmpty();
    TestAssert(thread_llvmContext->m_breakStmtTarget.size() == 0);
    TestAssert(thread_llvmContext->m_continueStmtTarget.size() == 0);
    TestAssert(thread_llvmContext->m_scopeStack.size() == 0);
    thread_llvmContext->m_curFunction = nullptr;
}

// The final stack frame size cannot be known until a function is generated.
// Therefore, for recursion or mutual recursion, we cannot select the correct boilerplate instance at generation time.
// We fix them here after all functions have been generated.
//
void AstCallExpr::FastInterpFixStackFrameSize(AstFunction* target)
{
    FIStackframeSizeCategory sfsCat = FIStackframeSizeCategoryHelper::SelectCategory(target->GetFastInterpStackFrameSize());
    m_fastInterpInst->ReplaceBluePrint(
                FastInterpBoilerplateLibrary<FICallExprImpl>::SelectBoilerplateBluePrint(
                    target->GetReturnType().GetOneLevelPtrFastInterpTypeId(),
                    !m_fastInterpSpillLoc.IsNoSpill(),
                    target->GetIsNoExcept(),
                    sfsCat));
}

void AstModule::PrepareForFastInterp()
{
    TestAssert(!m_fastInterpPrepared);
#ifdef TESTBUILD
    m_fastInterpPrepared = true;
#endif

    if (thread_pochiVMContext->m_fastInterpStackFrameManager == nullptr)
    {
        thread_pochiVMContext->m_fastInterpStackFrameManager = new FIStackFrameManager();
    }
    if (thread_pochiVMContext->m_fastInterpEngine == nullptr)
    {
        thread_pochiVMContext->m_fastInterpEngine = new FastInterpCodegenEngine();
    }

    thread_pochiVMContext->m_fastInterpEngine->Reset();
    thread_pochiVMContext->m_fastInterpFnCallFixList.clear();
    thread_llvmContext->m_scopeStack.clear();

    AstTraverseColorMark::ClearAll();
    for (auto iter = m_functions.begin(); iter != m_functions.end(); iter++)
    {
        AstFunction* fn = iter->second;
        fn->PrepareForFastInterp();
    }

    for (auto iter = thread_pochiVMContext->m_fastInterpFnCallFixList.begin();
         iter != thread_pochiVMContext->m_fastInterpFnCallFixList.end(); iter++)
    {
        AstFunction* fn = iter->first;
        AstCallExpr* callExpr = iter->second;
        callExpr->FastInterpFixStackFrameSize(fn);
    }

    {
        std::unique_ptr<FastInterpGeneratedProgram> gp = thread_pochiVMContext->m_fastInterpEngine->Materialize();
        if (thread_pochiVMContext->m_fastInterpGeneratedProgram != nullptr)
        {
            delete thread_pochiVMContext->m_fastInterpGeneratedProgram;
            thread_pochiVMContext->m_fastInterpGeneratedProgram = nullptr;
        }
        thread_pochiVMContext->m_fastInterpGeneratedProgram = gp.release();
    }

    for (auto iter = m_functions.begin(); iter != m_functions.end(); iter++)
    {
        AstFunction* fn = iter->second;
        fn->SetFastInterpCppEntryPoint(thread_pochiVMContext->m_fastInterpGeneratedProgram->GetGeneratedFunctionAddress(fn));
    }
}

}   // namespace PochiVM
