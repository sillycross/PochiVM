#pragma once

// This file is linked to both fastinterp_tpl.cpp and the PochiVM library.
//

#include "common.h"
#include <csetjmp>

namespace PochiVM
{

// From CppReference: "The std::jmp_buf type is an array type suitable for storing information to restore a calling environment."
// We decay it to a pointer type to store it in a non-owning way.
//
using std_jmp_buf_ptr = std::decay<std::jmp_buf>::type;
static_assert(std::is_pointer<std_jmp_buf_ptr>::value && sizeof(std_jmp_buf_ptr) == 8, "unexpected type");

// This is directly used as a thread_local for performance reasons.
// So it must be trivially constructible.
//
struct FastInterpContext
{
    // Current stack frame
    //
    uintptr_t m_stackFrame;

    // longjmp() target if a C++ exception is thrown out
    // The code should jump to it after calling all destructors.
    //
    std_jmp_buf_ptr m_ehTarget;
};

}   // namespace PochiVM

// Intentionally defined in root namespace so the symbol name is simply the variable name
// For fastinterp_tpl.cpp, we need to make it a real definition (instead of an ODR one)
// so accesses do not need to go through TLS wrapper
//
#ifndef INSIDE_FASTINTERP_TPL_CPP
inline thread_local PochiVM::FastInterpContext __pochivm_thread_fastinterp_context;
#else
extern thread_local PochiVM::FastInterpContext __pochivm_thread_fastinterp_context;
thread_local PochiVM::FastInterpContext __pochivm_thread_fastinterp_context;
#endif
