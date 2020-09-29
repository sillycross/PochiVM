#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_if_stmt.h"
#include "fastinterp_tpl_condition_shape.hpp"
#include "fastinterp_tpl_cfr_limit_checker.hpp"
#include "fastinterp_tpl_common.hpp"

namespace PochiVM
{

struct FIIfStatementImpl
{
    template<typename CondOperatorType>
    static constexpr bool cond()
    {
        return FIConditionCombChecker::cond<CondOperatorType>();
    }

    template<typename CondOperatorType,
             FIIfStmtNumStatements numTrueBranchStmtsEnum,
             FIIfStmtMayCFRMask trueBranchMayCFRMaskEnum>
    static constexpr bool cond()
    {
        constexpr int numStmts = static_cast<int>(numTrueBranchStmtsEnum);
        constexpr int mayCFRMask = static_cast<int>(trueBranchMayCFRMaskEnum);
        return FICheckIsUnderCfrLimit(x_fastinterp_if_stmt_cfr_limit, numStmts, mayCFRMask);
    }

    template<typename CondOperatorType,
             FIIfStmtNumStatements numTrueBranchStmtsEnum,
             FIIfStmtMayCFRMask trueBranchMayCFRMaskEnum,
             FIIfStmtNumStatements numFalseBranchStmtsEnum,
             FIIfStmtMayCFRMask falseBranchMayCFRMaskEnum>
    static constexpr bool cond()
    {
        constexpr int numStmts = static_cast<int>(numFalseBranchStmtsEnum);
        constexpr int mayCFRMask = static_cast<int>(falseBranchMayCFRMaskEnum);
        return FICheckIsUnderCfrLimit(x_fastinterp_if_stmt_cfr_limit, numStmts, mayCFRMask);
    }

    template<typename CondOperatorType,
             FIIfStmtNumStatements numTrueBranchStmtsEnum,
             FIIfStmtMayCFRMask trueBranchMayCFRMaskEnum,
             FIIfStmtNumStatements numFalseBranchStmtsEnum,
             FIIfStmtMayCFRMask falseBranchMayCFRMaskEnum,
             FIConditionShapeCategory condShape>
    static constexpr bool cond()
    {
        // Additionally disallow 'literal true' as if-condition: it's stupid, and it results
        // in compiler optimizing out the 'false' clause, firing a false positive assertion in placeholder checker
        //
        if (condShape == FIConditionShapeCategory::LITERAL_TRUE) { return false; }
        return FIConditionCombChecker::cond<CondOperatorType, condShape>();
    }

    template<typename CondOperatorType,
             FIIfStmtNumStatements numTrueBranchStmtsEnum,
             FIIfStmtMayCFRMask trueBranchMayCFRMaskEnum,
             FIIfStmtNumStatements numFalseBranchStmtsEnum,
             FIIfStmtMayCFRMask falseBranchMayCFRMaskEnum,
             FIConditionShapeCategory condShape,
             AstComparisonExprType condComparator>
    static constexpr bool cond()
    {
        return FIConditionCombChecker::cond<CondOperatorType, condShape, condComparator>();
    }

    template<typename CondOperatorType,
             FIIfStmtNumStatements numTrueBranchStmtsEnum,
             FIIfStmtMayCFRMask trueBranchMayCFRMaskEnum,
             FIIfStmtNumStatements numFalseBranchStmtsEnum,
             FIIfStmtMayCFRMask falseBranchMayCFRMaskEnum,
             FIConditionShapeCategory condShape,
             AstComparisonExprType condComparator,
             FIConditionOperandShapeCategory condLhsShape>
    static constexpr bool cond()
    {
        return FIConditionCombChecker::cond<CondOperatorType, condShape, condComparator, condLhsShape>();
    }

    template<typename CondOperatorType,
             FIIfStmtNumStatements numTrueBranchStmtsEnum,
             FIIfStmtMayCFRMask trueBranchMayCFRMaskEnum,
             FIIfStmtNumStatements numFalseBranchStmtsEnum,
             FIIfStmtMayCFRMask falseBranchMayCFRMaskEnum,
             FIConditionShapeCategory condShape,
             AstComparisonExprType condComparator,
             FIConditionOperandShapeCategory condLhsShape,
             FIConditionOperandShapeCategory condRhsShape>
    static constexpr bool cond()
    {
        return FIConditionCombChecker::cond<CondOperatorType, condShape, condComparator, condLhsShape, condRhsShape>();
    }

    template<FIIfStmtMayCFRMask trueBranchMayCFRMaskEnum, FIIfStmtMayCFRMask falseBranchMayCFRMaskEnum>
    using ReturnTypeHelper = typename std::conditional<
        static_cast<int>(trueBranchMayCFRMaskEnum) == 0 && static_cast<int>(falseBranchMayCFRMaskEnum) == 0,
        void, InterpControlSignal>::type;

    // BoilerplateFn placeholder 0: the condition
    // BoilerplateFn placeholder 1-5: the true branch
    // BoilerplateFn placeholder 6-10: the false branch
    //
    template<typename CondOperatorType,
             FIIfStmtNumStatements numTrueBranchStmtsEnum,
             FIIfStmtMayCFRMask trueBranchMayCFRMaskEnum,
             FIIfStmtNumStatements numFalseBranchStmtsEnum,
             FIIfStmtMayCFRMask falseBranchMayCFRMaskEnum,
             FIConditionShapeCategory condShape,
             AstComparisonExprType condComparator,
             FIConditionOperandShapeCategory condLhsShape,
             FIConditionOperandShapeCategory condRhsShape>
    static ReturnTypeHelper<trueBranchMayCFRMaskEnum, falseBranchMayCFRMaskEnum> f() noexcept
    {
        using ReturnType = ReturnTypeHelper<trueBranchMayCFRMaskEnum, falseBranchMayCFRMaskEnum>;

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

        if (FIConditionShapeHelper::get_0_1<CondOperatorType, condShape, condComparator, condLhsShape, condRhsShape>())
        {
            constexpr int numStmts = static_cast<int>(numTrueBranchStmtsEnum);
            constexpr int mayCFRMask = static_cast<int>(trueBranchMayCFRMaskEnum);
            EXECUTE_STMT(0, 1)
            EXECUTE_STMT(1, 2)
            EXECUTE_STMT(2, 3)
            EXECUTE_STMT(3, 4)
            EXECUTE_STMT(4, 5)

            if constexpr(!std::is_same<ReturnType, void>::value)
            {
                return InterpControlSignal::None;
            }
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

            if constexpr(!std::is_same<ReturnType, void>::value)
            {
                return InterpControlSignal::None;
            }
        }
        static_assert(static_cast<int>(FIIfStmtNumStatements::X_END_OF_ENUM) == 5 + 1);

#undef EXECUTE_STMT
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("condOperatorType"),
                    CreateEnumMetaVar<FIIfStmtNumStatements::X_END_OF_ENUM>("trueBranchNumStmts"),
                    CreateEnumMetaVar<FIIfStmtMayCFRMask::X_END_OF_ENUM>("trueBranchMayCFRMask"),
                    CreateEnumMetaVar<FIIfStmtNumStatements::X_END_OF_ENUM>("falseBranchNumStmts"),
                    CreateEnumMetaVar<FIIfStmtMayCFRMask::X_END_OF_ENUM>("falseBranchMayCFRMask"),
                    CreateEnumMetaVar<FIConditionShapeCategory::X_END_OF_ENUM>("condShape"),
                    CreateEnumMetaVar<AstComparisonExprType::X_END_OF_ENUM>("condComparator"),
                    CreateEnumMetaVar<FIConditionOperandShapeCategory::X_END_OF_ENUM>("condLhsOperandShape"),
                    CreateEnumMetaVar<FIConditionOperandShapeCategory::X_END_OF_ENUM>("condRhsOperandShape")
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
