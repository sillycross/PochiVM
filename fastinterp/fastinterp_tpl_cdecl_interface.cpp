#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"

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
// ***WARNING***:
//     DO NOT CHANGE THE STRUCT NAME BELOW.
//     This struct name is hardcoded in build_fast_interp_lib.cpp.
//     build_fast_interp_lib.cpp special-cases this struct name, keeping the cdecl calling convention
//     only for this struct, and changes everything else to use GHC convention.
//
struct FICdeclInterfaceImpl
{
    template<typename T>
    static constexpr bool cond()
    {
        return true;
    }

    // "disable_tail_calls" is required for this function: the compiler must not optimize the call to a tail-call:
    // the call will actually be later (at LLVM IR level) modified to use GHC convention,
    // and tail call from cdecl to GHC does not work. C++ compiler is not aware of this of course, so disable it.
    //
    template<typename T>
    static T f(uintptr_t stackframe) noexcept __attribute__((disable_tail_calls))
    {
        DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(T(*)(uintptr_t) noexcept);
        return BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe);
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("returnType")
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
    RegisterBoilerplate<FICdeclInterfaceImpl>();
}
