#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"
#include "fastinterp_tpl_arith_operator_helper.hpp"

namespace PochiVM
{

// Fully outlined assign expr
// Takes 2 operands, outputs 0 operand
// This struct handles the case that Lhs is using quick access
//
struct FIOutlinedAssignLhsQapImpl
{
    template<typename OperandType>
    static constexpr bool cond()
    {
        if (std::is_same<OperandType, void>::value) { return false; }
        return true;
    }

    template<typename OperandType,
             bool isRhsQAP,
             FINumOpaqueIntegralParams numOIP>
    static constexpr bool cond()
    {
        if (!FIOpaqueParamsHelper::CanPush(numOIP))
        {
            return false;
        }
        if (!std::is_floating_point<OperandType>::value)
        {
            // We always evaluate lhs before rhs, and rhs is also integral type,
            // so if lhs is QAP, rhs must also be QAP
            //
            if (!isRhsQAP)
            {
                return false;
            }
            if (!FIOpaqueParamsHelper::CanPush(numOIP, 2))
            {
                return false;
            }
        }
        return true;
    }

    template<typename OperandType,
             bool isRhsQAP,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (std::is_floating_point<OperandType>::value)
        {
            if (!isRhsQAP)
            {
                if (!FIOpaqueParamsHelper::IsEmpty(numOFP)) { return false; }
            }
            else
            {
                if (!FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
            }
        }
        return true;
    }

    // Placeholder rules:
    // constant placeholder 0 for RHS if not quickaccess
    //
    template<typename OperandType,
             bool isRhsQAP,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams, OperandType* lhs, [[maybe_unused]] OperandType qa) noexcept
    {
        OperandType rhs;
        if constexpr(!isRhsQAP)
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(uint64_t);
            rhs = *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_0);
        }
        else
        {
            rhs = qa;
        }
        *lhs = rhs;

        DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams...) noexcept);
        BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams...);
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("operandType"),
                    CreateBoolMetaVar("isRhsQAP"),
                    CreateOpaqueIntegralParamsLimit(),
                    CreateOpaqueFloatParamsLimit()
        );
    }
};

// Fully outlined assign expr
// Takes 2 operands, outputs 0 operand
// This struct handles the case that Lhs is NOT using quick access
//
struct FIOutlinedAssignLhsNotQapImpl
{
    template<typename OperandType>
    static constexpr bool cond()
    {
        if (std::is_same<OperandType, void>::value) { return false; }
        return true;
    }

    template<typename OperandType,
             bool isRhsQAP,
             FINumOpaqueIntegralParams numOIP>
    static constexpr bool cond()
    {
        if (!std::is_floating_point<OperandType>::value)
        {
            if (!isRhsQAP)
            {
                if (!FIOpaqueParamsHelper::IsEmpty(numOIP)) { return false; }
            }
            else
            {
                if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
            }
        }
        return true;
    }

    template<typename OperandType,
             bool isRhsQAP,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (std::is_floating_point<OperandType>::value)
        {
            if (!isRhsQAP)
            {
                if (!FIOpaqueParamsHelper::IsEmpty(numOFP)) { return false; }
            }
            else
            {
                if (!FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
            }
        }
        return true;
    }

    // Placeholder rules:
    // constant placeholder 0 for lhs
    // constant placeholder 1 for rhs if not quickaccess
    //
    template<typename OperandType,
             bool isRhsQAP,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams, [[maybe_unused]] OperandType qa) noexcept
    {
        OperandType* lhs;
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(uint64_t);
            lhs = *GetLocalVarAddress<OperandType*>(stackframe, CONSTANT_PLACEHOLDER_0);
        }
        OperandType rhs;
        if constexpr(!isRhsQAP)
        {
            DEFINE_CONSTANT_PLACEHOLDER_1(uint64_t);
            rhs = *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_1);
        }
        else
        {
            rhs = qa;
        }
        *lhs = rhs;

        DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams...) noexcept);
        BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams...);
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("operandType"),
                    CreateBoolMetaVar("isRhsQAP"),
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
    RegisterBoilerplate<FIOutlinedAssignLhsQapImpl>();
    RegisterBoilerplate<FIOutlinedAssignLhsNotQapImpl>();
}
