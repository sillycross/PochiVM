#pragma once

// This file should only be included by fastinterp_tpl_*.cpp or build_fast_interp_lib.cpp
//
#ifndef POCHIVM_INSIDE_FASTINTERP_TPL_CPP
#ifndef POCHIVM_INSIDE_BUILD_FASTINTERP_LIB_CPP
#ifndef POCHIVM_INSIDE_METAVAR_UNIT_TEST_CPP
static_assert(false, "This file should only be included by fastinterp_tpl_*.cpp or build_fast_interp_lib.cpp");
#endif
#endif
#endif

// Most of the PochiVM headers indirectly depend on this file
// Be very careful if you want to add a include here
//
#include "pochivm/common.h"
#include "pochivm/reflective_stringify_helper.h"
#include "pochivm/for_each_primitive_type.h"
#include "fastinterp_tpl_opaque_params.h"
#include "fastinterp_boilerplate_allowed_shapes.h"

namespace PochiVM
{

// The MetaVar Framework is intended to solve the following problem:
//   Given
//     (1) Definition of a list of sets S_1, S_2, ..., S_n,
//         with each 'S_i' being either a finite set of types, or a finite set of integral values.
//         Let 'L' be the domain S_1 x S_2 x ... x S_n
//     (2) A constexpr constraint function 'cond': L -> bool
//     (3) A templated function f<L> which is instantiatable for l in L if cond(l) is true
//
//   We want to get a list of <l, f<l>> for all l in L that satisfies cond(l).
//   This allows us to O(1)-select f<l> based on *runtime-known* value of 'l'.
//
//   This functionality is similar to the 'SelectTemplatedFnImplGeneric' in 'ast_type_helper.h',
//   except that this functionality is intended for printing generated C++ headers in a custom build step,
//   instead of in-place generation of helper functions (like 'SelectTemplatedFnImplGeneric' does).
//   So it is more generic (allowing selecting on value-parameter as well),
//   and it also allows any complicated offline transformation of f<l>.
//
//   For now, it completely disallows selecting on C++-type or pointers to C++ types, since we don't need it.
//
//   Since this utlity is only used in a build step to generate C++ header files, performance is not a issue.
//
// Usage:
// (1) Defining each 'MetaVar' S_i (the 'name' parameter is only for informational purposes):
//     (a) Given enum class E { a, b, c, X_END_OF_ENUM };
//         CreateEnumMetaVar<E::X_END_OF_ENUM>(name) defines a S that consists of all values in the enum E (except X_END_OF_ENUM)
//     (b) CreateBoolMetaVar(name) defines a S that consists of { false, true }
//     (c) CreateTypeMetaVar(name) defines a S that consists of all C++ fundamental types, pointer to fundamental types, and void**
//     All type-metavars must show up before all non-type-metavars.
// (2) Define a Materializer. It should provide
//     (a) a templated static member function 'template<....> bool cond()'
//         where each template parameter is enum class / bool / typename corresponding to the type of the metavar
//     (b) a templated static member function 'template<....> f' which shall be instantiatable if cond(l) is true.
// (3) Call CreateMetaVarList(....).Materialize<Materializer>(), which will return the desired list of <l, f<l>>.
//

enum class MetaVarType
{
    // Enumerate through all fundamental types, pointers to fundamental types, and void**
    // C++ types, pointers to C++ types, or types with >=2 layers of pointers (except void**) is not enumerated.
    PRIMITIVE_TYPE,
    // Enumerate through { false, true }
    BOOL,
    // Enumerate through all values in a enum class.
    ENUM
};

class MetaVar
{
public:
    // the type of this metavar
    //
    MetaVarType m_type;
    // the name of this metavar, for informational purpose only
    //
    const char* m_name;
    // the name of the enum if the metavar is a enum type
    // it is a string obtained from __pochivm_stringify_type__
    //
    const char* m_enum_typename;
    // the upper bound of the enum (exclusive)
    //
    int m_enum_bound;

protected:
    MetaVar() = default;
};

// A not type-erased metavar, preserving the type and enum bound in constexpr form
//
template<auto v>
class TypedMetaVar : public MetaVar
{
public:
    template<auto enumValueRangeExclusive>
    friend TypedMetaVar<enumValueRangeExclusive> CreateEnumMetaVar(const char* name);
    friend TypedMetaVar<MetaVarType::PRIMITIVE_TYPE> CreateTypeMetaVar(const char* name);
    friend TypedMetaVar<MetaVarType::BOOL> CreateBoolMetaVar(const char* name);
    template<int maxIntegralParamsInclusive>
    friend TypedMetaVar<static_cast<FINumOpaqueIntegralParams>(maxIntegralParamsInclusive + 1)> CreateOpaqueIntegralParamsLimit();
    template<int maxFloatingParamsInclusive>
    friend TypedMetaVar<static_cast<FINumOpaqueFloatingParams>(maxFloatingParamsInclusive + 1)> CreateOpaqueFloatParamsLimit();

private:
    TypedMetaVar() = default;
};

// Create a ENUM MetaVar, with enum value range [0, enumValueRangeExclusive)
// All enum values in [0, enumValueRangeExclusive) will be enumerated
//
template<auto enumValueRangeExclusive>
TypedMetaVar<enumValueRangeExclusive> CreateEnumMetaVar(const char* name)
{
    using EnumType = decltype(enumValueRangeExclusive);
    static_assert(std::is_enum<EnumType>::value, "Provided type is not an enum type");
    static_assert(!std::is_same<EnumType, MetaVarType>::value, "Cannot use MetaVarType as enum type");
    TypedMetaVar<enumValueRangeExclusive> ret;
    ret.m_type = MetaVarType::ENUM;
    ret.m_name = name;
    ret.m_enum_bound = static_cast<int>(enumValueRangeExclusive);
    ReleaseAssert(ret.m_enum_bound > 0);
    ret.m_enum_typename = __pochivm_stringify_type__<EnumType>();
    return ret;
}

template<int maxIntegralParamsInclusive = x_fastinterp_max_integral_params>
TypedMetaVar<static_cast<FINumOpaqueIntegralParams>(maxIntegralParamsInclusive + 1)> CreateOpaqueIntegralParamsLimit()
{
    static_assert(0 <= maxIntegralParamsInclusive && maxIntegralParamsInclusive <= x_fastinterp_max_integral_params);
    TypedMetaVar<static_cast<FINumOpaqueIntegralParams>(maxIntegralParamsInclusive + 1)> ret;
    ret.m_type = MetaVarType::ENUM;
    ret.m_name = "numOpaqueIntegralParams";
    ret.m_enum_bound = maxIntegralParamsInclusive + 1;
    ReleaseAssert(ret.m_enum_bound > 0);
    ret.m_enum_typename = __pochivm_stringify_type__<FINumOpaqueIntegralParams>();
    return ret;
}

template<int maxFloatingParamsInclusive = x_fastinterp_max_floating_point_params>
TypedMetaVar<static_cast<FINumOpaqueFloatingParams>(maxFloatingParamsInclusive + 1)> CreateOpaqueFloatParamsLimit()
{
    static_assert(0 <= maxFloatingParamsInclusive && maxFloatingParamsInclusive <= x_fastinterp_max_floating_point_params);
    TypedMetaVar<static_cast<FINumOpaqueFloatingParams>(maxFloatingParamsInclusive + 1)> ret;
    ret.m_type = MetaVarType::ENUM;
    ret.m_name = "numOpaqueFloatingParams";
    ret.m_enum_bound = maxFloatingParamsInclusive + 1;
    ReleaseAssert(ret.m_enum_bound > 0);
    ret.m_enum_typename = __pochivm_stringify_type__<FINumOpaqueFloatingParams>();
    return ret;
}

// Create a MetaVar enumerating types
//
TypedMetaVar<MetaVarType::PRIMITIVE_TYPE> CreateTypeMetaVar(const char* name)
{
    TypedMetaVar<MetaVarType::PRIMITIVE_TYPE> ret;
    ret.m_type = MetaVarType::PRIMITIVE_TYPE;
    ret.m_name = name;
    ret.m_enum_typename = nullptr;
    return ret;
}

// Create a MetaVar enumerating false and true
//
TypedMetaVar<MetaVarType::BOOL> CreateBoolMetaVar(const char* name)
{
    TypedMetaVar<MetaVarType::BOOL> ret;
    ret.m_type = MetaVarType::BOOL;
    ret.m_name = name;
    ret.m_enum_typename = nullptr;
    return ret;
}

// A single instance of <l, f<l>>
//
struct MetaVarMaterializedInstance
{
    // The value of 'l'. Enum, bool or TypeId can all be converted to uint64_t,
    // allowing us to simply store it in a std::vector<uint64_t>
    //
    std::vector<uint64_t> m_values;
    // The resulted function pointer f<l>
    //
    void* m_fnPtr;
};

// The whole materialized list of <l, f<l>>
//
struct MetaVarMaterializedList
{
    std::vector<MetaVar> m_metavars;
    std::vector<MetaVarMaterializedInstance> m_instances;
};

namespace internal
{

struct PartialMetaVarValueInstance
{
    std::vector<uint64_t> value;

    // Compiler should not inline this function, inlining it bloats up the code by too much,
    // and significantly slows down compilation.
    //
    PartialMetaVarValueInstance __attribute__((__noinline__)) Push(uint64_t v) const
    {
        PartialMetaVarValueInstance ret = *this;
        ret.value.push_back(v);
        return ret;
    }
};

// metavar_has_cond_fn<T, TArgs...>::impl<VArgs...>::value
//     true if either
//     (1) T does NOT have cond<TArgs..., VArgs>
//     (2) T has cond<TArgs..., VArgs...> and T::cond<TArgs..., VArgs...>() returned true
// Note: a non-templated 'cond' cannot be detected
//
// Member function existence SFINAE is modified from
//     https://stackoverflow.com/questions/257288/templated-check-for-the-existence-of-a-class-member-function
//
template<typename T, typename... TArgs>
struct metavar_prefix_cond_fn_checker
{
    template<auto... VArgs>
    struct impl3
    {
        // Member function existence SFINAE
        //
        struct has_cond
        {
            typedef char one;
            struct two { char x[2]; };

            template <typename C> static one test( decltype(&C::template cond<TArgs..., VArgs...>) );
            template <typename C> static two test(...);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
            constexpr static bool value = (sizeof(test<T>(0)) == sizeof(char));
#pragma clang diagnostic pop
        };

        template<typename Dummy, typename Enable = void>
        struct impl4
        {
            // the case that the 'cond' function does not exist
            //
            struct impl5 : std::true_type {};
        };

        template<typename Dummy>
        struct impl4<Dummy, typename std::enable_if<(std::is_same<Dummy, void>::value && has_cond::value)>::type>
        {
            // the case that 'cond' function exists
            //
            struct impl5 : std::integral_constant<bool, (T::template cond<TArgs..., VArgs...>())> {};
        };
    };

    template<auto... VArgs>
    using impl = typename impl3<VArgs...>::template impl4<void>::impl5;
};

template<typename... >
struct type_tpl_sequence {};

template<typename T, int N, typename... S>
struct gen_type_tpl_sequence : gen_type_tpl_sequence<T, N-1, T, S...> {};

template<typename T, typename... S>
struct gen_type_tpl_sequence<T, 0, S...>
{
    using type = type_tpl_sequence<S...>;
};

template<typename T, int N, typename C>
struct append_type_tpl_sequence_front;

template<typename T, int N, typename... TS1>
struct append_type_tpl_sequence_front<T, N, type_tpl_sequence<TS1...>>
{
    using type = typename gen_type_tpl_sequence<T, N, TS1...>::type;
};

template<typename T1, int N1, typename T2, int N2>
using type_tpl_sequence_t = typename append_type_tpl_sequence_front<T1, N1, typename gen_type_tpl_sequence<T2, N2>::type>::type;

template<typename C>
struct metavar_get_user_fn_helper;

template<typename... TSeq>
struct metavar_get_user_fn_helper<type_tpl_sequence<TSeq...>>
{
    template<typename Materializer, typename... TArgs>
    struct impl
    {
        template<auto... VArgs>
        struct impl2
        {
            using FnType = decltype(Materializer::template f<TArgs..., VArgs..., TSeq...>);
            static void* get()
            {
                return reinterpret_cast<void*>(Materializer::template f<TArgs..., VArgs..., TSeq...>);
            }
        };
    };
};

template<typename Materializer, auto... metaVarTypes>
struct metavar_materialize_helper
{
    template<int numBtParams, int numFpParams, auto... remainingMetaVarTypes>
    struct impl
    {
        // This class is only used for empty parameter pack
        // All other cases are specialized
        //
        static_assert(sizeof...(remainingMetaVarTypes) == 0, "unexpected");

        template<typename... TArgs>
        struct impl2
        {
            template<auto... VArgs>
            struct impl3
            {
                template<typename Dummy, typename Enable = void>
                struct impl4
                {
                    static void invoke(MetaVarMaterializedList* /*result*/, const PartialMetaVarValueInstance& /*instance*/)
                    { }
                };

                template<typename Dummy>
                struct impl4<Dummy, typename std::enable_if<(
                        std::is_same<Dummy, void>::value &&
                        std::integral_constant<bool, (Materializer::template cond<TArgs..., VArgs...>())>::value)>::type>
                {

                    // This function is not performance-sensitive, but compile-time sensitive (we are going to compile
                    // tens of thousands of these but they are only executed once and at build phase), tell compiler to not optimize
                    //
                    static void __attribute__((__optnone__, __noinline__)) invoke(MetaVarMaterializedList* result,
                                                                                  const PartialMetaVarValueInstance& instance)
                    {
                        using UserFnGetter = typename metavar_get_user_fn_helper<type_tpl_sequence_t<uint64_t, numBtParams, double, numFpParams>>
                            ::template impl<Materializer, TArgs...>::template impl2<VArgs...>;

                        ReleaseAssert(instance.value.size() == result->m_metavars.size());
                        MetaVarMaterializedInstance inst;
                        inst.m_values = instance.value;
                        /*
                        using FnType = decltype(Materializer::template f<TArgs..., VArgs...>);
                        static_assert(is_allowed_boilerplate_shape<FnType>::value,
                                "'f' is not among the allowed shapes of boilerplate functions. "
                                "If you need a new shape, put it in fastinterp_boilerplate_allowed_shapes.h.");
                        */
                        inst.m_fnPtr = UserFnGetter::get();
                        result->m_instances.push_back(inst);
                    }
                };
            };
        };
    };

    static void materialize(MetaVarMaterializedList* result)
    {
        PartialMetaVarValueInstance inst;
        impl<0 /*numBtParams*/, 0 /*numFpParams*/, metaVarTypes...>::template impl2<>::template impl3<>::template impl4<void>::invoke(result, inst);
    }
};

// Must be kept in sync with the order of TypeId
//
enum class MVTypeIdLabelHelper
{
    Enum_void
#define F(type) , Enum_ ## type
FOR_EACH_PRIMITIVE_TYPE
#undef F
};

template<typename Materializer, auto... metaVarTypes>
template<int numBtParams, int numFpParams, auto first, auto... remainingMetaVarTypes>
struct metavar_materialize_helper<Materializer, metaVarTypes...>::impl<numBtParams, numFpParams, first, remainingMetaVarTypes...>
{
    template<typename... TArgs>
    struct impl2
    {
        template<auto... VArgs>
        struct impl3
        {
            template<typename Dummy, typename Enable = void>
            struct impl4
            {
                // Enable case (prefix 'cond' does not exist, or 'cond' returned true')
                //

                // This compiler should optimize this function, since it is only a simple tail-recursion optimization,
                // and it significantly reduces the number of symbols to resolve, so the build-time-JIT is significantly (~3x) faster.
                //
                template<typename T, int lb, int ub>
                static void invoke_enum(MetaVarMaterializedList* result, const PartialMetaVarValueInstance& instance)
                {
                    if constexpr(lb + 1 == ub)
                    {
                        constexpr T value = static_cast<T>(lb);
                        if constexpr(std::is_same<T, FINumOpaqueIntegralParams>::value)
                        {
                            impl<lb, numFpParams, remainingMetaVarTypes...>::template impl2<TArgs...>::template impl3<VArgs..., value>::template impl4<void>::invoke(
                                        result, instance.Push(static_cast<uint64_t>(lb)));
                        }
                        else if constexpr(std::is_same<T, FINumOpaqueFloatingParams>::value)
                        {
                            impl<numBtParams, lb, remainingMetaVarTypes...>::template impl2<TArgs...>::template impl3<VArgs..., value>::template impl4<void>::invoke(
                                        result, instance.Push(static_cast<uint64_t>(lb)));
                        }
                        else
                        {
                            impl<numBtParams, numFpParams, remainingMetaVarTypes...>::template impl2<TArgs...>::template impl3<VArgs..., value>::template impl4<void>::invoke(
                                        result, instance.Push(static_cast<uint64_t>(lb)));
                        }
                    }
                    else
                    {
                        // We have to use a binary recursive strategy to reduce recursion depth to logrithmic...
                        // or clang crashes.....
                        //
                        constexpr int mid = (lb + ub) / 2;
                        invoke_enum<T, lb, mid>(result, instance);
                        invoke_enum<T, mid, ub>(result, instance);
                    }
                }

                // This function is not performance-sensitive, but compile-time sensitive (we are going to compile
                // tens of thousands of these but they are only executed once and at build phase), tell compiler to not optimize
                //
                static void  __attribute__((__optnone__, __noinline__)) invoke(MetaVarMaterializedList* result,
                                                                               const PartialMetaVarValueInstance& instance)
                {
                    using T = decltype(first);
                    if constexpr(std::is_same<T, MetaVarType>::value)
                    {
                        if constexpr(first == MetaVarType::BOOL)
                        {
                            impl<numBtParams, numFpParams, remainingMetaVarTypes...>::template impl2<TArgs...>::template impl3<VArgs..., false>::template impl4<void>::invoke(
                                        result, instance.Push(false));
                            impl<numBtParams, numFpParams, remainingMetaVarTypes...>::template impl2<TArgs...>::template impl3<VArgs..., true>::template impl4<void>::invoke(
                                        result, instance.Push(true));
                        }
                        else if constexpr(first == MetaVarType::PRIMITIVE_TYPE)
                        {
                            // DEVNOTE: hardcoded from ast_type_helper.h! Make sure to keep in sync!
                            //
                            constexpr uint64_t x_typeid_pointer_typeid_inc = 1000000000;
#define F(type)                                                                                                                \
    impl<numBtParams, numFpParams, remainingMetaVarTypes...>::template impl2<TArgs..., type>::template impl3<VArgs...>::template impl4<void>::invoke(    \
            result, instance.Push(static_cast<uint64_t>(MVTypeIdLabelHelper::Enum_ ## type)));
                            F(void)
                            FOR_EACH_PRIMITIVE_TYPE
#undef F
#define F(type)                                                                                                                 \
    impl<numBtParams, numFpParams, remainingMetaVarTypes...>::template impl2<TArgs..., type*>::template impl3<VArgs...>::template impl4<void>::invoke(    \
            result, instance.Push(static_cast<uint64_t>(MVTypeIdLabelHelper::Enum_ ## type) + x_typeid_pointer_typeid_inc));
                             F(void)
                             FOR_EACH_PRIMITIVE_TYPE
#undef F
                             impl<numBtParams, numFpParams, remainingMetaVarTypes...>::template impl2<TArgs..., void**>::template impl3<VArgs...>::template impl4<void>::invoke(
                                result, instance.Push(static_cast<uint64_t>(MVTypeIdLabelHelper::Enum_void) + 2 * x_typeid_pointer_typeid_inc));
                        }
                        else
                        {
                            static_assert(type_dependent_false<Materializer>::value, "Unexpected MetaVarType");
                        }
                    }
                    else
                    {
                        static_assert(std::is_enum<T>::value, "Unexpected MetaVarType");
                        constexpr int ub = static_cast<int>(first);
                        static_assert(ub > 0);
                        invoke_enum<T, 0, ub>(result, instance);
                    }
                }
            };

            template<typename Dummy>
            struct impl4<Dummy, typename std::enable_if<(
                    std::is_same<Dummy, void>::value &&
                    !metavar_prefix_cond_fn_checker<Materializer, TArgs...>::template impl<VArgs...>::value)>::type>
            {
                // Disabled case: prefix 'cond' exists and returned false
                // Do nothing.
                //
                static void invoke(MetaVarMaterializedList* /*result*/, const PartialMetaVarValueInstance& /*instance*/)
                { }
            };
        };
    };
};

}   // namespace internal

template<auto... metaVarTypes>
class TypedMetaVarList
{
public:
    template<auto... _metaVarTypes>
    friend TypedMetaVarList<_metaVarTypes...> CreateMetaVarList(TypedMetaVar<_metaVarTypes>... metavars);

    template<typename Materializer>
    MetaVarMaterializedList Materialize()
    {
        MetaVarMaterializedList result;
        result.m_metavars = m_mvl;
        internal::metavar_materialize_helper<Materializer, metaVarTypes...>::materialize(&result);
        return result;
    }

private:
    template<bool, bool, bool>
    void SanityCheck()
    { }

    template<bool seenValue, bool seenNumIntegralParams, bool seenNumFloatParams, auto first, auto... remainingMetaVarTypes>
    void SanityCheck()
    {
        using Type = decltype(first);
        if constexpr(std::is_same<Type, MetaVarType>::value)
        {
            if constexpr(first == MetaVarType::PRIMITIVE_TYPE)
            {
                static_assert(!seenValue, "All type-metavar must precede all value-metavar");
                ReleaseAssert(!seenValue);
                SanityCheck<seenValue, seenNumIntegralParams, seenNumFloatParams, remainingMetaVarTypes...>();
            }
            else
            {
                static_assert(first == MetaVarType::BOOL, "unexpected metavar param");
                SanityCheck<true, seenNumIntegralParams, seenNumFloatParams, remainingMetaVarTypes...>();
            }
        }
        else if constexpr(std::is_same<Type, FINumOpaqueIntegralParams>::value)
        {
            static_assert(!seenNumIntegralParams, "can at most specify numIntegralParamsLimit once");
            ReleaseAssert(!seenNumIntegralParams);
            SanityCheck<true, true, seenNumFloatParams, remainingMetaVarTypes...>();
        }
        else if constexpr(std::is_same<Type, FINumOpaqueFloatingParams>::value)
        {
            static_assert(!seenNumFloatParams, "can at most specify numFloatParamsLimit once");
            ReleaseAssert(!seenNumFloatParams);
            SanityCheck<true, seenNumIntegralParams, true, remainingMetaVarTypes...>();
        }
        else
        {
            SanityCheck<true, seenNumIntegralParams, seenNumFloatParams, remainingMetaVarTypes...>();
        }
    }

    void BuildMetaVarList()
    { }

    template<typename FirstT, typename... RemainingT>
    void BuildMetaVarList(FirstT firstMetaVar, RemainingT... remainingMetaVars)
    {
        m_mvl.push_back(firstMetaVar);
        BuildMetaVarList(remainingMetaVars...);
    }

    TypedMetaVarList(TypedMetaVar<metaVarTypes>... metavars)
    {
        SanityCheck<false, false, false, metaVarTypes...>();
        BuildMetaVarList(metavars...);
    }

    std::vector<MetaVar> m_mvl;
};

template<auto... metaVarTypes>
TypedMetaVarList<metaVarTypes...> CreateMetaVarList(TypedMetaVar<metaVarTypes>... metavars)
{
    static_assert(sizeof...(metaVarTypes) > 0, "Use CreateTrivialMaterialization for empty MetaVarList");
    return TypedMetaVarList<metaVarTypes...>(metavars...);
}

// Create a MetaVarMaterializedList with an empty MetaVarList
// In that case the list simply contains a plain function pointer
//
inline MetaVarMaterializedList CreateTrivialMaterialization(void* ptr)
{
    MetaVarMaterializedList result;
    MetaVarMaterializedInstance inst;
    inst.m_fnPtr = ptr;
    result.m_instances.push_back(inst);
    return result;
}

}   // namespace PochiVM
