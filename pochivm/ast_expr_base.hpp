#pragma once

#include "ast_expr_base.h"
#include "ast_type_helper.hpp"

namespace PochiVM
{

using namespace llvm;

inline Value* WARN_UNUSED AstNodeBase::EmitIR()
{
    Value* value = EmitIRImpl();
    TestAssertImp(GetTypeId().IsVoid(), value == nullptr);
    TestAssertImp(!GetTypeId().IsVoid(), AstTypeHelper::llvm_value_has_type(GetTypeId(), value));
    return value;
}

}   // namespace PochiVM
