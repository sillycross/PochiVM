#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_function_alignment.h"
#include "fastinterp_tpl_return_type.h"

namespace PochiVM
{

struct FICallExprCallDestructorCallerImpl
{
    template<bool dummy>
    static constexpr bool cond()
    {
        return true;
    }

    template<bool dummy>
    static void f(uintptr_t stackframe) noexcept
    {
        DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_1_NO_TAILCALL(void(*)(uintptr_t) noexcept);
        BOILERPLATE_FNPTR_PLACEHOLDER_1(stackframe);

        DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t) noexcept);
        BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe);
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateBoolMetaVar("dummy")
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
    RegisterBoilerplate<FICallExprCallDestructorCallerImpl>();
}
