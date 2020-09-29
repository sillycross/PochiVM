#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_logical_expr.h"
#include "fastinterp_tpl_common.hpp"

namespace PochiVM
{

struct FILogicalExprImpl
{
    template<FILogicalOperatorLv2TreeMask treeMaskEnum>
    static constexpr bool cond()
    {
        return true;
    }

#define LOGIC_EXPR_GENERATE_LV1_METHOD(meth_name, placeholder1, placeholder2)                       \
    template<int lv1TreeMask>                                                                       \
    static bool WARN_UNUSED __attribute__((__always_inline__)) meth_name()                          \
    {                                                                                               \
        constexpr FILogicalOperatorTree<1> opTree { lv1TreeMask };                                  \
        bool result;                                                                                \
        if constexpr(opTree.IsSingleton())                                                          \
        {                                                                                           \
            INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholder1, bool(*)() noexcept);        \
            result = BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholder1();                              \
        }                                                                                           \
        else                                                                                        \
        {                                                                                           \
            {                                                                                       \
                constexpr FILogicalOperatorTree<0> lhs = opTree.GetLhs();                           \
                INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholder1, bool(*)() noexcept);    \
                result = BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholder1();                          \
                if constexpr(lhs.HasTopLevelNot())                                                  \
                {                                                                                   \
                    result = !result;                                                               \
                }                                                                                   \
                if constexpr(opTree.IsAnd())                                                        \
                {                                                                                   \
                    if (!result)                                                                    \
                    {                                                                               \
                        goto short_circuit;                                                         \
                    }                                                                               \
                }                                                                                   \
                else                                                                                \
                {                                                                                   \
                    static_assert(opTree.IsOr());                                                   \
                    if (result)                                                                     \
                    {                                                                               \
                        goto short_circuit;                                                         \
                    }                                                                               \
                }                                                                                   \
            }                                                                                       \
            {                                                                                       \
                constexpr FILogicalOperatorTree<0> rhs = opTree.GetRhs();                           \
                INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholder2, bool(*)() noexcept);    \
                result = BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholder2();                          \
                if constexpr(rhs.HasTopLevelNot())                                                  \
                {                                                                                   \
                    result = !result;                                                               \
                }                                                                                   \
            }                                                                                       \
        }                                                                                           \
short_circuit:                                                                                      \
        if (opTree.HasTopLevelNot())                                                                \
        {                                                                                           \
            result = !result;                                                                       \
        }                                                                                           \
        return result;                                                                              \
    }

    LOGIC_EXPR_GENERATE_LV1_METHOD(eval_0_1, 0, 1)
    LOGIC_EXPR_GENERATE_LV1_METHOD(eval_2_3, 2, 3)

#undef LOGIC_EXPR_GENERATE_LV1_METHOD

    template<FILogicalOperatorLv2TreeMask treeMaskEnum>
    static bool f() noexcept
    {
        constexpr FILogicalOperatorTree<2> opTree { static_cast<int>(treeMaskEnum) };
        bool result;
        if constexpr(opTree.IsSingleton())
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(bool(*)() noexcept);
            result = BOILERPLATE_FNPTR_PLACEHOLDER_0();
        }
        else
        {
            {
                result = eval_0_1<opTree.GetLhs().GetValue()>();
                if constexpr(opTree.IsAnd())
                {
                    if (!result)
                    {
                        goto short_circuit;
                    }
                }
                else
                {
                    static_assert(opTree.IsOr());
                    if (result)
                    {
                        goto short_circuit;
                    }
                }
            }
            {
                result = eval_2_3<opTree.GetRhs().GetValue()>();
            }
        }
short_circuit:
        if constexpr(opTree.HasTopLevelNot())
        {
            result = !result;
        }
        return result;
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateEnumMetaVar<FILogicalOperatorLv2TreeMask::X_END_OF_ENUM>("lv2OperatorTreeMask")
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
    RegisterBoilerplate<FILogicalExprImpl>();
}
