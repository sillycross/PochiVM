#pragma once

// Intentionally defined in root namespace, not PochiVM namespace
//

// Returns something like
//    const char *__pochivm_stringify_type__() [T = ###### ]
// where ###### is the interesting part
// WARNING: this breaks down when called outside a function.
//
template<typename T>
constexpr const char* __pochivm_stringify_type__()
{
    const char* const p = __PRETTY_FUNCTION__;
    return p;
}

// When v is a function pointer or member function pointer, returns something like
//    const char *__pochivm_stringify_value__() [v = &###### ]
// where ###### is the interesting part
// WARNING: this breaks down when called outside a function.
//
template<auto v>
constexpr const char* __pochivm_stringify_value__()
{
    const char* const p = __PRETTY_FUNCTION__;
    return p;
}
