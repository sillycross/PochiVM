#pragma once

// This file should only be included by fastinterp_tpl_*.cpp or build_fast_interp_lib.cpp
//
#ifndef POCHIVM_INSIDE_FASTINTERP_TPL_CPP
#ifndef POCHIVM_INSIDE_BUILD_FASTINTERP_LIB_CPP
#ifndef POCHIVM_INSIDE_METAVAR_UNIT_TEST_CPP
static_assert(false, "This file should only be included by fastinterp_tpl_*.cpp or build_fast_interp_lib.cpp");
#endif
#endif
#endif

#ifdef FASTINTERP_TPL_USE_MEDIUM_MCMODEL
#ifdef FASTINTERP_TPL_USE_LARGE_MCMODEL
static_assert(false, "FASTINTERP_TPL_USE_MEDIUM_MCMODEL and FASTINTERP_TPL_USE_LARGE_MCMODEL cannot be both defined!");
#endif
#endif

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
    // The translational unit shall be compiled using 'medium' code model.
    // Note that this is a per-translational-unit attribute: all boilerplate pack in one
    // translational unit must use the same code model attribute, or the builder fires an assert.
    //
#if defined(FASTINTERP_TPL_USE_MEDIUM_MCMODEL) || defined(POCHIVM_INSIDE_BUILD_FASTINTERP_LIB_CPP)
    const static FIAttribute CodeModelMedium;
#endif
    // The translational unit shall be compiled using 'large' code model. Same as above.
    //
#if defined(FASTINTERP_TPL_USE_LARGE_MCMODEL) || defined(POCHIVM_INSIDE_BUILD_FASTINTERP_LIB_CPP)
    const static FIAttribute CodeModelLarge;
#endif

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
#if defined(FASTINTERP_TPL_USE_MEDIUM_MCMODEL) || defined(POCHIVM_INSIDE_BUILD_FASTINTERP_LIB_CPP)
inline const FIAttribute FIAttribute::CodeModelMedium = FIAttribute(1 << 4);
#endif
#if defined(FASTINTERP_TPL_USE_LARGE_MCMODEL) || defined(POCHIVM_INSIDE_BUILD_FASTINTERP_LIB_CPP)
inline const FIAttribute FIAttribute::CodeModelLarge = FIAttribute(1 << 5);
#endif

}   // namespace PochiVM
