#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_object_size_kind.h"
#include "pochivm/ast_arithmetic_expr_type.h"

namespace PochiVM
{

// Partially inlined pointer arithmetic expression.
// computes var + %
//
struct FIPartialInlineLhsPointerArithmeticImpl
{
    template<typename IndexType>
    static constexpr bool cond()
    {
        if (!std::is_integral<IndexType>::value) { return false; }
        return true;
    }

    template<typename IndexType,
             AstArithmeticExprType operatorType>
    static constexpr bool cond()
    {
        if (operatorType != AstArithmeticExprType::ADD && operatorType != AstArithmeticExprType::SUB) { return false; }
        return true;
    }

    template<typename IndexType,
             AstArithmeticExprType operatorType,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP>
    static constexpr bool cond()
    {
        if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        if (!spillOutput)
        {
            if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        }
        return true;
    }

    template<typename IndexType,
             AstArithmeticExprType operatorType,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
        return true;
    }

    template<typename IndexType,
             AstArithmeticExprType operatorType,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             FIPowerOfTwoObjectSize powerOfTwoObjectSize>
    static constexpr bool cond()
    {
        return true;
    }

    // placeholder rules:
    // constant placeholder 0: spill position, if spillOutput
    // constant placeholder 1: outlined operand position, if not quickaccess
    // constant placeholder 2: var offset
    // constant placeholder 3: object size, if not power of 2
    //
    template<typename IndexType,
             AstArithmeticExprType operatorType,
             bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             FIPowerOfTwoObjectSize powerOfTwoObjectSize,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, DEF_MEM2REG_PARAMS, OpaqueParams... opaqueParams, IndexType qaOperand) noexcept
    {
        uintptr_t base;
        {
            DEFINE_INDEX_CONSTANT_PLACEHOLDER_1;
            base = *GetLocalVarAddress<uintptr_t>(stackframe, CONSTANT_PLACEHOLDER_1);
        }

        size_t scale;
        if constexpr(powerOfTwoObjectSize != FIPowerOfTwoObjectSize::NOT_POWER_OF_TWO)
        {
            scale = 1ULL << (static_cast<int>(powerOfTwoObjectSize));
        }
        else
        {
            // TODO: this breaks down if object size >= 2^31
            //
            DEFINE_INDEX_CONSTANT_PLACEHOLDER_2;
            scale = CONSTANT_PLACEHOLDER_2;
        }

        IndexType offset = qaOperand;

        uintptr_t result;
        if constexpr(operatorType == AstArithmeticExprType::ADD)
        {
            result = base + static_cast<size_t>(offset) * scale;
        }
        else
        {
            static_assert(operatorType == AstArithmeticExprType::SUB);
            result = base - static_cast<size_t>(offset) * scale;
        }

        if constexpr(!spillOutput)
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, MEM2REG_TYPES, OpaqueParams..., uintptr_t) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, PASS_MEM2REG_PARAMS, opaqueParams..., result);
        }
        else
        {
            DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
            *GetLocalVarAddress<uintptr_t>(stackframe, CONSTANT_PLACEHOLDER_0) = result;

            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, MEM2REG_TYPES, OpaqueParams...) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, PASS_MEM2REG_PARAMS, opaqueParams...);
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("indexType"),
                    CreateEnumMetaVar<AstArithmeticExprType::X_END_OF_ENUM>("operatorType"),
                    CreateBoolMetaVar("spillOutput"),
                    CreateOpaqueIntegralParamsLimit(),
                    CreateOpaqueFloatParamsLimit(),
                    CreateEnumMetaVar<FIPowerOfTwoObjectSize::X_END_OF_ENUM>("powerOf2ObjectSize")
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
    RegisterBoilerplate<FIPartialInlineLhsPointerArithmeticImpl>();
}
