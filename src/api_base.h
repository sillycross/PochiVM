#pragma once

#include "ast_expr_base.h"

#include "cast_expr.h"
#include "common_expr.h"
#include "arith_expr.h"
#include "lang_constructs.h"
#include "logical_operator.h"

namespace Ast
{

template<typename T>
class Variable;

// A wrapper class holding a RValue of type known at C++ build time (so static_asserts are possible),
// It holds a pointer to a AstNodeBase class with same type. This class is safe to pass around by value.
// This class is the core of the various APIs that allows user to build up the AST tree.
//
template<typename T>
class Value
{
public:
    // CPP types values are not supported
    //
    static_assert(std::is_same<T, void>::value || AstTypeHelper::is_primitive_type<T>::value ||
                  std::is_pointer<T>::value, "Bad type T. Add to runtime/pochivm_register_runtime.cpp?");

    // Constructor: trivially take a pointer and wraps it.
    //
    Value(AstNodeBase* ptr)
        : m_ptr(ptr)
    {
        TestAssert(m_ptr->GetTypeId().IsType<T>());
    }

    // Implicit type conversion: an integer to a wider integer
    //
    template<typename U, typename = std::enable_if_t<AstTypeHelper::may_implicit_convert<T, U>::value> >
    operator Value<U>() const
    {
        static_assert(AstTypeHelper::may_implicit_convert<T, U>::value, "cannot implicitly convert T to U");
        return StaticCast<U>();
    }

    // Explicit static_cast conversion between primitive types, or up/down cast between pointers
    //
    template<typename U, typename = std::enable_if_t<AstTypeHelper::may_static_cast<T, U>::value> >
    Value<U> StaticCast() const
    {
        static_assert(AstTypeHelper::may_static_cast<T, U>::value, "cannot static_cast T to U");
        return Value<U>(new AstStaticCastExpr(m_ptr, TypeId::Get<U>()));
    }

    // reinterpret_cast conversion between pointers, or between pointer and uint64_t
    //
    template<typename U, typename = std::enable_if_t<AstTypeHelper::may_reinterpret_cast<T, U>::value> >
    Value<U> ReinterpretCast() const
    {
        static_assert(AstTypeHelper::may_reinterpret_cast<T, U>::value, "cannot reinterpret_cast T to U");
        return Value<U>(new AstReinterpretCastExpr(m_ptr, TypeId::Get<U>()));
    }

    // StoreIntoAddress(T* ptr): execute *ptr = v
    //
    template<typename Enable = void, std::enable_if_t<(
             std::is_same<Enable, void>::value && !std::is_same<T, void>::value
    )>* = nullptr >
    Value<void> StoreIntoAddress(Value<T*> dest)
    {
        static_assert(!std::is_same<T, void>::value, "cannot store a void");
        return Value<void>(new AstAssignExpr(dest.m_ptr, m_ptr));
    }

    // Deref(): possible for T* where T is not void*.
    // It is intentional design decision that Deref() returns RValue (class Value), not LValue (class Variable)
    // like in C. The motivation is to prevent users from getting unexpected behaviors. If we were returning a LValue,
    // suppose b is RValue int*, then our 'auto a = b.Deref()' would actually behaves like 'int& a = *b' in C++,
    // despite the grammer looks intuitively more like 'int a = *b'. The user who later used 'a' as RValue
    // might wrongly expected 'a' to be the value held in *b at the time the deref was executed.
    //
    // If the type is T* where T is a CPP class type, this deref itself will be a no-op, since when we later
    // invoke a method of the class, an Addr() must be executed and cancel out this Deref() (and since we don't
    // support composite RValue, access a method/member of the class is the only way a class type could be used).
    // Instead, a Variable<T> class is returned. The class provides the various APIs to access T's members and methods.
    //
    template<typename Enable = void, std::enable_if_t<(
             std::is_same<Enable, void>::value && std::is_pointer<T>::value &&
             !std::is_same<T, void*>::value && !AstTypeHelper::is_cpp_class_type<typename std::remove_pointer<T>::type>::value
    )>* = nullptr >
    Value<typename std::remove_pointer<T>::type> Deref() const
    {
        static_assert(std::is_pointer<T>::value && !std::is_same<T, void*>::value,
                      "must be a non void* pointer to deref");

        using _PointerElementType = typename std::remove_pointer<T>::type;
        static_assert(!AstTypeHelper::is_cpp_class_type<_PointerElementType>::value, "must not be cpp class type");
        return Value<_PointerElementType>(new AstDereferenceExpr(m_ptr));
    }

    template<typename Enable = void, std::enable_if_t<(
             std::is_same<Enable, void>::value && std::is_pointer<T>::value &&
             !std::is_same<T, void*>::value && AstTypeHelper::is_cpp_class_type<typename std::remove_pointer<T>::type>::value
    )>* = nullptr >
    Variable<typename std::remove_pointer<T>::type> Deref() const;

    // Immutable. There is no reason to modify m_ptr after construction, and
    // it is catches errors like a = b (should instead write Assign(a, b))
    //
    AstNodeBase* const m_ptr;
};

// Arithmetic ops convenience operator overloading
//
template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::ADD>::value> >
Value<T> operator+(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<T>(new AstArithmeticExpr('+' /*op*/, lhs.m_ptr, rhs.m_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::SUB>::value> >
Value<T> operator-(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<T>(new AstArithmeticExpr('-' /*op*/, lhs.m_ptr, rhs.m_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::MUL>::value> >
Value<T> operator*(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<T>(new AstArithmeticExpr('*' /*op*/, lhs.m_ptr, rhs.m_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::DIV>::value> >
Value<T> operator/(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<T>(new AstArithmeticExpr('/' /*op*/, lhs.m_ptr, rhs.m_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::MODULO>::value> >
Value<T> operator%(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<T>(new AstArithmeticExpr('%' /*op*/, lhs.m_ptr, rhs.m_ptr));
}

// Comparison ops convenience operator overloading
//
template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::EQUAL>::value> >
Value<bool> operator==(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<bool>(new AstComparisonExpr("==", lhs.m_ptr, rhs.m_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::EQUAL>::value> >
Value<bool> operator!=(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<bool>(new AstComparisonExpr("!=", lhs.m_ptr, rhs.m_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::GREATER>::value> >
Value<bool> operator<(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<bool>(new AstComparisonExpr("<", lhs.m_ptr, rhs.m_ptr));
}

template<typename T, typename = std::enable_if_t<
             AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::GREATER>::value> >
Value<bool> operator>(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<bool>(new AstComparisonExpr(">", lhs.m_ptr, rhs.m_ptr));
}

template<typename T, typename = std::enable_if_t<
             (AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::GREATER>::value
          && AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::EQUAL>::value)> >
Value<bool> operator<=(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<bool>(new AstComparisonExpr("<=", lhs.m_ptr, rhs.m_ptr));
}

template<typename T, typename = std::enable_if_t<
             (AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::GREATER>::value
          && AstTypeHelper::primitive_type_supports_binary_op<T, AstTypeHelper::BinaryOps::EQUAL>::value)> >
Value<bool> operator>=(const Value<T>& lhs, const Value<T>& rhs)
{
    return Value<bool>(new AstComparisonExpr(">=", lhs.m_ptr, rhs.m_ptr));
}

inline Value<bool> operator&&(const Value<bool>& lhs, const Value<bool>& rhs)
{
    return Value<bool>(new AstLogicalAndOrExpr(true /*isAnd*/, lhs.m_ptr, rhs.m_ptr));
}

inline Value<bool> operator||(const Value<bool>& lhs, const Value<bool>& rhs)
{
    return Value<bool>(new AstLogicalAndOrExpr(false /*isAnd*/, lhs.m_ptr, rhs.m_ptr));
}

inline Value<bool> operator!(const Value<bool>& op)
{
    return Value<bool>(new AstLogicalNotExpr(op.m_ptr));
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

// Stores a m_ptr of type T*
// This class inherits Value<T>, allowing a Variable to be used in all constructs that
// expects a Value (and the current value stored in the variable is used)
//
template<typename T>
class Variable : public Value<T>
{
public:
    // CPP types are specialized, should not hit here
    //
    static_assert(std::is_same<T, void>::value || AstTypeHelper::is_primitive_type<T>::value ||
                  std::is_pointer<T>::value, "Bad type T. Add to runtime/pochivm_register_runtime.cpp?");

    Variable(AstVariable* ptr)
        : Value<T>(new AstDereferenceVariableExpr(ptr))
        , m_varPtr(ptr)
    {
        TestAssert(m_varPtr->GetTypeId().IsType<T*>());
    }

    // Explicitly load the value currently stored in the variable.
    //
    Value<T> Load() const
    {
        return *static_cast<const Value<T>*>(this);
    }

    // Address of this variable
    //
    Value<T*> Addr() const
    {
        return Value<T*>(m_varPtr);
    }

    // Immutable. There is no reason to modify m_varPtr after construction, and
    // it is catches errors like a = b (should instead write Assign(a, b))
    //
    AstVariable* const m_varPtr;
};

template<typename T>
template<typename Enable, std::enable_if_t<(
         std::is_same<Enable, void>::value && std::is_pointer<T>::value &&
         !std::is_same<T, void*>::value && AstTypeHelper::is_cpp_class_type<typename std::remove_pointer<T>::type>::value
)>* >
Variable<typename std::remove_pointer<T>::type> Value<T>::Deref() const
{
    static_assert(std::is_pointer<T>::value && !std::is_same<T, void*>::value,
                  "must be a non void* pointer to deref");

    using _PointerElementType = typename std::remove_pointer<T>::type;
    static_assert(AstTypeHelper::is_cpp_class_type<_PointerElementType>::value);
    return Variable<_PointerElementType>(m_ptr);
}

// Language utility: Assign a value to a variable
//
template<typename T>
Value<void> Assign(const Variable<T>& lhs, const Value<T>& rhs)
{
    return Value<void>(new AstAssignExpr(lhs.m_varPtr, rhs.m_ptr));
}

// Language utility: increment an integer
// TODO: support pointers as well maybe?
//
template<typename T>
Value<void> Increment(const Variable<T>& var)
{
    static_assert(AstTypeHelper::is_primitive_int_type<T>::value, "may only increment integer");
    return Assign(var, var + Literal<T>(1));
}

}   // namespace Ast
