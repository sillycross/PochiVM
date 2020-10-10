#pragma once

#include "fastinterp_snippet.h"
#include "fastinterp/fastinterp.h"

namespace PochiVM
{

inline FastInterpSnippet WARN_UNUSED FastInterpSnippet::AddContinuation(FastInterpSnippet other)
{
    TestAssert(m_tail != nullptr);
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
