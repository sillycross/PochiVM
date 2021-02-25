#include "llvm_ast_helper.hpp"
#include "ast_mem2reg.h"

namespace PochiVM
{

using namespace llvm;

Value* WARN_UNUSED AstRegisterCachedVariableExpr::EmitIRImpl()
{
    Value* op = m_variable->EmitIR();
    return AstTypeHelper::create_load_helper(GetTypeId(), op);
}

}   // namespace PochiVM
