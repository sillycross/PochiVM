#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_return_type.h"

namespace PochiVM
{

struct FIOutlinedReturnImpl
{
    template<typename ReturnType,
             bool isNoExcept,
             bool exceptionThrown>
    static constexpr bool cond()
    {
        if (std::is_pointer<ReturnType>::value && !std::is_same<ReturnType, void*>::value) { return false; }
        if (isNoExcept && exceptionThrown) { return false; }
        return true;
    }

    template<typename ReturnType,
             bool isNoExcept,
             bool exceptionThrown,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (!exceptionThrown && !std::is_same<ReturnType, void>::value)
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
        if (exceptionThrown)
        {
            if (numOIP != static_cast<FINumOpaqueIntegralParams>(0)) { return false; }
            if (numOFP != static_cast<FINumOpaqueFloatingParams>(0)) { return false; }
        }
        return true;
    }

    template<typename T>
    using WorkaroundVoidType = typename std::conditional<std::is_same<T, void>::value, void*, T>::type;

    template<typename ReturnType,
             bool isNoExcept,
             bool exceptionThrown,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static FIReturnType<ReturnType, isNoExcept> f(uintptr_t /*stackframe*/,
                                                  OpaqueParams... /*opaqueParams*/,
                                                  [[maybe_unused]] WorkaroundVoidType<ReturnType> qa) noexcept
    {
        if constexpr(exceptionThrown)
        {
            static_assert(!isNoExcept);
            return FIReturnValueHelper::GetForExn<ReturnType>();
        }
        else if constexpr(std::is_same<ReturnType, void>::value)
        {
            return FIReturnValueHelper::GetForRetVoid<isNoExcept>();
        }
        else
        {
            return FIReturnValueHelper::GetForRet<ReturnType, isNoExcept>(qa);
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("returnType"),
                    CreateBoolMetaVar("isNoExcept"),
                    CreateBoolMetaVar("exceptionThrown"),
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
    RegisterBoilerplate<FIOutlinedReturnImpl>(FIAttribute::NoContinuation);
}
