#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_helper.h"

namespace PochiVM
{

struct FILiteralImpl
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
    static LiteralType f() noexcept
    {
        if constexpr(isAllUnderlyingBitsZero)
        {
            constexpr LiteralType v = PochiVM::get_all_bits_zero_value<LiteralType>();
            return v;
        }
        else
        {
            DEFINE_CONSTANT_PLACEHOLDER_0(LiteralType);
            return CONSTANT_PLACEHOLDER_0;
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
    RegisterBoilerplate<FILiteralImpl>();
}
