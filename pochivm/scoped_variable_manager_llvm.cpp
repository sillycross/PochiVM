#include "destructor_helper.h"
#include "llvm_ast_helper.hpp"
#include "scoped_variable_manager.h"
#include "function_proto.h"
#include "exception_helper.hpp"

namespace PochiVM
{

using namespace llvm;

void ScopedVariableManager::EmitIRDestructAllVariablesUntilScope(AstNodeBase* boundaryScope)
{
    TestAssert(m_operationMode == OperationMode::LLVM);
    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    auto rit = m_scopeStack.rbegin();
    while (rit != m_scopeStack.rend())
    {
        const std::vector<DestructorIREmitter*>& vec = rit->second;
        for (auto rit2 = vec.rbegin(); rit2 != vec.rend(); rit2++)
        {
            DestructorIREmitter* e = *rit2;
            e->EmitDestructorIR();
        }
        if (rit->first == boundaryScope)
        {
            break;
        }
        rit++;
    }
    TestAssertImp(boundaryScope != nullptr, rit != m_scopeStack.rend() && rit->first == boundaryScope);
    TestAssertImp(boundaryScope == nullptr, rit == m_scopeStack.rend());
}

BasicBlock* ScopedVariableManager::CreateEmptyDtorTreeBlock()
{
    BasicBlock* bb = BasicBlock::Create(
                *thread_llvmContext->m_llvmContext,
                Twine("dtor_tree_").concat(Twine(m_llvmDtorTreeBlockOrdinal)),
                thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
    m_llvmDtorTreeBlockOrdinal++;
    return bb;
}

// We are using too many int-to-unsigned (to access vector index) here.. They are apparently safe so disable the warning.
//
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
BasicBlock* WARN_UNUSED ScopedVariableManager::EmitIRExceptionHandlerCleanupLogic()
{
    TestAssert(m_operationMode == OperationMode::LLVM);

    auto savedIp = thread_llvmContext->m_builder->saveIP();
    Auto(thread_llvmContext->m_builder->restoreIP(savedIp));

    TestAssert(m_scopeStack.size() > 0);

    if (m_llvmEhCurExceptionObject == nullptr)
    {
        assert(m_llvmEhCurExceptionType == nullptr);
        // Create the alloca that stores the return value of personalityFn
        // The personalityFn returns { i8*, i32 }
        //
        thread_llvmContext->m_builder->SetInsertPoint(thread_llvmContext->GetCurFunction()->GetEntryBlock());
        m_llvmEhCurExceptionObject = thread_llvmContext->m_builder->CreateAlloca(
                    AstTypeHelper::llvm_type_of(TypeId::Get<void*>()), nullptr /*ArraySize*/, Twine("_cur_exception_object"));
        m_llvmEhCurExceptionType = thread_llvmContext->m_builder->CreateAlloca(
                    AstTypeHelper::llvm_type_of(TypeId::Get<int32_t>()), nullptr /*ArraySize*/, Twine("_cur_exception_type"));
        TestAssert(m_llvmEhCurExceptionObject != nullptr);
        TestAssert(m_llvmEhCurExceptionType != nullptr);
        TestAssert(AstTypeHelper::llvm_value_has_type(TypeId::Get<void**>(), m_llvmEhCurExceptionObject));
        TestAssert(AstTypeHelper::llvm_value_has_type(TypeId::Get<int32_t*>(), m_llvmEhCurExceptionType));
    }

    if (m_llvmCurrentEHCatchBlock == nullptr)
    {
        // Generate the catch block for the case that we are not in a try-catch block
        //
        if (thread_llvmContext->m_curFunction->GetIsNoExcept())
        {
            // If the function is marked noexcept, the action is to terminate
            //
            m_llvmCurrentEHCatchBlock = BasicBlock::Create(
                        *thread_llvmContext->m_llvmContext,
                        Twine("unexpected_exn_terminate"),
                        thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
            thread_llvmContext->m_builder->SetInsertPoint(m_llvmCurrentEHCatchBlock);
            Value* exnObject = AstTypeHelper::create_load_helper(TypeId::Get<void*>(), m_llvmEhCurExceptionObject);

            // %tmp = call i8* @__cxa_begin_catch(i8* %0)
            //
            Function* callee = thread_llvmContext->m_module->getFunction("__cxa_begin_catch");
            if (callee == nullptr)
            {
                Type* returnType = AstTypeHelper::llvm_type_of(TypeId::Get<void*>());
                Type* paramTypes[1] = { AstTypeHelper::llvm_type_of(TypeId::Get<void*>()) };
                FunctionType* funcType = FunctionType::get(returnType,
                                                           ArrayRef<Type*>(paramTypes, paramTypes + 1),
                                                           false /*isVariadic*/);
                callee = Function::Create(
                            funcType, Function::ExternalLinkage, "__cxa_begin_catch", thread_llvmContext->m_module);
                callee->setDSOLocal(true);
                callee->addFnAttr(Attribute::AttrKind::NoUnwind);
            }
            TestAssert(callee != nullptr && callee->arg_size() == 1);
            Value* params[1] = { exnObject };
            thread_llvmContext->m_builder->CreateCall(callee, ArrayRef<Value*>(params, params + 1));

            // call void @_ZSt9terminatev()
            //
            callee = thread_llvmContext->m_module->getFunction("_ZSt9terminatev");
            if (callee == nullptr)
            {
                Type* returnType = AstTypeHelper::llvm_type_of(TypeId::Get<void>());
                FunctionType* funcType = FunctionType::get(returnType,
                                                           ArrayRef<Type*>(),
                                                           false /*isVariadic*/);
                callee = Function::Create(
                            funcType, Function::ExternalLinkage, "_ZSt9terminatev", thread_llvmContext->m_module);
                callee->setDSOLocal(true);
                callee->addFnAttr(Attribute::AttrKind::NoReturn);
                callee->addFnAttr(Attribute::AttrKind::NoUnwind);
            }
            TestAssert(callee != nullptr && callee->arg_size() == 0);
            thread_llvmContext->m_builder->CreateCall(callee, ArrayRef<Value*>());
            thread_llvmContext->m_builder->CreateUnreachable();
        }
        else
        {
            // We just propagate the exception to the caller
            //
            m_llvmCurrentEHCatchBlock = BasicBlock::Create(
                        *thread_llvmContext->m_llvmContext,
                        Twine("resume_exception"),
                        thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
            thread_llvmContext->m_builder->SetInsertPoint(m_llvmCurrentEHCatchBlock);
            Value* exnObject = AstTypeHelper::create_load_helper(TypeId::Get<void*>(), m_llvmEhCurExceptionObject);
            Value* exnType = AstTypeHelper::create_load_helper(TypeId::Get<int32_t>(), m_llvmEhCurExceptionType);

            Value* exn = UndefValue::get(GetPersonalityFnReturnType());
            exn = thread_llvmContext->m_builder->CreateInsertValue(exn, exnObject, 0);
            exn = thread_llvmContext->m_builder->CreateInsertValue(exn, exnType, 1);
            thread_llvmContext->m_builder->CreateResume(exn);
        }
    }

    if (thread_llvmContext->m_curFunction->GetIsNoExcept())
    {
        return m_llvmCurrentEHCatchBlock;
    }

    int n = static_cast<int>(m_llvmExceptionDtorTree.size());
    TestAssert(n <= static_cast<int>(m_scopeStack.size()));
#ifdef TESTBUILD
    for (int i = 0; i < n - 1; i++)
    {
        TestAssert(m_llvmExceptionDtorTree[i].first == static_cast<int>(m_scopeStack[i].second.size()));
    }
#endif
    // Create the root dtor tree block
    //
    if (n == 0)
    {
        BasicBlock* bb = CreateEmptyDtorTreeBlock();
        thread_llvmContext->m_builder->SetInsertPoint(bb);
        int allInLevel = static_cast<int>(m_scopeStack[0].second.size());
        for (int i = allInLevel - 1; i >= 0; i--)
        {
            DestructorIREmitter* e = m_scopeStack[0].second[i];
            e->EmitDestructorIR();
        }
        thread_llvmContext->m_builder->CreateBr(m_llvmCurrentEHCatchBlock);
        m_llvmExceptionDtorTree.push_back(std::make_pair(allInLevel, bb));
        n++;
    }

    // Add destructors for the current scope for the newly constructed local vars after the previous generated block
    //
    {
        assert(n > 0 && m_llvmExceptionDtorTree.size() == static_cast<size_t>(n));
        int numInCurLevel = m_llvmExceptionDtorTree[n - 1].first;
        int allInCurLevel = static_cast<int>(m_scopeStack[n - 1].second.size());
        TestAssert(numInCurLevel <= allInCurLevel);
        if (numInCurLevel < allInCurLevel)
        {
            BasicBlock* bb = CreateEmptyDtorTreeBlock();
            thread_llvmContext->m_builder->SetInsertPoint(bb);
            for (int i = allInCurLevel - 1; i >= numInCurLevel; i--)
            {
                DestructorIREmitter* e = m_scopeStack[n - 1].second[i];
                e->EmitDestructorIR();
            }
            thread_llvmContext->m_builder->CreateBr(m_llvmExceptionDtorTree[n - 1].second);
            m_llvmExceptionDtorTree[n - 1] = std::make_pair(allInCurLevel, bb);
        }
    }

    while (static_cast<size_t>(n) < m_scopeStack.size())
    {
        BasicBlock* bb = CreateEmptyDtorTreeBlock();
        thread_llvmContext->m_builder->SetInsertPoint(bb);
        int allInLevel = static_cast<int>(m_scopeStack[n].second.size());
        for (int i = allInLevel - 1; i >= 0; i--)
        {
            DestructorIREmitter* e = m_scopeStack[n].second[i];
            e->EmitDestructorIR();
        }
        thread_llvmContext->m_builder->CreateBr(m_llvmExceptionDtorTree[n - 1].second);
        m_llvmExceptionDtorTree.push_back(std::make_pair(allInLevel, bb));
        n++;
    }
    TestAssert(static_cast<size_t>(n) == m_llvmExceptionDtorTree.size());
    TestAssert(m_llvmExceptionDtorTree.size() == m_scopeStack.size());
#ifdef TESTBUILD
    for (size_t i = 0; i < m_llvmExceptionDtorTree.size() - 1; i++)
    {
        TestAssert(m_llvmExceptionDtorTree[i].first == static_cast<int>(m_scopeStack[i].second.size()));
    }
#endif
    return m_llvmExceptionDtorTree.back().second;
}
#pragma clang diagnostic pop    // for "-Wno-sign-conversion"

}   // namespace PochiVM
