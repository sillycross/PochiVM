#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_function_alignment.h"
#include "fastinterp_tpl_stackframe_category.h"
#include "fastinterp_tpl_conditional_jump_helper.hpp"

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
    // boilerplate placeholder 1: exception case continuation
    // boilerplate placeholder 0: no-exception case continuation
    //
    template<FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams, uint64_t hasException) noexcept
    {
        // Put the false branch (which is the no-exception case) at end.
        //
        FIConditionalJumpHelper::execute_1_0<FIConditionalJumpHelper::Mode::LikelyMode, OpaqueParams...>(hasException != 0, stackframe, opaqueParams...);
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
