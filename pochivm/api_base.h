#pragma once

#include "ast_expr_base.h"

#include "cast_expr.h"
#include "common_expr.h"
#include "arith_expr.h"
#include "lang_constructs.h"
#include "logical_operator.h"

namespace PochiVM
{

template<typename T>
class Reference;

template<typename T>
class ConstPrimitiveReference;

// A wrapper class holding a RValue of type known at C++ build time (so static_asserts on types are possible),
// It holds a pointer to a AstNodeBase class with same type. This class is safe to pass around by value.
// This class is the core of the various APIs that allows user to build up the AST tree.
//
template<typename T>
class Value
{
public:
    // CPP types are specialized, should not hit here
    //
    static_assert(std::is_same<T, void>::value || AstTypeHelper::is_primitive_type<T>::value ||
                  std::is_pointer<T>::value, "Bad type T. Add to runtime/pochivm_register_runtime.cpp?");

    // Constructor: trivially take a pointer and wraps it.
    //
    Value(AstNodeBase* ptr)
        : __pochivm_value_ptr(ptr)
    {
        TestAssert(__pochivm_value_ptr->GetTypeId().IsType<T>());
    }

    // Implicit type conversion: an integer to a wider integer
    //
    template<typename U, typename = std::enable_if_t<AstTypeHelper::may_implicit_convert<T, U>::value> >
    operator Value<U>() const;

    // Implicit type conversion: RValue to const primitive reference
    //
    template<typename Enable = void, typename = std::enable_if_t<std::is_same<Enable, void>::value && !std::is_same<T, void>::value> >
    operator ConstPrimitiveReference<T>() const;

    // operator dereference: possible for T* where T is not void*.
    // Returns a reference to the value at the pointer address.
    // This is internally a no-op, since a reference is internally just a value of type T*,
    // but this allows users to write code in a more C++-like manner.
    //
    template<typename Enable = void, std::enable_if_t<(
             std::is_same<Enable, void>::value && std::is_pointer<T>::value &&
             !std::is_same<T, void*>::value
    )>* = nullptr >
    Reference<typename std::remove_pointer<T>::type> operator*() const;

    // operator->(): possible for T* where T is a C++ type
    //
    template<typename Enable = void, std::enable_if_t<(
             std::is_same<Enable, void>::value && std::is_pointer<T>::value &&
             AstTypeHelper::is_cpp_class_type<typename std::remove_pointer<T>::type>::value
    )>* = nullptr >
    Reference<typename std::remove_pointer<T>::type>* operator->() const;

    // Operator[]: possible for T*
    //
    template<typename I, std::enable_if_t<(
            std::is_pointer<T>::value && !std::is_same<T, void*>::value &&
            AstTypeHelper::is_primitive_int_type<I>::value
    )>* = nullptr >
    Reference<typename std::remove_pointer<T>::type> operator[](const Value<I>& index) const;

    template<typename I, std::enable_if_t<(
            std::is_pointer<T>::value && !std::is_same<T, void*>::value &&
            AstTypeHelper::is_primitive_int_type<I>::value
    )>* = nullptr >
    Reference<typename std::remove_pointer<T>::type> operator[](I index) const;

    // Immutable. There is no reason to modify __pochivm_value_ptr after construction, and
    // it is catches errors like a = b (should instead write Assign(a, b))
    //
    AstNodeBase* const __pochivm_value_ptr;
};

// Explicit static_cast conversion between primitive types, or up/down cast between pointers
//
template<typename U, typename T, typename = std::enable_if_t<AstTypeHelper::may_static_cast<T, U>::value> >
Value<U> StaticCast(const Value<T>& src)
{
    static_assert(AstTypeHelper::may_static_cast<T, U>::value, "cannot static_cast T to U");
    return Value<U>(new AstStaticCastExpr(src.__pochivm_value_ptr, TypeId::Get<U>()));
}

// reinterpret_cast conversion between pointers, or between pointer and uint64_t
//
template<typename U, typename T, typename = std::enable_if_t<AstTypeHelper::may_reinterpret_cast<T, U>::value> >
Value<U> ReinterpretCast(const Value<T>& src)
{
    static_assert(AstTypeHelper::may_reinterpret_cast<T, U>::value, "cannot reinterpret_cast T to U");
    return Value<U>(new AstReinterpretCastExpr(src.__pochivm_value_ptr, TypeId::Get<U>()));
}

// Implicit type conversion: an integer to a wider integer
//
template<typename T>
template<typename U, typename>
Value<T>::operator Value<U>() const
{
    static_assert(AstTypeHelper::may_implicit_convert<T, U>::value, "cannot implicitly convert T to U");
    return StaticCast<U>(*this);
}

// Arithmetic ops convenience operator overloading
//
template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::ADD>::value> >
Value<T> operator+(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<T>(new AstArithmeticExpr(AstArithmeticExprType::ADD, lhs.__pochivm_value_ptr, rhs.__pochivm_value_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::SUB>::value> >
Value<T> operator-(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<T>(new AstArithmeticExpr(AstArithmeticExprType::SUB, lhs.__pochivm_value_ptr, rhs.__pochivm_value_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::MUL>::value> >
Value<T> operator*(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<T>(new AstArithmeticExpr(AstArithmeticExprType::MUL, lhs.__pochivm_value_ptr, rhs.__pochivm_value_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::DIV>::value> >
Value<T> operator/(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<T>(new AstArithmeticExpr(AstArithmeticExprType::DIV, lhs.__pochivm_value_ptr, rhs.__pochivm_value_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::MODULO>::value> >
Value<T> operator%(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<T>(new AstArithmeticExpr(AstArithmeticExprType::MOD, lhs.__pochivm_value_ptr, rhs.__pochivm_value_ptr));
}

// Convenience overloading: arithmetic operation with literal
//
template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::ADD>::value> >
Value<T> operator+(const Value<T>& lhs, T rhs)
{
    return Value<T>(new AstArithmeticExpr(AstArithmeticExprType::ADD, lhs.__pochivm_value_ptr, new AstLiteralExpr(TypeId::Get<T>(), &rhs)));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::SUB>::value> >
Value<T> operator-(const Value<T>& lhs, T rhs)
{
    return Value<T>(new AstArithmeticExpr(AstArithmeticExprType::SUB, lhs.__pochivm_value_ptr, new AstLiteralExpr(TypeId::Get<T>(), &rhs)));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::MUL>::value> >
Value<T> operator*(const Value<T>& lhs, T rhs)
{
    return Value<T>(new AstArithmeticExpr(AstArithmeticExprType::MUL, lhs.__pochivm_value_ptr, new AstLiteralExpr(TypeId::Get<T>(), &rhs)));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::DIV>::value> >
Value<T> operator/(const Value<T>& lhs, T rhs)
{
    return Value<T>(new AstArithmeticExpr(AstArithmeticExprType::DIV, lhs.__pochivm_value_ptr, new AstLiteralExpr(TypeId::Get<T>(), &rhs)));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::MODULO>::value> >
Value<T> operator%(const Value<T>& lhs, T rhs)
{
    return Value<T>(new AstArithmeticExpr(AstArithmeticExprType::MOD, lhs.__pochivm_value_ptr, new AstLiteralExpr(TypeId::Get<T>(), &rhs)));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::ADD>::value> >
Value<T> operator+(T lhs, const Value<T>& rhs)
{
    return Value<T>(new AstArithmeticExpr(AstArithmeticExprType::ADD, new AstLiteralExpr(TypeId::Get<T>(), &lhs), rhs.__pochivm_value_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::SUB>::value> >
Value<T> operator-(T lhs, const Value<T>& rhs)
{
    return Value<T>(new AstArithmeticExpr(AstArithmeticExprType::SUB, new AstLiteralExpr(TypeId::Get<T>(), &lhs), rhs.__pochivm_value_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::MUL>::value> >
Value<T> operator*(T lhs, const Value<T>& rhs)
{
    return Value<T>(new AstArithmeticExpr(AstArithmeticExprType::MUL, new AstLiteralExpr(TypeId::Get<T>(), &lhs), rhs.__pochivm_value_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::DIV>::value> >
Value<T> operator/(T lhs, const Value<T>& rhs)
{
    return Value<T>(new AstArithmeticExpr(AstArithmeticExprType::DIV, new AstLiteralExpr(TypeId::Get<T>(), &lhs), rhs.__pochivm_value_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::MODULO>::value> >
Value<T> operator%(T lhs, const Value<T>& rhs)
{
    return Value<T>(new AstArithmeticExpr(AstArithmeticExprType::MOD, new AstLiteralExpr(TypeId::Get<T>(), &lhs), rhs.__pochivm_value_ptr));
}

// Pointer arithmetic ops convenience operator overloading
//
template<typename B, typename I, typename = std::enable_if_t<
             std::is_pointer<B>::value && AstTypeHelper::is_primitive_int_type<I>::value> >
Value<B> operator+(const Value<B>& base, const Value<I>& index)
{
    static_assert(!std::is_same<B, void*>::value, "Pointer arithmetic on 'void*' is against C++ standard.");
    return Value<B>(new AstPointerArithmeticExpr(base.__pochivm_value_ptr, index.__pochivm_value_ptr, true /*isAddition*/));
}

template<typename B, typename I, typename = std::enable_if_t<
             std::is_pointer<B>::value && AstTypeHelper::is_primitive_int_type<I>::value> >
Value<B> operator-(const Value<B>& base, const Value<I>& index)
{
    static_assert(!std::is_same<B, void*>::value, "Pointer arithmetic on 'void*' is against C++ standard.");
    return Value<B>(new AstPointerArithmeticExpr(base.__pochivm_value_ptr, index.__pochivm_value_ptr, false /*isAddition*/));
}

// Convenience overloading: pointer arithmetic with literal offset
//
template<typename B, typename I, typename = std::enable_if_t<
             std::is_pointer<B>::value && AstTypeHelper::is_primitive_int_type<I>::value> >
Value<B> operator+(const Value<B>& base, I index)
{
    static_assert(!std::is_same<B, void*>::value, "Pointer arithmetic on 'void*' is against C++ standard.");
    return Value<B>(new AstPointerArithmeticExpr(base.__pochivm_value_ptr, new AstLiteralExpr(TypeId::Get<I>(), &index), true /*isAddition*/));
}

template<typename B, typename I, typename = std::enable_if_t<
             std::is_pointer<B>::value && AstTypeHelper::is_primitive_int_type<I>::value> >
Value<B> operator-(const Value<B>& base, I index)
{
    static_assert(!std::is_same<B, void*>::value, "Pointer arithmetic on 'void*' is against C++ standard.");
    return Value<B>(new AstPointerArithmeticExpr(base.__pochivm_value_ptr, new AstLiteralExpr(TypeId::Get<I>(), &index), false /*isAddition*/));
}

// Comparison ops convenience operator overloading
//
template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::EQUAL>::value> >
Value<bool> operator==(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<bool>(new AstComparisonExpr(AstComparisonExprType::EQUAL, lhs.__pochivm_value_ptr, rhs.__pochivm_value_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::EQUAL>::value> >
Value<bool> operator!=(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<bool>(new AstComparisonExpr(AstComparisonExprType::NOT_EQUAL, lhs.__pochivm_value_ptr, rhs.__pochivm_value_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::GREATER>::value> >
Value<bool> operator<(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<bool>(new AstComparisonExpr(AstComparisonExprType::LESS_THAN, lhs.__pochivm_value_ptr, rhs.__pochivm_value_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::GREATER>::value> >
Value<bool> operator>(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<bool>(new AstComparisonExpr(AstComparisonExprType::GREATER_THAN, lhs.__pochivm_value_ptr, rhs.__pochivm_value_ptr));
}

template<typename T, typename = std::enable_if_t<
             (AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::GREATER>::value
          && AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::EQUAL>::value)> >
Value<bool> operator<=(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<bool>(new AstComparisonExpr(AstComparisonExprType::LESS_EQUAL, lhs.__pochivm_value_ptr, rhs.__pochivm_value_ptr));
}

template<typename T, typename = std::enable_if_t<
             (AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::GREATER>::value
          && AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::EQUAL>::value)> >
Value<bool> operator>=(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<bool>(new AstComparisonExpr(AstComparisonExprType::GREATER_EQUAL, lhs.__pochivm_value_ptr, rhs.__pochivm_value_ptr));
}

// Convenience overloading: comparing with a literal
//
template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::EQUAL>::value> >
Value<bool> operator==(const Value<T>& lhs, T rhs)
{
    return Value<bool>(new AstComparisonExpr(AstComparisonExprType::EQUAL, lhs.__pochivm_value_ptr, new AstLiteralExpr(TypeId::Get<T>(), &rhs)));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::EQUAL>::value> >
Value<bool> operator!=(const Value<T>& lhs, T rhs)
{
    return Value<bool>(new AstComparisonExpr(AstComparisonExprType::NOT_EQUAL, lhs.__pochivm_value_ptr, new AstLiteralExpr(TypeId::Get<T>(), &rhs)));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::GREATER>::value> >
Value<bool> operator<(const Value<T>& lhs, T rhs)
{
    return Value<bool>(new AstComparisonExpr(AstComparisonExprType::LESS_THAN, lhs.__pochivm_value_ptr, new AstLiteralExpr(TypeId::Get<T>(), &rhs)));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::GREATER>::value> >
Value<bool> operator>(const Value<T>& lhs, T rhs)
{
    return Value<bool>(new AstComparisonExpr(AstComparisonExprType::GREATER_THAN, lhs.__pochivm_value_ptr, new AstLiteralExpr(TypeId::Get<T>(), &rhs)));
}

template<typename T, typename = std::enable_if_t<
             (AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::GREATER>::value
          && AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::EQUAL>::value)> >
Value<bool> operator<=(const Value<T>& lhs, T rhs)
{
    return Value<bool>(new AstComparisonExpr(AstComparisonExprType::LESS_EQUAL, lhs.__pochivm_value_ptr, new AstLiteralExpr(TypeId::Get<T>(), &rhs)));
}

template<typename T, typename = std::enable_if_t<
             (AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::GREATER>::value
          && AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::EQUAL>::value)> >
Value<bool> operator>=(const Value<T>& lhs, T rhs)
{
    return Value<bool>(new AstComparisonExpr(AstComparisonExprType::GREATER_EQUAL, lhs.__pochivm_value_ptr, new AstLiteralExpr(TypeId::Get<T>(), &rhs)));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::EQUAL>::value> >
Value<bool> operator==(T lhs, const Value<T>& rhs)
{
    return Value<bool>(new AstComparisonExpr(AstComparisonExprType::EQUAL, new AstLiteralExpr(TypeId::Get<T>(), &lhs), rhs.__pochivm_value_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::EQUAL>::value> >
Value<bool> operator!=(T lhs, const Value<T>& rhs)
{
    return Value<bool>(new AstComparisonExpr(AstComparisonExprType::NOT_EQUAL, new AstLiteralExpr(TypeId::Get<T>(), &lhs), rhs.__pochivm_value_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::GREATER>::value> >
Value<bool> operator<(T lhs, const Value<T>& rhs)
{
    return Value<bool>(new AstComparisonExpr(AstComparisonExprType::LESS_THAN, new AstLiteralExpr(TypeId::Get<T>(), &lhs), rhs.__pochivm_value_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::GREATER>::value> >
Value<bool> operator>(T lhs, const Value<T>& rhs)
{
    return Value<bool>(new AstComparisonExpr(AstComparisonExprType::GREATER_THAN, new AstLiteralExpr(TypeId::Get<T>(), &lhs), rhs.__pochivm_value_ptr));
}

template<typename T, typename = std::enable_if_t<
             (AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::GREATER>::value
          && AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::EQUAL>::value)> >
Value<bool> operator<=(T lhs, const Value<T>& rhs)
{
    return Value<bool>(new AstComparisonExpr(AstComparisonExprType::LESS_EQUAL, new AstLiteralExpr(TypeId::Get<T>(), &lhs), rhs.__pochivm_value_ptr));
}

template<typename T, typename = std::enable_if_t<
             (AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::GREATER>::value
          && AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::EQUAL>::value)> >
Value<bool> operator>=(T lhs, const Value<T>& rhs)
{
    return Value<bool>(new AstComparisonExpr(AstComparisonExprType::GREATER_EQUAL, new AstLiteralExpr(TypeId::Get<T>(), &lhs), rhs.__pochivm_value_ptr));
}

inline Value<bool> operator&&(const Value<bool>& lhs, const Value<bool>& rhs)
{
    return Value<bool>(new AstLogicalAndOrExpr(true /*isAnd*/, lhs.__pochivm_value_ptr, rhs.__pochivm_value_ptr));
}

inline Value<bool> operator||(const Value<bool>& lhs, const Value<bool>& rhs)
{
    return Value<bool>(new AstLogicalAndOrExpr(false /*isAnd*/, lhs.__pochivm_value_ptr, rhs.__pochivm_value_ptr));
}

inline Value<bool> operator!(const Value<bool>& op)
{
    return Value<bool>(new AstLogicalNotExpr(op.__pochivm_value_ptr));
}

// Const primitive reference: only used as an intermediate helper for calling C++ functions.
// If a C++ function has parameter 'const int&', C++ allows both rvalue (e.g. '1') and reference (e.g. 'a')
// to bind to that parameter. We need this helper struct to support this behavior.
// If the input is rvalue, the conversion to this struct generates a 'AstRvalueToConstPrimitiveRefExpr' operator.
//
template<typename T>
class ConstPrimitiveReference
{
public:
    static_assert(AstTypeHelper::is_primitive_type<T>::value || std::is_pointer<T>::value,
                  "Only primitive types allowed");

    ConstPrimitiveReference(AstNodeBase* ptr)
        : __pochivm_ref_ptr(ptr)
    {
        TestAssert(__pochivm_ref_ptr->GetTypeId().IsType<T*>());
    }

    AstNodeBase* const __pochivm_ref_ptr;
};

// Implicit type conversion: RValue to const primitive reference
//
template<typename T>
template<typename, typename>
Value<T>::operator ConstPrimitiveReference<T>() const
{
    return ConstPrimitiveReference<T>(new AstRvalueToConstPrimitiveRefExpr(__pochivm_value_ptr));
}

// Language utility: construct a literal
// Example: Literal<int64_t>(1)
//
template<typename T>
Value<T> Literal(T x)
{
    return Value<T>(new AstLiteralExpr(TypeId::Get<T>(), &x));
}

// Language utility: construct a nullptr (allowed for comparison, but disallowed for static_cast)
// Example: Nullptr<int*>()
//
template<typename T, typename = std::enable_if_t<std::is_pointer<T>::value> >
Value<T> Nullptr()
{
    return Value<T>(new AstNullptrExpr(TypeId::Get<T>()));
}

// Language utility: construct a trash pointer (allowed for static_cast, but undefined behavior for comparison)
// Example: Trashptr<int*>()
//
template<typename T, typename = std::enable_if_t<std::is_pointer<T>::value> >
Value<T> Trashptr()
{
    return Value<T>(new AstTrashPtrExpr(TypeId::Get<T>()));
}

// Stores a reference to Type T
// Internally this is just a value of type T*
// This class inherits Value<T>, allowing a Reference to be used in all constructs that
// expects a Value, and in that case, the value stored in the address is used.
//
// Note that this class, like most nodes, cannot be reused in AST tree (unless when this class
// is used as the base class of Variable), so there is no ambiguity on the value stored in the address.
//
template<typename T>
class Reference : public Value<T>
{
public:
    // CPP types are specialized, should not hit here
    //
    static_assert(AstTypeHelper::is_primitive_type<T>::value ||
                  std::is_pointer<T>::value, "Bad type T. Add to runtime/pochivm_register_runtime.cpp?");

    Reference(AstNodeBase* ptr)
        : Value<T>(new AstDereferenceExpr(ptr))
        , __pochivm_ref_ptr(ptr)
    {
        TestAssert(__pochivm_ref_ptr->GetTypeId().IsType<T*>());
    }

    // Implicit type conversion: reference to const primitive reference
    //
    operator ConstPrimitiveReference<T>() const
    {
        return ConstPrimitiveReference<T>(__pochivm_ref_ptr);
    }

protected:
    // constructor used only by Variable
    // AstDereferenceVariableExpr is an exceptional node that may be reused,
    // allowing the variable to be used in multiple places in the AST tree.
    //
    // The unused 'bool' parameter is just to distinguish with the ctor that takes AstNodeBase*
    //
    Reference(AstVariable* ptr, bool /*unused*/)
        : Value<T>(new AstDereferenceVariableExpr(ptr))
        , __pochivm_ref_ptr(ptr)
    {
        TestAssert(__pochivm_ref_ptr->GetTypeId().IsType<T*>());
    }

public:

    // Address of this variable
    //
    Value<T*> Addr() const
    {
        return Value<T*>(__pochivm_ref_ptr);
    }

    // Immutable. There is no reason to modify __pochivm_var_ptr after construction, and
    // it is catches errors like a = b (should instead write Assign(a, b))
    //
    AstNodeBase* const __pochivm_ref_ptr;
};

// A local variable of type T.
// This class inherits Reference<T>, allowing a Variable to be used in all constructs that
// expects a Reference or Value (and in the case of Value the current value stored in the variable is used)
//
template<typename T>
class Variable : public Reference<T>
{
public:
    Variable(AstVariable* __pochivm_var_ptr_)
        : Reference<T>(__pochivm_var_ptr_, true)
        , __pochivm_var_ptr(__pochivm_var_ptr_)
    {
        TestAssert(__pochivm_var_ptr->GetTypeId().IsType<T*>());
    }

    // Immutable. There is no reason to modify __pochivm_var_ptr after construction, and
    // it is catches errors like a = b (should instead write Assign(a, b))
    //
    AstVariable* const __pochivm_var_ptr;
};

template<typename T>
template<typename Enable, std::enable_if_t<(
         std::is_same<Enable, void>::value && std::is_pointer<T>::value &&
         !std::is_same<T, void*>::value
)>*>
Reference<typename std::remove_pointer<T>::type> Value<T>::operator*() const
{
    static_assert(std::is_pointer<T>::value && !std::is_same<T, void*>::value,
                  "must be a non void* pointer to deref");

    using _PointerElementType = typename std::remove_pointer<T>::type;
    return Reference<_PointerElementType>(__pochivm_value_ptr);
}

template<typename T>
template<typename Enable, std::enable_if_t<(
         std::is_same<Enable, void>::value && std::is_pointer<T>::value &&
         AstTypeHelper::is_cpp_class_type<typename std::remove_pointer<T>::type>::value
)>*>
Reference<typename std::remove_pointer<T>::type>* Value<T>::operator->() const
{
    using _PointerElementType = typename std::remove_pointer<T>::type;
    // operator-> must return a pointer
    // This operation is actually a no-op, it is just a type system conversion.
    // We take advantage of the fact that Reference<CPP class> is just storing the
    // same pointer as this class, to get rid of the std::new.
    //
    static_assert(sizeof(Reference<_PointerElementType>) == 8, "unexpected size of Reference");
    return reinterpret_cast<Reference<_PointerElementType>*>(const_cast<AstNodeBase**>(&__pochivm_value_ptr));
}

template<typename T>
template<typename I, std::enable_if_t<(
        std::is_pointer<T>::value && !std::is_same<T, void*>::value &&
        AstTypeHelper::is_primitive_int_type<I>::value
)>*>
Reference<typename std::remove_pointer<T>::type> Value<T>::operator[](const Value<I>& index) const
{
    using _PointerElementType = typename std::remove_pointer<T>::type;
    return Reference<_PointerElementType>(
                new AstPointerArithmeticExpr(
                    __pochivm_value_ptr, index.__pochivm_value_ptr, true /*isAddition*/));
}

template<typename T>
template<typename I, std::enable_if_t<(
        std::is_pointer<T>::value && !std::is_same<T, void*>::value &&
        AstTypeHelper::is_primitive_int_type<I>::value
)>*>
Reference<typename std::remove_pointer<T>::type> Value<T>::operator[](I index) const
{
    using _PointerElementType = typename std::remove_pointer<T>::type;
    if (index == 0)
    {
        return Reference<_PointerElementType>(__pochivm_value_ptr);
    }
    else
    {
        return Reference<_PointerElementType>(
                    new AstPointerArithmeticExpr(
                        __pochivm_value_ptr, new AstLiteralExpr(TypeId::Get<I>(), &index), true /*isAddition*/));
    }
}

// Language utility: Assign a value to a variable
//
template<typename T>
Value<void> Assign(const Reference<T>& lhs, const Value<T>& rhs)
{
    return Value<void>(new AstAssignExpr(lhs.__pochivm_ref_ptr, rhs.__pochivm_value_ptr));
}

// Language utility: increment an integer
// TODO: support pointers as well maybe?
//
template<typename T>
Value<void> Increment(const Reference<T>& var)
{
    static_assert(AstTypeHelper::is_primitive_int_type<T>::value, "may only increment integer");
    return Assign(var, var + Literal<T>(1));
}

}   // namespace PochiVM
