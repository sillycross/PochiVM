#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_for_loop.h"
#include "fastinterp_tpl_cfr_limit_checker.hpp"
#include "fastinterp_tpl_common.hpp"

namespace PochiVM
{

struct FIForLoopImpl
{
    template<typename CondOperatorType>
    static constexpr bool cond()
    {
        return FIConditionCombChecker::cond<CondOperatorType>();
    }

    template<typename CondOperatorType,
             FIForLoopBodyNumStatements bodyNumStmtsEnum,
             FIForLoopBodyMayCFRMask mayCFRMaskEnum>
    static constexpr bool cond()
    {
        constexpr int numStmts = static_cast<int>(bodyNumStmtsEnum);
        constexpr int mayCFRMask = static_cast<int>(mayCFRMaskEnum);
        return FICheckIsUnderCfrLimit(x_fastinterp_for_loop_cfr_limit, numStmts, mayCFRMask);
    }

    template<typename CondOperatorType,
             FIForLoopBodyNumStatements bodyNumStmtsEnum,
             FIForLoopBodyMayCFRMask mayCFRMaskEnum,
             FIConditionShapeCategory condShape>
    static constexpr bool cond()
    {
        return FIConditionCombChecker::cond<CondOperatorType, condShape>();
    }

    template<typename CondOperatorType,
             FIForLoopBodyNumStatements bodyNumStmtsEnum,
             FIForLoopBodyMayCFRMask mayCFRMaskEnum,
             FIConditionShapeCategory condShape,
             AstComparisonExprType condComparator>
    static constexpr bool cond()
    {
        return FIConditionCombChecker::cond<CondOperatorType, condShape, condComparator>();
    }

    template<typename CondOperatorType,
             FIForLoopBodyNumStatements bodyNumStmtsEnum,
             FIForLoopBodyMayCFRMask mayCFRMaskEnum,
             FIConditionShapeCategory condShape,
             AstComparisonExprType condComparator,
             FIConditionOperandShapeCategory condLhsShape>
    static constexpr bool cond()
    {
        return FIConditionCombChecker::cond<CondOperatorType, condShape, condComparator, condLhsShape>();
    }

    template<typename CondOperatorType,
             FIForLoopBodyNumStatements bodyNumStmtsEnum,
             FIForLoopBodyMayCFRMask mayCFRMaskEnum,
             FIConditionShapeCategory condShape,
             AstComparisonExprType condComparator,
             FIConditionOperandShapeCategory condLhsShape,
             FIConditionOperandShapeCategory condRhsShape>
    static constexpr bool cond()
    {
        return FIConditionCombChecker::cond<CondOperatorType, condShape, condComparator, condLhsShape, condRhsShape>();
    }

    template<typename CondOperatorType,
             FIForLoopBodyNumStatements bodyNumStmtsEnum,
             FIForLoopBodyMayCFRMask mayCFRMaskEnum,
             FIConditionShapeCategory condShape,
             AstComparisonExprType condComparator,
             FIConditionOperandShapeCategory condLhsShape,
             FIConditionOperandShapeCategory condRhsShape,
             FIForLoopStepNumStatements stepNumStmtsEnum>
    static constexpr bool cond()
    {
        return true;
    }

    // If 'condShape' is SIMPLE_COMPARISON,
    // 'CondOperatorType', 'condComparator', 'condLhsIsVariable', 'condRhsIsVariable' describes the type of comparison,
    // and constant boilerplate 0/1 is used to describe the data of comparison.
    // Otherwise, the dummy CondOperatorType = int32_t, condComparator = EQUAL, condLhsIsVariable = condRhsIsVariable = false shall be used,
    // and function boilerplate 0 or constant boilerplate 0 is used to describe the comparison.
    //
    // function boilerplate 1 - n is used for the body of the function.
    //
    template<typename CondOperatorType,
             FIForLoopBodyNumStatements bodyNumStmtsEnum,
             FIForLoopBodyMayCFRMask mayCFRMaskEnum,
             FIConditionShapeCategory condShape,
             AstComparisonExprType condComparator,
             FIConditionOperandShapeCategory condLhsShape,
             FIConditionOperandShapeCategory condRhsShape,
             FIForLoopStepNumStatements stepNumStmtsEnum>
    static typename std::conditional<static_cast<int>(mayCFRMaskEnum) == 0, void, InterpControlSignal>::type f() noexcept
    {
        using ReturnType = typename std::conditional<static_cast<int>(mayCFRMaskEnum) == 0, void, InterpControlSignal>::type;

        while (true)
        {
            // Evaluate loop condition
            //
            if constexpr(condShape == FIConditionShapeCategory::COMPLEX)
            {
                DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(bool(*)() noexcept);
                if (!BOILERPLATE_FNPTR_PLACEHOLDER_0())
                {
                    break;
                }
            }
            else if constexpr(condShape == FIConditionShapeCategory::VARIABLE)
            {
                DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
                if (!*GetLocalVarAddress<bool>(CONSTANT_PLACEHOLDER_0))
                {
                    break;
                }
            }
            else if constexpr(condShape == FIConditionShapeCategory::SIMPLE_COMPARISON)
            {
                CondOperatorType lhs, rhs;
                if constexpr(condLhsShape == FIConditionOperandShapeCategory::VARIABLE)
                {
                    DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
                    lhs = *GetLocalVarAddress<CondOperatorType>(CONSTANT_PLACEHOLDER_0);
                }
                else if constexpr(condLhsShape == FIConditionOperandShapeCategory::LITERAL_NONZERO)
                {
                    DEFINE_CONSTANT_PLACEHOLDER_0(CondOperatorType);
                    lhs = CONSTANT_PLACEHOLDER_0;
                }
                else
                {
                    static_assert(condLhsShape == FIConditionOperandShapeCategory::LITERAL_ZERO);
                    lhs = 0;
                }
                if constexpr(condRhsShape == FIConditionOperandShapeCategory::VARIABLE)
                {
                    DEFINE_CONSTANT_PLACEHOLDER_1(uint32_t);
                    rhs = *GetLocalVarAddress<CondOperatorType>(CONSTANT_PLACEHOLDER_1);
                }
                else if constexpr(condRhsShape == FIConditionOperandShapeCategory::LITERAL_NONZERO)
                {
                    DEFINE_CONSTANT_PLACEHOLDER_1(CondOperatorType);
                    rhs = CONSTANT_PLACEHOLDER_1;
                }
                else
                {
                    static_assert(condRhsShape == FIConditionOperandShapeCategory::LITERAL_ZERO);
                    rhs = 0;
                }
                bool comparisonResult;
                if constexpr(condComparator == AstComparisonExprType::EQUAL)
                {
                    comparisonResult = (lhs == rhs);
                }
                else if constexpr(condComparator == AstComparisonExprType::LESS_THAN)
                {
                    comparisonResult = (lhs < rhs);
                }
                else
                {
                    static_assert(condComparator == AstComparisonExprType::LESS_EQUAL);
                    comparisonResult = (lhs <= rhs);
                }
                if (!comparisonResult)
                {
                    break;
                }
            }
            else
            {
                static_assert(condShape == FIConditionShapeCategory::LITERAL_TRUE);
            }

            // Evaluate loop body
            //

#define EXECUTE_STMT(stmtOrd, placeholderOrd)                                                                      \
    if constexpr(numStmts > (stmtOrd))                                                                             \
    {                                                                                                              \
        if constexpr((mayCFRMask & (1 << (stmtOrd))) != 0)                                                         \
        {                                                                                                          \
            INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholderOrd, InterpControlSignal(*)() noexcept);      \
            InterpControlSignal ics = BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholderOrd();                          \
            if (ics != InterpControlSignal::None)                                                                  \
            {                                                                                                      \
                if (ics == InterpControlSignal::Break)                                                             \
                {                                                                                                  \
                    break;                                                                                         \
                }                                                                                                  \
                else if (ics == InterpControlSignal::Continue)                                                     \
                {                                                                                                  \
                    goto step_block;                                                                               \
                }                                                                                                  \
                else                                                                                               \
                {                                                                                                  \
                    return ics;                                                                                    \
                }                                                                                                  \
            }                                                                                                      \
        }                                                                                                          \
        else                                                                                                       \
        {                                                                                                          \
            INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholderOrd, void(*)() noexcept);                     \
            BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholderOrd();                                                    \
        }                                                                                                          \
    }

            {
                constexpr int numStmts = static_cast<int>(bodyNumStmtsEnum);
                constexpr int mayCFRMask = static_cast<int>(mayCFRMaskEnum);
                EXECUTE_STMT(0, 1)
                EXECUTE_STMT(1, 2)
                EXECUTE_STMT(2, 3)
                EXECUTE_STMT(3, 4)
                EXECUTE_STMT(4, 5)
                EXECUTE_STMT(5, 6)
                EXECUTE_STMT(6, 7)
                EXECUTE_STMT(7, 8)
            }

#undef EXECUTE_STMT

            static_assert(static_cast<int>(FIForLoopBodyNumStatements::X_END_OF_ENUM) == 8 + 1);

            // Evaluate step block
            //

#define EXECUTE_STMT(stmtOrd, placeholderOrd)                                                                      \
    if constexpr(numStmts > (stmtOrd))                                                                             \
    {                                                                                                              \
        INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholderOrd, void(*)() noexcept);                         \
        BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholderOrd();                                                        \
    }

            {
step_block:
                constexpr int numStmts = static_cast<int>(bodyNumStmtsEnum);
                EXECUTE_STMT(0, 9)
                EXECUTE_STMT(1, 10)
                EXECUTE_STMT(2, 11)
            }

#undef EXECUTE_STMT

            static_assert(static_cast<int>(FIForLoopStepNumStatements::X_END_OF_ENUM) == 3 + 1);
        }

        if constexpr(!std::is_same<ReturnType, void>::value)
        {
            return InterpControlSignal::None;
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("condOperatorType"),
                    CreateEnumMetaVar<FIForLoopBodyNumStatements::X_END_OF_ENUM>("bodyNumStmts"),
                    CreateEnumMetaVar<FIForLoopBodyMayCFRMask::X_END_OF_ENUM>("mayCFRMask"),
                    CreateEnumMetaVar<FIConditionShapeCategory::X_END_OF_ENUM>("condShape"),
                    CreateEnumMetaVar<AstComparisonExprType::X_END_OF_ENUM>("condComparator"),
                    CreateEnumMetaVar<FIConditionOperandShapeCategory::X_END_OF_ENUM>("condLhsOperandShape"),
                    CreateEnumMetaVar<FIConditionOperandShapeCategory::X_END_OF_ENUM>("condRhsOperandShape"),
                    CreateEnumMetaVar<FIForLoopStepNumStatements::X_END_OF_ENUM>("stepNumStmts")
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
    RegisterBoilerplate<FIForLoopImpl>();
}
