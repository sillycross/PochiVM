#pragma once

#include "common.h"

// This file is used by both pochivm and fastinterp
//

namespace PochiVM
{

// The function alignment of the generated fastinterp functions
// This must at least the function alignment used to compile fastinterp_tpl_*.cpp
// Default clang++ function alignment is 16.
//
constexpr size_t x_fastinterp_function_alignment = 16;

enum class AstArithmeticExprType
{
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    X_END_OF_ENUM
};

enum class AstComparisonExprType
{
    EQUAL,
    NOT_EQUAL,
    LESS_THAN,
    LESS_EQUAL,
    GREATER_THAN,
    GREATER_EQUAL,
    X_END_OF_ENUM
};

class PossibleControlSignals
{
public:
    PossibleControlSignals() : m_value(0) {}
    explicit PossibleControlSignals(uint32_t value)
        : m_value(value)
    {
        TestAssert(m_value < 8);
    }

    bool MayContinue() const { return (m_value & x_MAY_CONTINUE) != 0; }
    bool MayBreak() const { return (m_value & x_MAY_BREAK) != 0; }
    bool MayReturn() const { return (m_value & x_MAY_RETURN) != 0; }

    PossibleControlSignals WARN_UNUSED operator|(const PossibleControlSignals& other) const {
        return PossibleControlSignals { m_value | other.m_value };
    }
    PossibleControlSignals WARN_UNUSED SetMayContinue() const {
        return PossibleControlSignals { m_value | x_MAY_CONTINUE };
    }
    PossibleControlSignals WARN_UNUSED SetMayBreak() const {
        return PossibleControlSignals { m_value | x_MAY_BREAK };
    }
    PossibleControlSignals WARN_UNUSED SetMayReturn() const {
        return PossibleControlSignals { m_value | x_MAY_RETURN };
    }
    PossibleControlSignals WARN_UNUSED ClearMayContinue() const {
        return PossibleControlSignals { m_value & (~x_MAY_CONTINUE) };
    }
    PossibleControlSignals WARN_UNUSED ClearMayBreak() const {
        return PossibleControlSignals { m_value & (~x_MAY_BREAK) };
    }
    PossibleControlSignals WARN_UNUSED ClearMayReturn() const {
        return PossibleControlSignals { m_value & (~x_MAY_RETURN) };
    }

private:
    const static uint32_t x_MAY_CONTINUE = 1;
    const static uint32_t x_MAY_BREAK = 2;
    const static uint32_t x_MAY_RETURN = 4;
    uint32_t m_value;
};

// Interp mode control signal
// Used for control flow redirection statements (Break/Continue/Return)
//
enum class InterpControlSignal
{
    None,
    Break,
    Continue,
    Return
};

}   // namespace PochiVM
