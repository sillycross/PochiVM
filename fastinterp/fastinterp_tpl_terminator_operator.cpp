#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"
#include "pochivm/ast_arithmetic_expr_type.h"

namespace PochiVM
{

struct FITerminatorOperatorImpl
{
    template<bool isNoExcept,
             bool exceptionThrown>
    static constexpr bool cond()
    {
        if (isNoExcept && exceptionThrown) { return false; }
        return true;
    }

    template<bool isNoExcept,
             bool exceptionThrown>
    static typename std::conditional<isNoExcept, void, bool>::type f(uintptr_t /*stackframe*/) noexcept
    {
        if constexpr(!isNoExcept)
        {
            return exceptionThrown;
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateBoolMetaVar("isNoExcept"),
                    CreateBoolMetaVar("exceptionThrown")
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
