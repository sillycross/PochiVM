#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"
#include "fastinterp_tpl_arith_operator_helper.hpp"

namespace PochiVM
{

// Inlined dereference impl
// var[lit/var[var/lit]]
//
struct FIInlinedDereferenceImpl
{
    template<typename OperandType>
    static constexpr bool cond()
    {
        if (std::is_same<OperandType, void>::value) { return false; }
        if (std::is_pointer<OperandType>::value && !std::is_same<OperandType, void*>::value) { return false; }
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
             typename IndexType,
             FIOperandShapeCategory shapeCategory>
    static constexpr bool cond()
    {
        if (!FIOperandShapeCategoryHelper::cond<IndexOperandType, IndexType, shapeCategory>()) { return false; }
        return true;
    }

    template<typename OperandType,
             typename IndexOperandType,
             typename IndexType,
             FIOperandShapeCategory shapeCategory,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP>
    static constexpr bool cond()
    {
        if (!spillOutput && !std::is_floating_point<OperandType>::value)
        {
            if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        }
        else
        {
            if (FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        }
        return true;
    }

    template<typename OperandType,
             typename IndexOperandType,
             typename IndexType,
             FIOperandShapeCategory shapeCategory,
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

    // placeholder rules:
    // constant placeholder 0: spill position, if spillOutput
    // constant placeholder 1: var offset
    // constant placeholder 2/3: inlined operand shape
    //
    template<typename OperandType,
             typename IndexOperandType,
             typename IndexType,
             FIOperandShapeCategory shapeCategory,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams) noexcept
    {
        IndexOperandType index = FIOperandShapeCategoryHelper::get_2_3<IndexOperandType, IndexType, shapeCategory>(stackframe);

        DEFINE_INDEX_CONSTANT_PLACEHOLDER_1;
        OperandType* var = *GetLocalVarAddress<OperandType*>(stackframe, CONSTANT_PLACEHOLDER_1);

        OperandType result = var[index];

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
                    CreateTypeMetaVar("indexType"),
                    CreateEnumMetaVar<FIOperandShapeCategory::X_END_OF_ENUM>("shapeCategory"),
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
    RegisterBoilerplate<FIInlinedDereferenceImpl>();
}
