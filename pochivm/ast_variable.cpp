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

}   // namespace PochiVM
