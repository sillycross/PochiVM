#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"
#include "fastinterp_tpl_comparison_operator_helper.hpp"
#include "fastinterp_tpl_conditional_jump_helper.hpp"

namespace PochiVM
{

// Outlined conditional branch
//
struct FIOutlinedConditionalUnpredictableBranchImpl
{
    template<FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        if (FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
        return true;
    }

    template<FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, DEF_MEM2REG_PARAMS, OpaqueParams... opaqueParams, bool cond) noexcept
    {
        FIConditionalJumpHelper::execute_0_1<FIConditionalJumpHelper::Mode::OptForSizeMode, MEM2REG_TYPES, OpaqueParams...>(cond, stackframe, PASS_MEM2REG_PARAMS, opaqueParams...);
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
    RegisterBoilerplate<FIOutlinedConditionalUnpredictableBranchImpl>(FIAttribute::OptSize);
}
