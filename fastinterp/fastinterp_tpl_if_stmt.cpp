#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_helper.h"

namespace PochiVM
{

struct FIIfStatementImpl
{
    template<FIIfStmtNumStatements numTrueBranchStmtsEnum,
             FIIfStmtMayCFRMask trueBranchMayCFRMaskEnum>
    static constexpr bool cond()
    {
        constexpr int numStmts = static_cast<int>(numTrueBranchStmtsEnum);
        constexpr int mayCFRMask = static_cast<int>(trueBranchMayCFRMaskEnum);
        if (mayCFRMask >= (1 << numStmts)) { return false; }
        return true;
    }

    template<FIIfStmtNumStatements numTrueBranchStmtsEnum,
             FIIfStmtMayCFRMask trueBranchMayCFRMaskEnum,
             FIIfStmtNumStatements numFalseBranchStmtsEnum,
             FIIfStmtMayCFRMask falseBranchMayCFRMaskEnum>
    static constexpr bool cond()
    {
        constexpr int numStmts = static_cast<int>(numFalseBranchStmtsEnum);
        constexpr int mayCFRMask = static_cast<int>(falseBranchMayCFRMaskEnum);
        if (mayCFRMask >= (1 << numStmts)) { return false; }
        return true;
    }

    // BoilerplateFn placeholder 0: the condition
    // BoilerplateFn placeholder 1-5: the true branch
    // BoilerplateFn placeholder 6-10: the false branch
    //
    template<FIIfStmtNumStatements numTrueBranchStmtsEnum,
             FIIfStmtMayCFRMask trueBranchMayCFRMaskEnum,
             FIIfStmtNumStatements numFalseBranchStmtsEnum,
             FIIfStmtMayCFRMask falseBranchMayCFRMaskEnum>
    static void f([[maybe_unused]] InterpControlSignal* ics) noexcept
    {
        bool cond;
        DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(bool*) noexcept);
        BOILERPLATE_FNPTR_PLACEHOLDER_0(&cond);

#define EXECUTE_STMT(stmtOrd, placeholderOrd)                                                                      \
    if constexpr(numStmts > (stmtOrd))                                                                             \
    {                                                                                                              \
        if constexpr((mayCFRMask & (1 << (stmtOrd))) != 0)                                                         \
        {                                                                                                          \
            INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholderOrd, void(*)(InterpControlSignal*) noexcept); \
            BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholderOrd(ics);                                                 \
            if (*ics != InterpControlSignal::None)                                                                 \
            {                                                                                                      \
                return;                                                                                            \
            }                                                                                                      \
        }                                                                                                          \
        else                                                                                                       \
        {                                                                                                          \
            INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholderOrd, void(*)(InterpControlSignal*) noexcept); \
            BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholderOrd(nullptr);                                             \
        }                                                                                                          \
    }

        if (cond)
        {
            constexpr int numStmts = static_cast<int>(numTrueBranchStmtsEnum);
            constexpr int mayCFRMask = static_cast<int>(trueBranchMayCFRMaskEnum);
            EXECUTE_STMT(0, 1)
            EXECUTE_STMT(1, 2)
            EXECUTE_STMT(2, 3)
            EXECUTE_STMT(3, 4)
            EXECUTE_STMT(4, 5)
        }
        else
        {
            constexpr int numStmts = static_cast<int>(numFalseBranchStmtsEnum);
            constexpr int mayCFRMask = static_cast<int>(falseBranchMayCFRMaskEnum);
            EXECUTE_STMT(0, 6)
            EXECUTE_STMT(1, 7)
            EXECUTE_STMT(2, 8)
            EXECUTE_STMT(3, 9)
            EXECUTE_STMT(4, 10)
        }
        static_assert(static_cast<int>(FIIfStmtNumStatements::X_END_OF_ENUM) == 5 + 1);

#undef EXECUTE_STMT
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateEnumMetaVar<FIIfStmtNumStatements::X_END_OF_ENUM>("trueBranchNumStmts"),
                    CreateEnumMetaVar<FIIfStmtMayCFRMask::X_END_OF_ENUM>("trueBranchMayCFRMask"),
                    CreateEnumMetaVar<FIIfStmtNumStatements::X_END_OF_ENUM>("falseBranchNumStmts"),
                    CreateEnumMetaVar<FIIfStmtMayCFRMask::X_END_OF_ENUM>("falseBranchMayCFRMask")
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
    RegisterBoilerplate<FIIfStatementImpl>();
}
