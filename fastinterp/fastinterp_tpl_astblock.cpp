#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_helper.h"

namespace PochiVM
{

struct FIBlockImpl
{
    template<FIBlockNumStatements numStmtsEnum,
             FIBlockMayCFRMask mayCFRMaskEnum>
    static constexpr bool cond()
    {
        constexpr int numStmts = static_cast<int>(numStmtsEnum);
        constexpr int mayCFRMask = static_cast<int>(mayCFRMaskEnum);
        if (mayCFRMask >= (1 << numStmts)) { return false; }
        return true;
    }

    // BoilerplateFn placeholder 0 - n: every statement
    //
    template<FIBlockNumStatements numStmtsEnum,
             FIBlockMayCFRMask mayCFRMaskEnum>
    static typename std::conditional<static_cast<int>(mayCFRMaskEnum) == 0, void, InterpControlSignal>::type f() noexcept
    {
        using ReturnType = typename std::conditional<static_cast<int>(mayCFRMaskEnum) == 0, void, InterpControlSignal>::type;

#define EXECUTE_STMT(stmtOrd, placeholderOrd)                                                                      \
    if constexpr(numStmts > (stmtOrd))                                                                             \
    {                                                                                                              \
        if constexpr((mayCFRMask & (1 << (stmtOrd))) != 0)                                                         \
        {                                                                                                          \
            INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholderOrd, InterpControlSignal(*)() noexcept);      \
            InterpControlSignal ics = BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholderOrd();                          \
            if (ics != InterpControlSignal::None)                                                                  \
            {                                                                                                      \
                return ics;                                                                                        \
            }                                                                                                      \
        }                                                                                                          \
        else                                                                                                       \
        {                                                                                                          \
            INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholderOrd, void(*)() noexcept);                     \
            BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholderOrd();                                                    \
        }                                                                                                          \
    }

        constexpr int numStmts = static_cast<int>(numStmtsEnum);
        constexpr int mayCFRMask = static_cast<int>(mayCFRMaskEnum);

        EXECUTE_STMT(0, 0)
        EXECUTE_STMT(1, 1)
        EXECUTE_STMT(2, 2)
        EXECUTE_STMT(3, 3)
        EXECUTE_STMT(4, 4)
        EXECUTE_STMT(5, 5)
        EXECUTE_STMT(6, 6)
        EXECUTE_STMT(7, 7)
        EXECUTE_STMT(8, 8)
        EXECUTE_STMT(9, 9)
        EXECUTE_STMT(10, 10)

        if constexpr(!std::is_same<ReturnType, void>::value)
        {
            return InterpControlSignal::None;
        }
        static_assert(static_cast<int>(FIBlockNumStatements::X_END_OF_ENUM) == 11 + 1);

#undef EXECUTE_STMT
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateEnumMetaVar<FIBlockNumStatements::X_END_OF_ENUM>("numStmts"),
                    CreateEnumMetaVar<FIBlockMayCFRMask::X_END_OF_ENUM>("mayCFRMask")
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
    RegisterBoilerplate<FIBlockImpl>();
}
