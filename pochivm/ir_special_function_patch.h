#pragma once

#include <exception>

// clang++ generates some special internal functions,
// which has visibility=hidden (so not accessible from outside)
//
// The generated code will not be able to call into the host process to call
// those functions, so we must replace them with the implementation below.
//
// Those functions are not directly called by any C++ code,
// but we will replace the IR CallInstruction from calling the clang internal version
// to point to the functions below.
//

// From libc++abi Specification
//
extern "C" void* __cxa_begin_catch(void* exceptionObject) noexcept(true);

// Our replacement for __clang_call_terminate
// '__used__' is required so the symbol is not dropped despite it's never called directly in C++
//
// Reverse-translated from the following IR of __clang_call_terminate:
//     ; Function Attrs: noinline noreturn nounwind
//     define linkonce_odr hidden void @__clang_call_terminate(i8* %0) local_unnamed_addr comdat {
//       %2 = tail call i8* @__cxa_begin_catch(i8* %0)
//       tail call void @_ZSt9terminatev()
//       unreachable
//     }
//
extern "C" __attribute__((__noreturn__, __noinline__, __used__))
inline void __pochivm_clang_call_terminate(void* ptr) noexcept(true)
{
    __cxa_begin_catch(ptr);
    std::terminate();
}

namespace PochiVM
{

// The list of all functions to replace, each entry is <original function, our replacement>
//
const char* const x_ir_special_function_replacement_list[][2] = {
    { "__clang_call_terminate", "__pochivm_clang_call_terminate" }
};

}   // namespace PochiVM
