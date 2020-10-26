#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_function_alignment.h"
#include "fastinterp_tpl_stackframe_category.h"

namespace PochiVM
{

struct FICallExprSpillStackAddrImpl
{
    template<bool isFakeSpillForNoParamCase>
    static constexpr bool cond()
    {
        return true;
    }

    // Placeholder rules:
    // constant placeholder 0: offset to spill
    //
    template<bool isFakeSpillForNoParamCase>
    static void f([[maybe_unused]] uintptr_t oldStackframe, uintptr_t newStackFrame) noexcept
    {
        if constexpr(!isFakeSpillForNoParamCase)
        {
            DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
            *GetLocalVarAddress<uintptr_t>(oldStackframe, CONSTANT_PLACEHOLDER_0) = newStackFrame;

            // Its continuation keeps computing parameters for this function call
            //
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(oldStackframe);
        }
        else
        {
            // This is a fake spill, its continuation is the function call
            //
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(newStackFrame);
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateBoolMetaVar("isFakeSpillForNoParamCase")
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
    RegisterBoilerplate<FICallExprSpillStackAddrImpl>();
}
