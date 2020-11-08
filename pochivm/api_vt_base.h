#pragma once

#include "api_base.h"

namespace PochiVM
{

class ReferenceVT;

// A wrapper class similar to Value, except that it holds a AST node of type unknown at C++ build time.
// VT = Variable-Typed
//
class ValueVT
{
public:
    ValueVT(AstNodeBase* ptr)
        : __pochivm_value_ptr(ptr)
    { }

    template<typename T>
    ValueVT(const Value<T>& v)
        : __pochivm_value_ptr(v.__pochivm_value_ptr)
    { }

    // Returns true if this expression has type 'typeId'
    //
    bool WARN_UNUSED HasType(TypeId typeId)
    {
        return __pochivm_value_ptr->GetTypeId() == typeId;
    }

    TypeId WARN_UNUSED GetType()
    {
        return __pochivm_value_ptr->GetTypeId();
    }

    // Currently, the only operation supported is to convert this class to a Value<T, valueType>,
    // which fires an assert if the typename is not correct. In future, we should support
    // operations acting directly on this class.
    //
    template<typename T>
    operator Value<T>() const
    {
        TestAssert(__pochivm_value_ptr->GetTypeId().IsType<T>());
        return Value<T>(__pochivm_value_ptr);
    }

    ReferenceVT operator*() const;

    AstNodeBase* __pochivm_value_ptr;
};

class ReferenceVT : public ValueVT
{
public:
    ReferenceVT(AstNodeBase* ptr)
        : ValueVT(ptr->GetTypeId().RemovePointer().IsCppClassType() ? nullptr : new AstDereferenceExpr(ptr))
        , __pochivm_ref_ptr(ptr)
    {
        TestAssert(__pochivm_ref_ptr->GetTypeId().IsPointerType());
        TestAssert(!(__pochivm_ref_ptr->GetTypeId() == TypeId::Get<void*>()));
    }

protected:
    ReferenceVT(AstVariable* ptr, bool /*unused*/)
        : ValueVT(ptr->GetTypeId().RemovePointer().IsCppClassType() ? nullptr : new AstDereferenceVariableExpr(ptr))
        , __pochivm_ref_ptr(ptr)
    {
        TestAssert(__pochivm_ref_ptr->GetTypeId().IsPointerType());
        TestAssert(!(__pochivm_ref_ptr->GetTypeId() == TypeId::Get<void*>()));
    }

public:
    // Address of this reference
    //
    ValueVT Addr() const
    {
        return ValueVT(__pochivm_ref_ptr);
    }

    // Immutable. There is no reason to modify __pochivm_var_ptr after construction, and
    // it is catches errors like a = b (should instead write Assign(a, b))
    //
    AstNodeBase* const __pochivm_ref_ptr;
};

inline ReferenceVT ValueVT::operator*() const
{
    return ReferenceVT(__pochivm_value_ptr);
}

class VariableVT : public ReferenceVT
{
public:
    VariableVT(AstVariable* __pochivm_var_ptr_)
        : ReferenceVT(__pochivm_var_ptr_, true)
        , __pochivm_var_ptr(__pochivm_var_ptr_)
    {
        TestAssert(__pochivm_var_ptr->GetTypeId().IsPointerType());
        TestAssert(!(__pochivm_var_ptr->GetTypeId() == TypeId::Get<void*>()));
    }

    template<typename T>
    operator Variable<T>() const
    {
        TestAssert(__pochivm_var_ptr->GetTypeId().IsType<T*>());
        return Variable<T>(__pochivm_var_ptr);
    }

    // Immutable. There is no reason to modify __pochivm_var_ptr after construction, and
    // it is catches errors like a = b (should instead write Assign(a, b))
    //
    AstVariable* const __pochivm_var_ptr;
};

template<typename T>
ValueVT ReinterpretCast(TypeId dstType, const Value<T>& src)
{
    static_assert(std::is_pointer<T>::value || std::is_same<T, uint64_t>::value);
    return ValueVT(new AstReinterpretCastExpr(src.__pochivm_value_ptr, dstType));
}

template<typename T>
Value<T> ReinterpretCast(const ValueVT& src)
{
    static_assert(std::is_pointer<T>::value || std::is_same<T, uint64_t>::value);
    return ValueVT(new AstReinterpretCastExpr(src.__pochivm_value_ptr, TypeId::Get<T>()));
}

inline ValueVT ReinterpretCast(TypeId dstType, const ValueVT& src)
{
    return ValueVT(new AstReinterpretCastExpr(src.__pochivm_value_ptr, dstType));
}

template<typename U>
Value<U> StaticCast(const ValueVT& src)
{
    return Value<U>(new AstStaticCastExpr(src.__pochivm_value_ptr, TypeId::Get<U>()));
}

template<typename T>
ValueVT StaticCast(TypeId dstType, const Value<T>& src)
{
    return ValueVT(new AstStaticCastExpr(src.__pochivm_value_ptr, dstType));
}

inline ValueVT StaticCast(TypeId dstType, const ValueVT& src)
{
    return ValueVT(new AstStaticCastExpr(src.__pochivm_value_ptr, dstType));
}

inline ValueVT Literal(TypeId literalType, void* literalValueAddress)
{
    return ValueVT(new AstLiteralExpr(literalType, literalValueAddress));
}

inline ValueVT CreateArithmeticExpr(const ValueVT& lhs, const ValueVT& rhs, AstArithmeticExprType op)
{
    return ValueVT(new AstArithmeticExpr(op, lhs.__pochivm_value_ptr, rhs.__pochivm_value_ptr));
}

inline Value<bool> CreateComparisonExpr(const ValueVT& lhs, const ValueVT& rhs, AstComparisonExprType op)
{
    return Value<bool>(new AstComparisonExpr(op, lhs.__pochivm_value_ptr, rhs.__pochivm_value_ptr));
}

inline Value<void> Assign(const ReferenceVT& lhs, const ValueVT& rhs)
{
    return Value<void>(new AstAssignExpr(lhs.__pochivm_ref_ptr, rhs.__pochivm_value_ptr));
}

}   // namespace PochiVM
