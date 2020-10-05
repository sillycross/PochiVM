#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"
#include "pochivm/ast_arithmetic_expr_type.h"

namespace PochiVM
{

struct FITerminatorOperatorImpl
{
    template<bool dummy>
    static constexpr bool cond()
    {
        return true;
    }

    template<bool dummy>
    static void f(uintptr_t /*stackframe*/) noexcept
    {
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
    RegisterBoilerplate<FITerminatorOperatorImpl>();
}
