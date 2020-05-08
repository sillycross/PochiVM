#pragma once

#include "src/common.h"

// a bunch of utility helpers for tests

// return if string 'a' starts with 'b'
//
inline bool WARN_UNUSED StartsWith(const char* a, const char* b)
{
    size_t la = strlen(a);
    size_t lb = strlen(b);
    CHECK(la >= lb);
    return strncmp(a, b, lb) == 0;
}
