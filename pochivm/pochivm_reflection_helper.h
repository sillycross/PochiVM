#pragma once

// TODO: header guard

#include "constexpr_array_concat_helper.h"
#include "for_each_primitive_type.h"
#include "get_mem_fn_address_helper.h"
#include "reflective_stringify_helper.h"
#include "ast_comparison_expr_type.h"
#include "ast_arithmetic_expr_type.h"

namespace PochiVM
{

namespace ReflectionHelper
{

template<typename T> struct is_primitive_type : std::false_type {};
#define F(t) template<> struct is_primitive_type<t> : std::true_type {};
FOR_EACH_PRIMITIVE_TYPE_AND_CHAR
#undef F

// remove_type_ref_internal<T>::type
// Transform a C++-type to a primitive type that we support by removing refs (but does not drop cv-qualifier)
//    Transform reference to pointer (e.g. 'int&' becomes 'int*')
//    Transform non-primitive pass-by-value parameter to pointer (e.g. 'std::string' becomes 'std::string*)
//    Lockdown rvalue-reference (e.g. 'int&&')
//
template<typename T>
struct remove_type_ref_internal {
    // Non-primitive pass-by-value parameter, or pass-by-reference parameter, becomes a pointer
    //
    using type = typename std::add_pointer<T>::type;
};

// Primitive types are unchanged
//
#define F(t) template<> struct remove_type_ref_internal<t> { using type = t; };
FOR_EACH_PRIMITIVE_TYPE_AND_CHAR
#undef F

#define F(t) template<> struct remove_type_ref_internal<const t> { using type = const t; };
FOR_EACH_PRIMITIVE_TYPE_AND_CHAR
#undef F

// Pointer types are unchanged
//
template<typename T>
struct remove_type_ref_internal<T*> {
    using type = T*;
};

// void type is unchanged
//
template<>
struct remove_type_ref_internal<void> {
    using type = void;
};

template<>
struct remove_type_ref_internal<const void> {
    using type = const void;
};

// Lockdown rvalue-reference parameter
//
template<typename T>
struct remove_type_ref_internal<T&&> {
    static_assert(sizeof(T) == 0, "Function with rvalue-reference parameter is not supported!");
};

// convert 'const primitive type&' to 'primitive type'
//
template<typename T>
struct convert_const_primitive_type_ref
{
    using type = T;
};

#define F(t) template<> struct convert_const_primitive_type_ref<const t&> { using type = t; };
FOR_EACH_PRIMITIVE_TYPE_AND_CHAR
#undef F

template<typename T>
struct convert_const_primitive_type_ref<T* const&> {
    using type = T*;
};

template<typename T>
struct is_const_primitive_type_ref_type : std::integral_constant<bool,
    (!std::is_same<typename convert_const_primitive_type_ref<T>::type, T>::value)>
{ };

template<typename T>
struct remove_param_type_ref
{
    using type = typename remove_type_ref_internal<T>::type;
};

// recursive_remove_cv<T>::type
//    Drop const-qualifier recursively ('const int* const* const' becomes 'int**')
//    Lockdown volatile-qualifier
//
// It assumes that T is a type generated from remove_param_type_ref or remove_ret_type_ref.
//
template<typename T>
struct recursive_remove_cv {
    using type = T;
};

template<typename T>
struct recursive_remove_cv<const T> {
    using type = typename recursive_remove_cv<T>::type;
};

template<typename T>
struct recursive_remove_cv<volatile T> {
    static_assert(sizeof(T) == 0, "Function with volatile parameter is not supported!");
};

template<typename T>
struct recursive_remove_cv<const volatile T> {
    static_assert(sizeof(T) == 0, "Function with volatile parameter is not supported!");
};

template<typename T>
struct recursive_remove_cv<T*> {
    using type = typename std::add_pointer<
                        typename recursive_remove_cv<T>::type
                 >::type;
};

// is_converted_pointer_type<T>::value
//    true if it is a parameter converted from a reference or a pass-by-value non-primitive type to a pointer
//
template<typename T>
struct is_converted_reference_type : std::integral_constant<bool,
    !std::is_same<typename remove_type_ref_internal<T>::type, T>::value
> {};

// Whether the conversion is non-trivial (changes LLVM prototype)
// We make pass-by-value non-primitive-type parameter a pointer, which changes LLVM prototype.
// The reference-to-pointer conversion does not, because C++ Itanium ABI specifies
// that reference should be passed as if it were a pointer to the object in ABI.
//
template<typename T>
struct is_nontrivial_arg_conversion : std::integral_constant<bool,
    !std::is_reference<T>::value && is_converted_reference_type<T>::value
> {};

template<typename T>
struct arg_transform_helper
{
    using RemovedRefArgType = typename remove_param_type_ref<T>::type;
    using RemovedCvArgType = typename recursive_remove_cv<RemovedRefArgType>::type;
    using ApiArgType = typename std::conditional<
                    is_converted_reference_type<T>::value,
                    typename std::remove_pointer<RemovedCvArgType>::type,
                    RemovedCvArgType>::type;
    static const bool isApiArgVariable = is_converted_reference_type<T>::value;
    static const bool isApiArgConstPrimitiveReference = is_const_primitive_type_ref_type<T>::value;
};

template<typename R, typename... Args>
struct function_typenames_helper_internal
{
    static const size_t numArgs = sizeof...(Args);

    using ReturnType = R;
    using ApiReturnType = typename arg_transform_helper<ReturnType>::ApiArgType;

    template<size_t i> using ArgType = typename std::tuple_element<i, std::tuple<Args...>>::type;
    template<size_t i> using RemovedRefArgType = typename arg_transform_helper<ArgType<i>>::RemovedRefArgType;
    template<size_t i> using ApiArgType = typename arg_transform_helper<ArgType<i>>::ApiArgType;

    template<size_t i>
    static constexpr bool isApiArgTypeVariable() { return arg_transform_helper<ArgType<i>>::isApiArgVariable; }

    template<size_t i>
    static constexpr bool isApiArgTypeConstPrimitiveReference() { return arg_transform_helper<ArgType<i>>::isApiArgConstPrimitiveReference; }

    static constexpr bool isApiReturnTypeVariable() { return std::is_reference<ReturnType>::value; }

    template<size_t i>
    static constexpr bool isArgNontriviallyConverted() { return is_nontrivial_arg_conversion<ArgType<i>>::value; }

    static constexpr bool isRetValNontriviallyConverted() { return is_nontrivial_arg_conversion<ReturnType>::value; }

    template<size_t n, typename Enable = void>
    struct build_original_typenames_array_internal
    {
        static constexpr std::array<const char*, n+1> get()
        {
            return PochiVM::AstTypeHelper::constexpr_std_array_concat(
                    build_original_typenames_array_internal<n-1>::get(),
                    std::array<const char*, 1>{
                            __pochivm_stringify_type__<ArgType<n-1>>() });
        }
    };

    template<size_t n>
    struct build_original_typenames_array_internal<n, typename std::enable_if<(n == 0)>::type>
    {
        static constexpr std::array<const char*, n+1> get()
        {
            return std::array<const char*, 1>{ __pochivm_stringify_type__<ReturnType>() };
        }
    };

    static const char* const* get_original_ret_and_param_typenames()
    {
        static constexpr std::array<const char*, numArgs + 1> data =
                build_original_typenames_array_internal<numArgs>::get();
        return data.data();
    }

    template<size_t n, typename Enable = void>
    struct build_api_typenames_array_internal
    {
        static constexpr std::array<std::pair<const char*, std::pair<bool, bool> >, n+1> get()
        {
            return PochiVM::AstTypeHelper::constexpr_std_array_concat(
                    build_api_typenames_array_internal<n-1>::get(),
                    std::array<std::pair<const char*, std::pair<bool, bool> >, 1>{
                            std::make_pair(
                                    __pochivm_stringify_type__<ApiArgType<n-1>>(),
                                    std::make_pair(
                                        isApiArgTypeVariable<n-1>() /*isVar*/,
                                        isApiArgTypeConstPrimitiveReference<n-1>() /*isConstPrimitiveRef*/))
                    });
        }
    };

    template<size_t n>
    struct build_api_typenames_array_internal<n, typename std::enable_if<(n == 0)>::type>
    {
        static constexpr std::array<std::pair<const char*, std::pair<bool, bool> >, n+1> get()
        {
            return std::array<std::pair<const char*, std::pair<bool, bool> >, 1>{
                    std::make_pair(__pochivm_stringify_type__<ApiReturnType>(),
                                   std::make_pair(isApiReturnTypeVariable() /*isVar*/,
                                                  false /*isConstPrimitiveRef*/)) };
        }
    };

    static const std::pair<const char*, std::pair<bool, bool> >* get_api_ret_and_param_typenames()
    {
        static constexpr std::array<std::pair<const char*, std::pair<bool, bool> >, numArgs + 1> data =
                build_api_typenames_array_internal<numArgs>::get();
        return data.data();
    }

    template<size_t n, typename Enable = void>
    struct is_wrapper_fn_required_internal
    {
        static constexpr bool get()
        {
            return isArgNontriviallyConverted<n-1>() || is_wrapper_fn_required_internal<n-1>::get();
        }
    };

    template<size_t n>
    struct is_wrapper_fn_required_internal<n, typename std::enable_if<(n == 0)>::type>
    {
        static constexpr bool get()
        {
            return isRetValNontriviallyConverted();
        }
    };

    constexpr static bool is_wrapper_fn_required()
    {
        return is_wrapper_fn_required_internal<numArgs>::get();
    }
};

template<typename T>
struct function_typenames_helper
{
    static_assert(sizeof(T) == 0, "T must be a a pointer to a free function or a static or non-static class method");
};

struct return_nullptr_class_typename
{
    static const char* get_class_typename()
    {
        return nullptr;
    }
};

// A static class method or a free function takes following optional qualifications:
//     except(optional) attr(optional)
// We support 'except', and lockdown 'attr'.
//
template<typename R, typename... Args>
struct function_typenames_helper<R(*)(Args...)>
    : function_typenames_helper_internal<R, Args...>
    , return_nullptr_class_typename
{
    using ReturnType = R;
    static constexpr bool is_noexcept() { return false; }
    static constexpr bool is_const() { return false; }
};

template<typename R, typename... Args>
struct function_typenames_helper<R(*)(Args...) noexcept>
    : function_typenames_helper_internal<R, Args...>
    , return_nullptr_class_typename
{
    using ReturnType = R;
    static constexpr bool is_noexcept() { return true; }
    static constexpr bool is_const() { return false; }
};

template<typename T>
struct class_name_helper_internal
{
    static const char* get_class_typename()
    {
        return __pochivm_stringify_type__<T>();
    }
};

// A non-static class method takes following optional qualifications:
//     cv(optional) ref(optional) except(optional) attr(optional)
// We support 'cv' and 'except', and lockdown 'ref' and 'attr'.
// ref '&&' is not supportable for same reason as rvalue-ref parameters.
// ref '&' seems not a problem, but just for simplicity also lock it down for now.
//
template<typename R, typename C, typename... Args>
struct function_typenames_helper<R(C::*)(Args...)>
    : function_typenames_helper_internal<R, Args...>
    , class_name_helper_internal<C>
{
    using ReturnType = R;
    using ClassType = C;
    static constexpr bool is_noexcept() { return false; }
    static constexpr bool is_const() { return false; }
};

template<typename R, typename C, typename... Args>
struct function_typenames_helper<R(C::*)(Args...) noexcept>
    : function_typenames_helper_internal<R, Args...>
    , class_name_helper_internal<C>
{
    using ReturnType = R;
    using ClassType = C;
    static constexpr bool is_noexcept() { return true; }
    static constexpr bool is_const() { return false; }
};

template<typename R, typename C, typename... Args>
struct function_typenames_helper<R(C::*)(Args...) const>
    : function_typenames_helper_internal<R, Args...>
    , class_name_helper_internal<C>
{
    using ReturnType = R;
    using ClassType = C;
    static constexpr bool is_noexcept() { return false; }
    static constexpr bool is_const() { return true; }
};

template<typename R, typename C, typename... Args>
struct function_typenames_helper<R(C::*)(Args...) const noexcept>
    : function_typenames_helper_internal<R, Args...>
    , class_name_helper_internal<C>
{
    using ReturnType = R;
    using ClassType = C;
    static constexpr bool is_noexcept() { return true; }
    static constexpr bool is_const() { return true; }
};

template<int... >
struct tpl_sequence {};

template<int N, int... S>
struct gen_tpl_sequence : gen_tpl_sequence<N-1, N-1, S...> {};

template<int... S>
struct gen_tpl_sequence<0, S...>
{
    using type = tpl_sequence<S...>;
};

// function_wrapper_helper<fnPtr>::wrapperFn
//    For functions with non-primitive parameter or return values,
//    we generate a wrapper function which wraps it with primitive parameters and return values.
//
//    The transformation rule is the following:
//    (1) If the function is a class member function, it is first converted to a free function
//        by appending the 'this' object as the first parameter.
//    (2) (a) If the return value is a by-value non-primitive type, the return value becomes void,
//            and a pointer-type is appended as the first parameter.
//             The return value is then constructed-in-place at the given address using in-place new.
//        (b) If the return value is by-reference, it becomes a pointer type.
//        (c) If the return value is a primitive type, it is unchanged.
//    (3) Each parameter which is a by-value non-primitive type or a by-reference type becomes a pointer.
//
// function_wrapper_helper<fnPtr>::isTrivial()
//    Return true if the transformation is trivial (so the transformation is not needed)
//
template<auto fnPtr>
class function_wrapper_helper
{
private:
    using FnType = decltype(fnPtr);
    using FnTypeInfo = function_typenames_helper<FnType>;
    using SeqType = typename gen_tpl_sequence<FnTypeInfo::numArgs>::type;

    template<int k> using InType = typename FnTypeInfo::template RemovedRefArgType<k>;
    template<int k> using OutType = typename FnTypeInfo::template ArgType<k>;

    // object type parameter (passed in by value)
    //
    template<int k, typename Enable = void>
    struct convert_param_internal
    {
        static_assert(std::is_same<InType<k>, typename std::add_pointer<OutType<k>>::type>::value
                              && !std::is_reference<OutType<k>>::value, "wrong specialization");

        static constexpr bool is_nothrow = std::is_nothrow_copy_constructible<
                typename std::remove_const<OutType<k>>::type>::value;

        using type = OutType<k>&;
        static type get(InType<k> v) noexcept
        {
            return *v;
        }
    };

    // reference type parameter
    //
    template<int k>
    struct convert_param_internal<k, typename std::enable_if<(
            std::is_reference<OutType<k>>::value)>::type>
    {
        static_assert(std::is_lvalue_reference<OutType<k>>::value,
                      "function returning rvalue reference is not supported");
        static_assert(std::is_same<InType<k>, typename std::add_pointer<OutType<k>>::type>::value,
                      "InType should be the pointer type");

        static constexpr bool is_nothrow = true;

        using type = OutType<k>;
        static type get(InType<k> v) noexcept
        {
            return *v;
        }
    };

    // primitive type or pointer type parameter
    //
    template<int k>
    struct convert_param_internal<k, typename std::enable_if<(
            !is_converted_reference_type<OutType<k>>::value)>::type>
    {
        static_assert(std::is_same<InType<k>, OutType<k>>::value, "wrong specialization");

        static constexpr bool is_nothrow = true;

        using type = OutType<k>;
        static type get(InType<k> v) noexcept
        {
            return v;
        }
    };

    template<size_t n>
    static constexpr bool is_param_wrappings_nothrow()
    {
        if constexpr(n == 0)
        {
            return true;
        }
        else
        {
            return (convert_param_internal<n-1>::is_nothrow) && is_param_wrappings_nothrow<n-1>();
        }
    }

    static constexpr bool is_all_param_wrapping_nothrow = is_param_wrappings_nothrow<FnTypeInfo::numArgs>();
    static constexpr bool is_function_nothrow = FnTypeInfo::is_noexcept();
    static constexpr bool is_wrapper_nothrow = is_all_param_wrapping_nothrow && is_function_nothrow;

    template<int k, typename T>
    static typename convert_param_internal<k>::type convert_param(T input) noexcept
    {
        return convert_param_internal<k>::get(std::get<k>(input));
    }

    template<typename F2, typename Enable = void>
    struct call_fn_helper
    {
        template<typename R2, int... S, typename... Args>
        static R2 call(tpl_sequence<S...> /*unused*/, Args... args) noexcept(is_wrapper_nothrow)
        {
            return fnPtr(convert_param<S>(std::forward_as_tuple(args...))...);
        }
    };

    template<typename F2>
    struct call_fn_helper<F2, typename std::enable_if<(std::is_member_function_pointer<F2>::value)>::type>
    {
        template<typename R2, int... S, typename C, typename... Args>
        static R2 call(tpl_sequence<S...> /*unused*/, C c, Args... args) noexcept(is_wrapper_nothrow)
        {
            return (c->*fnPtr)(convert_param<S>(std::forward_as_tuple(args...))...);
        }
    };

    template<typename R2, typename... Args>
    static R2 call_fn_impl(Args... args) noexcept(is_wrapper_nothrow)
    {
        return call_fn_helper<FnType>::template call<R2>(SeqType(), args...);
    }

    // If the function returns a non-primitive object type, change it to a pointer as a special parameter
    //
    template<typename R2, typename Enable = void>
    struct call_fn_wrapper
    {
        using R2NoConst = typename std::remove_const<R2>::type;

        // C++17 guaranteed copy-elision makes sure that no copy/move constructor will be called
        // so we don't need to consider whether the copy/move constructor may throw
        //
        template<typename... Args>
        static void call(R2NoConst* ret, Args... args) noexcept(is_wrapper_nothrow)
        {
            new (ret) R2NoConst(call_fn_impl<R2NoConst>(args...));
        }

        template<typename F2, typename Enable2 = void>
        struct wrapper_fn_ptr
        {
            template<int N, int... S>
            struct impl : impl<N-1, N-1, S...> { };

            template<int... S>
            struct impl<0, S...>
            {
                using type = void(*)(R2NoConst*, InType<S>...) noexcept(is_wrapper_nothrow);
                static constexpr type value = call<InType<S>...>;
            };
        };

        template<typename F2>
        struct wrapper_fn_ptr<F2, typename std::enable_if<(std::is_member_function_pointer<F2>::value)>::type>
        {
            template<int N, int... S>
            struct impl : impl<N-1, N-1, S...> { };

            template<int... S>
            struct impl<0, S...>
            {
                using ClassTypeStar = typename std::add_pointer<typename FnTypeInfo::ClassType>::type;
                using type = void(*)(R2NoConst*, ClassTypeStar, InType<S>...) noexcept(is_wrapper_nothrow);
                static constexpr type value = call<ClassTypeStar, InType<S>...>;
            };
        };
    };

    // If the function returns a primitive or pointer type or void, return directly
    //
    template<typename R2>
    struct call_fn_wrapper<R2, typename std::enable_if<(
            !is_converted_reference_type<R2>::value)>::type>
    {
        template<typename... Args>
        static R2 call(Args... args) noexcept(is_wrapper_nothrow)
        {
            return call_fn_impl<R2>(args...);
        }

        template<typename F2, typename Enable2 = void>
        struct wrapper_fn_ptr
        {
            template<int N, int... S>
            struct impl : impl<N-1, N-1, S...> { };

            template<int... S>
            struct impl<0, S...>
            {
                using type = R2(*)(InType<S>...) noexcept(is_wrapper_nothrow);
                static constexpr type value = call<InType<S>...>;
            };
        };

        template<typename F2>
        struct wrapper_fn_ptr<F2, typename std::enable_if<(std::is_member_function_pointer<F2>::value)>::type>
        {
            template<int N, int... S>
            struct impl : impl<N-1, N-1, S...> { };

            template<int... S>
            struct impl<0, S...>
            {
                using ClassTypeStar = typename std::add_pointer<typename FnTypeInfo::ClassType>::type;
                using type = R2(*)(ClassTypeStar, InType<S>...) noexcept(is_wrapper_nothrow);
                static constexpr type value = call<ClassTypeStar, InType<S>...>;
            };
        };
    };

    // if the function returns lvalue reference, return a pointer instead
    //
    template<typename R2>
    struct call_fn_wrapper<R2, typename std::enable_if<(std::is_reference<R2>::value)>::type>
    {
        static_assert(std::is_lvalue_reference<R2>::value, "function returning rvalue reference is not supported");

        using R2Star = typename std::add_pointer<R2>::type;

        template<typename... Args>
        static R2Star call(Args... args) noexcept(is_wrapper_nothrow)
        {
            return &call_fn_impl<R2>(args...);
        }

        template<typename F2, typename Enable2 = void>
        struct wrapper_fn_ptr
        {
            template<int N, int... S>
            struct impl : impl<N-1, N-1, S...> { };

            template<int... S>
            struct impl<0, S...>
            {
                using type = R2Star(*)(InType<S>...) noexcept(is_wrapper_nothrow);
                static constexpr type value = call<InType<S>...>;
            };
        };

        template<typename F2>
        struct wrapper_fn_ptr<F2, typename std::enable_if<(std::is_member_function_pointer<F2>::value)>::type>
        {
            template<int N, int... S>
            struct impl : impl<N-1, N-1, S...> { };

            template<int... S>
            struct impl<0, S...>
            {
                using ClassTypeStar = typename std::add_pointer<typename FnTypeInfo::ClassType>::type;
                using type = R2Star(*)(ClassTypeStar, InType<S>...) noexcept(is_wrapper_nothrow);
                static constexpr type value = call<ClassTypeStar, InType<S>...>;
            };
        };
    };

    using WrapperFnPtrStructType =
            typename call_fn_wrapper<typename FnTypeInfo::ReturnType>::
                    template wrapper_fn_ptr<FnType>::
                            template impl<FnTypeInfo::numArgs>;

public:
    // The generated function that wraps the fnPtr, hiding all the non-trivial parameters and return values
    //
    using WrapperFnPtrType = typename WrapperFnPtrStructType::type;
    static constexpr WrapperFnPtrType wrapperFn = WrapperFnPtrStructType::value;

    // Whether the wrapper is required. It is required if the wrapped function actually
    // contains at least one non-trivial parameter or return value
    //
    static constexpr bool isWrapperFnRequired = FnTypeInfo::is_wrapper_fn_required();

    // Whether the wrapped function returns a non-primitive type by value.
    // In that case, the ret value is changed to void, and a pointer is added to the first parameter,
    // into which the return value will be in-place constructed.
    // This is essentially the same as the C++ ABI StructRet transform, except we do it in C++ and unconditionally
    // for all non-primitive return types (normally C++ ABI only do this if the returned struct is large)
    //
    static constexpr bool isSretTransformed = FnTypeInfo::isRetValNontriviallyConverted();

    static constexpr bool isWrapperNoExcept = is_wrapper_nothrow;
};

// constructor_wrapper_helper<C, Args...>::wrapperFn
//    A function of prototype void(*)(C* ret, Args... args), which executes 'new (ret) C(args...)'.
//    Non-primitive parameter types are transformed using the same rule as function_wrapper_helper.
//
template<typename C, typename... Args>
class constructor_wrapper_helper
{
private:
    static const size_t numArgs = sizeof...(Args);

    template<size_t i> using ArgType = typename std::tuple_element<i, std::tuple<Args...>>::type;
    template<size_t i> using RemovedRefArgType = typename arg_transform_helper<ArgType<i>>::RemovedRefArgType;

    using SeqType = typename gen_tpl_sequence<numArgs>::type;
    template<int k> using InType = RemovedRefArgType<k>;
    template<int k> using OutType = ArgType<k>;

    // object type parameter (passed in by value)
    //
    template<int k, typename Enable = void>
    struct convert_param_internal
    {
        static_assert(std::is_same<InType<k>, typename std::add_pointer<OutType<k>>::type>::value
                              && !std::is_reference<OutType<k>>::value, "wrong specialization");

        static constexpr bool is_nothrow = std::is_nothrow_copy_constructible<
                typename std::remove_const<OutType<k>>::type>::value;

        // This function returns reference, so it's always noexcept
        //
        using type = OutType<k>&;
        static type get(InType<k> v) noexcept
        {
            return *v;
        }
    };

    // reference type parameter
    //
    template<int k>
    struct convert_param_internal<k, typename std::enable_if<(
            std::is_reference<OutType<k>>::value)>::type>
    {
        static_assert(std::is_lvalue_reference<OutType<k>>::value,
                      "function returning rvalue reference is not supported");
        static_assert(std::is_same<InType<k>, typename std::add_pointer<OutType<k>>::type>::value,
                      "InType should be the pointer type");

        static constexpr bool is_nothrow = true;

        using type = OutType<k>;
        static type get(InType<k> v) noexcept
        {
            return *v;
        }
    };

    // primitive type or pointer type parameter
    //
    template<int k>
    struct convert_param_internal<k, typename std::enable_if<(
                                             !is_converted_reference_type<OutType<k>>::value)>::type>
    {
        static_assert(std::is_same<InType<k>, OutType<k>>::value, "wrong specialization");

        static constexpr bool is_nothrow = true;

        using type = OutType<k>;
        static type get(InType<k> v) noexcept
        {
            return v;
        }
    };

    template<size_t n>
    static constexpr bool is_param_wrappings_nothrow()
    {
        if constexpr(n == 0)
        {
            return true;
        }
        else
        {
            return (convert_param_internal<n-1>::is_nothrow) && is_param_wrappings_nothrow<n-1>();
        }
    }

    static constexpr bool is_all_param_wrapping_nothrow = is_param_wrappings_nothrow<numArgs>();
    static constexpr bool is_constructor_nothrow = std::is_nothrow_constructible<C, Args...>::value;
    static constexpr bool is_wrapper_nothrow = is_all_param_wrapping_nothrow && is_constructor_nothrow;

    template<int k, typename T>
    static typename convert_param_internal<k>::type convert_param(T input) noexcept
    {
        return convert_param_internal<k>::get(std::get<k>(input));
    }

    template<int... S, typename... TArgs>
    static void call_impl(tpl_sequence<S...> /*unused*/, C* ret, TArgs... args) noexcept(is_wrapper_nothrow)
    {
        new (ret) C(convert_param<S>(std::forward_as_tuple(args...))...);
    }

    template<typename... TArgs>
    static void call(C* ret, TArgs... args) noexcept(is_wrapper_nothrow)
    {
        call_impl(SeqType(), ret, args...);
    }

    template<int N, int... S>
    struct impl : impl<N-1, N-1, S...> { };

    template<int... S>
    struct impl<0, S...>
    {
        using type = void(*)(C*, InType<S>...) noexcept(is_wrapper_nothrow);
        static constexpr type value = call<InType<S>...>;
    };

public:
    using WrapperFnPtrType = typename impl<numArgs>::type;
    static constexpr WrapperFnPtrType wrapperFn = impl<numArgs>::value;
    static constexpr bool isWrapperFnNoExcept = is_wrapper_nothrow;
};

template<typename C>
struct destructor_wrapper_helper
{
    using WrapperFnPtrType = void(*)(C*);

    static void wrapperFn(C* c) noexcept
    {
        c->~C();
    }
};

template<typename T>
struct member_object_pointer_traits
{
    static_assert(sizeof(T) == 0, "T is not a pointer to non-static member object!");
};

template<typename R, typename C>
struct member_object_pointer_traits<R C::*>
{
    using DataMemberType = R;
    using ClassName = C;
};

template<auto p>
struct member_object_accessor_wrapper
{
    using PType = decltype(p);
    using R = typename member_object_pointer_traits<PType>::DataMemberType;
    using C = typename member_object_pointer_traits<PType>::ClassName;

    using WrapperFnPtrType = R&(*)(C*);
    static R& wrapperFn(C* c) noexcept
    {
        return (c->*p);
    }
};

struct OutlinedOperatorWrapper
{
    template<typename LHS, typename RHS, AstComparisonExprType op>
    static bool f(LHS lhs, RHS rhs)
    {
        static_assert(std::is_reference<LHS>::value && std::is_reference<RHS>::value);
        if constexpr(op == AstComparisonExprType::EQUAL)
        {
            return lhs == rhs;
        }
        else if constexpr(op == AstComparisonExprType::NOT_EQUAL)
        {
            return lhs != rhs;
        }
        else if constexpr(op == AstComparisonExprType::LESS_THAN)
        {
            return lhs < rhs;
        }
        else if constexpr(op == AstComparisonExprType::LESS_EQUAL)
        {
            return lhs <= rhs;
        }
        else if constexpr(op == AstComparisonExprType::GREATER_THAN)
        {
            return lhs > rhs;
        }
        else
        {
            static_assert(op == AstComparisonExprType::GREATER_EQUAL);
            return lhs >= rhs;
        }
    }

    template<typename C, bool isIncrement>
    static void f(C lhs)
    {
        static_assert(std::is_reference<C>::value);
        if constexpr(isIncrement)
        {
            lhs++;
        }
        else
        {
            lhs--;
        }
    }
};

// get_function_pointer_address(t)
// Returns the void* address of t, where t must be a pointer to a free function or a static or non-static class method
//
template<typename T, typename Enable = void>
struct function_pointer_address_helper
{
    static_assert(sizeof(T) == 0, "T must be a pointer to a free function or a static or non-static class method");
};

template<typename T>
struct function_pointer_address_helper<T, typename std::enable_if<
        std::is_member_function_pointer<T>::value>::type>
{
    static void* get(T t)
    {
        AstTypeHelper::ItaniumMemFnPointer fp(t);
        ReleaseAssert(fp.MayConvertToPlainPointer());
        return fp.GetRawFunctionPointer();
    }
};

template<typename T>
struct function_pointer_address_helper<T, typename std::enable_if<
        std::is_pointer<T>::value && std::is_function<typename std::remove_pointer<T>::type>::value >::type>
{
    static void* get(T t)
    {
        return reinterpret_cast<void*>(t);
    }
};

template<typename T>
void* get_function_pointer_address(T t)
{
    return function_pointer_address_helper<T>::get(t);
}

enum class FunctionType
{
    FreeFn,
    StaticMemberFn,
    NonStaticMemberFn,
    Constructor,
    Destructor,
    TypeInfoObject,
    NonStaticMemberObject,
    OutlineDefinedOverloadedOperator
};

struct RawFnTypeNamesInfo
{
    RawFnTypeNamesInfo(FunctionType fnType,
                       size_t numArgs,
                       const std::pair<const char*, std::pair<bool, bool> >* apiRetAndArgTypenames,
                       const char* const* originalRetAndArgTypenames,
                       const char* classTypename,
                       const char* fnName,
                       void* fnAddress,
                       bool isConst,
                       bool isNoExcept,
                       bool isUsingWrapper,
                       bool isWrapperUsingSret,
                       void* wrapperFnAddress,
                       bool isCopyCtorOrAssignmentOp)
          : m_fnType(fnType)
          , m_numArgs(numArgs)
          , m_apiRetAndArgTypenames(apiRetAndArgTypenames)
          , m_originalRetAndArgTypenames(originalRetAndArgTypenames)
          , m_classTypename(classTypename)
          , m_fnName(fnName)
          , m_fnAddress(fnAddress)
          , m_isConst(isConst)
          , m_isNoExcept(isNoExcept)
          , m_isUsingWrapper(isUsingWrapper)
          , m_isWrapperUsingSret(isWrapperUsingSret)
          , m_wrapperFnAddress(wrapperFnAddress)
          , m_isCopyCtorOrAssignmentOp(isCopyCtorOrAssignmentOp)
    {
        if (!isUsingWrapper) { ReleaseAssert(!isWrapperUsingSret); }
    }

    FunctionType m_fnType;
    // The number of arguments this function takes, not counting 'this' and potential sret
    //
    size_t m_numArgs;
    // The typenames of return value and args
    //    [0] is the typename of ret
    //    [1, m_numArgs] is the typename of the ith argument
    // original: the original C++ defintion
    // transformed: our transformed definition into our supported typesystem
    //
    const std::pair<const char*, std::pair<bool, bool> >* m_apiRetAndArgTypenames;
    const char* const* m_originalRetAndArgTypenames;
    // nullptr if it is a free function or a static class method,
    // otherwise the class typename
    //
    const char* m_classTypename;
    // the name of the function
    //
    const char* m_fnName;
    // the address of the function
    //
    void* m_fnAddress;
    // Whether the function has 'const' attribute (must be member function)
    //
    bool m_isConst;
    // Whether the function has 'noexcept' attribute
    //
    bool m_isNoExcept;

    // Whether we must call into the wrapper function instead of the original function
    //
    bool m_isUsingWrapper;
    // Whether the wrapper function is using the 'sret' attribute
    //
    bool m_isWrapperUsingSret;
    // The address of the wrapper function
    //
    void* m_wrapperFnAddress;
    // Whether this function is a copy constructor or assignment operator
    //
    bool m_isCopyCtorOrAssignmentOp;
};

// get_raw_fn_typenames_info<t>::get()
// Returns the RawFnTypeNamesInfo struct for function pointer 't'.
// 't' must be a pointer to a free function or a static or non-static class method.
//
template<auto t>
struct get_raw_fn_typenames_info
{
    using fnInfo = function_typenames_helper<decltype(t)>;
    using wrapper = function_wrapper_helper<t>;
    using wrappedFnType = typename wrapper::WrapperFnPtrType;

    static constexpr wrappedFnType wrapperFn = wrapper::wrapperFn;

    static const char* get_function_name()
    {
        return __pochivm_stringify_value__<t>();
    }

    static RawFnTypeNamesInfo get(FunctionType fnType)
    {
        ReleaseAssert(fnType != FunctionType::Constructor && fnType != FunctionType::Destructor);
        if (fnType == FunctionType::StaticMemberFn || fnType == FunctionType::FreeFn)
        {
            if (!(std::is_pointer<decltype(t)>::value &&
                  std::is_function<typename std::remove_pointer<decltype(t)>::type>::value))
            {
                fprintf(stderr, "The provided parameter is not a function pointer!\n");
                abort();
            }
        }
        else if (fnType == FunctionType::NonStaticMemberFn)
        {
            if (!std::is_member_pointer<decltype(t)>::value)
            {
                fprintf(stderr, "The provided parameter is not a member function pointer!\n");
                abort();
            }
        }

        return RawFnTypeNamesInfo(fnType,
                                  fnInfo::numArgs,
                                  fnInfo::get_api_ret_and_param_typenames(),
                                  fnInfo::get_original_ret_and_param_typenames(),
                                  fnInfo::get_class_typename(),
                                  get_function_name(),
                                  get_function_pointer_address(t),
                                  fnInfo::is_const(),
                                  fnInfo::is_noexcept(),
                                  wrapper::isWrapperFnRequired,
                                  wrapper::isSretTransformed,
                                  reinterpret_cast<void*>(wrapperFn),
                                  false /*isCopyCtorOrAssignmentOp*/);
    }

    static RawFnTypeNamesInfo get_constructor(size_t numArgs,
                                              const char* class_typename,
                                              const char* const* orig_ret_and_param_typenames,
                                              const std::pair<const char*, std::pair<bool, bool> >* api_ret_and_param_typenames,
                                              bool isNoExcept,
                                              bool isCopyConstructor)
    {
        static_assert(std::is_pointer<decltype(t)>::value &&
                      std::is_function<typename std::remove_pointer<decltype(t)>::type>::value,
                      "[INTERNAL ERROR] The wrapped constructor is not a function pointer!");
        static_assert(!wrapper::isWrapperFnRequired,
                      "[INTERNAL ERROR] The wrapped constructor should not require another wrapper!");
        static_assert(!wrapper::isSretTransformed,
                      "[INTERNAL ERROR] The wrapped constructor should not require sret transform!");

        ReleaseAssert(numArgs == fnInfo::numArgs);
        return RawFnTypeNamesInfo(FunctionType::Constructor,
                                  numArgs,
                                  api_ret_and_param_typenames,
                                  orig_ret_and_param_typenames,
                                  class_typename,
                                  nullptr /*function_name*/,
                                  get_function_pointer_address(t),
                                  false /*is_const*/,
                                  isNoExcept /*is_noexcept*/,
                                  false /*is_wrapper_fn_required*/,
                                  false /*is_sret_transform_required*/,
                                  nullptr /*wrapperFn*/,
                                  isCopyConstructor);
    }

    static RawFnTypeNamesInfo get_destructor(const char* class_typename)
    {
        static_assert(fnInfo::is_noexcept(), "Potentially throwing destructor is not supported.");
        return RawFnTypeNamesInfo(FunctionType::Destructor,
                                  fnInfo::numArgs,
                                  fnInfo::get_api_ret_and_param_typenames(),
                                  fnInfo::get_original_ret_and_param_typenames(),
                                  class_typename,
                                  get_function_name(),
                                  get_function_pointer_address(t),
                                  fnInfo::is_const(),
                                  fnInfo::is_noexcept(),
                                  false /*is_wrapper_fn_required*/,
                                  false /*is_sret_transform_required*/,
                                  nullptr /*wrapperFn*/,
                                  false /*isCopyCtorOrAssignmentOp*/);
    }

    static RawFnTypeNamesInfo get_outlined_comparison_operator(AstComparisonExprType opType)
    {
        static_assert(fnInfo::numArgs == 2);
        std::string* fnName = new std::string("operator");
        if (opType == AstComparisonExprType::EQUAL) {
            *fnName += "==";
        }
        else if (opType == AstComparisonExprType::NOT_EQUAL) {
            *fnName += "!=";
        }
        else if (opType == AstComparisonExprType::LESS_THAN) {
            *fnName += "<";
        }
        else if (opType == AstComparisonExprType::LESS_EQUAL) {
            *fnName += "<=";
        }
        else if (opType == AstComparisonExprType::GREATER_THAN) {
            *fnName += ">";
        }
        else if (opType == AstComparisonExprType::GREATER_EQUAL) {
            *fnName += ">=";
        }
        else
        {
            ReleaseAssert(false);
        }
        return RawFnTypeNamesInfo(FunctionType::OutlineDefinedOverloadedOperator,
                                  fnInfo::numArgs,
                                  fnInfo::get_api_ret_and_param_typenames(),
                                  fnInfo::get_original_ret_and_param_typenames(),
                                  nullptr /*classTypeName*/,
                                  fnName->c_str(),
                                  get_function_pointer_address(t),
                                  fnInfo::is_const(),
                                  fnInfo::is_noexcept(),
                                  false /*is_wrapper_fn_required*/,
                                  false /*is_sret_transform_required*/,
                                  nullptr /*wrapperFn*/,
                                  false /*isCopyCtorOrAssignmentOp*/);
    }

    static RawFnTypeNamesInfo get_outlined_increment_decrement_operator(bool isIncrement)
    {
        static_assert(fnInfo::numArgs == 1);
        std::string* fnName = new std::string("operator");
        if (isIncrement) {
            *fnName += "++";
        } else {
            *fnName += "--";
        }
        return RawFnTypeNamesInfo(FunctionType::OutlineDefinedOverloadedOperator,
                                  fnInfo::numArgs,
                                  fnInfo::get_api_ret_and_param_typenames(),
                                  fnInfo::get_original_ret_and_param_typenames(),
                                  nullptr /*classTypeName*/,
                                  fnName->c_str(),
                                  get_function_pointer_address(t),
                                  fnInfo::is_const(),
                                  fnInfo::is_noexcept(),
                                  false /*is_wrapper_fn_required*/,
                                  false /*is_sret_transform_required*/,
                                  nullptr /*wrapperFn*/,
                                  false /*isCopyCtorOrAssignmentOp*/);
    }

    static RawFnTypeNamesInfo get_member_object(const char* class_typename,
                                                const char* member_object_name)
    {
        static_assert(fnInfo::is_noexcept(), "[INTERNAL ERROR] Member object accessor should be noexcept!");
        static_assert(!wrapper::isWrapperFnRequired,
                      "[INTERNAL ERROR] The wrapped constructor should not require another wrapper!");
        static_assert(!wrapper::isSretTransformed,
                      "[INTERNAL ERROR] The wrapped constructor should not require sret transform!");
        return RawFnTypeNamesInfo(FunctionType::NonStaticMemberObject,
                                  fnInfo::numArgs,
                                  fnInfo::get_api_ret_and_param_typenames(),
                                  fnInfo::get_original_ret_and_param_typenames(),
                                  class_typename,
                                  member_object_name,
                                  get_function_pointer_address(t),
                                  fnInfo::is_const(),
                                  fnInfo::is_noexcept(),
                                  false /*is_wrapper_fn_required*/,
                                  false /*is_sret_transform_required*/,
                                  nullptr /*wrapperFn*/,
                                  false /*isCopyCtorOrAssignmentOp*/);
    }
};

}   // namespace ReflectionHelper

}   // namespace PochiVM

template<typename C> void __pochivm_dummy_register_type_helper(C* /*unused*/) {};

namespace PochiVM {

void __pochivm_report_info__(ReflectionHelper::RawFnTypeNamesInfo*);

template<typename C>
void RegisterDestructor()
{
    static_assert(std::is_same<typename std::remove_cv<C>::type, C>::value && !std::is_reference<C>::value,
            "C must be a non-cv-qualified class type");
    static_assert(std::is_nothrow_destructible<C>::value,
            "Register a class with inaccessible destructor, or potentially throwing destructor is not supported");
    using wrapper_generator_t = ReflectionHelper::destructor_wrapper_helper<C>;
    ReflectionHelper::RawFnTypeNamesInfo info =
            ReflectionHelper::get_raw_fn_typenames_info<wrapper_generator_t::wrapperFn>::get_destructor(
                    ReflectionHelper::class_name_helper_internal<C>::get_class_typename());
    __pochivm_report_info__(&info);
}

// Internal helper: if the function returns an object, register its destructor.
//
template<auto t>
void RegisterDestructorIfNeeded()
{
    using fnInfo = ReflectionHelper::function_typenames_helper<decltype(t)>;
    if constexpr(fnInfo::isRetValNontriviallyConverted())
    {
        using ReturnedClassType = typename ReflectionHelper::arg_transform_helper<typename fnInfo::ReturnType>::ApiArgType;
        RegisterDestructor<ReturnedClassType>();
    }
}

template<auto t>
void RegisterFreeFn()
{
    ReflectionHelper::RawFnTypeNamesInfo info =
            ReflectionHelper::get_raw_fn_typenames_info<t>::get(ReflectionHelper::FunctionType::FreeFn);
    __pochivm_report_info__(&info);
    RegisterDestructorIfNeeded<t>();
}

template<auto t>
void RegisterMemberFn()
{
    ReflectionHelper::RawFnTypeNamesInfo info =
            ReflectionHelper::get_raw_fn_typenames_info<t>::get(ReflectionHelper::FunctionType::NonStaticMemberFn);
    __pochivm_report_info__(&info);
    RegisterDestructorIfNeeded<t>();
}

template<auto t>
void RegisterStaticMemberFn()
{
    ReflectionHelper::RawFnTypeNamesInfo info =
            ReflectionHelper::get_raw_fn_typenames_info<t>::get(ReflectionHelper::FunctionType::StaticMemberFn);
    __pochivm_report_info__(&info);
    RegisterDestructorIfNeeded<t>();
}

template<auto t>
void RegisterMemberObject()
{
    using wrapper_generator_t = ReflectionHelper::member_object_accessor_wrapper<t>;
    ReflectionHelper::RawFnTypeNamesInfo info =
            ReflectionHelper::get_raw_fn_typenames_info<wrapper_generator_t::wrapperFn>::get_member_object(
                    ReflectionHelper::class_name_helper_internal<typename wrapper_generator_t::C>::get_class_typename(),
                    __pochivm_stringify_value__<t>());
    __pochivm_report_info__(&info);
}

template<typename C, typename... Args>
void RegisterConstructor()
{
    static_assert(std::is_constructible<C, Args...>::value, "Invalid constructor parameter types");
    // WARNING: If you change wrapper_t, make sure to change dump_symbols.cpp correspondingly.
    // The generated header currently hardcodes the definition of 'wrapper_t'.
    //
    using wrapper_t = ReflectionHelper::constructor_wrapper_helper<C, Args...>;
    // Check whether this constructor qualifies as copy constructor
    // A copy constructor is a constructor that takes exactly one parameter of type 'C&' or 'const C&'
    //
    bool qualifyForCopyCtor = false;
    if constexpr(sizeof...(Args) == 1)
    {
        using ArgType = typename std::tuple_element<0, std::tuple<Args...>>::type;
        if constexpr(std::is_same<ArgType, const C&>::value || std::is_same<ArgType, C&>::value)
        {
            qualifyForCopyCtor = true;
        }
    }
    ReflectionHelper::RawFnTypeNamesInfo info =
            ReflectionHelper::get_raw_fn_typenames_info<wrapper_t::wrapperFn>::get_constructor(
                    sizeof...(Args) + 1 /*numArgs*/,
                    ReflectionHelper::class_name_helper_internal<C>::get_class_typename(),
                    ReflectionHelper::function_typenames_helper_internal<
                            void /*ret*/, C* /*firstParam*/, Args...>::get_original_ret_and_param_typenames(),
                    ReflectionHelper::function_typenames_helper_internal<
                            void /*ret*/, C* /*firstParam*/, Args...>::get_api_ret_and_param_typenames(),
                    wrapper_t::isWrapperFnNoExcept,
                    qualifyForCopyCtor);
    __pochivm_report_info__(&info);
    RegisterDestructor<C>();
}

template<typename C>
void RegisterExceptionObjectType()
{
    static_assert(!std::is_same<C, void>::value, "Cannot throw void expression");
    static_assert(!std::is_reference<C>::value, "An exception object must not be a reference type");
    static_assert(!std::is_rvalue_reference<C>::value, "An exception object must not be a rvalue-reference type");
    static_assert(!std::is_const<C>::value, "top-level const-qualifier in an exception object has no effect. Please remove it.");
    static_assert(!std::is_volatile<C>::value, "volatile-qualifier is not supported");

    // We cannot use an indirection (e.g. a C++ function wrapper) to get the address of the
    // typeinfo object, since LLVM landingpad instruction must be the first instruction in the block.
    // So we have to add a special ReflectionHelper::FunctionType and get the symbol name of the object directly.
    //
    const std::type_info& t = typeid(C);
    void* addr = const_cast<void*>(static_cast<const void*>(&t));
    ReflectionHelper::RawFnTypeNamesInfo info(
                ReflectionHelper::FunctionType::TypeInfoObject,
                0 /*numArgs*/, nullptr /*apiRetAndParams*/, nullptr /*originalRetAndParams*/,
                ReflectionHelper::class_name_helper_internal<C>::get_class_typename() /*className*/,
                nullptr /*functionName*/, addr /*functionAddress*/, false /*isConst*/, false /*isNoExcept*/,
                false /*is_wrapper_fn_required*/, false /*is_sret_transform_required*/, nullptr /*wrapperFn*/,
                false /*isCopyCtorOrAssignmentOp*/);
    __pochivm_report_info__(&info);
    if constexpr(!std::is_pointer<C>::value && !ReflectionHelper::is_primitive_type<C>::value)
    {
        RegisterDestructor<C>();
    }
    // Make sure we have access to the LLVM type of C as well.
    //
    RegisterFreeFn<&__pochivm_dummy_register_type_helper<C>>();
}

template<typename LHS, typename RHS, AstComparisonExprType op>
void RegisterOutlineDefinedOverloadedOperator()
{
    constexpr auto f = ReflectionHelper::OutlinedOperatorWrapper::f<const LHS&, const RHS&, op>;
    ReflectionHelper::RawFnTypeNamesInfo info =
            ReflectionHelper::get_raw_fn_typenames_info<f>::get_outlined_comparison_operator(op);
    __pochivm_report_info__(&info);

    if constexpr(!std::is_pointer<LHS>::value && !ReflectionHelper::is_primitive_type<LHS>::value)
    {
        RegisterDestructor<LHS>();
    }
    if constexpr(!std::is_pointer<RHS>::value && !ReflectionHelper::is_primitive_type<RHS>::value)
    {
        RegisterDestructor<RHS>();
    }
}

template<typename C, bool isIncrement>
void RegisterOutlineIncrementOrDecrementOperator()
{
    constexpr auto f = ReflectionHelper::OutlinedOperatorWrapper::f<C&, isIncrement>;
    ReflectionHelper::RawFnTypeNamesInfo info =
            ReflectionHelper::get_raw_fn_typenames_info<f>::get_outlined_increment_decrement_operator(isIncrement);
    __pochivm_report_info__(&info);

    if constexpr(!std::is_pointer<C>::value && !ReflectionHelper::is_primitive_type<C>::value)
    {
        RegisterDestructor<C>();
    }
}

}   // namespace PochiVM
