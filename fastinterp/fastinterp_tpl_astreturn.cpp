#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"

namespace PochiVM
{

// Simple 'return' statement shape of 'return OSC'
//
struct FISimpleReturnImpl
{
    template<typename ReturnType, typename OscIndexType, FIOperandShapeCategory osc>
    static constexpr bool cond()
    {
        // If return type is 'void', must specify dummy osc = COMPLEX and oscIndex = int32_t
        //
        if (std::is_same<ReturnType, void>::value)
        {
            return std::is_same<OscIndexType, int32_t>::value && osc == FIOperandShapeCategory::COMPLEX;
        }
        if (!FIOperandShapeCategoryHelper::cond<OscIndexType, osc>()) { return false; }
        return true;
    }

    template<typename ReturnType, typename OscIndexType, FIOperandShapeCategory osc>
    static InterpControlSignal f() noexcept
    {
        if constexpr(!std::is_same<ReturnType, void>::value)
        {
            ReturnType value = FIOperandShapeCategoryHelper::get_0_1<ReturnType, OscIndexType, osc>();
            *GetLocalVarAddress<ReturnType>(0 /*offset*/) = value;
        }
        return InterpControlSignal::Return;
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("returnType"),
                    CreateTypeMetaVar("oscIndexType"),
                    CreateEnumMetaVar<FIOperandShapeCategory::X_END_OF_ENUM>("osc")
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
    RegisterBoilerplate<FISimpleReturnImpl>();
}
