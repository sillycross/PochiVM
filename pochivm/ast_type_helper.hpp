#pragma once

#include "ast_type_helper.h"
#include "codegen_context.hpp"

namespace PochiVM
{

namespace AstTypeHelper
{

using namespace llvm;

// Type* llvm_type_of(TypeId typeId, LLVMContext& C)
// This function returns the llvm type corresponding to CPP type T
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
    else if (typeId.IsPointerType())
    {
        // If after removing all pointers of T, the type is a primitive type (e.g. int****),
        // the LLVM type should have identical type.
        // Otherwise, LLVM type should be 'void' adding the same number of pointers.
        // (e.g. SomeCppClass**** becomes void****)
        //
        TypeId pointerElementType = typeId.RemovePointer();
        if (pointerElementType.IsPointerType() || pointerElementType.IsPrimitiveType())
        {
            // Either 'foo**' or 'primitive*', recurse.
            //
            return llvm_type_of(pointerElementType)->getPointerTo();
        }
        else
        {
            // 'non-primitive*' case: llvm type should be void*
            //
            return Type::getVoidTy(C)->getPointerTo();
        }
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
