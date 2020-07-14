#include "common_expr.h"
#include "error_context.h"
#include "ast_type_helper.hpp"
#include "pochivm.hpp"

namespace PochiVM
{

using namespace llvm;

Value* WARN_UNUSED AstDereferenceExpr::EmitIRImpl()
{
    Value* op = m_operand->EmitIR();
    return AstTypeHelper::create_load_helper(GetTypeId(), op);
}

Value* WARN_UNUSED AstLiteralExpr::EmitIRImpl()
{
    Value* inst = nullptr;
    TypeId typeId = GetTypeId();
    if (typeId.IsPrimitiveIntType())
    {
        // Integer type cases: bool needs to be handled specially
        //
        if (typeId.IsBool())
        {
            if (m_as_bool)
            {
                inst = ConstantInt::getTrue(*thread_llvmContext->m_llvmContext);
            }
            else
            {
                inst = ConstantInt::getFalse(*thread_llvmContext->m_llvmContext);
            }
        }
        else
        {
            inst = ConstantInt::get(*thread_llvmContext->m_llvmContext,
                                    APInt(static_cast<unsigned>(typeId.Size()) * 8 /*numBits*/,
                                          StaticCastIntTypeValueToUInt64(),
                                          typeId.IsSigned() /*isSigned*/));
        }
    }
    else if (typeId.IsPrimitiveFloatType())
    {
        // Float type cases
        // APFloat has different constructors for float and double parameter,
        // thus giving us the correct type for float and double
        //
        if (typeId.IsFloat())
        {
            inst = ConstantFP::get(*thread_llvmContext->m_llvmContext, APFloat(GetFloat()));
        }
        else if (typeId.IsDouble())
        {
            inst = ConstantFP::get(*thread_llvmContext->m_llvmContext, APFloat(GetDouble()));
        }
    }
    else if (typeId.IsPointerType())
    {
        uint64_t ptrVal = reinterpret_cast<uint64_t>(m_as_voidstar);
        Value* val = ConstantInt::get(*thread_llvmContext->m_llvmContext,
                                      APInt(64 /*numBits*/, ptrVal /*value*/, false /*isSigned*/));
        inst = thread_llvmContext->m_builder->CreateBitOrPointerCast(val, AstTypeHelper::llvm_type_of(typeId));
    }
    return inst;
}

Value* WARN_UNUSED AstAssignExpr::EmitIRImpl()
{
    Value* src = m_src->EmitIR();
    Value* dst = m_dst->EmitIR();
    AstTypeHelper::create_store_helper(m_src->GetTypeId(), src, dst);
    return nullptr;
}

Value* WARN_UNUSED AstNullptrExpr::EmitIRImpl()
{
    CHECK_REPORT_BUG(false, "unimplemented");
}

Value* WARN_UNUSED AstTrashPtrExpr::EmitIRImpl()
{
    CHECK_REPORT_BUG(false, "unimplemented");
}

}   // namespace PochiVM
