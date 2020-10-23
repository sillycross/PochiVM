#include "exception_helper.h"
#include "pochivm.hpp"

namespace PochiVM
{

using namespace llvm;

inline Type* GetPersonalityFnReturnType()
{
    Type* types[2];
    types[0] = AstTypeHelper::llvm_type_of(TypeId::Get<void*>());
    types[1] = AstTypeHelper::llvm_type_of(TypeId::Get<int32_t>());
    StructType* stype = StructType::get(*thread_llvmContext->m_llvmContext, ArrayRef<Type*>(types, types + 2));
    return stype;
}

}   // namespace PochiVM
