#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"
#include "fastinterp_tpl_arith_operator_helper.hpp"

namespace PochiVM
{

// Fully outlined assign expr
// Takes 2 operands, outputs 0 operand
//
struct FIOutlinedAssignImpl
{
    template<typename OperandType>
    static constexpr bool cond()
    {
        if (std::is_same<OperandType, void>::value) { return false; }
        if (std::is_pointer<OperandType>::value && !std::is_same<OperandType, void*>::value) { return false; }
        return true;
    }

    template<typename OperandType,
             bool isLhsQAP,
             FINumOpaqueIntegralParams numOIP>
    static constexpr bool cond()
    {
        if (!FIOpaqueParamsHelper::CanPush(numOIP, (!std::is_floating_point<OperandType>::value ? 1 : 0) + (isLhsQAP ? 1 : 0)))
        {
            return false;
        }
        return true;
    }

    template<typename OperandType,
             bool isLhsQAP,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (std::is_floating_point<OperandType>::value)
        {
            if (!FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
        }
        else
        {
            if (FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
        }
        return true;
    }

    // Placeholder rules:
    // constant placeholder 0 for RHS if not quickaccess
    //
    template<typename OperandType,
             bool isLhsQAP,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe,
                  OpaqueParams... opaqueParams,
                  typename std::conditional<isLhsQAP, OperandType*, OperandType>::type qa1,
                  [[maybe_unused]] OperandType qa2) noexcept
    {
        OperandType* lhs;
        OperandType rhs;
        if constexpr(isLhsQAP)
        {
            lhs = qa1;
            rhs = qa2;
        }
        else
        {
            DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
            lhs = *GetLocalVarAddress<OperandType*>(stackframe, CONSTANT_PLACEHOLDER_0);
            rhs = qa1;
        }

        *lhs = rhs;

        DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams...) noexcept);
        BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams...);
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("operandType"),
                    CreateBoolMetaVar("isLhsQAP"),
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
    RegisterBoilerplate<FIOutlinedAssignImpl>();
}
