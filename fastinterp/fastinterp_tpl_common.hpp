#pragma once

// This file should only be included by fastinterp_tpl_*.cpp
//
#ifndef POCHIVM_INSIDE_FASTINTERP_TPL_CPP
static_assert(false, "This file should only be included by fastinterp_tpl_*.cpp");
#endif

#include "metavar.hpp"
#include "dynamic_specialization_utils.hpp"
#include "fastinterp_context.h"
#include "pochivm/cxx2a_bit_cast_helper.h"
#include "fastinterp_tpl_boilerplate_attributes.hpp"

// fastinterp_tpl
//
//    Interp execution is slow, and one important reason is that the control flow
//    is coupled with data. To interp execute any operator, one must make an *indirect*
//    function call, hurting CPU pipelining. And the values which are constant to an operator
//    (e.g. its parameters and its input operators) still must be retrieved from memory,
//    resulting in cache misses.
//
//    FastInterp is a solution to the above issue. Each AST node still correspond
//    to a function which interp-executes this node. However, the initial function will contain
//    'placeholders', which are placeholder values that must be populated before execution.
//
//    This allows the function to refer to its operator-specific configurations as placeholders,
//    which will be directly "burned into" the binary later, getting rid of the indirect
//    function calls, or cache-missing memory accesses.
//
//    To execute such a "function boilerplate" with placeholders, we make a copy of the function
//    binary, and then populate the placeholder values, using a process similar to relocation of
//    dynamic libraries. Of course, the function binaries are pre-processed, so that at runtime
//    all we need to do is a memcpy of the function body plus a few fixed arithmetic operations.
//    This retains interp execution's advantage of extremely low setup time, while giving
//    drastically better performance than naive interpretation approach.
//
//    This file contains those "function boilerplates" for interp execution.
//
//    Since the function binary is copied to elsewhere before execution and we don't want to fix
//    the exception frames (for both engineering complexity and performance reasons), there is no
//    native C++ exception support. We soft-emulate C++ exception using a combination of C++ code
//    stubs (which reside outside this file and does not use placeholders) and soft-implemented
//    destructor sequence logic.
//

// Communication interface with build_fast_interp_lib.cpp
// This is used to obtain the symbol names of all the boilerplates, similar to the trick we used in dump_symbols.cpp
//
void __pochivm_register_fast_interp_boilerplate__(const char* /*stringified_name*/,
                                                  PochiVM::MetaVarMaterializedList* /*list*/,
                                                  PochiVM::FIAttribute* /*attr*/);

namespace PochiVM
{

// Helper function to get local variable address
// this function must be always inlined FOR CORRECTNESS, otherwise it would become an
// unexpected external symbol and fire an assert in build_fast_interp_lib.cpp.
//
template<typename LocalVarType>
inline LocalVarType* __attribute__((__always_inline__)) GetLocalVarAddress(uintptr_t stackframe, uint64_t offset) noexcept
{
    return reinterpret_cast<LocalVarType*>(stackframe + offset);
}

// Whether a type is suitable to be used as an array index
// To make the number of templates under control, we only inline the most common index types:
// unsigned/signed 32/64 bits
//
template<typename T>
constexpr bool is_valid_index_type()
{
    if (std::is_same<T, int32_t>::value) { return true; }
    if (std::is_same<T, int64_t>::value) { return true; }
    if (std::is_same<T, uint32_t>::value) { return true; }
    if (std::is_same<T, uint64_t>::value) { return true; }
    return false;
}

template<typename T>
void RegisterBoilerplate(FIAttribute attr = FIAttribute())
{
    MetaVarMaterializedList list = T::metavars().template Materialize<T>();
#ifdef FASTINTERP_TPL_USE_MEDIUM_MCMODEL
    ReleaseAssert(attr.HasAttribute(FIAttribute::CodeModelMedium));
#endif
#ifdef FASTINTERP_TPL_USE_LARGE_MCMODEL
    ReleaseAssert(attr.HasAttribute(FIAttribute::CodeModelLarge));
#endif
    __pochivm_register_fast_interp_boilerplate__(__pochivm_stringify_type__<T>(), &list, &attr);
}

}   // namespace PochiVM
