#pragma once

#include "ast_expr_base.h"
#include "ast_type_helper.hpp"

namespace PochiVM
{

using namespace llvm;

inline Value* WARN_UNUSED AstNodeBase::EmitIR()
{
    Value* value = EmitIRImpl();
#ifdef TESTBUILD
    if (GetTypeId().IsVoid() || GetTypeId().IsCppClassType())
    {
        TestAssert(value == nullptr);
    }
    else
    {
        TestAssert(AstTypeHelper::llvm_value_has_type(GetTypeId(), value));
    }
#endif
    return value;
}

}   // namespace PochiVM
