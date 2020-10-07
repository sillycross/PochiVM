#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_function_alignment.h"
#include "fastinterp_tpl_stackframe_category.h"

namespace PochiVM
{

// Store a param for a call expression
//
struct FICallExprStoreParamImpl
{
    template<typename ParamType,
             bool isQuickAccess,
             bool hasMore>
    static constexpr bool cond()
    {
        if (std::is_same<ParamType, void>::value) { return false; }
        if (std::is_pointer<ParamType>::value && !std::is_same<ParamType, void*>::value) { return false; }
        return true;
    }

    // Placeholder rules:
    // constant placeholder 1: offset if not quickaccess
    // constant placeholder 0: offset in newStackFrame to store param
    //
    template<typename ParamType,
             bool isQuickAccess,
             bool hasMore>
    static void f([[maybe_unused]] uintptr_t oldStackframe, uintptr_t newStackFrame, [[maybe_unused]] ParamType qa) noexcept
    {
        ParamType param;
        if constexpr(isQuickAccess)
        {
            param = qa;
        }
        else
        {
            DEFINE_CONSTANT_PLACEHOLDER_1(uint64_t);
            param = *GetLocalVarAddress<ParamType>(oldStackframe, CONSTANT_PLACEHOLDER_1);
        }

        {
            DEFINE_CONSTANT_PLACEHOLDER_0(uint64_t);
            *GetLocalVarAddress<ParamType>(newStackFrame, CONSTANT_PLACEHOLDER_0) = param;
        }

        if constexpr(hasMore)
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, uintptr_t) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(oldStackframe, newStackFrame);
        }
        else
        {
             DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t) noexcept);
             BOILERPLATE_FNPTR_PLACEHOLDER_0(newStackFrame);
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("paramType"),
                    CreateBoolMetaVar("isQuickAccess"),
                    CreateBoolMetaVar("hasMore")
        );
    }
};

struct FICallExprStoreParamNewSfSpilledImpl
{
    template<typename ParamType,
             bool isQuickAccess,
             bool hasMore>
    static constexpr bool cond()
    {
        if (std::is_same<ParamType, void>::value) { return false; }
        if (std::is_pointer<ParamType>::value && !std::is_same<ParamType, void*>::value) { return false; }
        return true;
    }

    // Placeholder rules:
    // constant placeholder 2: offset if not quickaccess
    // constant placeholder 1: spilled newstackframe offset
    // constant placeholder 0: offset in newStackFrame to store param
    //
    template<typename ParamType,
             bool isQuickAccess,
             bool hasMore>
    static void f([[maybe_unused]] uintptr_t oldStackframe, [[maybe_unused]] ParamType qa) noexcept
    {
        ParamType param;
        if constexpr(isQuickAccess)
        {
            param = qa;
        }
        else
        {
            DEFINE_CONSTANT_PLACEHOLDER_1(uint64_t);
            param = *GetLocalVarAddress<ParamType>(oldStackframe, CONSTANT_PLACEHOLDER_1);
        }

        uintptr_t newStackFrame;
        {
            DEFINE_CONSTANT_PLACEHOLDER_1(uint64_t);
            newStackFrame = *GetLocalVarAddress<uintptr_t>(oldStackframe, CONSTANT_PLACEHOLDER_1);
        }

        {
            DEFINE_CONSTANT_PLACEHOLDER_0(uint64_t);
            *GetLocalVarAddress<ParamType>(newStackFrame, CONSTANT_PLACEHOLDER_0) = param;
        }

        if constexpr(hasMore)
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(oldStackframe);
        }
        else
        {
             DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t) noexcept);
             BOILERPLATE_FNPTR_PLACEHOLDER_0(newStackFrame);
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("paramType"),
                    CreateBoolMetaVar("isQuickAccess"),
                    CreateBoolMetaVar("hasMore")
        );
    }
};

}   // namespace PochiVM

// build_fast_interp_lib.cpp JIT entry point
//
extern "C"
void __pochivm_build_fast_interp_library__()
{
    using namespace PochiVM;
    RegisterBoilerplate<FICallExprStoreParamImpl>();
    RegisterBoilerplate<FICallExprStoreParamNewSfSpilledImpl>();
}
