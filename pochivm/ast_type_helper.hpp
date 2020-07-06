#pragma once

#include "ast_type_helper.h"
#include "codegen_context.hpp"

namespace PochiVM
{

namespace AstTypeHelper
{

using namespace llvm;

// Type* llvm_type_of(TypeId typeId)
// This function returns the llvm type corresponding to typeId
//
inline Type* WARN_UNUSED llvm_type_of(TypeId typeId)
{
    LLVMContext& C = *thread_llvmContext->m_llvmContext;
    if (typeId.IsVoid())
    {
        return Type::getVoidTy(C);
    }
    else if (typeId.IsBool())
    {
        // bool type is i1 in LLVM
        //
        return IntegerType::get(C, 1 /*numBits*/);
    }
    else if (typeId.IsPrimitiveIntType())
    {
        // Integer types
        // LLVM integer types do not differentiate signness. So just create with right width.
        //
        return IntegerType::get(C, static_cast<unsigned>(typeId.Size()) * 8 /*numBits*/);
    }
    else if (typeId.IsPrimitiveFloatType())
    {
        // Floating point types
        // Each float type must be handled individually.
        //
        if (typeId.IsFloat())
        {
            return Type::getFloatTy(C);
        }
        else if (typeId.IsDouble())
        {
            return Type::getDoubleTy(C);
        }
        else
        {
            TestAssert(false);
            __builtin_unreachable();
        }
    }
    else if (typeId.IsCppClassType())
    {
        StructType* stype = thread_llvmContext->m_module->getTypeByName(typeId.GetCppTypeLLVMTypeName());
        TestAssert(stype != nullptr);
        return stype;
    }
    else if (typeId.IsPointerType())
    {
        TypeId pointerElementType = typeId.RemovePointer();
        return llvm_type_of(pointerElementType)->getPointerTo();
    }
    TestAssert(false);
    __builtin_unreachable();
}

// bool llvm_type_has_type(TypeId typeId, Type* type)
// Check if the Type 'type' has CPP type typeId
// According to manual, checking if two types in same LLVMContext are equal is trivial pointer comparison.
//
inline bool WARN_UNUSED llvm_type_has_type(TypeId typeId, Type* type)
{
    assert(type != nullptr && (&type->getContext()) == thread_llvmContext->m_llvmContext);
    return llvm_type_of(typeId) == type;
}

// bool llvm_value_has_type(TypeId typeId, Value* value)
// Check if the Value 'value' has CPP type typeId
//
inline bool WARN_UNUSED llvm_value_has_type(TypeId typeId, Value* value)
{
    assert(value != nullptr);
    return llvm_type_has_type(typeId, value->getType());
}

}   // namespace AstTypeHelper

}   // namespace PochiVM
