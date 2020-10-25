#pragma once

#include "pochivm/common.h"
#include "simple_constexpr_power_helper.h"

namespace PochiVM
{

class FIAttribute
{
public:
    FIAttribute() : m_attr(0) {}

    // The calling convention of this boilerplate should use CDecl convention, instead of GHC
    //
    const static FIAttribute CDecl;
    // The boilerplate should be compiled with LLVM 'optsize' option
    //
    const static FIAttribute OptSize;
    // The builder should assert that this boilerplate has no continuation.
    // If this option is not given, the builder instead asserts that it must have a continuation
    //
    const static FIAttribute NoContinuation;
    // At the end of the boilerplate binary code, a 'ud2' trap instruction shall be appended.
    // May only be used if 'NoContinuation' attribute is also given.
    // This is helpful if the boilerplate ends with an indirect branch. The trap instruction hints
    // CPU pipeline to not speculate for fallthrough (which is always true in our use case).
    //
    const static FIAttribute AppendUd2;

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
inline const FIAttribute FIAttribute::AppendUd2 = FIAttribute(1 << 3);

}   // namespace PochiVM
