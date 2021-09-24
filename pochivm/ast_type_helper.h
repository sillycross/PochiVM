#pragma once

#include "common.h"
#include "runtime/pochivm_runtime_headers.h"

#include "generated/pochivm_runtime_cpp_types.generated.h"

#include "for_each_primitive_type.h"
#include "constexpr_array_concat_helper.h"
#include "get_mem_fn_address_helper.h"
#include "cxx2a_bit_cast_helper.h"
#include "pochivm_context.h"
#include "fastinterp/fastinterp_tpl_return_type.h"

namespace PochiVM
{

namespace AstTypeHelper
{

// Pochi implicit conversions for arithmetic differ slightly from C++.
// C++ promotes all integers smaller than sizeof(int) to the signed int type before
// adding. Pochi just implicitly converts the "smaller" type to the "larger" type
// of the expression where the type sizing is defined as 
// (u)int8_t < (u)int16_t < (u)int32_t < (u)int64_t < float < double.
// 
template <typename T, typename U>
struct ArithReturnType {
    using type = typename std::conditional<sizeof(T) <= sizeof(int16_t) || sizeof(U) <= sizeof(int16_t),
                                           typename std::conditional<sizeof(T) <= sizeof(U), U, T>::type,
                                           typename std::common_type<T, U>::type>::type;
};
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

const size_t AstCppTypeStorageSizeInBytes[1 + x_num_cpp_class_types] = {
#define F(...) sizeof(__VA_ARGS__),
FOR_EACH_CPP_CLASS_TYPE
#undef F
    static_cast<size_t>(-1) /*dummy value for bad CPP type */
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

class FastInterpTypeId;

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

    constexpr bool operator==(const TypeId& other) const { return other.value == value; }
    constexpr bool operator!=(const TypeId& other) const { return !(*this == other); }

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
    bool IsFloatingPoint() const { return IsFloat() || IsDouble(); }
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
    constexpr TypeId WARN_UNUSED AddPointer() const {
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

    uint64_t GetCppClassTypeOrdinal() const
    {
        assert(IsCppClassType());
        uint64_t ord = value - x_num_primitive_types - 1;
        assert(ord < AstTypeHelper::x_num_cpp_class_types);
        return ord;
    }

    const char* GetCppTypeLLVMTypeName() const
    {
        uint64_t ord = GetCppClassTypeOrdinal();
        const char* ret = AstTypeHelper::AstCppTypeLLVMTypeName[ord];
        assert(ret != nullptr);
        return ret;
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
        else if (IsCppClassType())
        {
            uint64_t ord = GetCppClassTypeOrdinal();
            return AstTypeHelper::AstCppTypeStorageSizeInBytes[ord];
        }
        else if (IsGeneratedCompositeType())
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

    // Default conversion to FastInterpTypeId:
    // >=2 level of pointer => void**
    // CPP-type* => void*
    // CPP-type => locked down
    //
    FastInterpTypeId GetDefaultFastInterpTypeId();

    // same as above, except that >=1 level of pointer => void*
    //
    FastInterpTypeId GetOneLevelPtrFastInterpTypeId();

    // TypeId::Get<T>() return TypeId for T
    //
    template<typename T>
    static constexpr TypeId Get()
    {
        TypeId ret = AstTypeHelper::GetTypeId<T>::value;
        assert(!ret.IsInvalid());
        return ret;
    }

    static constexpr TypeId GetCppTypeFromOrdinal(uint64_t ord)
    {
        return TypeId { x_num_primitive_types + 1 + ord };
    }

    // TypeId::Get<T, valueType>() return TypeId for <T, valueType>
    //
    template<typename T, AstValueType valueType>
    static TypeId Get()
    {
        return (valueType == LValue) ? Get<T>().AddPointer() : Get<T>();
    }

    const static uint64_t x_generated_composite_type = 1000000000ULL * 1000000000ULL;
    // Craziness: if you want to change this constant for some reason,
    // make sure you make the same change in the definition in fastinterp/metavar.h as well.
    // Unfortunately due to build dependency that header file cannot include this one.
    //
    const static uint64_t x_pointer_typeid_inc = 1000000000;
    const static uint64_t x_invalid_typeid = static_cast<uint64_t>(-1);
};

}   // namespace PochiVM

// Inject std::hash for TypeId
//
namespace std
{
    template<> struct hash<PochiVM::TypeId>
    {
        std::size_t operator()(const PochiVM::TypeId& typeId) const noexcept
        {
            return std::hash<uint64_t>{}(typeId.value);
        }
    };
}   // namespace std

namespace PochiVM {

using InterpCallCppFunctionImpl = void(*)(void* /*ret*/, void** /*params*/);

struct FastInterpCppFunctionInfo
{
    TypeId m_returnType;
    bool m_isNoExcept;
    // a function pointer of shape 'FIReturnType<R, isNoExcept>(*)(void**) noexcept'
    //
    void* m_interpImpl;
};
using FastInterpCallCppFunctionGetter = FastInterpCppFunctionInfo(*)() noexcept;

struct CppFunctionMetadata
{
    // The symbol name, and the bitcode stub
    //
    const BitcodeData* m_bitcodeData;
    // Types of all params and return value
    //
    const TypeId* m_paramTypes;
    size_t m_numParams;
    TypeId m_returnType;
    // Whether this function is using StructRet attribute
    //
    bool m_isUsingSret;
    // Whether this function is noexcept
    //
    bool m_isNoExcept;
    // The interp function interface
    //
    InterpCallCppFunctionImpl m_debugInterpFn;
    FastInterpCallCppFunctionGetter m_getFastInterpFn;
    // The unique ordinal, starting from 0
    //
    size_t m_functionOrdinal;
};

class AstNodeBase;

// A helper storing the constructor, and the parameters passed to the constructor
//
class ConstructorParamInfo : NonCopyable
{
public:
    const CppFunctionMetadata* m_constructorMd;
    std::vector<AstNodeBase*> m_params;

protected:
    ConstructorParamInfo() {}
};

namespace AstTypeHelper
{

// 'char' behaves identical to either 'int8_t' or 'uint8_t' (platform dependent),
// but is a different type. This type figures out what it should behaves like
//
using CharAliasType = typename std::conditional<std::is_signed<char>::value, int8_t, uint8_t>::type;

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

template<> struct GetTypeId<char> {
    constexpr static TypeId value = GetTypeId<CharAliasType>::value;
};

#define F(...) \
template<> struct GetTypeId<__VA_ARGS__> {	\
    static_assert(cpp_type_ordinal<__VA_ARGS__>::ordinal != x_num_cpp_class_types, "unknown cpp type"); \
    constexpr static TypeId value = TypeId::GetCppTypeFromOrdinal(cpp_type_ordinal<__VA_ARGS__>::ordinal); \
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
FOR_EACH_PRIMITIVE_INT_TYPE_AND_CHAR
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
FOR_EACH_PRIMITIVE_TYPE_AND_CHAR
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

template<typename T>
struct supports_div_internal : std::integral_constant<bool,
        (is_primitive_type<T>::value && !std::is_same<T, bool>::value)
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

template <typename T, AstArithmeticExprType expr_type>
struct primitive_type_supports_arithmetic_expr_type : std::integral_constant<bool,
        is_primitive_type<T>::value && 
        ((expr_type == AstArithmeticExprType::ADD && primitive_type_supports_binary_op<T, BinaryOps::ADD>::value) ||
        (expr_type == AstArithmeticExprType::SUB && primitive_type_supports_binary_op<T, BinaryOps::SUB>::value) ||
        (expr_type == AstArithmeticExprType::MUL && primitive_type_supports_binary_op<T, BinaryOps::MUL>::value) ||
        (expr_type == AstArithmeticExprType::DIV && primitive_type_supports_binary_op<T, BinaryOps::DIV>::value) ||
        (expr_type == AstArithmeticExprType::MOD && primitive_type_supports_binary_op<T, BinaryOps::MODULO>::value))
> {};

template <typename T, AstComparisonExprType expr_type>
struct primitive_type_supports_comparison_expr_type : std::integral_constant<bool,
        is_primitive_type<T>::value && 
        ((expr_type == AstComparisonExprType::EQUAL && primitive_type_supports_binary_op<T, BinaryOps::EQUAL>::value) ||
        (expr_type == AstComparisonExprType::NOT_EQUAL && primitive_type_supports_binary_op<T, BinaryOps::EQUAL>::value) ||
        (expr_type == AstComparisonExprType::LESS_THAN && primitive_type_supports_binary_op<T, BinaryOps::GREATER>::value) ||
        (expr_type == AstComparisonExprType::GREATER_THAN && primitive_type_supports_binary_op<T, BinaryOps::GREATER>::value) ||
        (expr_type == AstComparisonExprType::LESS_EQUAL && primitive_type_supports_binary_op<T, BinaryOps::GREATER>::value
                                                        && primitive_type_supports_binary_op<T, BinaryOps::EQUAL>::value) ||
        (expr_type == AstComparisonExprType::GREATER_EQUAL && primitive_type_supports_binary_op<T, BinaryOps::GREATER>::value
                                                           && primitive_type_supports_binary_op<T, BinaryOps::EQUAL>::value))
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
//    T must be a C style function pointer
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
                  "T must be a C-style function");
};

template<typename R, typename... Args>
struct function_type_helper<R(*)(Args...)>
    : function_type_helper_internal<R, Args...>
{
    using ReturnType = typename function_type_helper_internal<R, Args...>::ReturnType;

    template<size_t i>
    using ArgType = typename function_type_helper_internal<R, Args...>::template ArgType<i>;
};

template<typename R, typename... Args>
struct function_type_helper<R(*)(Args...) noexcept>
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
// true_type if T is a C-style function pointer
//
template<typename T>
struct is_function_prototype : std::false_type
{ };

template<typename R, typename... Args>
struct is_function_prototype<R(*)(Args...)> : std::true_type
{ };

template<typename R, typename... Args>
struct is_function_prototype<R(*)(Args...) noexcept> : std::true_type
{ };

// is_function_prototype<T>
// true_type if T is a noexcept C-style function pointer
//
template<typename T>
struct is_noexcept_function_prototype : std::false_type
{ };

template<typename R, typename... Args>
struct is_noexcept_function_prototype<R(*)(Args...) noexcept> : std::true_type
{ };

// function_addr_to_callable<T>::get(void* addr):
//    Converts addr to a callable of type T, which must be a C-style function pointer
//
template<typename T>
struct function_addr_to_callable
{
    static_assert(sizeof(T) == 0, "T must be a C-style function pointer ");
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
struct function_addr_to_callable<R(*)(Args...) noexcept>
{
    using FnPrototype = R(*)(Args...) noexcept;
    static FnPrototype get(void* fnAddr)
    {
        return reinterpret_cast<FnPrototype>(fnAddr);
    }
};

// Concatenate tuple types. Example:
//   tuple_concat_type<tuple<int, double>, tuple<float, char>> is tuple<int, double, float, char>
//
template<typename... T>
using tuple_concat_type = decltype(std::tuple_cat(std::declval<T>()...));

// interp_call_cpp_fn_helper<f>::interpFn
//    Get the interp implementation to call a CPP function.
//
//    In LLVM mode, we only use ReflectionHelper::function_wrapper_helper to wrap the CPP function when actually needed
//    (i.e. when the CPP function actually contains a non-primitive paramater or return value) for performance.
//    However in interp mode, we always wrap the CPP function for simplicity.
//
//    As such, this class assumes that 'f' must be a function generated from ReflectionHelper::function_wrapper_helper,
//    which implies that 'f' is always a free function with primitive-or-pointer-type parameters/return-value
//    (having non-primitive or reference-type params/ret, or 'f' being a member function is not possible).
//
//    interp_call_cpp_fn_helper<f>::interpFn is a function with fixed prototype void(*)(void* ret, void** params)
//    'ret' is the address into which the return value of the call will be stored.
//    'params' is an array of length 'n' where n is # of parameters of 'f'.
//    params[i] should be a pointer pointing to the value of the i-th parameter of the call.
//
template<auto f>
struct interp_call_cpp_fn_helper
{
    using F = decltype(f);
    using R = typename function_type_helper<F>::ReturnType;

    const static size_t numArgs = function_type_helper<F>::numArgs;

    template<size_t i>
    using ArgType = typename function_type_helper<F>::template ArgType<i>;

    template<size_t i>
    using ArgTypePtr = typename std::add_pointer<ArgType<i>>::type;

    template<size_t n, typename Enable = void>
    struct build_param_helper
    {
        template<typename... TArgs>
        static void call(void* retVoid, void** params, TArgs... unpackedParams)
        {
            static_assert(AstTypeHelper::primitive_or_pointer_type<typename std::remove_cv<ArgType<numArgs-n>>::type>::value,
                          "parameter type must be pointer or primitive type");
            build_param_helper<n-1>::call(retVoid, params + 1,
                                          unpackedParams...,
                                          reinterpret_cast<ArgTypePtr<numArgs-n>>(reinterpret_cast<uintptr_t>(params)));
        }
    };

    template<typename R2, typename... TArgs>
    static R2 call_impl(TArgs... unpackedParams)
    {
        return f(*unpackedParams...);
    }

    template<typename R2, typename Enable = void>
    struct return_value_helper
    {
        using R2NoConst = typename std::remove_cv<R2>::type;

        template<typename... TArgs>
        static void call(void* retVoid, TArgs... unpackedParams)
        {
            static_assert(AstTypeHelper::primitive_or_pointer_type<R2NoConst>::value,
                          "return type must be pointer or primitive type");
            using R2Star = typename std::add_pointer<R2NoConst>::type;
            R2Star ret = reinterpret_cast<R2Star>(reinterpret_cast<uintptr_t>(retVoid));
            *ret = call_impl<R2>(unpackedParams...);
        }
    };

    template<typename R2>
    struct return_value_helper<R2, typename std::enable_if<(std::is_same<R2, void>::value)>::type>
    {
        template<typename... TArgs>
        static void call(void* DEBUG_ONLY(retVoid), TArgs... unpackedParams)
        {
            assert(retVoid == nullptr);
            call_impl<R2>(unpackedParams...);
        }
    };

    template<size_t n>
    struct build_param_helper<n, typename std::enable_if<(n == 0)>::type>
    {
        template<typename... TArgs>
        static void call(void* retVoid, void** /*params*/, TArgs... unpackedParams)
        {
            static_assert(sizeof...(unpackedParams) == numArgs, "wrong number of arguments");
            return_value_helper<R>::call(retVoid, unpackedParams...);
        }
    };

    static void call(void* retVoid, void** params)
    {
        build_param_helper<numArgs>::call(retVoid, params);
    }

    static constexpr InterpCallCppFunctionImpl interpFn = call;
};

// One extra layer of wrapping over interp_call_cpp_fn_helper,
// which handles the exception and return value in a manner expected by fastinterp
//
template<bool isNoExcept, typename R, InterpCallCppFunctionImpl fnPtr>
struct fastinterp_call_cpp_fn_helper
{
    static FIReturnType<R, isNoExcept> impl(void** params) noexcept
    {
        // This is a bit weird.. FastInterp has a 8-byte padding in the params,
        // since its constant placeholder cannot be 0.
        //
        params++;
        if constexpr(isNoExcept)
        {
            if constexpr(std::is_same<R, void>::value)
            {
                fnPtr(nullptr, params);
            }
            else
            {
                R ret;
                fnPtr(&ret, params);
                return ret;
            }
        }
        else
        {
            FIReturnValueOrExn<R> ret;
            ret.m_hasExn = false;
            try
            {
                if constexpr(std::is_same<R, void>::value)
                {
                    fnPtr(nullptr, params);
                }
                else
                {
                    fnPtr(&ret.m_ret, params);
                }
                return ret;
            }
            catch (...)
            {
                TestAssert(!thread_pochiVMFastInterpOutstandingExceptionPtr);

                // From cppreference on std::current_exception():
                //     If the implementation of this function requires a call to new and the call fails,
                //     the returned pointer will hold a reference to an instance of std::bad_alloc.
                //
                // No, we don't care about such corner case. Everyone knows C++ exception is broken.
                //
                thread_pochiVMFastInterpOutstandingExceptionPtr = std::current_exception();
                TestAssert(thread_pochiVMFastInterpOutstandingExceptionPtr);
                return FIReturnValueHelper::GetForExn<R>();
            }
        }
    }

    static FastInterpCppFunctionInfo get() noexcept
    {
        return FastInterpCppFunctionInfo {
            TypeId::Get<R>(),
            isNoExcept,
            reinterpret_cast<void*>(impl)
        };
    }
};

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

// In interp mode, we only know a limited set of types (fundamental types, pointer to fundamental types, and void**)
// This is a wrapper over typeId so that only such types are allowed.
//
class FastInterpTypeId
{
public:
    FastInterpTypeId() : m_typeId() {}
    explicit FastInterpTypeId(TypeId typeId)
    {
        TestAssert(typeId.NumLayersOfPointers() <= 2);
        TestAssertImp(typeId.NumLayersOfPointers() == 2, typeId == TypeId::Get<void**>());
        TestAssertImp(typeId.NumLayersOfPointers() == 1, typeId.RemovePointer().IsVoid() || typeId.RemovePointer().IsPrimitiveType());
        TestAssertImp(typeId.NumLayersOfPointers() == 0, typeId.IsVoid() || typeId.IsPrimitiveType());
        m_typeId = typeId;
    }

    TypeId GetTypeId() const
    {
        return m_typeId;
    }

private:
    TypeId m_typeId;
};

inline FastInterpTypeId TypeId::GetDefaultFastInterpTypeId()
{
    if (NumLayersOfPointers() >= 2)
    {
        return FastInterpTypeId(TypeId::Get<void**>());
    }
    else if (NumLayersOfPointers() == 1)
    {
        if (!RemovePointer().IsVoid() && !RemovePointer().IsPrimitiveType())
        {
            return FastInterpTypeId(TypeId::Get<void*>());
        }
        else
        {
            return FastInterpTypeId(*this);
        }
    }
    else
    {
        TestAssert(IsVoid() || IsPrimitiveType());
        return FastInterpTypeId(*this);
    }
}

inline FastInterpTypeId TypeId::GetOneLevelPtrFastInterpTypeId()
{
    if (NumLayersOfPointers() >= 1)
    {
        return FastInterpTypeId(TypeId::Get<void*>());
    }
    else
    {
        TestAssert(IsVoid() || IsPrimitiveType());
        return FastInterpTypeId(*this);
    }
}

}   // namespace PochiVM
