#include "exception_helper.h"
#include "lang_constructs.h"
#include "function_proto.h"
#include "llvm_ast_helper.hpp"
#include "scoped_variable_manager.h"
#include "exception_helper.hpp"

namespace PochiVM
{

using namespace llvm;

static BasicBlock* CreateEmptyLandingPadBlock()
{
    BasicBlock* bb = BasicBlock::Create(
                *thread_llvmContext->m_llvmContext,
                Twine("landing_pad_").concat(Twine(thread_llvmContext->m_llvmLandingPadBlockOrdinal)),
                thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
    thread_llvmContext->m_llvmLandingPadBlockOrdinal++;
    return bb;
}

BasicBlock* WARN_UNUSED EmitEHLandingPadForCurrentPosition()
{
    auto savedIp = thread_llvmContext->m_builder->saveIP();
    Auto(thread_llvmContext->m_builder->restoreIP(savedIp));

    BasicBlock* dtorTree = thread_pochiVMContext->m_scopedVariableManager.EmitIRExceptionHandlerCleanupLogic();
    BasicBlock* landingPad = CreateEmptyLandingPadBlock();
    thread_llvmContext->m_builder->SetInsertPoint(landingPad);
    LandingPadInst* exn = thread_llvmContext->m_builder->CreateLandingPad(GetPersonalityFnReturnType(), 0 /*numClausesHint*/);
    exn->setCleanup(true);
    llvm::Value* exnObjectAlloca = thread_pochiVMContext->m_scopedVariableManager.GetLLVMExceptionObjectAlloca();
    llvm::Value* exnTypeAlloca = thread_pochiVMContext->m_scopedVariableManager.GetLLVMExceptionTypeAlloca();

    Value* exnObject = thread_llvmContext->m_builder->CreateExtractValue(exn, 0);
    AstTypeHelper::create_store_helper(TypeId::Get<void*>(), exnObject, exnObjectAlloca);
    Value* exnType = thread_llvmContext->m_builder->CreateExtractValue(exn, 1);
    AstTypeHelper::create_store_helper(TypeId::Get<int32_t>(), exnType, exnTypeAlloca);
    thread_llvmContext->m_builder->CreateBr(dtorTree);
    return landingPad;
}

bool WARN_UNUSED IsNoLandingPadNeeded()
{
    // TODO: need extra logic when we support catch
    //
    return (thread_pochiVMContext->m_scopedVariableManager.GetNumNontrivialDestructorObjects() == 0) &&
            !thread_llvmContext->m_curFunction->GetIsNoExcept();
}

}   // namespace PochiVM
