#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_function_alignment.h"
#include "fastinterp_tpl_return_type.h"

namespace PochiVM
{

struct FIThrowExceptionImpl
{
    template<bool isQuickAccess>
    static constexpr bool cond()
    {
        return true;
    }

    // Placeholder rules:
    // constant placeholder 0: stack offset, if the exnAddr is not quickaccess
    // cpp placeholder 0: the cpp helper that sets std::exception_ptr
    // boilerplate placeholder 0: continuation to cleanup logic
    //
    template<bool isQuickAccess>
    static void f(uintptr_t stackframe, [[maybe_unused]] uintptr_t qa) noexcept
    {
        uintptr_t exnAddr;
        if constexpr(isQuickAccess)
        {
            exnAddr = qa;
        }
        else
        {
            DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
            exnAddr = stackframe + CONSTANT_PLACEHOLDER_0;
        }

        using CppFnPrototype = void(*)(uintptr_t) noexcept;
        DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_1_NO_TAILCALL(CppFnPrototype);
        BOILERPLATE_FNPTR_PLACEHOLDER_1(exnAddr);

        DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t) noexcept);
        BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe);
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateBoolMetaVar("isQuickAccess")
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
    RegisterBoilerplate<FIThrowExceptionImpl>();
}
