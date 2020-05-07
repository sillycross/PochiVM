#pragma once

#include "ast_type_helper.hpp"
#include "codegen_context.hpp"
#include "function_proto.h"

namespace Ast
{

using namespace llvm;

bool WARN_UNUSED AstFunction::EmitDefinition()
{
    CHECK_REPORT_BUG(false, "unimplemented");
}

bool WARN_UNUSED AstFunction::EmitIR()
{
    CHECK_REPORT_BUG(false, "unimplemented");
}

Value* WARN_UNUSED AstCallExpr::EmitIRImpl()
{
    CHECK_REPORT_BUG(false, "unimplemented");
}

Value* WARN_UNUSED AstReturnStmt::EmitIRImpl()
{
    CHECK_REPORT_BUG(false, "unimplemented");
}

}   // namespace Ast
