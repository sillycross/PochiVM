#pragma once

namespace PochiVM
{

enum class FILoopConditionShapeCategory
{
    COMPLEX,
    VARIABLE,
    LITERAL_TRUE,
    // Comparison in the shape of 'var/literal op var/literal' which are 32/64 bit integers
    //
    SIMPLE_COMPARISON,
    X_END_OF_ENUM
};

enum class FILoopConditionOperandShapeCategory
{
    LITERAL_ZERO,
    LITERAL_NONZERO,
    VARIABLE,
    X_END_OF_ENUM
};

}   // namespace PochiVM
