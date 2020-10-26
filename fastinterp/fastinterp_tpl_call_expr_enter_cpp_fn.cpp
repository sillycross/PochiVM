#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP
#define FASTINTERP_TPL_USE_LARGE_MCMODEL

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_function_alignment.h"
#include "fastinterp_tpl_return_type.h"

namespace PochiVM
{

struct FICallExprEnterCppFnImpl
{
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
        using CppFnPrototype = FIReturnType<ReturnType, isNoExcept>(*)(uintptr_t) noexcept;
        DEFINE_CPP_FNPTR_PLACEHOLDER_0(CppFnPrototype);
        return CPP_FNPTR_PLACEHOLDER_0(stackframe);
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
    RegisterBoilerplate<FICallExprEnterCppFnImpl>(FIAttribute::NoContinuation | FIAttribute::AppendUd2 | FIAttribute::CodeModelLarge);
}
