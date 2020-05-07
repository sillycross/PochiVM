#pragma once

#include "error_context.h"
#include "cast_expr.h"
#include "codegen_context.hpp"
#include "ast_type_helper.hpp"

namespace Ast
{

using namespace llvm;

Value* WARN_UNUSED AstStaticCastExpr::EmitIRImpl()
{
    Value* op = m_operand->EmitIR();
    TypeId src = m_operand->GetTypeId();
    TypeId dst = GetTypeId();

    if (src == dst)
    {
        return op;
    }

    Value* inst = nullptr;

    if (src.IsPrimitiveIntType())
    {
        if (dst.IsPrimitiveIntType())
        {
            if (dst.IsBool())
            {
                // int to bool cast is special: the cast that makes sense is
                // bool = (int != 0), not normal truncate cast
                //
                inst = thread_llvmContext->m_builder.CreateICmpNE(
                            op /*lhs*/,
                            ConstantInt::get(op->getType() /*type*/,
                                             0 /*value*/,
                                             src.IsSigned() /*isSigned*/) /*rhs*/);
            }
            else
            {
                // Conventional int-to-int widening or narrowing cast
                //
                inst = thread_llvmContext->m_builder.CreateIntCast(
                            op /*valueToCast*/,
                            AstTypeHelper::llvm_type_of(dst,
                                                        thread_llvmContext->m_llvmContext) /*destType*/,
                            src.IsSigned() /*isSourceTypeSigned*/);
            }
        }
        else if (dst.IsPrimitiveFloatType())
        {
            // int to float cast
            // Depending on signness we need different instructions
            //
            if (src.IsSigned())
            {
                inst = thread_llvmContext->m_builder.CreateSIToFP(
                            op /*valueToCast*/,
                            AstTypeHelper::llvm_type_of(dst,
                                                        thread_llvmContext->m_llvmContext) /*destType*/);
            }
            else
            {
                inst = thread_llvmContext->m_builder.CreateUIToFP(
                            op /*valueToCast*/,
                            AstTypeHelper::llvm_type_of(dst,
                                                        thread_llvmContext->m_llvmContext) /*destType*/);
            }
        }
    }
    else if (src.IsPrimitiveFloatType())
    {
        if (dst.IsPrimitiveFloatType())
        {
            // float to float cast
            //
            inst = thread_llvmContext->m_builder.CreateFPCast(
                        op /*valueToCast*/,
                        AstTypeHelper::llvm_type_of(dst,
                                                    thread_llvmContext->m_llvmContext) /*destType*/);
        }
    }
    /*
     * TODO: handle pointer
    else if (std::is_pointer<T>::value && std::is_pointer<U>::value)
    {
        // Generate:
        //     assert(op != NULL);  // when U is not void*
        //     return reinterpret_cast<U>(reinterpret_cast<uintptr_t>(op) + offset);
        //
        uint64_t offset = AstTypeHelper::static_cast_offset<T, U>::value;
    }
    */
    CHECK_REPORT_BUG(inst != nullptr, "unhandled static_cast codepath or llvm internal error");
    return inst;
}

Value* WARN_UNUSED AstReinterpretCastExpr::EmitIRImpl()
{
    Value* op = m_operand->EmitIR();
    TypeId src = m_operand->GetTypeId();
    TypeId dst = GetTypeId();

    if (src == dst)
    {
        return op;
    }

    if (src.IsPointerType() && dst.IsPointerType())
    {
        // pointer to pointer cast
        //
    }
    else if (src.IsType<uint64_t>() && dst.IsPointerType())
    {
        // uint64_t to pointer cast
        //
    }
    else if (src.IsPointerType() && dst.IsType<uint64_t>())
    {
        // pointer to uint64_t cast
        //
    }
    CHECK_REPORT_BUG(false, "unhandled reinterpret_cast codepath or llvm internal error");
}

}   // namespace Ast
