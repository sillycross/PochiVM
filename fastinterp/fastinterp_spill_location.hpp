#pragma once

#include "fastinterp_spill_location.h"
#include "fastinterp_codegen_helper.h"

namespace PochiVM
{

inline void FISpillLocation::PopulatePlaceholderIfSpill(FastInterpBoilerplateInstance* inst, uint32_t ph1)
{
    if (!IsNoSpill())
    {
        inst->PopulateConstantPlaceholder<uint64_t>(ph1, static_cast<uint64_t>(m_location));
    }
}

}   // namespace PochiVM
