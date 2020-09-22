#include "fastinterp_tpl_helper.h"
#include "pochivm/cxx2a_bit_cast_helper.h"

namespace PochiVM
{

struct FastInterpLiteralImpl
{
    // Only allow primitive type and 'void*'
    //
    template<typename LiteralType, bool isAllUnderlyingBitsZero>
    static constexpr bool cond()
    {
        if (std::is_same<LiteralType, void>::value) { return false; }
        if (std::is_pointer<LiteralType>::value && !std::is_same<LiteralType, void*>::value) { return false; }
        return true;
    }

    template<typename LiteralType, bool isAllUnderlyingBitsZero>
    static void f(LiteralType* out) noexcept
    {
        if constexpr(isAllUnderlyingBitsZero)
        {
            constexpr LiteralType v = PochiVM::get_all_bits_zero_value<LiteralType>();
            *out = v;
        }
        else
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(LiteralType);
            *out = CONSTANT_PLACEHOLDER_0;
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("literalType"),
                    CreateBoolMetaVar("isAllUnderlyingBitsZero")
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
    RegisterBoilerplate<FastInterpLiteralImpl>();
}
