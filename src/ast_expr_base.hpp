#pragma once

#include "ast_expr_base.h"
#include "ast_type_helper.hpp"

namespace Ast
{

using namespace llvm;

Value* WARN_UNUSED AstNodeBase::EmitIR()
{
    Value* value = EmitIRImpl();
    TestAssertImp(GetTypeId().IsVoid(), value == nullptr);
    TestAssertImp(!GetTypeId().IsVoid(), AstTypeHelper::llvm_value_has_type(GetTypeId(), value));
    return value;
}

}   // namespace Ast
