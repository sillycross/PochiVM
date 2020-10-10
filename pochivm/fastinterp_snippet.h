#pragma once

#include "common.h"

namespace PochiVM
{

class FastInterpBoilerplateInstance;

struct FastInterpSnippet
{
    // The entry point of the code snippet
    //
    FastInterpBoilerplateInstance* m_entry;

    // The tail to which continuation shall be attached
    // nullptr if a continuation is not possible (since the tail is a ret)
    //
    FastInterpBoilerplateInstance* m_tail;

    FastInterpSnippet WARN_UNUSED AddContinuation(FastInterpSnippet other);
    FastInterpSnippet WARN_UNUSED AddContinuation(FastInterpBoilerplateInstance* other);
};

}   // namespace PochiVM
