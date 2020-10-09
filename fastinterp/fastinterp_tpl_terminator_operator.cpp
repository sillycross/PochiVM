#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "pochivm/ast_arithmetic_expr_type.h"
#include "fastinterp_tpl_return_type.h"

namespace PochiVM
{

struct FITerminatorOperatorImpl
{
    template<typename ReturnType,
             bool isNoExcept,
             bool exceptionThrown,
             bool isLiteral,
             bool isQuickAccess>
    static constexpr bool cond()
    {
        if (isNoExcept && exceptionThrown) { return false; }
        if (std::is_same<ReturnType, void>::value && (isLiteral || isQuickAccess)) { return false; }
        if (exceptionThrown && (isLiteral || isQuickAccess)) { return false; }
        if (isLiteral && isQuickAccess) { return false; }
        return true;
    }

    template<typename ReturnType,
             bool isNoExcept,
             bool exceptionThrown,
             bool isLiteral,
             bool isQuickAccess,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (isQuickAccess)
        {
            if (std::is_floating_point<ReturnType>::value)
            {
                if (!FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
            }
            else
            {
                if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
            }
        }
        return true;
    }

    template<typename T>
    using WorkaroundVoidType = typename std::conditional<std::is_same<T, void>::value, void*, T>::type;

    template<typename ReturnType,
             bool isNoExcept,
             bool exceptionThrown,
             bool isLiteral,
             bool isQuickAccess,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static FIReturnType<ReturnType, isNoExcept> f([[maybe_unused]] uintptr_t stackframe,
                                                  OpaqueParams... /*opaqueParams*/,
                                                  [[maybe_unused]] WorkaroundVoidType<ReturnType> qa) noexcept
    {
        if constexpr(exceptionThrown)
        {
            static_assert(!isNoExcept && !isQuickAccess && !isLiteral);
            return FIReturnValueHelper::GetForExn<ReturnType>();
        }
        else if constexpr(std::is_same<ReturnType, void>::value)
        {
            static_assert(!isQuickAccess && !isLiteral);
            return FIReturnValueHelper::GetForRetVoid<isNoExcept>();
        }
        else
        {
            ReturnType ret;
            if constexpr(isQuickAccess)
            {
                ret = qa;
            }
            else if constexpr(isLiteral)
            {
                DEFINE_CONSTANT_PLACEHOLDER_0(ReturnType);
                ret = CONSTANT_PLACEHOLDER_0;
            }
            else
            {
                DEFINE_CONSTANT_PLACEHOLDER_0(uint64_t);
                ret = *GetLocalVarAddress<ReturnType>(stackframe, CONSTANT_PLACEHOLDER_0);
            }
            return FIReturnValueHelper::GetForRet<ReturnType, isNoExcept>(ret);
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("returnType"),
                    CreateBoolMetaVar("isNoExcept"),
                    CreateBoolMetaVar("exceptionThrown"),
                    CreateBoolMetaVar("isLiteral"),
                    CreateBoolMetaVar("isQuickAccess"),
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
    RegisterBoilerplate<FITerminatorOperatorImpl>();
}
