#pragma once

#include "fastinterp_tpl_common.hpp"

namespace PochiVM
{

// Internally a constant placeholder is actually a symbol address
// In small code model, the symbol address is assumed to fit in int32_t,
// so if the boilerplate is compiled under small code model, it must subject to this constraint.
//
template<typename T>
constexpr bool IsConstantValidInSmallCodeModel()
{
    if (sizeof(T) > 4) { return false; }
    if (std::is_same<T, uint32_t>::value) { return false; }
    return true;
}

}   // namespace PochiVM
