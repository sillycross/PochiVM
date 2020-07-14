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
        if (stype == nullptr)
        {
            // We have imported all CPP types which definitions are needed.
            // If it is not imported, its definition is never used, so an opaque type is enough.
            //
            stype = StructType::create(C, typeId.GetCppTypeLLVMTypeName());
            TestAssert(stype != nullptr);
        }
        return stype;
    }
    else if (typeId.IsPointerType())
    {
        TypeId pointerElementType = typeId.RemovePointer();
        // Special case:
        // (1) We store a bool value in i8. This is required to maintain compatibility with C++ ABI.
        //     So, type for 'bool' will be i1. But type for 'bool*' will be 'i8*', 'bool**' be 'i8**', etc
        // (2) void* has type i8* instead of void* in LLVM. (void** is i8**, etc).
        //     This is also required to maintain compatibility with C++ ABI.
        //
        if (pointerElementType.IsBool() || pointerElementType.IsVoid())
        {
            return llvm_type_of(TypeId::Get<uint8_t>())->getPointerTo();
        }
        else
        {
            return llvm_type_of(pointerElementType)->getPointerTo();
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

// Create a load instruction, handling the 'bool*' special case.
//
inline Value* WARN_UNUSED create_load_helper(TypeId resultType, Value* src)
{
    TestAssert(!resultType.IsVoid());
    TestAssert(llvm_value_has_type(resultType.AddPointer(), src));
    if (resultType.IsType<bool>())
    {
        // bool itself has type i1, but is stored in i8*.
        // We need to trunc it to i1 after loading it.
        //
        assert(llvm_value_has_type(TypeId::Get<uint8_t*>(), src));
        Value* i8val = thread_llvmContext->m_builder->CreateLoad(src);
        assert(llvm_value_has_type(TypeId::Get<uint8_t>(), i8val));
        Value* i1val = thread_llvmContext->m_builder->CreateTrunc(i8val, AstTypeHelper::llvm_type_of(TypeId::Get<bool>()));
        assert(llvm_value_has_type(TypeId::Get<bool>(), i1val));
        return i1val;
    }
    else
    {
        Value* inst = thread_llvmContext->m_builder->CreateLoad(src);
        return inst;
    }
}

// Create a store instruction, handling the 'bool*' special case.
//
inline void create_store_helper(TypeId srcType, Value* src, Value* dst)
{
    TestAssert(!srcType.IsVoid());
    TestAssert(llvm_value_has_type(srcType, src));
    TestAssert(llvm_value_has_type(srcType.AddPointer(), dst));
    if (srcType.IsBool())
    {
        // bool itself has type i1, but is stored in i8*. We need to zeroext it.
        //
        assert(llvm_value_has_type(TypeId::Get<bool>(), src));
        Value* zext = thread_llvmContext->m_builder->CreateZExt(src, AstTypeHelper::llvm_type_of(TypeId::Get<uint8_t>()));
        assert(llvm_value_has_type(TypeId::Get<uint8_t>(), zext));
        assert(llvm_value_has_type(TypeId::Get<uint8_t*>(), dst));
        thread_llvmContext->m_builder->CreateStore(zext, dst);
    }
    else
    {
        thread_llvmContext->m_builder->CreateStore(src, dst);
    }
}

}   // namespace AstTypeHelper

}   // namespace PochiVM
