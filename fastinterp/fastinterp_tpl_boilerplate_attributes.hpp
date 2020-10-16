#pragma once

#include "pochivm/common.h"
#include "simple_constexpr_power_helper.h"

namespace PochiVM
{

class FIAttribute
{
public:
    FIAttribute() : m_attr(0) {}

    const static FIAttribute CDecl;
    const static FIAttribute OptSize;
    const static FIAttribute NoContinuation;

    bool WARN_UNUSED HasAttribute(FIAttribute attr)
    {
        ReleaseAssert(math::is_power_of_2(attr.m_attr));
        return (m_attr & attr.m_attr) != 0;
    }

    FIAttribute operator|(const FIAttribute& other) const
    {
        return FIAttribute(m_attr | other.m_attr);
    }

private:
    FIAttribute(int attr) : m_attr(attr) { }
    int m_attr;
};

inline const FIAttribute FIAttribute::CDecl = FIAttribute(1 << 0);
inline const FIAttribute FIAttribute::OptSize = FIAttribute(1 << 1);
inline const FIAttribute FIAttribute::NoContinuation = FIAttribute(1 << 2);

}   // namespace PochiVM
