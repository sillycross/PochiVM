#include "error_context.h"
#include "cast_expr.h"
#include "pochivm.hpp"

namespace PochiVM
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

    if (src.IsPrimitiveIntType())
    {
        if (dst.IsPrimitiveIntType())
        {
            if (dst.IsBool())
            {
                // int to bool cast is special: the cast that makes sense is
                // bool = (int != 0), not normal truncate cast
                //
                return thread_llvmContext->m_builder->CreateICmpNE(
                            op /*lhs*/,
                            ConstantInt::get(op->getType() /*type*/,
                                             0 /*value*/,
                                             src.IsSigned() /*isSigned*/) /*rhs*/);
            }
            else
            {
                // Conventional int-to-int widening or narrowing cast
                //
                return thread_llvmContext->m_builder->CreateIntCast(
                            op /*valueToCast*/,
                            AstTypeHelper::llvm_type_of(dst) /*destType*/,
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
                return thread_llvmContext->m_builder->CreateSIToFP(
                            op /*valueToCast*/,
                            AstTypeHelper::llvm_type_of(dst) /*destType*/);
            }
            else
            {
                return thread_llvmContext->m_builder->CreateUIToFP(
                            op /*valueToCast*/,
                            AstTypeHelper::llvm_type_of(dst) /*destType*/);
            }
        }
    }
    else if (src.IsPrimitiveFloatType())
    {
        if (dst.IsPrimitiveFloatType())
        {
            // float to float cast
            //
            return thread_llvmContext->m_builder->CreateFPCast(
                        op /*valueToCast*/,
                        AstTypeHelper::llvm_type_of(dst) /*destType*/);
        }
    }
    else if (src.IsPointerType() && dst.IsPointerType())
    {
        // Generate:
        //     assert(op != NULL);  // when U is not void*
        //     return reinterpret_cast<U>(reinterpret_cast<uintptr_t>(op) + offset);
        //
        // TODO: currently we do not generate the assert
        //
        ssize_t offset = AstTypeHelper::get_static_cast_offset(src, dst);
        if (offset == 0)
        {
            return thread_llvmContext->m_builder->CreateBitOrPointerCast(op, AstTypeHelper::llvm_type_of(dst));
        }
        else
        {
            Value* addrU64 = thread_llvmContext->m_builder->CreateBitOrPointerCast(
                    op, AstTypeHelper::llvm_type_of(TypeId::Get<uint64_t>()));
            assert(AstTypeHelper::llvm_value_has_type(TypeId::Get<uint64_t>(), addrU64));

            Value* offsetU64 = ConstantInt::get(*thread_llvmContext->m_llvmContext,
                                                APInt(64 /*numBits*/,
                                                      static_cast<uint64_t>(offset) /*value*/,
                                                      false /*isSigned*/));
            assert(AstTypeHelper::llvm_value_has_type(TypeId::Get<uint64_t>(), offsetU64));

            Value* dstAddrU64 = thread_llvmContext->m_builder->CreateAdd(addrU64, offsetU64);
            assert(AstTypeHelper::llvm_value_has_type(TypeId::Get<uint64_t>(), dstAddrU64));

            return thread_llvmContext->m_builder->CreateBitOrPointerCast(dstAddrU64, AstTypeHelper::llvm_type_of(dst));
        }
    }
    TestAssert(false);
    __builtin_unreachable();
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
    return thread_llvmContext->m_builder->CreateBitOrPointerCast(op, AstTypeHelper::llvm_type_of(dst));
}

}   // namespace PochiVM
