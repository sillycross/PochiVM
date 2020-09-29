#pragma once

#include "common.h"
#include "fastinterp_tpl_abi_distinct_type_helper.h"

namespace PochiVM {
namespace math {

constexpr int power(int base, int exp)
{
    int result = 1;
    for (int i = 0; i < exp; i++) result *= base;
    return result;
}

}   // namespace math
}   // namespace PochiVM
