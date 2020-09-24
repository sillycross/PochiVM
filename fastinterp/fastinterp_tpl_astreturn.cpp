#include "fastinterp_tpl_helper.h"

namespace PochiVM
{

// Simple 'return' statement shape of 'return OSC'
//
struct FISimpleReturnImpl
{
    template<typename ReturnType, typename OscIndexType, OperandShapeCategory osc>
    static constexpr bool cond()
    {
        // If return type is 'void', must specify dummy osc = COMPLEX and oscIndex = int32_t
        //
        if (std::is_same<ReturnType, void>::value)
        {
            return std::is_same<OscIndexType, int32_t>::value && osc == OperandShapeCategory::COMPLEX;
        }
        if (!is_valid_index_type<OscIndexType>())
        {
            return false;
        }
        // If osc is not an array-element shape, we should always pass in the fake oscIndexType of int32_t
        //
        if (!(osc == OperandShapeCategory::VARPTR_VAR ||
            osc == OperandShapeCategory::VARPTR_LIT_NONZERO) &&
            !std::is_same<OscIndexType, int32_t>::value)
        {
            return false;
        }
        return true;
    }

    template<typename ReturnType, typename OscIndexType, OperandShapeCategory osc>
    static void f(InterpControlSignal* out) noexcept
    {
        if constexpr(!std::is_same<ReturnType, void>::value)
        {
            ReturnType value;
            if constexpr(osc == OperandShapeCategory::COMPLEX)
            {
                DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(ReturnType*) noexcept);
                BOILERPLATE_FNPTR_PLACEHOLDER_0(&value /*out*/);
            }
            else if constexpr(osc == OperandShapeCategory::LITERAL_NONZERO)
            {
                DEFINE_CONSTANT_PLACEHOLDER_0(ReturnType);
                value = CONSTANT_PLACEHOLDER_0;
            }
            else if constexpr(osc == OperandShapeCategory::ZERO)
            {
                constexpr ReturnType v = PochiVM::get_all_bits_zero_value<ReturnType>();
                value = v;
            }
            else if constexpr(osc == OperandShapeCategory::VARIABLE)
            {
                DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
                value = *GetLocalVarAddress<ReturnType>(CONSTANT_PLACEHOLDER_0);
            }
            else if constexpr(osc == OperandShapeCategory::VARPTR_DEREF)
            {
                DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
                value = **GetLocalVarAddress<ReturnType*>(CONSTANT_PLACEHOLDER_0);
            }
            else if constexpr(osc == OperandShapeCategory::VARPTR_VAR)
            {
                DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
                DEFINE_CONSTANT_PLACEHOLDER_1(uint32_t);
                ReturnType* varPtr = *GetLocalVarAddress<ReturnType*>(CONSTANT_PLACEHOLDER_0);
                OscIndexType index = *GetLocalVarAddress<OscIndexType>(CONSTANT_PLACEHOLDER_1);
                value = varPtr[index];
            }
            else if constexpr(osc == OperandShapeCategory::VARPTR_LIT_NONZERO)
            {
                DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
                DEFINE_CONSTANT_PLACEHOLDER_1(OscIndexType);
                ReturnType* varPtr = *GetLocalVarAddress<ReturnType*>(CONSTANT_PLACEHOLDER_0);
                value = varPtr[CONSTANT_PLACEHOLDER_1];
            }
            else
            {
                static_assert(type_dependent_false<ReturnType>::value, "unexpected literal category");
            }
            *GetLocalVarAddress<ReturnType>(0 /*offset*/) = value;
        }
        *out = InterpControlSignal::Return;
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("returnType"),
                    CreateTypeMetaVar("oscIndexType"),
                    CreateEnumMetaVar<OperandShapeCategory::X_END_OF_ENUM>("osc")
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
