#pragma once

#include "pochivm/common.h"

namespace PochiVM
{

constexpr int x_fastinterp_internal_max_cfr = 16;

struct FIInternalPopcountTable
{
    constexpr FIInternalPopcountTable() : result()
    {
        result[0] = 0;
        for (size_t i = 1; i < (1U << x_fastinterp_internal_max_cfr); i++)
        {
            result[i] = result[i / 2] + static_cast<int>(i % 2);
        }
    }
    int result[1U << x_fastinterp_internal_max_cfr];
};

constexpr FIInternalPopcountTable x_fastinterp_internal_popcount_table;

template<size_t maxlimit>
constexpr bool FICheckIsUnderCfrLimit(const std::array<int, maxlimit>& limit, int numStmts, int stmtMask)
{
    assert(static_cast<size_t>(numStmts) < maxlimit && 0 <= stmtMask);
    if (stmtMask >= (1 << numStmts)) { return false; }
    if (limit[static_cast<size_t>(numStmts)] == -1) { return true; }
    int value = stmtMask & (~(1 << (numStmts - 1)));
    return x_fastinterp_internal_popcount_table.result[value] <= limit[static_cast<size_t>(numStmts)];
}

}   // namespace PochiVM
