#pragma once

// TODO: header guard

#include "constexpr_array_concat_helper.h"

// Returns something like
//    const char *__pochivm_stringify_type__() [T = ###### ]
// where ###### is the interesting part
// WARNING: this breaks down when called outside a function.
//
template<typename T>
constexpr const char* __pochivm_stringify_type__()
{
    const char* const p = __PRETTY_FUNCTION__;
    return p;
}

// When v is a function pointer or member function pointer, returns something like
//    const char *__pochivm_stringify_value__() [v = &###### ]
// where ###### is the interesting part
// WARNING: this breaks down when called outside a function.
//
template<auto v>
constexpr const char* __pochivm_stringify_value__()
{
    const char* const p = __PRETTY_FUNCTION__;
    return p;
}

namespace PochiVM
{

namespace ReflectionHelper
{

// transform_param_type<T>::type
// Transform a C++-type to a type that we support
//    Transform reference to pointer (e.g. 'int&' param becomes 'int*')
//    Lockdown rvalue-reference (e.g. 'int&&')
//    Drop const-qualifier recursively ('const int* const*' becomes 'int**')
//    Lockdown volatile-qualifier
//
template<typename T>
struct remove_param_type_cv_internal {
    using type = T;
};

template<typename T>
struct remove_param_type_cv_internal<const T> {
    using type = T;
};

template<typename T>
struct remove_param_type_cv_internal<volatile T> {
    static_assert(sizeof(T) == 0, "Function with volatile parameter is not supported!");
};

template<typename T>
struct remove_param_type_cv_internal<const volatile T> {
    static_assert(sizeof(T) == 0, "Function with volatile parameter is not supported!");
};

template<typename T>
struct transform_param_type {
    using type = typename remove_param_type_cv_internal<T>::type;
};

template<typename T>
struct transform_param_type<T&> {
    using type = typename std::add_pointer<
                         typename transform_param_type<
                                 typename remove_param_type_cv_internal<T>::type
                         >::type
                 >::type;
};

template<typename T>
struct transform_param_type<T*> {
    using type = typename std::add_pointer<
                         typename transform_param_type<
                                 typename remove_param_type_cv_internal<T>::type
                         >::type
                 >::type;
};

template<typename T>
struct transform_param_type<T&&> {
    static_assert(sizeof(T) == 0, "Function with rvalue-reference parameter is not supported!");
};

template<typename R, typename... Args>
struct function_typenames_helper_internal
{
    static const size_t numArgs = sizeof...(Args);

    using ReturnType = R;
    using TransformedReturnType = typename transform_param_type<R>::type;

    template<size_t i> using ArgType = typename std::tuple_element<i, std::tuple<Args...>>::type;
    template<size_t i> using TransformedArgType = typename transform_param_type<ArgType<i>>::type;

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
    struct build_transformed_typenames_array_internal
    {
        static constexpr std::array<const char*, n+1> get()
        {
            return PochiVM::AstTypeHelper::constexpr_std_array_concat(
                    build_transformed_typenames_array_internal<n-1>::get(),
                    std::array<const char*, 1>{
                            __pochivm_stringify_type__<TransformedArgType<n-1>>() });
        }
    };

    template<size_t n>
    struct build_transformed_typenames_array_internal<n, typename std::enable_if<(n == 0)>::type>
    {
        static constexpr std::array<const char*, n+1> get()
        {
            return std::array<const char*, 1>{ __pochivm_stringify_type__<TransformedReturnType>() };
        }
    };

    static const char* const* get_transformed_ret_and_param_typenames()
    {
        static constexpr std::array<const char*, numArgs + 1> data =
                build_transformed_typenames_array_internal<numArgs>::get();
        return data.data();
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
};

template<typename R, typename... Args>
struct function_typenames_helper<R(*)(Args...) noexcept>
    : function_typenames_helper_internal<R, Args...>
    , return_nullptr_class_typename
{
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
{ };

template<typename R, typename C, typename... Args>
struct function_typenames_helper<R(C::*)(Args...) noexcept>
    : function_typenames_helper_internal<R, Args...>
    , class_name_helper_internal<C>
{ };

template<typename R, typename C, typename... Args>
struct function_typenames_helper<R(C::*)(Args...) const>
    : function_typenames_helper_internal<R, Args...>
    , class_name_helper_internal<C>
{ };

template<typename R, typename C, typename... Args>
struct function_typenames_helper<R(C::*)(Args...) const noexcept>
    : function_typenames_helper_internal<R, Args...>
    , class_name_helper_internal<C>
{ };

// get_function_pointer_address(t)
// Returns the void* address of t, where t must be a pointer to a free function or a static or non-static class method
//
template<typename T, typename Enable = void>
struct function_pointer_address_helper
{
    static_assert(sizeof(T) == 0, "T must be a pointer to a free function or a static or non-static class method");
};

template <typename MethPtr>
void* GetClassMethodPtrHelper(MethPtr p)
{
    union U { MethPtr meth; void* ptr; };
    return (reinterpret_cast<U*>(&p))->ptr;
}

template<typename T>
struct function_pointer_address_helper<T, typename std::enable_if<
        std::is_member_function_pointer<T>::value>::type>
{
    static void* get(T t)
    {
        return GetClassMethodPtrHelper(t);
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
    NonStaticMemberFn
};

struct RawFnTypeNamesInfo
{
    RawFnTypeNamesInfo(FunctionType fnType,
                       size_t numArgs,
                       const char* const* transformedRetAndArgTypenames,
                       const char* const* originalRetAndArgTypenames,
                       const char* classTypename,
                       const char* fnName,
                       void* fnAddress)
          : m_fnType(fnType)
          , m_numArgs(numArgs)
          , m_transformedRetAndArgTypenames(transformedRetAndArgTypenames)
          , m_originalRetAndArgTypenames(originalRetAndArgTypenames)
          , m_classTypename(classTypename)
          , m_fnName(fnName)
          , m_fnAddress(fnAddress)
    { }

    FunctionType m_fnType;
    // The number of arguments this function takes
    //
    size_t m_numArgs;
    // The typenames of return value and args
    //    [0] is the typename of ret
    //    [1, m_numArgs] is the typename of the ith argument
    // original: the original C++ defintion
    // transformed: our transformed definition into our supported typesystem
    //
    const char* const* m_transformedRetAndArgTypenames;
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
};

// get_raw_fn_typenames_info<t>::get()
// Returns the RawFnTypeNamesInfo struct for function pointer 't'.
// 't' must be a pointer to a free function or a static or non-static class method.
//
template<auto t>
struct get_raw_fn_typenames_info : function_typenames_helper<decltype(t)>
{
    using base = function_typenames_helper<decltype(t)>;

    static const char* get_function_name()
    {
        return __pochivm_stringify_value__<t>();
    }

    static RawFnTypeNamesInfo get(FunctionType fnType)
    {
        if (fnType == FunctionType::StaticMemberFn || fnType == FunctionType::FreeFn)
        {
            if (!(std::is_pointer<decltype(t)>::value &&
                  std::is_function<typename std::remove_pointer<decltype(t)>::type>::value))
            {
                fprintf(stderr, "The provided parameter is not a function pointer!\n");
                abort();
            }
        }
        else
        {
            if (!std::is_member_pointer<decltype(t)>::value)
            {
                fprintf(stderr, "The provided parameter is not a member function pointer!\n");
                abort();
            }
        }
        return RawFnTypeNamesInfo(fnType,
                                  base::numArgs,
                                  base::get_transformed_ret_and_param_typenames(),
                                  base::get_original_ret_and_param_typenames(),
                                  base::get_class_typename(),
                                  get_function_name(),
                                  get_function_pointer_address(t));
    }
};

}   // namespace ReflectionHelper

void __pochivm_report_info__(ReflectionHelper::RawFnTypeNamesInfo*);

template<auto t>
void RegisterFreeFn()
{
    ReflectionHelper::RawFnTypeNamesInfo info =
            ReflectionHelper::get_raw_fn_typenames_info<t>::get(ReflectionHelper::FunctionType::FreeFn);
    __pochivm_report_info__(&info);
}

template<auto t>
void RegisterMemberFn()
{
    ReflectionHelper::RawFnTypeNamesInfo info =
            ReflectionHelper::get_raw_fn_typenames_info<t>::get(ReflectionHelper::FunctionType::NonStaticMemberFn);
    __pochivm_report_info__(&info);
}

template<auto t>
void RegisterStaticMemberFn()
{
    ReflectionHelper::RawFnTypeNamesInfo info =
            ReflectionHelper::get_raw_fn_typenames_info<t>::get(ReflectionHelper::FunctionType::StaticMemberFn);
    __pochivm_report_info__(&info);
}

}   // namespace PochiVM
