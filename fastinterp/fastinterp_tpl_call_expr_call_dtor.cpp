#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_function_alignment.h"
#include "fastinterp_tpl_return_type.h"

namespace PochiVM
{

// A dedicated operator to call destructor
// While this is possible to implement the same functionality by chaining a few call_* operators,
// this one creates smaller and faster assembly
//
struct FICallExprCallDestructorOpImpl
{
    template<bool dummy>
    static constexpr bool cond()
    {
        return true;
    }

    // Placeholder rules:
    // constant placeholder 0: offset of variable
    // cpp placeholder 0: entry point
    //
    template<bool dummy>
    static void f(uintptr_t stackframe) noexcept
    {
        {
            DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
            *reinterpret_cast<uint64_t*>(stackframe) = stackframe + CONSTANT_PLACEHOLDER_0;
        }

        static_assert(std::is_same<FIReturnType<void, true /*isNoExcept*/>, void>::value);

        DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t) noexcept);
        BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe - 8);
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
    RegisterBoilerplate<FICallExprCallDestructorOpImpl>();
}
