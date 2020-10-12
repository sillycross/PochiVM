#pragma once

#include "pochivm/common.h"

namespace PochiVM
{

class FastInterpBoilerplateInstance;

class FISpillLocation
{
public:
    constexpr FISpillLocation() : m_location(x_nospill) {}

    bool IsNoSpill() const
    {
        return m_location == x_nospill;
    }

    void SetSpillLocation(uint32_t loc)
    {
        TestAssert(IsNoSpill() && loc != x_nospill);
        m_location = loc;
    }

    uint64_t GetSpillLocation() const
    {
        TestAssert(!IsNoSpill());
        return m_location;
    }

    void PopulatePlaceholderIfSpill(FastInterpBoilerplateInstance* inst, uint32_t ph1);

private:
    static constexpr uint32_t x_nospill = static_cast<uint32_t>(-1);
    uint32_t m_location;
};

inline constexpr FISpillLocation x_FINoSpill = FISpillLocation();

}   // namespace PochiVM
