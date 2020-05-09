#pragma once

#include "ast_type_helper.hpp"
#include "codegen_context.hpp"
#include "function_proto.h"

namespace Ast
{

using namespace llvm;

void AstFunction::EmitDefinition()
{
    TestAssert(m_generatedPrototype == nullptr);
    std::vector<Type*> args;
    for (AstVariable* arg : m_params)
    {
        args.push_back(AstTypeHelper::llvm_type_of(
                           arg->GetTypeId().RemovePointer(),
                           thread_llvmContext->m_llvmContext));
    }
    Type* returnType = AstTypeHelper::llvm_type_of(
                           m_returnType,
                           thread_llvmContext->m_llvmContext);
    FunctionType* funcType = FunctionType::get(returnType, args, false /*isVarArg*/);
    m_generatedPrototype = Function::Create(
                funcType, Function::ExternalLinkage, m_name, thread_llvmContext->m_module.get());

    // Set parameter names
    //
    {
        size_t index = 0;
        for (auto &arg : m_generatedPrototype->args())
        {
            arg.setName(m_params[index]->GetVarName());
            index++;
        }
    }
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
