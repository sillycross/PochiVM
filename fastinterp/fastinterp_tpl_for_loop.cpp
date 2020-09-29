#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_for_loop.h"
#include "fastinterp_tpl_common.hpp"

namespace PochiVM
{

struct FIForLoopImpl
{
    template<typename CondOperatorType>
    static constexpr bool cond()
    {
        if (!std::is_same<CondOperatorType, int32_t>::value &&
            !std::is_same<CondOperatorType, uint32_t>::value &&
            !std::is_same<CondOperatorType, int64_t>::value &&
            !std::is_same<CondOperatorType, uint64_t>::value)
        {
            return false;
        }
        return true;
    }

    template<typename CondOperatorType,
             FILoopConditionShapeCategory condShape>
    static constexpr bool cond()
    {
        if (condShape != FILoopConditionShapeCategory::SIMPLE_COMPARISON)
        {
            if (!std::is_same<CondOperatorType, int32_t>::value) { return false; }
        }
        return true;
    }

    template<typename CondOperatorType,
             FILoopConditionShapeCategory condShape,
             AstComparisonExprType condComparator>
    static constexpr bool cond()
    {
        if (condShape != FILoopConditionShapeCategory::SIMPLE_COMPARISON)
        {
            if (condComparator != AstComparisonExprType::EQUAL) { return false; }
        }
        if (condComparator != AstComparisonExprType::EQUAL &&
            condComparator != AstComparisonExprType::LESS_THAN &&
            condComparator != AstComparisonExprType::LESS_EQUAL)
        {
            return false;
        }
        return true;
    }

    template<typename CondOperatorType,
             FILoopConditionShapeCategory condShape,
             AstComparisonExprType condComparator,
             FILoopConditionOperandShapeCategory condLhsShape>
    static constexpr bool cond()
    {
        if (condShape != FILoopConditionShapeCategory::SIMPLE_COMPARISON)
        {
            if (condLhsShape != FILoopConditionOperandShapeCategory::LITERAL_ZERO) { return false; }
        }
        else
        {
            if (sizeof(CondOperatorType) != 8 && condLhsShape == FILoopConditionOperandShapeCategory::LITERAL_ZERO)
            {
                return false;
            }
        }
        return true;
    }

    template<typename CondOperatorType,
             FILoopConditionShapeCategory condShape,
             AstComparisonExprType condComparator,
             FILoopConditionOperandShapeCategory condLhsShape,
             FILoopConditionOperandShapeCategory condRhsShape>
    static constexpr bool cond()
    {
        if (condShape != FILoopConditionShapeCategory::SIMPLE_COMPARISON)
        {
            if (condRhsShape != FILoopConditionOperandShapeCategory::LITERAL_ZERO) { return false; }
        }
        else
        {
            if (sizeof(CondOperatorType) != 8 && condRhsShape == FILoopConditionOperandShapeCategory::LITERAL_ZERO)
            {
                return false;
            }
            // At least one side of the comparison should be a variable
            //
            if (condLhsShape != FILoopConditionOperandShapeCategory::VARIABLE &&
                condRhsShape != FILoopConditionOperandShapeCategory::VARIABLE)
            {
                return false;
            }
        }
        return true;
    }

    template<typename CondOperatorType,
             FILoopConditionShapeCategory condShape,
             AstComparisonExprType condComparator,
             FILoopConditionOperandShapeCategory condLhsShape,
             FILoopConditionOperandShapeCategory condRhsShape,
             FIForLoopBodyNumStatements bodyNumStmtsEnum,
             FIForLoopBodyMayCFRMask mayCFRMaskEnum>
    static constexpr bool cond()
    {
        constexpr int numStmts = static_cast<int>(bodyNumStmtsEnum);
        constexpr int mayCFRMask = static_cast<int>(mayCFRMaskEnum);
        if (mayCFRMask >= (1 << numStmts))
        {
            return false;
        }
        return true;
    }

    template<typename CondOperatorType,
             FILoopConditionShapeCategory condShape,
             AstComparisonExprType condComparator,
             FILoopConditionOperandShapeCategory condLhsShape,
             FILoopConditionOperandShapeCategory condRhsShape,
             FIForLoopBodyNumStatements bodyNumStmtsEnum,
             FIForLoopBodyMayCFRMask mayCFRMaskEnum,
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
             FILoopConditionShapeCategory condShape,
             AstComparisonExprType condComparator,
             FILoopConditionOperandShapeCategory condLhsShape,
             FILoopConditionOperandShapeCategory condRhsShape,
             FIForLoopBodyNumStatements bodyNumStmtsEnum,
             FIForLoopBodyMayCFRMask mayCFRMaskEnum,
             FIForLoopStepNumStatements stepNumStmtsEnum>
    static typename std::conditional<static_cast<int>(mayCFRMaskEnum) == 0, void, InterpControlSignal>::type f() noexcept
    {
        using ReturnType = typename std::conditional<static_cast<int>(mayCFRMaskEnum) == 0, void, InterpControlSignal>::type;

        while (true)
        {
            // Evaluate loop condition
            //
            if constexpr(condShape == FILoopConditionShapeCategory::COMPLEX)
            {
                DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(bool(*)() noexcept);
                if (!BOILERPLATE_FNPTR_PLACEHOLDER_0())
                {
                    break;
                }
            }
            else if constexpr(condShape == FILoopConditionShapeCategory::VARIABLE)
            {
                DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
                if (!*GetLocalVarAddress<bool>(CONSTANT_PLACEHOLDER_0))
                {
                    break;
                }
            }
            else if constexpr(condShape == FILoopConditionShapeCategory::SIMPLE_COMPARISON)
            {
                CondOperatorType lhs, rhs;
                if constexpr(condLhsShape == FILoopConditionOperandShapeCategory::VARIABLE)
                {
                    DEFINE_CONSTANT_PLACEHOLDER_0(uint32_t);
                    lhs = *GetLocalVarAddress<CondOperatorType>(CONSTANT_PLACEHOLDER_0);
                }
                else if constexpr(condLhsShape == FILoopConditionOperandShapeCategory::LITERAL_NONZERO)
                {
                    DEFINE_CONSTANT_PLACEHOLDER_0(CondOperatorType);
                    lhs = CONSTANT_PLACEHOLDER_0;
                }
                else
                {
                    static_assert(condLhsShape == FILoopConditionOperandShapeCategory::LITERAL_ZERO);
                    lhs = 0;
                }
                if constexpr(condRhsShape == FILoopConditionOperandShapeCategory::VARIABLE)
                {
                    DEFINE_CONSTANT_PLACEHOLDER_1(uint32_t);
                    rhs = *GetLocalVarAddress<CondOperatorType>(CONSTANT_PLACEHOLDER_1);
                }
                else if constexpr(condRhsShape == FILoopConditionOperandShapeCategory::LITERAL_NONZERO)
                {
                    DEFINE_CONSTANT_PLACEHOLDER_1(CondOperatorType);
                    rhs = CONSTANT_PLACEHOLDER_1;
                }
                else
                {
                    static_assert(condRhsShape == FILoopConditionOperandShapeCategory::LITERAL_ZERO);
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
                static_assert(condShape == FILoopConditionShapeCategory::LITERAL_TRUE);
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
            }

#undef EXECUTE_STMT

            static_assert(static_cast<int>(FIForLoopBodyNumStatements::X_END_OF_ENUM) == 5 + 1);

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
                EXECUTE_STMT(0, 6)
                EXECUTE_STMT(1, 7)
                EXECUTE_STMT(2, 8)
                EXECUTE_STMT(3, 9)
            }

#undef EXECUTE_STMT

            static_assert(static_cast<int>(FIForLoopStepNumStatements::X_END_OF_ENUM) == 4 + 1);
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
                    CreateEnumMetaVar<FILoopConditionShapeCategory::X_END_OF_ENUM>("condShape"),
                    CreateEnumMetaVar<AstComparisonExprType::X_END_OF_ENUM>("condComparator"),
                    CreateEnumMetaVar<FILoopConditionOperandShapeCategory::X_END_OF_ENUM>("condLhsOperandShape"),
                    CreateEnumMetaVar<FILoopConditionOperandShapeCategory::X_END_OF_ENUM>("condRhsOperandShape"),
                    CreateEnumMetaVar<FIForLoopBodyNumStatements::X_END_OF_ENUM>("bodyNumStmts"),
                    CreateEnumMetaVar<FIForLoopBodyMayCFRMask::X_END_OF_ENUM>("mayCFRMask"),
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
