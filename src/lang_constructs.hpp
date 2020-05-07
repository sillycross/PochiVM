#pragma once

#include "error_context.h"
#include "lang_constructs.h"
#include "codegen_context.hpp"
#include "ast_type_helper.hpp"

namespace Ast
{

using namespace llvm;

Value* WARN_UNUSED AstVariable::EmitIRImpl()
{
    TestAssert(m_llvmValue != nullptr);
    return m_llvmValue;
}

Value* WARN_UNUSED AstDeclareVariable::EmitIRImpl()
{
    TestAssert(false && "unimplemented");
}

Value* WARN_UNUSED AstBlock::EmitIRImpl()
{
    TestAssert(false && "unimplemented");
}

Value* WARN_UNUSED AstScope::EmitIRImpl()
{
    TestAssert(false && "unimplemented");
}

Value* WARN_UNUSED AstIfStatement::EmitIRImpl()
{
    TestAssert(false && "unimplemented");
}

Value* WARN_UNUSED AstWhileLoop::EmitIRImpl()
{
    TestAssert(false && "unimplemented");
}

Value* WARN_UNUSED AstForLoop::EmitIRImpl()
{
    TestAssert(false && "unimplemented");
}

Value* WARN_UNUSED AstBreakOrContinueStmt::EmitIRImpl()
{
    TestAssert(false && "unimplemented");
}

}   // namespace Ast
