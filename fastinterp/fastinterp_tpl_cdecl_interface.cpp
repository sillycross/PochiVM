#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_return_type.h"

namespace PochiVM
{

// Inside fastinterp, we always use GHC calling convention for performance (it fits our use case best).
// However, C++ code does not natively support GHC calling convention, so we cannot give a GHC-conventioned
// function pointer to C++ and let C++ call it.
// This operator wraps a GHC-conventioned function using a cdecl-conventioned wrapper.
// The function pointer from this operator is the only kind that is safe to call from C++.
//
// GHC calling convention: https://releases.llvm.org/10.0.0/docs/LangRef.html#calling-conventions
//
struct FICdeclInterfaceImpl
{
    template<typename T,
             bool isNoExcept>
    static constexpr bool cond()
    {
        return true;
    }

    template<typename T,
             bool isNoExcept>
    static FIReturnType<T, isNoExcept> f(uintptr_t stackframe) noexcept
    {
        // "no tailcall" is required: tail call from cdecl to GHC does not work.
        //
        DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0_NO_TAILCALL(FIReturnType<T, isNoExcept>(*)(uintptr_t) noexcept);
        return BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe);
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
    RegisterBoilerplate<FICdeclInterfaceImpl>(FIAttribute::CDecl | FIAttribute::NoContinuation);
}
