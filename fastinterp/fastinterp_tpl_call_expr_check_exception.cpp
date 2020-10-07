#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_function_alignment.h"
#include "fastinterp_tpl_stackframe_category.h"

namespace PochiVM
{

struct FICallExprCheckExceptionImpl
{
    template<FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (static_cast<int>(numOIP) + static_cast<int>(numOFP) > 1) { return false; }
        return true;
    }

    // Placeholder rules:
    // boilerplate placeholder 0: exception catch clause
    // CPP placeholder 0: exception soft-emulator
    // constant placeholder 0: program position indicator
    // boilerplate placeholder 1: no-exception case continuation
    //
    template<FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams, bool hasException) noexcept
    {
        // This 'likely' is weird. We want to put the false branch (which is the no-exception case) at end.
        // TODO: revisit this if we have time to fix LLVM's conditional tail call folding.
        //
        if (likely(hasException))
        {
            // Call our C++ EH soft-emulator stub, which would call all destructors before the catch block
            //
            DEFINE_CPP_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, uintptr_t) noexcept);
            DEFINE_CONSTANT_PLACEHOLDER_0(uintptr_t);
            CPP_FNPTR_PLACEHOLDER_0(stackframe, CONSTANT_PLACEHOLDER_0);

            // Transfer control to catch block
            //
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams...) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams...);
        }
        else
        {
            // Transfer control to normal block
            //
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_1(void(*)(uintptr_t, OpaqueParams...) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_1(stackframe, opaqueParams...);
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
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
    RegisterBoilerplate<FICallExprCheckExceptionImpl>();
}
