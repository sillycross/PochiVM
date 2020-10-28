#include "common_expr.h"
#include "error_context.h"
#include "ast_type_helper.hpp"
#include "llvm_ast_helper.hpp"
#include "function_proto.h"

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

void AstExceptionAddressPlaceholder::SetLLVMValue(llvm::Value *value)
{
    TestAssert(!m_llvmValueSet);
    TestAssert(value != nullptr && AstTypeHelper::llvm_value_has_type(GetTypeId(), value));
    m_llvmValueSet = true;
    m_llvmValue = value;
}

llvm::Value* WARN_UNUSED AstExceptionAddressPlaceholder::EmitIRImpl()
{
    TestAssert(m_llvmValueSet);
    return m_llvmValue;
}

Value* WARN_UNUSED AstAssignExpr::EmitIRImpl()
{
    Value* src = m_src->EmitIR();
    Value* dst = m_dst->EmitIR();
    AstTypeHelper::create_store_helper(m_src->GetTypeId(), src, dst);
    return nullptr;
}

Value* WARN_UNUSED AstPointerArithmeticExpr::EmitIRImpl()
{
    Value* base = m_base->EmitIR();
    Value* index = m_index->EmitIR();
    // Cast index to 64 bits
    //
    index = thread_llvmContext->m_builder->CreateIntCast(
                                index /*valueToCast*/,
                                AstTypeHelper::llvm_type_of(TypeId::Get<uint64_t>()) /*destType*/,
                                m_index->GetTypeId().IsSigned() /*isSourceTypeSigned*/);

    if (!m_isAddition)
    {
        index = thread_llvmContext->m_builder->CreateNeg(index);
    }
    // Sometimes we don't have the definition of the C++ class,
    // since the LLVM type is only linked in when it is actually used.
    // In that case, we will not use GEP, and use direct arithmetic.
    //
    bool isOpaque = false;
    if (m_base->GetTypeId().RemovePointer().IsCppClassType())
    {
        Type* llvmType = AstTypeHelper::llvm_type_of(m_base->GetTypeId().RemovePointer());
        StructType* structType = dyn_cast<StructType>(llvmType);
        TestAssert(structType != nullptr);
        isOpaque = structType->isOpaque();
    }

    if (!isOpaque)
    {
        // TODO: revisit if we can add 'inbounds' option. The document is confusing to me.
        //
        Value* result = thread_llvmContext->m_builder->CreateGEP(base, index);
        return result;
    }
    else
    {
        // Fallback to direct arithmetic. This is bad for the optimizer, however,
        // since we never use the contents in the struct, the optimizer does not need to do anything anyway.
        //
        base = thread_llvmContext->m_builder->CreateBitOrPointerCast(base, AstTypeHelper::llvm_type_of(TypeId::Get<uint64_t>()));
        size_t sz = GetTypeId().RemovePointer().Size();
        Value* literal = ConstantInt::get(*thread_llvmContext->m_llvmContext, APInt(64 /*numBits*/, sz, false /*isSigned*/));
        index = thread_llvmContext->m_builder->CreateMul(index, literal);
        base = thread_llvmContext->m_builder->CreateAdd(base, index);
        base = thread_llvmContext->m_builder->CreateBitOrPointerCast(base, AstTypeHelper::llvm_type_of(GetTypeId()));
        return base;
    }
}

Value* WARN_UNUSED AstNullptrExpr::EmitIRImpl()
{
    CHECK_REPORT_BUG(false, "unimplemented");
}

Value* WARN_UNUSED AstTrashPtrExpr::EmitIRImpl()
{
    CHECK_REPORT_BUG(false, "unimplemented");
}

Value* WARN_UNUSED AstRvalueToConstPrimitiveRefExpr::EmitIRImpl()
{
    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    auto savedIp = thread_llvmContext->m_builder->saveIP();
    thread_llvmContext->m_builder->SetInsertPoint(thread_llvmContext->GetCurFunction()->GetEntryBlock());
    assert(GetTypeId().IsPointerType());
    Type* llvmType = AstTypeHelper::llvm_type_of(GetTypeId().RemovePointer());
    Value* addr = thread_llvmContext->m_builder->CreateAlloca(
                      llvmType,
                      nullptr /*ArraySize*/);
    thread_llvmContext->m_builder->restoreIP(savedIp);
    Value* data = m_operand->EmitIR();
    AstTypeHelper::create_store_helper(GetTypeId().RemovePointer(), data, addr);
    return addr;
}

}   // namespace PochiVM
