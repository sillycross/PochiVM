#include "arith_expr.h"
#include "error_context.h"
#include "pochivm.hpp"

namespace Ast
{

using namespace llvm;

Value* WARN_UNUSED AstArithmeticExpr::EmitIRImpl()
{
    // Important to evaluate left side first: it may have side effects
    //
    Value* lhs = m_lhs->EmitIR();
    Value* rhs = m_rhs->EmitIR();

    Value* inst = nullptr;
    TypeId typeId = GetTypeId();
    if (m_op == '+')
    {
        if (typeId.IsPrimitiveIntType())
        {
            inst = thread_llvmContext->m_builder.CreateAdd(lhs, rhs);
        }
        else if (typeId.IsPrimitiveFloatType())
        {
            inst = thread_llvmContext->m_builder.CreateFAdd(lhs, rhs);
        }
    }
    else if (m_op == '-')
    {
        if (typeId.IsPrimitiveIntType())
        {
            inst = thread_llvmContext->m_builder.CreateSub(lhs, rhs);
        }
        else if (typeId.IsPrimitiveFloatType())
        {
            inst = thread_llvmContext->m_builder.CreateFSub(lhs, rhs);
        }
    }
    else if (m_op == '*')
    {
        if (typeId.IsPrimitiveIntType())
        {
            inst = thread_llvmContext->m_builder.CreateMul(lhs, rhs);
        }
        else if (typeId.IsPrimitiveFloatType())
        {
            inst = thread_llvmContext->m_builder.CreateFMul(lhs, rhs);
        }
    }
    else if (m_op == '/')
    {
        if (typeId.IsPrimitiveIntType())
        {
            if (typeId.IsSigned())
            {
                inst = thread_llvmContext->m_builder.CreateSDiv(lhs, rhs);
            }
            else
            {
                inst = thread_llvmContext->m_builder.CreateUDiv(lhs, rhs);
            }
        }
        else if (typeId.IsPrimitiveFloatType())
        {
            inst = thread_llvmContext->m_builder.CreateFDiv(lhs, rhs);
        }
    }
    else if (m_op == '%')
    {
        if (typeId.IsPrimitiveIntType())
        {
            if (typeId.IsSigned())
            {
                inst = thread_llvmContext->m_builder.CreateSRem(lhs, rhs);
            }
            else
            {
                inst = thread_llvmContext->m_builder.CreateURem(lhs, rhs);
            }
        }
        else
        {
            CHECK_REPORT_BUG(false, "modulo operation on floating point");
        }
    }
    CHECK_REPORT_BUG(inst != nullptr, "unhandled arithmetic codepath or llvm internal error");
    return inst;
}

Value* WARN_UNUSED AstComparisonExpr::EmitIRImpl()
{
    // Important to evaluate left side first: it may have side effects
    //
    Value* lhs = m_lhs->EmitIR();
    Value* rhs = m_rhs->EmitIR();

    Value* inst = nullptr;
    TypeId typeId = m_lhs->GetTypeId();
    if (m_op[0] == '=' && m_op[1] == '=')
    {
        if (typeId.IsPrimitiveIntType())
        {
            inst = thread_llvmContext->m_builder.CreateICmpEQ(lhs, rhs);
        }
        else if (typeId.IsPrimitiveFloatType())
        {
            inst = thread_llvmContext->m_builder.CreateFCmpUEQ(lhs, rhs);
        }
    }
    else if (m_op[0] == '!' && m_op[1] == '=')
    {
        if (typeId.IsPrimitiveIntType())
        {
            inst = thread_llvmContext->m_builder.CreateICmpNE(lhs, rhs);
        }
        else if (typeId.IsPrimitiveFloatType())
        {
            inst = thread_llvmContext->m_builder.CreateFCmpUNE(lhs, rhs);
        }
    }
    else if (m_op[0] == '<' && m_op[1] == '\0')
    {
        if (typeId.IsPrimitiveIntType())
        {
            // bool is just a i1 in LLVM so we can codegen that with no problem,
            // but probably it indicates a bug in upper level code.. same below.
            //
            TestAssert(typeId != TypeId::Get<bool>());
            if (typeId.IsSigned())
            {
                inst = thread_llvmContext->m_builder.CreateICmpSLT(lhs, rhs);
            }
            else
            {
                inst = thread_llvmContext->m_builder.CreateICmpULT(lhs, rhs);
            }
        }
        else if (typeId.IsPrimitiveFloatType())
        {
            inst = thread_llvmContext->m_builder.CreateFCmpULT(lhs, rhs);
        }
    }
    else if (m_op[0] == '>' && m_op[1] == '\0')
    {
        if (typeId.IsPrimitiveIntType())
        {
            TestAssert(typeId != TypeId::Get<bool>());
            if (typeId.IsSigned())
            {
                inst = thread_llvmContext->m_builder.CreateICmpSGT(lhs, rhs);
            }
            else
            {
                inst = thread_llvmContext->m_builder.CreateICmpUGT(lhs, rhs);
            }
        }
        else if (typeId.IsPrimitiveFloatType())
        {
            inst = thread_llvmContext->m_builder.CreateFCmpUGT(lhs, rhs);
        }
    }
    else if (m_op[0] == '<' && m_op[1] == '=')
    {
        if (typeId.IsPrimitiveIntType())
        {
            TestAssert(typeId != TypeId::Get<bool>());
            if (typeId.IsSigned())
            {
                inst = thread_llvmContext->m_builder.CreateICmpSLE(lhs, rhs);
            }
            else
            {
                inst = thread_llvmContext->m_builder.CreateICmpULE(lhs, rhs);
            }
        }
        else if (typeId.IsPrimitiveFloatType())
        {
            inst = thread_llvmContext->m_builder.CreateFCmpULE(lhs, rhs);
        }
    }
    else if (m_op[0] == '>' && m_op[1] == '=')
    {
        if (typeId.IsPrimitiveIntType())
        {
            TestAssert(typeId != TypeId::Get<bool>());
            if (typeId.IsSigned())
            {
                inst = thread_llvmContext->m_builder.CreateICmpSGE(lhs, rhs);
            }
            else
            {
                inst = thread_llvmContext->m_builder.CreateICmpUGE(lhs, rhs);
            }
        }
        else if (typeId.IsPrimitiveFloatType())
        {
            inst = thread_llvmContext->m_builder.CreateFCmpUGE(lhs, rhs);
        }
    }
    CHECK_REPORT_BUG(inst != nullptr, "unhandled comparison codepath or llvm internal error");
    return inst;
}

}   // namespace Ast
