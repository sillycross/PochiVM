#pragma once

#include "common.h"
#include "runtime/pochivm_runtime_headers.h"

#include "generated/pochivm_runtime_cpp_types.generated.h"

#include "for_each_primitive_type.h"
#include "constexpr_array_concat_helper.h"

namespace PochiVM
{

namespace AstTypeHelper
{

// Give each non-pointer type a unique label
//
enum AstTypeLabelEnum
{
    // The order of this enum is fixed (void, primitive types, cpp types).
    // Various places are hardcoded with this assumption of order.
    //
    AstTypeLabelEnum_void

#define F(type) , AstTypeLabelEnum_ ## type
FOR_EACH_PRIMITIVE_TYPE
#undef F

CPP_CLASS_ENUM_TYPE_LIST

    // must be last element
    //
,   TOTAL_VALUES_IN_TYPE_LABEL_ENUM
};

// human-friendly names of the types, used in pretty-print
//
const char* const AstPrimitiveTypePrintName[1 + x_num_primitive_types] = {
    "void"
#define F(type) , #type
FOR_EACH_PRIMITIVE_TYPE
#undef F
};

const size_t AstPrimitiveTypeSizeInBytes[1 + x_num_primitive_types] = {
      0 /*dummy value for void*/
#define F(type) , sizeof(type)
FOR_EACH_PRIMITIVE_TYPE
#undef F
};

const bool AstPrimitiveTypesIsSigned[1 + x_num_primitive_types] = {
    false /*dummy value for void*/
#define F(type) , std::is_signed<type>::value
FOR_EACH_PRIMITIVE_TYPE
#undef F
};

template<typename T>
struct GetTypeId;

}   // namespace AstTypeHelper

// TODO: REMOVE
// Like in C, RValue is an intermediate value that may or may not have a memory address,
// while LValue is a value known to be residing in memory.
//
// A LValue of type T is represented by a TypeId (and also llvm::Value) of type T*, which is its address.
// A RValue of type T is represented by a TypeId (and also llvm::Value) of type T.
//
// A LValue can always convert to a RValue by AstDereferenceExpr, which generates a 'load' LLVM IR instruction.
// A <T, LValue> can always no-op convert to <T*, RValue>, since they are underlyingly the same type.
//
enum AstValueType
{
    LValue,
    RValue
};

// Unique TypeId for each type possible in codegen
//
struct TypeId
{
    // This is the unique value identifying the type.
    // It should be sufficient to treat it as opaque and only use the API functions,
    // but for informational purpose, the representation is n * x_pointer_typeid_inc +
    // typeLabel + (is generated composite type ? x_generated_composite_type : 0)
    // e.g. int32_t** has TypeId 2 * x_pointer_typeid_inc + int32_t's label in AstTypeLabelEnum
    //
    uint64_t value;

    constexpr explicit TypeId() : value(x_invalid_typeid) {}
    constexpr explicit TypeId(uint64_t _value): value(_value) {}

    bool operator==(const TypeId& other) const { return other.value == value; }
    bool operator!=(const TypeId& other) const { return !(*this == other); }

    constexpr bool IsInvalid() const
    {
        return (value == x_invalid_typeid) ? true : (
                    (value >= x_generated_composite_type) ? false : (
                        !(value % x_pointer_typeid_inc < AstTypeHelper::TOTAL_VALUES_IN_TYPE_LABEL_ENUM)));
    }
    bool IsVoid() const { return IsType<void>(); }
    bool IsPrimitiveType() const { return 1 <= value && value <= x_num_primitive_types; }
    bool IsBool() const { return IsType<bool>(); }
    // Including bool type
    //
    bool IsPrimitiveIntType() const { return 1 <= value && value <= x_num_primitive_int_types; }
    bool IsFloat() const { return IsType<float>(); }
    bool IsDouble() const { return IsType<double>(); }
    bool IsPrimitiveFloatType() const {
        return x_num_primitive_int_types < value && value <= x_num_primitive_int_types + x_num_primitive_float_types;
    }
    bool IsSigned() const {
        assert(IsPrimitiveType());
        return AstTypeHelper::AstPrimitiveTypesIsSigned[value];
    }
    bool IsCppClassType() const {
        return x_num_primitive_types < value && value < AstTypeHelper::TOTAL_VALUES_IN_TYPE_LABEL_ENUM;
    }
    bool IsGeneratedCompositeType() const {
        return x_generated_composite_type <= value && value < x_generated_composite_type + x_pointer_typeid_inc;
    }
    bool IsPointerType() const {
        return !IsInvalid() && (value % x_generated_composite_type >= x_pointer_typeid_inc);
    }
    // e.g. int**** has 4 layers of pointers
    //
    size_t NumLayersOfPointers() const {
        assert(!IsInvalid());
        return (value % x_generated_composite_type) / x_pointer_typeid_inc;
    }
    TypeId WARN_UNUSED AddPointer() const {
        assert(!IsInvalid());
        return TypeId { value + x_pointer_typeid_inc };
    }
    TypeId WARN_UNUSED RemovePointer() const {
        assert(IsPointerType());
        return TypeId { value - x_pointer_typeid_inc };
    }
    // The type after removing all layers of pointers
    //
    TypeId GetRawType() const {
        assert(!IsInvalid());
        return TypeId { value - x_pointer_typeid_inc * NumLayersOfPointers() };
    }

    AstTypeHelper::AstTypeLabelEnum ToTypeLabelEnum() const
    {
        assert(!IsInvalid() && !IsPointerType() && !IsGeneratedCompositeType());
        assert(value < AstTypeHelper::TOTAL_VALUES_IN_TYPE_LABEL_ENUM);
        return static_cast<AstTypeHelper::AstTypeLabelEnum>(value);
    }

    template<typename T>
    bool IsType() const
    {
        return (*this == Get<T>());
    }

    template<typename T, AstValueType valueType>
    bool IsType() const
    {
        return (*this == Get<T, valueType>());
    }

    // Check if static_cast to the other type makes sense
    //
    bool MayStaticCastTo(TypeId other) const;

    // Check if reinterpret_cast to the other type makes sense
    //
    bool MayReinterpretCastTo(TypeId other) const;

    // Return the size of this type in bytes.
    // Does not work for CPP class type or generated types because it's not necessary.
    //
    // This agrees with the type size in llvm, except that
    // bool has a size of 1 byte in C++, but 1 bit (i1) in llvm
    //
    size_t Size() const
    {
        assert(!IsInvalid());
        if (IsVoid())
        {
            // why do you want to get the size of void?
            //
            TestAssert(false);
            __builtin_unreachable();
        }
        else if (IsPrimitiveType())
        {
            return AstTypeHelper::AstPrimitiveTypeSizeInBytes[value];
        }
        else if (IsCppClassType() || IsGeneratedCompositeType())
        {
            // Not supported for now
            //
            TestAssert(false);
            __builtin_unreachable();
        }
        else if (IsPointerType())
        {
            return sizeof(void*);
        }
        TestAssert(false);
        __builtin_unreachable();
    }

    // Print the human-friendly type name in text
    //
    std::string Print() const
    {
        if (IsInvalid())
        {
            return std::string("(invalid type)");
        }
        if (IsPointerType())
        {
            return GetRawType().Print() + std::string(NumLayersOfPointers(), '*');
        }
        else if (IsGeneratedCompositeType())
        {
            return std::string("GeneratedStruct") + std::to_string(value - x_generated_composite_type);
        }
        else
        {
            assert(value < AstTypeHelper::TOTAL_VALUES_IN_TYPE_LABEL_ENUM);
            if (value <= x_num_primitive_types)
            {
                return std::string(AstTypeHelper::AstPrimitiveTypePrintName[value]);
            }
            else
            {
                return std::string(AstTypeHelper::AstCppTypePrintName[value - x_num_primitive_types - 1]);
            }
        }
    }

    // TypeId::Get<T>() return TypeId for T
    //
    template<typename T>
    static constexpr TypeId Get()
    {
        return AstTypeHelper::GetTypeId<T>::value;
    }

    // TypeId::Get<T, valueType>() return TypeId for <T, valueType>
    //
    template<typename T, AstValueType valueType>
    static TypeId Get()
    {
        return (valueType == LValue) ? Get<T>().AddPointer() : Get<T>();
    }

    const static uint64_t x_generated_composite_type = 1000000000ULL * 1000000000ULL;
    const static uint64_t x_pointer_typeid_inc = 1000000000;
    const static uint64_t x_invalid_typeid = static_cast<uint64_t>(-1);
};

namespace AstTypeHelper
{

// GetTypeId<T>::value gives the unique TypeId representing type T
//
template<typename T>
struct GetTypeId
{
    constexpr static TypeId value = TypeId();
    static_assert(sizeof(T) == 0, "Bad Type T");
};

#define F(type) \
template<> struct GetTypeId<type> {	\
    constexpr static TypeId value = TypeId { AstTypeLabelEnum_ ## type }; \
};

F(void)
FOR_EACH_PRIMITIVE_TYPE
#undef F

#define F(...) \
template<> struct GetTypeId<__VA_ARGS__> {	\
    static_assert(cpp_type_ordinal<__VA_ARGS__>::ordinal != x_num_cpp_class_types, "unknown cpp type"); \
    constexpr static TypeId value = TypeId { x_num_primitive_types + 1 + cpp_type_ordinal<__VA_ARGS__>::ordinal }; \
};
FOR_EACH_CPP_CLASS_TYPE
#undef F

template<typename T> struct GetTypeId<T*> {
    constexpr static TypeId value = TypeId { GetTypeId<T>::value.value + TypeId::x_pointer_typeid_inc };
};

template<typename T>
struct is_any_possible_type : std::integral_constant<bool,
        !GetTypeId<T>::value.IsInvalid()
> {};

// is_cpp_class_type<T>::value
//
template<typename T> struct is_cpp_class_type : std::integral_constant<bool,
    (cpp_type_ordinal<T>::ordinal != x_num_cpp_class_types)
> {};

// is_primitive_int_type<T>::value
// true for primitive int types, false otherwise
//
template<typename T>
struct is_primitive_int_type : std::false_type {};
#define F(type) \
template<> struct is_primitive_int_type<type> : std::true_type {};
FOR_EACH_PRIMITIVE_INT_TYPE
#undef F

// is_primitive_float_type<T>::value
// true for primitive float types, false otherwise
//
template<typename T>
struct is_primitive_float_type : std::false_type {};
#define F(type) \
template<> struct is_primitive_float_type<type> : std::true_type {};
FOR_EACH_PRIMITIVE_FLOAT_TYPE
#undef F

// is_primitive_type<T>::value
// true for primitive types, false otherwise
//
template<typename T>
struct is_primitive_type : std::false_type {};
#define F(type) \
template<> struct is_primitive_type<type> : std::true_type {};
FOR_EACH_PRIMITIVE_TYPE
#undef F

// may_explicit_convert<T, U>::value (T, U must be primitive types)
// true if T may be explicitly converted to U using a StaticCast(), false otherwise
//
// Explicit convert is allowed between int-types, between float-types,
// and from int-type to float-type
//
template<typename T, typename U>
struct may_explicit_convert : std::integral_constant<bool,
        (is_primitive_int_type<T>::value && is_primitive_int_type<U>::value) ||
        (is_primitive_int_type<T>::value && is_primitive_float_type<U>::value) ||
        (is_primitive_float_type<T>::value && is_primitive_float_type<U>::value)
> {};

// may_implicit_convert<T, U>::value (T, U must be primitive types)
// true if T may be implicitly converted to U, false otherwise
// The only allowed implicit conversions currently is integer widening conversion
//
template<typename T, typename U>
struct may_implicit_convert : std::false_type {};

#define F(type1, type2) \
template<> struct may_implicit_convert<type1, type2> : std::true_type {                     \
    static_assert(may_explicit_convert<type1, type2>::value, "Bad implicit conversion");    \
};
FOR_EACH_PRIMITIVE_INT_TYPE_WIDENING_CONVERSION
#undef F

// may_static_cast<T, U>::value (T, U must be primitive or pointer types)
// Allows explicit-castable types and up/down casts between pointers
//
// reinterpret_cast and static_cast are different!
// Example: class A : public B, public C {};
// Now static_cast A* to C* results in a shift in pointer value,
// while reinterpret_cast A* to C* is likely a bad idea.
//
// For above reason, we disallow static_cast acting on NULL pointer,
// as there is hardly any legit use to silently changing NULL to a non-zero invalid pointer.
// User should always check for NULL before performing the cast.
// We assert that the operand is not NULL in generated code.
//
// TODO: add runtime check for downcast?
//
template<typename T, typename U>
struct may_static_cast : std::integral_constant<bool,
        may_explicit_convert<T, U>::value
     || (std::is_pointer<T>::value && std::is_pointer<U>::value && std::is_convertible<T, U>::value)
> {};

// may_reinterpret_cast<T, U>::value
// Allows reinterpret_cast between any pointers, and between pointer and uint64_t
//
template<typename T, typename U>
struct may_reinterpret_cast : std::integral_constant<bool,
        (std::is_pointer<T>::value && std::is_pointer<U>::value)
     || (std::is_pointer<T>::value && std::is_same<U, uint64_t>::value)
     || (std::is_same<T, uint64_t>::value && std::is_pointer<U>::value)
> {};

// A list of binary operations supported by operator overloading
// Logical operators (and/or/not) are not listed here.
//
enum class BinaryOps
{
    ADD,
    SUB,
    MUL,
    DIV,
    MODULO,
    EQUAL,
    GREATER
};

// All types except bool supports ADD, SUB, MUL
//
template<typename T>
struct supports_addsubmul_internal : std::integral_constant<bool,
        (is_primitive_type<T>::value && !std::is_same<T, bool>::value)
> {};

// Only float types support DIV
// It is intentional that int types do not support DIV: one should always check
// for divisor != 0 before doing int DIV, so having a convenience operator is error prone
//
template<typename T>
struct supports_div_internal : std::integral_constant<bool,
        (is_primitive_float_type<T>::value)
> {};

// All int-type except bool supports MODULO
//
template<typename T>
struct supports_modulo_internal : std::integral_constant<bool,
        (is_primitive_int_type<T>::value && !std::is_same<T, bool>::value)
> {};

// All types support EQUAL
//
template<typename T>
struct supports_equal_internal : std::integral_constant<bool,
        (is_primitive_type<T>::value)
> {};

// All types except bool supports GREATER
//
template<typename T>
struct supports_greater_internal : std::integral_constant<bool,
        (is_primitive_type<T>::value && !std::is_same<T, bool>::value)
> {};

// Generate list of supported binary ops for each primitive type
//
template<typename T>
struct primitive_type_supports_binary_op_internal
{
    const static uint64_t value =
              (static_cast<uint64_t>(supports_addsubmul_internal<T>::value) << static_cast<int>(BinaryOps::ADD))
            + (static_cast<uint64_t>(supports_addsubmul_internal<T>::value) << static_cast<int>(BinaryOps::SUB))
            + (static_cast<uint64_t>(supports_addsubmul_internal<T>::value) << static_cast<int>(BinaryOps::MUL))
            + (static_cast<uint64_t>(supports_div_internal<T>::value) << static_cast<int>(BinaryOps::DIV))
            + (static_cast<uint64_t>(supports_modulo_internal<T>::value) << static_cast<int>(BinaryOps::MODULO))
            + (static_cast<uint64_t>(supports_equal_internal<T>::value) << static_cast<int>(BinaryOps::EQUAL))
            + (static_cast<uint64_t>(supports_greater_internal<T>::value) << static_cast<int>(BinaryOps::GREATER));
};

template<typename T, BinaryOps op>
struct primitive_type_supports_binary_op : std::integral_constant<bool,
        is_primitive_type<T>::value &&
        ((primitive_type_supports_binary_op_internal<T>::value & (static_cast<uint64_t>(1) << static_cast<int>(op))) != 0)
> {};

// static_cast_offset<T, U>::get()
// On static_cast-able <T, U>-pair (T, U must both be pointers),
// the value is the shift in bytes needed to add to T when converted to U
// Otherwise, the value is std::numeric_limits<ssize_t>::max() to not cause a compilation error when used.
//
// E.g.
//    class A { uint64_t a}; class B { uint64_t b; } class C : public A, public B { uint64_t c };
// Then:
//    static_cast_offset<A*, C*> = 0
//    static_cast_offset<B*, C*> = -8
//    static_cast_offset<C*, A*> = 0
//    static_cast_offset<C*, B*> = 8
//    static_cast_offset<A*, B*> = std::numeric_limits<ssize_t>::max()
//    static_cast_offset<std::string, int> = std::numeric_limits<ssize_t>::max()
//
template<typename T, typename U, typename Enable = void>
struct static_cast_offset
{
    static ssize_t get()
    {
        return std::numeric_limits<ssize_t>::max();
    }
};

template<typename T, typename U>
struct static_cast_offset<T*, U*, typename std::enable_if<
        std::is_convertible<T*, U*>::value
>::type > {
    static ssize_t get()
    {
        // Figure out the shift offset by doing a fake cast on 0x1000. Any address should
        // also work, but nullptr will not, so just to make things obvious..
        //
        return static_cast<ssize_t>(
            reinterpret_cast<uintptr_t>(static_cast<U*>(reinterpret_cast<T*>(0x1000))) -
            static_cast<uintptr_t>(0x1000));
    }
};

// Get the function address for a class method.
// From: How Facebook's HHVM Uses Modern C++
//       https://github.com/CppCon/CppCon2014/tree/master/Presentations
//
template <typename MethPtr>
void* GetClassMethodPtr(MethPtr p)
{
    union U { MethPtr meth; void* ptr; };
    return (reinterpret_cast<U*>(&p))->ptr;
}

// void* SelectTemplatedFnImplGeneric<F, Cond>(TypeId... typeIds)
//   Taking templated class F and Cond, and a list of TypeId values as parameters,
//   it returns the result of F<typename... T>::get() (which must return type void*)
//     where T... is the types corresponding to the provided list of TypeId values
//
// This allows you to dynamically select a templated function implementation based on
// typeId information not known at C++ build time.
//
// Each T must satisfy constexpr Cond<T>::value == true (only such T will be instantiated).
// This is helpful if your templated function gives compile error on certain instantiation.
//
// Example:
//   template<typename T> struct Cond : std::true_type {};
//   SelectTemplatedFnImplGeneric<F, Cond>(TypeId::Get<int>(), TypeId::Get<double>())
// will invoke F<int, double>::get() and return its result
//
// If a typeId has NumLayersOfPointers() >= 2, it will be translated to void**
//
template<template<typename... > class F, template<typename> class Cond, typename... Targs>
void* SelectTemplatedFnImplGeneric()
{
    return F<Targs...>::get();
}

template<bool CondV, template<typename> class CondT, typename... Targs2>
struct select_template_fn_impl_helper;

template<template<typename... > class F, template<typename> class Cond, typename... Targs, typename... Targs2>
void* SelectTemplatedFnImplGeneric(TypeId typeId, Targs2... args2)
{
    using FnType = void*(*)(Targs2...);

#define INTERNAL_GET_RECURSE_FN_BY_TYPE(...) \
    select_template_fn_impl_helper<Cond<__VA_ARGS__>::value, Cond, Targs2...>:: \
    template helper<F, Targs...>::template internal<__VA_ARGS__>::value

    static constexpr FnType jump_table_prim[TOTAL_VALUES_IN_TYPE_LABEL_ENUM] = {
        INTERNAL_GET_RECURSE_FN_BY_TYPE(void)
#define F(t) , INTERNAL_GET_RECURSE_FN_BY_TYPE(t)
FOR_EACH_PRIMITIVE_TYPE
#undef F

#define F(...) , INTERNAL_GET_RECURSE_FN_BY_TYPE(__VA_ARGS__)
FOR_EACH_CPP_CLASS_TYPE
#undef F
    };
    static constexpr FnType jump_table_ptr[TOTAL_VALUES_IN_TYPE_LABEL_ENUM] = {
        INTERNAL_GET_RECURSE_FN_BY_TYPE(void*)
#define F(t) , INTERNAL_GET_RECURSE_FN_BY_TYPE(t*)
FOR_EACH_PRIMITIVE_TYPE
#undef F

#define F(...) , INTERNAL_GET_RECURSE_FN_BY_TYPE(__VA_ARGS__ *)
FOR_EACH_CPP_CLASS_TYPE
#undef F
    };
    TestAssert(!typeId.IsInvalid() && !typeId.IsGeneratedCompositeType());
    if (!typeId.IsPointerType())
    {
        FnType recurseFn = jump_table_prim[typeId.ToTypeLabelEnum()];
        TestAssert(recurseFn != nullptr);
        return recurseFn(args2...);
    }
    else
    {
        if (typeId.NumLayersOfPointers() == 1)
        {
            TestAssert(!typeId.RemovePointer().IsGeneratedCompositeType());
            FnType recurseFn = jump_table_ptr[typeId.RemovePointer().ToTypeLabelEnum()];
            TestAssert(recurseFn != nullptr);
            return recurseFn(args2...);
        }
        else
        {
            // All types with >=2 layers of pointers become void**
            // since all those types behave the same as void** for all operations.
            // This is required to make the possible # of types finite.
            //
            static constexpr FnType recurseFnVoidStarStar = INTERNAL_GET_RECURSE_FN_BY_TYPE(void**);
            TestAssert(recurseFnVoidStarStar != nullptr);
            return recurseFnVoidStarStar(args2...);
        }
    }
    TestAssert(false);
    return nullptr;

#undef INTERNAL_GET_RECURSE_FN_BY_TYPE
}

// Essentially: this guy gives SelectTemplatedFnImplGeneric<F, CondT, Targs..., T> if CondV is true,
// or nullptr if CondV is false. Nested structs are needed only to make template parameter pack happy.
//
template<bool CondV, template<typename> class CondT, typename... Targs2>
struct select_template_fn_impl_helper
{
    using FnType = void*(*)(Targs2...);

    template<template<typename... > class F, typename... Targs>
    struct helper
    {
        template<typename T, typename Enable = void>
        struct internal
        {
            constexpr static FnType value = nullptr;
        };

        template<typename T>
        struct internal<T, typename std::enable_if<(
                        (std::is_same<T, void>::value || !std::is_same<T, void>::value) && CondV)>::type>
        {
            constexpr static FnType value = SelectTemplatedFnImplGeneric<F, CondT, Targs..., T>;
        };
    };
};

// Macro GEN_CLASS_METHOD_SELECTOR(selectorFnName, className, selectedMethodName, cond)
// Generates a function
//       void* selectorFnName(TypeId... typeIds)
// which returns the function address for class method
//       className::selectedMethodName<typename... T>
// and each T must satisfy constexpr cond<T>::value == true
//
// E.g. GEN_CLASS_METHOD_SELECTOR(selector, Foo, Bar)
// then calling selector(TypeId::Get<int>(), TypeId::Get<double>()) will
// give you the address of Foo::Bar<int, double>
//
// If a typeId has NumLayersOfPointers() >= 2, it will be translated to void**
//
#define GEN_CLASS_METHOD_SELECTOR(selectorFnName, className, selectedMethodName, cond) \
    template<typename... Targs>                                                        \
    struct MethodSelectorWrapper_ ## className ## _ ## selectedMethodName {            \
        static void* get() {                                                           \
            return AstTypeHelper::GetClassMethodPtr(                                   \
                    &className::selectedMethodName<Targs...>);	                       \
        }                                                                              \
    };                                                                                 \
    template<typename... Targs>                                                        \
    void* selectorFnName(Targs... args) {                                              \
        return AstTypeHelper::SelectTemplatedFnImplGeneric<                            \
            MethodSelectorWrapper_ ## className ## _ ## selectedMethodName, cond       \
        >(args...);                                                                    \
    }

// Similar to GEN_CLASS_METHOD_SELECTOR, but selects a function
//
#define GEN_FUNCTION_SELECTOR(selectorFnName, selectedFnName, cond)                   \
    template<typename... Targs>                                                       \
    struct FunctionSelectorWrapper_ ## selectedFnName {                               \
        static void* get() {                                                          \
            return reinterpret_cast<void*>(selectedFnName<Targs...>);	              \
        }                                                                             \
    };                                                                                \
    template<typename... Targs>                                                       \
    void* selectorFnName(Targs... args) {                                             \
        return AstTypeHelper::SelectTemplatedFnImplGeneric<                           \
            FunctionSelectorWrapper_ ## selectedFnName, cond                          \
        >(args...);                                                                   \
    }

// TypeId arith_common_type::get(TypeId a, TypeId b)
// Returns the arithmetic common type of two TypeIds. Both TypeId MUST be primitive types.
// In order to perform an arithmetic or comparison operation, one must convert both to this type first.
//
template<typename T, typename U, typename Enable = void>
struct arith_common_type_internal
{
    constexpr static TypeId internal = TypeId();
};

template<typename T, typename U>
struct arith_common_type_internal<T, U, typename std::enable_if<
    // This is just to SFINAE the case that std::common_type<T, U>::type does not exist
    (sizeof(typename std::common_type<T, U>::type) > 0)
>::type >
{
    constexpr static TypeId internal = GetTypeId< typename std::common_type<T, U>::type >::value;
};

namespace internal
{

template<typename T, typename U>
TypeId get_arith_common_type_impl()
{
    return arith_common_type_internal<T, U>::internal;
}

GEN_FUNCTION_SELECTOR(get_arith_common_type_selector,
                      get_arith_common_type_impl,
                      is_primitive_type)

}   // namespace internal

struct arith_common_type
{
    static TypeId get(TypeId typeId1, TypeId typeId2)
    {
        assert(typeId1.IsPrimitiveType() && typeId2.IsPrimitiveType());
        using _FnType = TypeId(*)();
        _FnType fn = reinterpret_cast<_FnType>(
                    internal::get_arith_common_type_selector(typeId1, typeId2));
        return fn();
    }
};

namespace internal
{

template<typename T, typename U>
ssize_t get_static_cast_offset_impl()
{
    ssize_t ret = static_cast_offset<T, U>::get();
    TestAssert(ret != std::numeric_limits<ssize_t>::max());
    return ret;
}

GEN_FUNCTION_SELECTOR(get_static_cast_offset_selector,
                      get_static_cast_offset_impl,
                      std::is_pointer)

}   // namespace internal

inline ssize_t get_static_cast_offset(TypeId src, TypeId dst)
{
    assert(src.IsPointerType() && dst.IsPointerType());
    using _FnType = ssize_t(*)();
    _FnType fn = reinterpret_cast<_FnType>(
            internal::get_static_cast_offset_selector(src, dst));
    ssize_t ret = fn();
    TestAssert(ret != std::numeric_limits<ssize_t>::max());
    return ret;
}

// Typecheck for but cpp class type (but pointers to cpp class types are OK)
//
template<typename T>
struct not_cpp_class_type: std::integral_constant<bool,
        is_any_possible_type<T>::value
     && !is_cpp_class_type<T>::value
> {};

template<typename T>
struct not_cpp_class_or_void_type: std::integral_constant<bool,
        not_cpp_class_type<T>::value
     && !std::is_same<T, void>::value
> {};

template<typename T>
struct pointer_or_uint64_type: std::integral_constant<bool,
        std::is_pointer<T>::value
     || std::is_same<T, uint64_t>::value
> {};

template<typename T>
struct primitive_or_pointer_type: std::integral_constant<bool,
        std::is_pointer<T>::value
     || is_primitive_type<T>::value
> {};

namespace internal
{

template<typename T, typename U>
bool may_static_cast_wrapper()
{
    return may_static_cast<T, U>::value;
}

GEN_FUNCTION_SELECTOR(may_static_cast_selector,
                      may_static_cast_wrapper,
                      not_cpp_class_type)

template<typename T, typename U>
bool may_reinterpret_cast_wrapper()
{
    return may_reinterpret_cast<T, U>::value;
}

GEN_FUNCTION_SELECTOR(may_reinterpret_cast_selector,
                      may_reinterpret_cast_wrapper,
                      not_cpp_class_type)

}   // namespace internal

// Execute *dst = *src. If type is void*, nothing is executed and no compile error.
//
template<typename T>
void void_safe_store_value_impl(T* dst, T* src)
{
    *dst = *src;
}

template<>
inline void void_safe_store_value_impl<void>(void* /*dst*/, void* /*src*/)
{ }

GEN_FUNCTION_SELECTOR(void_safe_store_value_selector,
                      void_safe_store_value_impl,
                      not_cpp_class_type)

// function_type_helper<T>:
//    T must be a C style function pointer or a std::function
//    gives information about arg and return types of the function
//
template<typename R, typename... Args>
struct function_type_helper_internal
{
    static const size_t numArgs = sizeof...(Args);

    using ReturnType = R;

    template<size_t i>
    using ArgType = typename std::tuple_element<i, std::tuple<Args...>>::type;

    template<size_t n, typename Enable = void>
    struct build_typeid_array_internal
    {
        static constexpr std::array<TypeId, n> value =
                constexpr_std_array_concat(
                    build_typeid_array_internal<n-1>::value,
                    std::array<TypeId, 1>{
                        GetTypeId<ArgType<n-1>>::value });
    };

    template<size_t n>
    struct build_typeid_array_internal<n, typename std::enable_if<(n == 0)>::type>
    {
        static constexpr std::array<TypeId, n> value = std::array<TypeId, 0>{};
    };

    static constexpr std::array<TypeId, numArgs> argTypeId =
            build_typeid_array_internal<numArgs>::value;

    static constexpr TypeId returnTypeId = GetTypeId<ReturnType>::value;
};

template<typename T>
struct function_type_helper
{
    static_assert(sizeof(T) == 0,
                  "T must be a C-style function or std::function");
};

template<typename R, typename... Args>
struct function_type_helper<std::function<R(Args...)> >
    : function_type_helper_internal<R, Args...>
{
    using ReturnType = typename function_type_helper_internal<R, Args...>::ReturnType;

    template<size_t i>
    using ArgType = typename function_type_helper_internal<R, Args...>::template ArgType<i>;
};

template<typename R, typename... Args>
struct function_type_helper<R(*)(Args...)>
    : function_type_helper_internal<R, Args...>
{
    using ReturnType = typename function_type_helper_internal<R, Args...>::ReturnType;

    template<size_t i>
    using ArgType = typename function_type_helper_internal<R, Args...>::template ArgType<i>;
};

template<typename T>
using function_return_type = typename function_type_helper<T>::ReturnType;

template<typename T, size_t i>
using function_arg_type = typename function_type_helper<T>::template ArgType<i>;

// is_function_prototype<T>
// true_type if T is a C-style function pointer or a std::function object
//
template<typename T>
struct is_function_prototype : std::false_type
{ };

template<typename R, typename... Args>
struct is_function_prototype<R(*)(Args...)> : std::true_type
{ };

template<typename R, typename... Args>
struct is_function_prototype< std::function<R(Args...)> > : std::true_type
{ };

// function_addr_to_callable<T>::get(void* addr):
//    Converts addr to a callable of type T, which may be a C-style function pointer or a std::function object.
//
template<typename T>
struct function_addr_to_callable
{
    static_assert(sizeof(T) == 0, "T must be either a C-style function pointer or a std::function object");
};

template<typename R, typename... Args>
struct function_addr_to_callable<R(*)(Args...)>
{
    using FnPrototype = R(*)(Args...);
    static FnPrototype get(void* fnAddr)
    {
        return reinterpret_cast<FnPrototype>(fnAddr);
    }
};

template<typename R, typename... Args>
struct function_addr_to_callable<std::function<R(Args...)>>
{
    using FnPrototype = std::function<R(Args...)>;
    static FnPrototype get(void* fnAddr)
    {
        return FnPrototype(reinterpret_cast<R(*)(Args...)>(fnAddr));
    }
};

// callable_to_c_style_fnptr_type<T>::type
//    If T is a std::function object, the result is its holding C-style function pointer type
//    If T is a C-style function pointer type, the result is still T.
//
template<typename T>
struct callable_to_c_style_fnptr_type
{
    static_assert(sizeof(T) == 0, "T must be either a C-style function pointer or a std::function object");
};

template<typename R, typename... Args>
struct callable_to_c_style_fnptr_type<R(*)(Args...)>
{
    using type = R(*)(Args...);
};

template<typename R, typename... Args>
struct callable_to_c_style_fnptr_type<std::function<R(Args...)>>
{
    using type = R(*)(Args...);
};

// callable_to_std_function_type<T>::type
//    If T is a std::function object, the result is still T.
//    If T is a C-style function pointer type, the result is the std::function object type that holds this type.
//
template<typename T>
struct callable_to_std_function_type
{
    static_assert(sizeof(T) == 0, "T must be either a C-style function pointer or a std::function object");
};

template<typename R, typename... Args>
struct callable_to_std_function_type<R(*)(Args...)>
{
    using type = std::function<R(Args...)>;
};

template<typename R, typename... Args>
struct callable_to_std_function_type<std::function<R(Args...)>>
{
    using type = std::function<R(Args...)>;
};

// Concatenate tuple types. Example:
//   tuple_concat_type<tuple<int, double>, tuple<float, char>> is tuple<int, double, float, char>
//
template<typename... T>
using tuple_concat_type = decltype(std::tuple_cat(std::declval<T>()...));

}   // namespace AstTypeHelper

inline bool TypeId::MayStaticCastTo(TypeId other) const
{
    assert(!IsInvalid());
    using FnProto = bool(*)();
    FnProto fn = reinterpret_cast<FnProto>(
                AstTypeHelper::internal::may_static_cast_selector(*this, other));
    return fn();
}

inline bool TypeId::MayReinterpretCastTo(TypeId other) const
{
    assert(!IsInvalid());
    using FnProto = bool(*)();
    FnProto fn = reinterpret_cast<FnProto>(
                AstTypeHelper::internal::may_reinterpret_cast_selector(*this, other));
    return fn();
}

}   // namespace PochiVM
