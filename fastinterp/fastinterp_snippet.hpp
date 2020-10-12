#pragma once

#include "fastinterp_snippet.h"
#include "fastinterp/fastinterp.h"

namespace PochiVM
{

inline FastInterpSnippet WARN_UNUSED FastInterpSnippet::AddContinuation(FastInterpSnippet other)
{
    if (other.m_entry == nullptr && other.m_tail == nullptr)
    {
        return *this;
    }
    TestAssert(m_tail != nullptr && other.m_entry != nullptr);
    m_tail->PopulateBoilerplateFnPtrPlaceholder(0, other.m_entry);
    return FastInterpSnippet {
        m_entry, other.m_tail
    };
}

inline FastInterpSnippet WARN_UNUSED FastInterpSnippet::AddContinuation(FastInterpBoilerplateInstance* other)
{
    TestAssert(other != nullptr);
    m_tail->PopulateBoilerplateFnPtrPlaceholder(0, other);
    return FastInterpSnippet {
        m_entry, other
    };
}

}   // namespace PochiVM
