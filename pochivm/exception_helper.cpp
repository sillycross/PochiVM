#include "exception_helper.h"
#include "lang_constructs.h"
#include "function_proto.h"
#include "pochivm.hpp"

namespace PochiVM
{

using namespace llvm;

static Type* GetPersonalityFnReturnType()
{
    Type* types[2];
    types[0] = AstTypeHelper::llvm_type_of(TypeId::Get<void*>());
    types[1] = AstTypeHelper::llvm_type_of(TypeId::Get<int32_t>());
    StructType* stype = StructType::get(*thread_llvmContext->m_llvmContext, ArrayRef<Type*>(types, types + 2));
    return stype;
}

static BasicBlock* CreateEmptyDtorTreeBlock()
{
    BasicBlock* bb = BasicBlock::Create(
                *thread_llvmContext->m_llvmContext,
                Twine("dtor_tree_").concat(Twine(thread_llvmContext->m_dtorTreeBlockOrdinal)),
                thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
    thread_llvmContext->m_dtorTreeBlockOrdinal++;
    return bb;
}

static BasicBlock* CreateEmptyLandingPadBlock()
{
    BasicBlock* bb = BasicBlock::Create(
                *thread_llvmContext->m_llvmContext,
                Twine("landing_pad_").concat(Twine(thread_llvmContext->m_landingPadBlockOrdinal)),
                thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
    thread_llvmContext->m_landingPadBlockOrdinal++;
    return bb;
}

// Emit the exception handler cleanup logic
// One is supposed to branch to the logic after catching the exception with
// 'landingpad {i8*, i32} cleanup [catch ...] instruction.
// At the end of cleanup logic, control is transferred to 'catch' blocks
// (or 'resume' if there is no catch block).
//
// We are using too many int-to-unsigned (to access vector index) here.. They are apparently safe so disable the warning.
//
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
static BasicBlock* WARN_UNUSED EmitEHCleanupLogic()
{
    auto savedIp = thread_llvmContext->m_builder->saveIP();
    Auto(thread_llvmContext->m_builder->restoreIP(savedIp));

    TestAssert(thread_llvmContext->m_scopeStack.size() > 0);

    if (thread_llvmContext->m_ehCurExceptionObject == nullptr)
    {
        assert(thread_llvmContext->m_ehCurExceptionType == nullptr);
        // Create the alloca that stores the return value of personalityFn
        // The personalityFn returns { i8*, i32 }
        //
        thread_llvmContext->m_builder->SetInsertPoint(thread_llvmContext->GetCurFunction()->GetEntryBlock());
        thread_llvmContext->m_ehCurExceptionObject = thread_llvmContext->m_builder->CreateAlloca(
                    AstTypeHelper::llvm_type_of(TypeId::Get<void*>()), nullptr /*ArraySize*/, Twine("_cur_exception_object"));
        thread_llvmContext->m_ehCurExceptionType = thread_llvmContext->m_builder->CreateAlloca(
                    AstTypeHelper::llvm_type_of(TypeId::Get<int32_t>()), nullptr /*ArraySize*/, Twine("_cur_exception_type"));
        TestAssert(thread_llvmContext->m_ehCurExceptionObject != nullptr);
        TestAssert(thread_llvmContext->m_ehCurExceptionType != nullptr);
        TestAssert(AstTypeHelper::llvm_value_has_type(TypeId::Get<void**>(), thread_llvmContext->m_ehCurExceptionObject));
        TestAssert(AstTypeHelper::llvm_value_has_type(TypeId::Get<int32_t*>(), thread_llvmContext->m_ehCurExceptionType));
    }

    if (thread_llvmContext->m_currentEHCatchBlock == nullptr)
    {
        // Generate the catch block for the case that we are not in a try-catch block
        // We just propagate the exception to the caller
        //
        thread_llvmContext->m_currentEHCatchBlock = BasicBlock::Create(
                    *thread_llvmContext->m_llvmContext,
                    Twine("resume_exception"),
                    thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
        thread_llvmContext->m_builder->SetInsertPoint(thread_llvmContext->m_currentEHCatchBlock);
        Value* exnObject = AstTypeHelper::create_load_helper(TypeId::Get<void*>(), thread_llvmContext->m_ehCurExceptionObject);
        Value* exnType = AstTypeHelper::create_load_helper(TypeId::Get<int32_t>(), thread_llvmContext->m_ehCurExceptionType);

        Value* exn = UndefValue::get(GetPersonalityFnReturnType());
        exn = thread_llvmContext->m_builder->CreateInsertValue(exn, exnObject, 0);
        exn = thread_llvmContext->m_builder->CreateInsertValue(exn, exnType, 1);
        thread_llvmContext->m_builder->CreateResume(exn);
    }

    int n = static_cast<int>(thread_llvmContext->m_exceptionDtorTree.size());
    TestAssert(n <= static_cast<int>(thread_llvmContext->m_scopeStack.size()));
#ifdef TESTBUILD
    for (int i = 0; i < n - 1; i++)
    {
        TestAssert(thread_llvmContext->m_exceptionDtorTree[i].first == static_cast<int>(thread_llvmContext->m_scopeStack[i].second.size()));
    }
#endif
    // Create the root dtor tree block
    //
    if (n == 0)
    {
        BasicBlock* bb = CreateEmptyDtorTreeBlock();
        thread_llvmContext->m_builder->SetInsertPoint(bb);
        int allInLevel = static_cast<int>(thread_llvmContext->m_scopeStack[0].second.size());
        for (int i = allInLevel - 1; i >= 0; i--)
        {
            AstVariable* var = thread_llvmContext->m_scopeStack[0].second[i];
            var->EmitDestructVariableIR();
        }
        thread_llvmContext->m_builder->CreateBr(thread_llvmContext->m_currentEHCatchBlock);
        thread_llvmContext->m_exceptionDtorTree.push_back(std::make_pair(allInLevel, bb));
        n++;
    }

    // Add destructors for the current scope for the newly constructed local vars after the previous generated block
    //
    {
        assert(n > 0 && thread_llvmContext->m_exceptionDtorTree.size() == static_cast<size_t>(n));
        int numInCurLevel = thread_llvmContext->m_exceptionDtorTree[n - 1].first;
        int allInCurLevel = static_cast<int>(thread_llvmContext->m_scopeStack[n - 1].second.size());
        TestAssert(numInCurLevel <= allInCurLevel);
        if (numInCurLevel < allInCurLevel)
        {
            BasicBlock* bb = CreateEmptyDtorTreeBlock();
            thread_llvmContext->m_builder->SetInsertPoint(bb);
            for (int i = allInCurLevel - 1; i >= numInCurLevel; i--)
            {
                AstVariable* var = thread_llvmContext->m_scopeStack[n - 1].second[i];
                var->EmitDestructVariableIR();
            }
            thread_llvmContext->m_builder->CreateBr(thread_llvmContext->m_exceptionDtorTree[n - 1].second);
            thread_llvmContext->m_exceptionDtorTree[n - 1] = std::make_pair(allInCurLevel, bb);
        }
    }

    while (static_cast<size_t>(n) < thread_llvmContext->m_scopeStack.size())
    {
        BasicBlock* bb = CreateEmptyDtorTreeBlock();
        thread_llvmContext->m_builder->SetInsertPoint(bb);
        int allInLevel = static_cast<int>(thread_llvmContext->m_scopeStack[n].second.size());
        for (int i = allInLevel - 1; i >= 0; i--)
        {
            AstVariable* var = thread_llvmContext->m_scopeStack[n].second[i];
            var->EmitDestructVariableIR();
        }
        thread_llvmContext->m_builder->CreateBr(thread_llvmContext->m_exceptionDtorTree[n - 1].second);
        thread_llvmContext->m_exceptionDtorTree.push_back(std::make_pair(allInLevel, bb));
        n++;
    }
    TestAssert(static_cast<size_t>(n) == thread_llvmContext->m_exceptionDtorTree.size());
    TestAssert(thread_llvmContext->m_exceptionDtorTree.size() == thread_llvmContext->m_scopeStack.size());
#ifdef TESTBUILD
    for (size_t i = 0; i < thread_llvmContext->m_exceptionDtorTree.size() - 1; i++)
    {
        TestAssert(thread_llvmContext->m_exceptionDtorTree[i].first == static_cast<int>(thread_llvmContext->m_scopeStack[i].second.size()));
    }
#endif
    return thread_llvmContext->m_exceptionDtorTree.back().second;
}
#pragma clang diagnostic pop    // for "-Wno-sign-conversion"

BasicBlock* WARN_UNUSED EmitEHLandingPadForCurrentPosition()
{
    auto savedIp = thread_llvmContext->m_builder->saveIP();
    Auto(thread_llvmContext->m_builder->restoreIP(savedIp));

    BasicBlock* dtorTree = EmitEHCleanupLogic();
    BasicBlock* landingPad = CreateEmptyLandingPadBlock();
    thread_llvmContext->m_builder->SetInsertPoint(landingPad);
    LandingPadInst* exn = thread_llvmContext->m_builder->CreateLandingPad(GetPersonalityFnReturnType(), 0 /*numClausesHint*/);
    exn->setCleanup(true);
    TestAssert(thread_llvmContext->m_ehCurExceptionObject != nullptr);
    Value* exnObject = thread_llvmContext->m_builder->CreateExtractValue(exn, 0);
    AstTypeHelper::create_store_helper(TypeId::Get<void*>(), exnObject, thread_llvmContext->m_ehCurExceptionObject);
    Value* exnType = thread_llvmContext->m_builder->CreateExtractValue(exn, 1);
    AstTypeHelper::create_store_helper(TypeId::Get<int32_t>(), exnType, thread_llvmContext->m_ehCurExceptionType);
    thread_llvmContext->m_builder->CreateBr(dtorTree);
    return landingPad;
}

bool WARN_UNUSED IsNoLandingPadNeeded()
{
    // TODO: need extra logic when we support catch
    //
    for (size_t i = 0; i < thread_llvmContext->m_scopeStack.size(); i++)
    {
        if (thread_llvmContext->m_scopeStack[i].second.size() > 0)
        {
            return false;
        }
    }
    return true;
}

}   // namespace PochiVM
