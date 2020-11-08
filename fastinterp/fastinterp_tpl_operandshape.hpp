#pragma once

#include "fastinterp_tpl_operandshape.h"
#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_constant_valid_in_mcmodel.h"

namespace PochiVM
{

struct FISimpleOperandShapeCategoryHelper
{
    template<typename OperandType, FISimpleOperandShapeCategory osc>
    static constexpr bool cond()
    {
        if (osc == FISimpleOperandShapeCategory::LITERAL_NONZERO)
        {
            if (!IsConstantValidInSmallCodeModel<OperandType>())
            {
                return false;
            }
        }
        return true;
    }

#define OSCHELPER_GENERATE_METHOD(meth_name, placeholder1)                                                      \
    template<typename OperandType, FISimpleOperandShapeCategory osc>                                            \
    static OperandType WARN_UNUSED __attribute__((__always_inline__)) meth_name(uintptr_t stackframe) noexcept  \
    {                                                                                                           \
        if constexpr(osc == FISimpleOperandShapeCategory::LITERAL_NONZERO)                                      \
        {                                                                                                       \
            INTERNAL_DEFINE_CONSTANT_PLACEHOLDER(placeholder1, OperandType);                                    \
            return CONSTANT_PLACEHOLDER_ ## placeholder1;                                                       \
        }                                                                                                       \
        else if constexpr(osc == FISimpleOperandShapeCategory::ZERO)                                            \
        {                                                                                                       \
            constexpr OperandType v = PochiVM::get_all_bits_zero_value<OperandType>();                          \
            return v;                                                                                           \
        }                                                                                                       \
        else if constexpr(osc == FISimpleOperandShapeCategory::VARIABLE)                                        \
        {                                                                                                       \
            INTERNAL_DEFINE_INDEX_CONSTANT_PLACEHOLDER(placeholder1);                                           \
            return *GetLocalVarAddress<OperandType>(stackframe, CONSTANT_PLACEHOLDER_ ## placeholder1);         \
        }                                                                                                       \
        else                                                                                                    \
        {                                                                                                       \
            static_assert(type_dependent_false<OperandType>::value, "unexpected literal category");             \
        }                                                                                                       \
    }

    OSCHELPER_GENERATE_METHOD(get_1, 1)
    OSCHELPER_GENERATE_METHOD(get_2, 2)

#undef OSCHELPER_GENERATE_METHOD
};

struct FIOperandShapeCategoryHelper
{
    template<typename OperandType, typename OscIndexType, FIOperandShapeCategory osc>
    static constexpr bool cond()
    {
        if (!is_valid_index_type<OscIndexType>())
        {
            return false;
        }
        if (osc == FIOperandShapeCategory::LITERAL_NONZERO)
        {
            if (!IsConstantValidInSmallCodeModel<OperandType>())
            {
                return false;
            }
        }
        // If osc is not an array-element shape, we should always pass in the fake oscIndexType of int32_t
        //
        if (!(osc == FIOperandShapeCategory::VARPTR_VAR ||
            osc == FIOperandShapeCategory::VARPTR_LIT_NONZERO) &&
            !std::is_same<OscIndexType, int32_t>::value)
        {
            return false;
        }
        if (osc == FIOperandShapeCategory::VARPTR_LIT_NONZERO)
        {
            // just to supress compile error, the 'void' case has been locked down in earlier logic
            //
            if constexpr(!std::is_same<OscIndexType, void>::value)
            {
                if (!IsConstantValidInSmallCodeModel<OscIndexType>())
                {
                    return false;
                }
            }
        }
        return true;
    }

    // Generate a method which get the operand using a pair of placeholders of specified ordinals
    // As is all helprs in fastinterp_tpl, always_inline is required.
    //
#define OSCHELPER_GENERATE_METHOD(meth_name, placeholder1, placeholder2)                                        \
    template<typename OperandType, typename OscIndexType, FIOperandShapeCategory osc>                           \
    static OperandType WARN_UNUSED __attribute__((__always_inline__)) meth_name(uintptr_t sf) noexcept          \
    {                                                                                                           \
        if constexpr(osc == FIOperandShapeCategory::LITERAL_NONZERO)                                            \
        {                                                                                                       \
            INTERNAL_DEFINE_CONSTANT_PLACEHOLDER(placeholder1, OperandType);                                    \
            return CONSTANT_PLACEHOLDER_ ## placeholder1;                                                       \
        }                                                                                                       \
        else if constexpr(osc == FIOperandShapeCategory::ZERO)                                                  \
        {                                                                                                       \
            constexpr OperandType v = PochiVM::get_all_bits_zero_value<OperandType>();                          \
            return v;                                                                                           \
        }                                                                                                       \
        else if constexpr(osc == FIOperandShapeCategory::VARIABLE)                                              \
        {                                                                                                       \
            INTERNAL_DEFINE_INDEX_CONSTANT_PLACEHOLDER(placeholder1);                                           \
            return *GetLocalVarAddress<OperandType>(sf, CONSTANT_PLACEHOLDER_ ## placeholder1);                 \
        }                                                                                                       \
        else if constexpr(osc == FIOperandShapeCategory::VARPTR_DEREF)                                          \
        {                                                                                                       \
            INTERNAL_DEFINE_INDEX_CONSTANT_PLACEHOLDER(placeholder1);                                           \
            return **GetLocalVarAddress<OperandType*>(sf, CONSTANT_PLACEHOLDER_ ## placeholder1);               \
        }                                                                                                       \
        else if constexpr(osc == FIOperandShapeCategory::VARPTR_VAR)                                            \
        {                                                                                                       \
            INTERNAL_DEFINE_INDEX_CONSTANT_PLACEHOLDER(placeholder1);                                           \
            INTERNAL_DEFINE_INDEX_CONSTANT_PLACEHOLDER(placeholder2);                                           \
            OperandType* varPtr = *GetLocalVarAddress<OperandType*>(sf, CONSTANT_PLACEHOLDER_ ## placeholder1); \
            OscIndexType index = *GetLocalVarAddress<OscIndexType>(sf, CONSTANT_PLACEHOLDER_ ## placeholder2);  \
            return varPtr[index];                                                                               \
        }                                                                                                       \
        else if constexpr(osc == FIOperandShapeCategory::VARPTR_LIT_NONZERO)                                    \
        {                                                                                                       \
            INTERNAL_DEFINE_INDEX_CONSTANT_PLACEHOLDER(placeholder1);                                           \
            INTERNAL_DEFINE_CONSTANT_PLACEHOLDER(placeholder2, OscIndexType);                                   \
            OperandType* varPtr = *GetLocalVarAddress<OperandType*>(sf, CONSTANT_PLACEHOLDER_ ## placeholder1); \
            return varPtr[CONSTANT_PLACEHOLDER_ ## placeholder2];                                               \
        }                                                                                                       \
        else if constexpr(osc == FIOperandShapeCategory::VARPTR_LIT_DIRECT_OFFSET)                              \
        {                                                                                                       \
            INTERNAL_DEFINE_INDEX_CONSTANT_PLACEHOLDER(placeholder1);                                           \
            INTERNAL_DEFINE_INDEX_CONSTANT_PLACEHOLDER(placeholder2);                                           \
            uintptr_t varPtr = *GetLocalVarAddress<uintptr_t>(sf, CONSTANT_PLACEHOLDER_ ## placeholder1);       \
            return *reinterpret_cast<OperandType*>(varPtr + CONSTANT_PLACEHOLDER_ ## placeholder2);             \
        }                                                                                                       \
        else                                                                                                    \
        {                                                                                                       \
            static_assert(type_dependent_false<OperandType>::value, "unexpected literal category");             \
        }                                                                                                       \
    }

    OSCHELPER_GENERATE_METHOD(get_0_1, 0, 1)
    OSCHELPER_GENERATE_METHOD(get_2_3, 2, 3)

    OSCHELPER_GENERATE_METHOD(get_1_2, 1, 2)
    OSCHELPER_GENERATE_METHOD(get_3_4, 3, 4)

#undef OSCHELPER_GENERATE_METHOD

// Get the address, instead of value
//
#define OSCHELPER_GENERATE_METHOD(meth_name, placeholder1, placeholder2)                                        \
    template<typename OperandType, typename OscIndexType, FIOperandShapeCategory osc>                           \
    static OperandType* WARN_UNUSED __attribute__((__always_inline__)) meth_name(uintptr_t sf) noexcept         \
    {                                                                                                           \
        if constexpr(osc == FIOperandShapeCategory::VARIABLE)                                                   \
        {                                                                                                       \
            INTERNAL_DEFINE_INDEX_CONSTANT_PLACEHOLDER(placeholder1);                                           \
            return GetLocalVarAddress<OperandType>(sf, CONSTANT_PLACEHOLDER_ ## placeholder1);                  \
        }                                                                                                       \
        else if constexpr(osc == FIOperandShapeCategory::VARPTR_DEREF)                                          \
        {                                                                                                       \
            INTERNAL_DEFINE_INDEX_CONSTANT_PLACEHOLDER(placeholder1);                                           \
            return *GetLocalVarAddress<OperandType*>(sf, CONSTANT_PLACEHOLDER_ ## placeholder1);                \
        }                                                                                                       \
        else if constexpr(osc == FIOperandShapeCategory::VARPTR_VAR)                                            \
        {                                                                                                       \
            INTERNAL_DEFINE_INDEX_CONSTANT_PLACEHOLDER(placeholder1);                                           \
            INTERNAL_DEFINE_INDEX_CONSTANT_PLACEHOLDER(placeholder2);                                           \
            OperandType* varPtr = *GetLocalVarAddress<OperandType*>(sf, CONSTANT_PLACEHOLDER_ ## placeholder1); \
            OscIndexType index = *GetLocalVarAddress<OscIndexType>(sf, CONSTANT_PLACEHOLDER_ ## placeholder2);  \
            return varPtr + index;                                                                              \
        }                                                                                                       \
        else if constexpr(osc == FIOperandShapeCategory::VARPTR_LIT_NONZERO)                                    \
        {                                                                                                       \
            INTERNAL_DEFINE_INDEX_CONSTANT_PLACEHOLDER(placeholder1);                                           \
            INTERNAL_DEFINE_CONSTANT_PLACEHOLDER(placeholder2, OscIndexType);                                   \
            OperandType* varPtr = *GetLocalVarAddress<OperandType*>(sf, CONSTANT_PLACEHOLDER_ ## placeholder1); \
            return varPtr + CONSTANT_PLACEHOLDER_ ## placeholder2;                                              \
        }                                                                                                       \
        else if constexpr(osc == FIOperandShapeCategory::VARPTR_LIT_DIRECT_OFFSET)                              \
        {                                                                                                       \
            INTERNAL_DEFINE_INDEX_CONSTANT_PLACEHOLDER(placeholder1);                                           \
            INTERNAL_DEFINE_INDEX_CONSTANT_PLACEHOLDER(placeholder2);                                           \
            uintptr_t varPtr = *GetLocalVarAddress<uintptr_t>(sf, CONSTANT_PLACEHOLDER_ ## placeholder1);       \
            return reinterpret_cast<OperandType*>(varPtr + CONSTANT_PLACEHOLDER_ ## placeholder2);              \
        }                                                                                                       \
        else                                                                                                    \
        {                                                                                                       \
            static_assert(type_dependent_false<OperandType>::value, "unexpected literal category");             \
        }                                                                                                       \
    }

    OSCHELPER_GENERATE_METHOD(get_address_0_1, 0, 1)
    OSCHELPER_GENERATE_METHOD(get_address_2_3, 2, 3)

    OSCHELPER_GENERATE_METHOD(get_address_1_2, 1, 2)
    OSCHELPER_GENERATE_METHOD(get_address_3_4, 3, 4)

#undef OSCHELPER_GENERATE_METHOD
};

}   // namespace PochiVM
