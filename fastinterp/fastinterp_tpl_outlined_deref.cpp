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
struct FIOutlinedDereferenceImpl
{
    template<typename OperandType>
    static constexpr bool cond()
    {
        if (std::is_same<OperandType, void>::value) { return false; }
        return true;
    }

    template<typename OperandType,
             typename IndexOperandType>
    static constexpr bool cond()
    {
        if (std::is_same<IndexOperandType, void>::value ||
            std::is_floating_point<IndexOperandType>::value ||
            std::is_pointer<IndexOperandType>::value)
        {
            return false;
        }
        return true;
    }

    template<typename OperandType,
             typename IndexOperandType,
             bool isLhsQAP,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP>
    static constexpr bool cond()
    {
        if (!FIOpaqueParamsHelper::CanPush(numOIP, 1 + (isLhsQAP ? 1 : 0))) { return false; }
        return true;
    }

    template<typename OperandType,
             typename IndexOperandType,
             bool isLhsQAP,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (!spillOutput && std::is_floating_point<OperandType>::value)
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
             typename IndexOperandType,
             bool isLhsQAP,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe,
                  OpaqueParams... opaqueParams,
                  typename std::conditional<isLhsQAP, OperandType*, IndexOperandType>::type qa1,
                  [[maybe_unused]] IndexOperandType qa2) noexcept
    {
        OperandType* base;
        IndexOperandType offset;
        if constexpr(isLhsQAP)
        {
            base = qa1;
            offset = qa2;
        }
        else
        {
            DEFINE_INDEX_CONSTANT_PLACEHOLDER_1;
            base = *GetLocalVarAddress<OperandType*>(stackframe, CONSTANT_PLACEHOLDER_1);
            offset = qa1;
        }

        OperandType result = base[offset];

        if constexpr(!spillOutput)
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams..., OperandType) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams..., result);
        }
        else
        {
            DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
            *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_0) = result;

            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams...) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams...);
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("operandType"),
                    CreateTypeMetaVar("indexOperandType"),
                    CreateBoolMetaVar("isLhsQAP"),
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
    RegisterBoilerplate<FIOutlinedDereferenceImpl>();
}
