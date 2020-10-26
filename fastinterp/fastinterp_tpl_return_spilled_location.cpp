#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_return_type.h"

namespace PochiVM
{

struct FIReturnSpilledLocationImpl
{
    template<typename ReturnType>
    static constexpr bool cond()
    {
        if (std::is_same<ReturnType, void>::value) { return false; }
        if (std::is_pointer<ReturnType>::value && !std::is_same<ReturnType, void*>::value) { return false; }
        return true;
    }

    template<typename ReturnType,
             bool isNoExcept>
    static constexpr bool cond()
    {
        return true;
    }

    template<typename ReturnType,
             bool isNoExcept>
    static FIReturnType<ReturnType, isNoExcept> f(uintptr_t stackframe) noexcept
    {
        DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
        ReturnType ret = *GetLocalVarAddress<ReturnType>(stackframe, CONSTANT_PLACEHOLDER_0);
        return FIReturnValueHelper::GetForRet<ReturnType, isNoExcept>(ret);
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("returnType"),
                    CreateBoolMetaVar("isNoExcept")
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
    RegisterBoilerplate<FIReturnSpilledLocationImpl>(FIAttribute::NoContinuation);
}
