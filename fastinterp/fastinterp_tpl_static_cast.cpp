#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"

namespace PochiVM
{

// static_cast between non-pointer types
//
struct FIStaticCastImpl
{
    template<typename SrcType,
             typename DstType>
    static constexpr bool cond()
    {
        if (std::is_pointer<SrcType>::value || std::is_void<SrcType>::value) { return false; }
        if (std::is_pointer<DstType>::value || std::is_void<DstType>::value) { return false; }
        bool isSrcIntegral = !std::is_floating_point<SrcType>::value;
        bool isDstIntegral = !std::is_floating_point<DstType>::value;
        // Disallow cast from float to int, all other casts are OK
        //
        if (!isSrcIntegral && isDstIntegral) { return false; }

        // Cast from uint64_t to double needs constant table
        // We don't support that relocation, and we don't have time to fix it now.
        // This is manually implemented in fastinterp_tpl_static_cast_u64_double.cpp
        // We have to implement it in another CPP file since that implementation needs a
        // 64-bit placeholder thus needs medium code model
        //
        if (std::is_same<SrcType, uint64_t>::value && std::is_same<DstType, double>::value) { return false; }
        return true;
    }

    template<typename SrcType,
             typename DstType,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (!std::is_floating_point<SrcType>::value)
        {
            if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        }
        else
        {
            if (!FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
        }
        if (!spillOutput)
        {
            if (!std::is_floating_point<DstType>::value)
            {
                if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
            }
            else
            {
                if (!FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
            }
        }
        return true;
    }

    // Placeholder rules:
    // constant placeholder 0: spill location, if spilled
    //
    template<typename SrcType,
             typename DstType,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams, SrcType qa) noexcept
    {
        DstType result = static_cast<DstType>(qa);
        if constexpr(!spillOutput)
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams..., DstType) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams..., result);
        }
        else
        {
            DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
            *GetLocalVarAddress<DstType>(stackframe, CONSTANT_PLACEHOLDER_0) = result;

            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams...) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams...);
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("srcType"),
                    CreateTypeMetaVar("dstType"),
                    CreateBoolMetaVar("spillOutput"),
                    CreateOpaqueIntegralParamsLimit(),
                    CreateOpaqueFloatParamsLimit()
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
    RegisterBoilerplate<FIStaticCastImpl>();
}
