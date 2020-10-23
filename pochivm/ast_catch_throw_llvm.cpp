#include "ast_catch_throw.h"
#include "pochivm.hpp"

namespace PochiVM
{

using namespace llvm;

class CallCxaFreeExceptionHelper : public DestructorIREmitter
{
public:
    CallCxaFreeExceptionHelper(Value* exnObject)
        : m_exnObject(exnObject)
    {
        TestAssert(AstTypeHelper::llvm_value_has_type(TypeId::Get<void*>(), m_exnObject));
    }

    virtual bool HasNontrivialDestructor() override final
    {
        return true;
    }

    virtual void EmitDestructorIR() override
    {
        Function* callee = thread_llvmContext->m_module->getFunction("__cxa_free_exception");
        TestAssert(callee != nullptr);
        TestAssert(callee->arg_size() == 1);
        Value* params[1];
        params[0] = m_exnObject;
        TestAssert(params[0]->getType() == callee->getArg(0)->getType());
        TestAssert(AstTypeHelper::llvm_value_has_type(TypeId::Get<void*>(), params[0]));
        Value* ret = thread_llvmContext->m_builder->CreateCall(callee, ArrayRef<Value*>(params, params + 1));
        TestAssert(AstTypeHelper::llvm_value_has_type(TypeId::Get<void>(), ret));
        std::ignore = ret;
    }

    Value* m_exnObject;
};

Value* WARN_UNUSED AstThrowStmt::EmitIRImpl()
{
    // Logic:
    //     void* exn_object = __cxa_allocate_exception(size);
    //     bool reached_throw = false;
    //     Auto(
    //         if (!reached_throw) { __cxa_free_exception(exn_object); }
    //     );
    //     // By design of PochiVM, the expression may only throw if it is a call to C++ function
    //     //
    //     ... evaluate expression, copy to (or directly constructed into) exn_object...
    //     reached_throw = true;
    //     __cxa_throw(exn_object, reinterpret_cast<i8*>(typeid_symbol), reinterpret_cast<i8*>(dtor))
    //
    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);

    // Emit: void* exn_object = __cxa_allocate_exception(size)
    // This never throws. std::terminate() is called if allocation fails.
    //
    Value* exnObjectVoidStar;
    {
        size_t exnObjectSize = m_exceptionTypeId.Size();
        Function* callee = thread_llvmContext->m_module->getFunction("__cxa_allocate_exception");
        TestAssert(callee != nullptr);
        TestAssert(callee->arg_size() == 1);
        Value* params[1];
        params[0] = ConstantInt::get(AstTypeHelper::llvm_type_of(TypeId::Get<uint64_t>()), exnObjectSize);
        TestAssert(params[0]->getType() == callee->getArg(0)->getType());
        TestAssert(AstTypeHelper::llvm_value_has_type(TypeId::Get<uint64_t>(), params[0]));
        exnObjectVoidStar = thread_llvmContext->m_builder->CreateCall(callee, ArrayRef<Value*>(params, params + 1));
        TestAssert(AstTypeHelper::llvm_value_has_type(TypeId::Get<void*>(), exnObjectVoidStar));
    }

    // Push a new frame of variable scope, to store the clean-up IR that calls __cxa_free_exception,
    // called in case we throw during the construction of the exception object.
    //
    CallCxaFreeExceptionHelper dtor(exnObjectVoidStar);

    {
        AutoScopedVariableManagerScope asvms(nullptr /*scope*/);
        thread_pochiVMContext->m_scopedVariableManager.PushObject(&dtor);

        // Cast the exnObject to the actual exception type
        //
        Value* exnObject = thread_llvmContext->m_builder->CreateBitCast(
                    exnObjectVoidStar, AstTypeHelper::llvm_type_of(m_exceptionTypeId.AddPointer()));

        // Construct exception object
        //
        if (m_isCtor || m_isLValueObject)
        {
            // The m_operand is the callExpr for constructing the exception object,
            // with its first parameter be a AstLiteral placeholder (since we don't know the address into
            // which the exception object should be constructed until we have allocated it now)
            //
            TestAssert(m_operand->GetAstNodeType() == AstNodeType::AstCallExpr);
            AstCallExpr* callExpr = assert_cast<AstCallExpr*>(m_operand);
            TestAssert(callExpr->GetParams().size() > 0 && callExpr->GetParams()[0]->GetAstNodeType() == AstNodeType::AstLiteralExpr);
            AstLiteralExpr* placeholder = assert_cast<AstLiteralExpr*>(callExpr->GetParams()[0]);
            placeholder->HijackPointerValueLLVM(exnObject);
            Value* ret = m_operand->EmitIR();
            TestAssert(ret == nullptr);
            std::ignore = ret;
        }
        else
        {
            if (m_exceptionTypeId.IsCppClassType())
            {
                // The only way to get a RValue CPP type is from the return value of a callExpr
                //
                TestAssert(m_operand->GetAstNodeType() == AstNodeType::AstCallExpr);
                AstCallExpr* callExpr = assert_cast<AstCallExpr*>(m_operand);
                TestAssert(callExpr->IsCppFunction() && callExpr->GetCppFunctionMetadata()->m_isUsingSret);
                callExpr->SetSretAddress(exnObject);
                Value* ret = m_operand->EmitIR();
                TestAssert(ret == nullptr);
                std::ignore = ret;
            }
            else
            {
                TestAssert(m_exceptionTypeId.IsPrimitiveType() || m_exceptionTypeId.IsPointerType());
                Value* ret = m_operand->EmitIR();
                AstTypeHelper::create_store_helper(m_exceptionTypeId /*srcType*/, ret /*src*/, exnObject /*dst*/);
            }
        }

        // No other temporaries should have been created in this variable scope, since it's an expression
        //
        TestAssert(thread_pochiVMContext->m_scopedVariableManager.GetNumObjectsInCurrentScope() == 1);

        // The variable scope we just pushed goes out of scope here.
        // This is important, since the exception object has been constructed successfully,
        // we don't need to call __cxa_free_exception in the destructor sequence.
        //
    }

    // Emit: __cxa_throw(exn_object, reinterpret_cast<i8*>(typeid_symbol), reinterpret_cast<i8*>(dtor))
    //
    {
        Function* callee = thread_llvmContext->m_module->getFunction("__cxa_throw");
        TestAssert(callee != nullptr);
        TestAssert(callee->arg_size() == 3);
        Value* params[3];
        // param0: the exn object
        //
        params[0] = exnObjectVoidStar;
        // param1: the std::type_info object address, cast to i8*
        {
            const char* stdTypeInfoObjectSymbolName = GetStdTypeInfoObjectSymbolName(m_exceptionTypeId);
            GlobalVariable* stdTypeInfoObject = thread_llvmContext->m_module->getGlobalVariable(stdTypeInfoObjectSymbolName);
            TestAssert(stdTypeInfoObject != nullptr);
            params[1] = thread_llvmContext->m_builder->CreateBitCast(stdTypeInfoObject, AstTypeHelper::llvm_type_of(TypeId::Get<void*>()));
        }
        // param2: destructor, cast to i8*. null if none
        //
        if (!m_exceptionTypeId.IsCppClassType())
        {
            PointerType* i8ptr = Type::getInt8Ty(*thread_llvmContext->m_llvmContext)->getPointerTo();
            params[2] = ConstantPointerNull::get(i8ptr);
        }
        else
        {
            const CppFunctionMetadata* md = GetDestructorMetadata(m_exceptionTypeId);
            TestAssert(md != nullptr);
            Function* dtorFn = thread_llvmContext->m_module->getFunction(md->m_bitcodeData->m_symbolName);
            TestAssert(dtorFn != nullptr);
            params[2] = thread_llvmContext->m_builder->CreateBitCast(dtorFn, AstTypeHelper::llvm_type_of(TypeId::Get<void*>()));
        }
        TestAssert(params[0]->getType() == callee->getArg(0)->getType());
        TestAssert(params[1]->getType() == callee->getArg(1)->getType());
        TestAssert(params[2]->getType() == callee->getArg(2)->getType());
        TestAssert(AstTypeHelper::llvm_value_has_type(TypeId::Get<void*>(), params[0]));
        TestAssert(AstTypeHelper::llvm_value_has_type(TypeId::Get<void*>(), params[1]));
        TestAssert(AstTypeHelper::llvm_value_has_type(TypeId::Get<void*>(), params[2]));
        Value* ret;
        if (IsNoLandingPadNeeded())
        {
            ret = thread_llvmContext->m_builder
                    ->CreateCall(callee, ArrayRef<Value*>(params, params + callee->arg_size()));
        }
        else
        {
            BasicBlock* unwindDest = EmitEHLandingPadForCurrentPosition();
            BasicBlock* normalDest = BasicBlock::Create(
                        *thread_llvmContext->m_llvmContext,
                        Twine("") /*name*/,
                        thread_llvmContext->GetCurFunction()->GetGeneratedPrototype());
            ret = thread_llvmContext->m_builder
                    ->CreateInvoke(callee, normalDest, unwindDest, ArrayRef<Value*>(params, params + callee->arg_size()));
            thread_llvmContext->m_builder->SetInsertPoint(normalDest);
        }
        TestAssert(AstTypeHelper::llvm_value_has_type(TypeId::Get<void>(), ret));
        std::ignore = ret;
        thread_llvmContext->m_builder->CreateUnreachable();
        thread_llvmContext->SetInsertPointToDummyBlock();
    }
    return nullptr;
}

}   // namespace PochiVM
