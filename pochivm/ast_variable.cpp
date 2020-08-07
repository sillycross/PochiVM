#include "ast_variable.h"
#include "function_proto.h"
#include "pochivm.hpp"

namespace PochiVM
{

using namespace llvm;

Value* WARN_UNUSED AstVariable::EmitIRImpl()
{
    // If a storage location has been assigned, just return it
    //
    if (m_llvmValue != nullptr)
    {
        return m_llvmValue;
    }
    // Otherwise, assign a storage location for this variable
    // Save insertion point, insert the alloca into end of entry block, and restore insertion point
    //
    auto savedIp = thread_llvmContext->m_builder->saveIP();
    thread_llvmContext->m_builder->SetInsertPoint(thread_llvmContext->GetCurFunction()->GetEntryBlock());
    assert(GetTypeId().IsPointerType());
    Type* llvmType = AstTypeHelper::llvm_type_of(GetTypeId());
    assert(llvmType->isPointerTy());
    m_llvmValue = thread_llvmContext->m_builder->CreateAlloca(
                      llvmType->getPointerElementType(),
                      nullptr /*ArraySize*/,
                      Twine(m_varname).concat("_").concat(Twine(m_varnameSuffix)) /*name*/);
    thread_llvmContext->m_builder->restoreIP(savedIp);
    return m_llvmValue;
}

void AstVariable::EmitDestructVariableIR()
{
    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    TestAssert(GetTypeId().RemovePointer().IsCppClassType());
    const CppFunctionMetadata* cppFunctionMd = GetDestructorMetadata(GetTypeId().RemovePointer());
    TestAssert(cppFunctionMd != nullptr);
    TestAssert(!cppFunctionMd->m_isUsingSret && cppFunctionMd->m_returnType.IsVoid() &&
               cppFunctionMd->m_numParams == 1 && cppFunctionMd->m_paramTypes[0] == GetTypeId());
    Function* callee = thread_llvmContext->m_module->getFunction(cppFunctionMd->m_bitcodeData->m_symbolName);
    TestAssert(callee != nullptr);
    TestAssert(callee->arg_size() == 1);
    Value* params[1];
    params[0] = EmitIR();
    TestAssert(params[0]->getType() == callee->getArg(0)->getType());
    TestAssert(AstTypeHelper::llvm_value_has_type(GetTypeId(), params[0]));
    Value* ret = thread_llvmContext->m_builder->CreateCall(callee, ArrayRef<Value*>(params, params + 1));
    TestAssert(AstTypeHelper::llvm_value_has_type(TypeId::Get<void>(), ret));
    std::ignore = ret;
}

}   // namespace PochiVM
