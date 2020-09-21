#include "fastinterp_tpl_helper.h"

namespace PochiVM
{

struct FastInterpVariableImpl
{
    template<typename VarTypePtr>
    static constexpr bool cond()
    {
        if (!std::is_pointer<VarTypePtr>::value) { return false; }
        return true;
    }

    template<typename VarTypePtr>
    static void f(VarTypePtr* out) noexcept
    {
        static_assert(std::is_pointer<VarTypePtr>::value, "unexpected VarTypePtr");
        // Must not use uint64_t, since it may be zero
        //
        DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
        uint32_t offset = CONSTANT_PLACEHOLDER_0;
        *out = GetLocalVarAddress<typename std::remove_pointer<VarTypePtr>::type>(offset);
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("varTypePtr")
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
    RegisterBoilerplate<FastInterpVariableImpl>();
}
