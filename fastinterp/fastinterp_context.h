#pragma once

// This file is linked to both fastinterp_tpl.cpp and the PochiVM library.
//

#include "common.h"

namespace PochiVM
{

// This is directly used as a thread_local for performance reasons.
// So it must be trivially constructible.
//
struct FastInterpContext
{
    // Current stack frame
    //
    uintptr_t m_stackFrame;

    // __builtin_longjmp() target if a C++ exception is thrown out
    // The code should jump to it after calling all destructors.
    //
    // See fastinterp_tpl_helper.h on why we are using __builtin_setjmp/__builtin_longjmp
    // instead of glibc setjmp/longjmp
    //
    void** m_ehTarget;
};

}   // namespace PochiVM

// Intentionally defined in root namespace so the symbol name is simply the variable name
// For fastinterp_tpl.cpp, we need to make it a real definition (instead of an ODR one)
// so accesses do not need to go through TLS wrapper
//
#ifndef POCHIVM_INSIDE_FASTINTERP_TPL_CPP
inline thread_local PochiVM::FastInterpContext __pochivm_thread_fastinterp_context;
#else
extern thread_local PochiVM::FastInterpContext __pochivm_thread_fastinterp_context;
thread_local PochiVM::FastInterpContext __pochivm_thread_fastinterp_context;
#endif
