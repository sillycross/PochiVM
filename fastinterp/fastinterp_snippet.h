#pragma once

#include "common.h"

namespace PochiVM
{

class FastInterpBoilerplateInstance;

struct FastInterpSnippet
{
    FastInterpSnippet(FastInterpBoilerplateInstance* entry, FastInterpBoilerplateInstance* tail)
        : m_entry(entry), m_tail(tail)
    { }

    // No-op snippet
    //
    FastInterpSnippet() : m_entry(nullptr), m_tail(nullptr) { }

    bool IsEmpty() const
    {
        AssertImp(m_entry == nullptr, m_tail == nullptr);
        return m_entry == nullptr;
    }

    bool IsUncontinuable() const
    {
        return m_entry != nullptr && m_tail == nullptr;
    }

    // The entry point of the code snippet
    //
    FastInterpBoilerplateInstance* m_entry;

    // The tail to which continuation shall be attached
    // nullptr if a continuation is not possible, since the tail is a control flow redirection (return/break/continue/throw)
    //
    FastInterpBoilerplateInstance* m_tail;

    FastInterpSnippet WARN_UNUSED AddContinuation(FastInterpSnippet other);
    FastInterpSnippet WARN_UNUSED AddContinuation(FastInterpBoilerplateInstance* other);
};

}   // namespace PochiVM
