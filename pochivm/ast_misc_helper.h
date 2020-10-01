#pragma once

#include "common.h"
#include "pochivm_context.h"

namespace PochiVM
{

// A simple helper for marking nodes as visited when traversing AST
//
class AstTraverseColorMark
{
public:
    AstTraverseColorMark() : m_value(0) {}

    void MarkColorA() { MarkAs(0); }
    void MarkColorB() { MarkAs(1); }
    void MarkColorC() { MarkAs(2); }
    bool IsColorA() const { return IsMarkedAs(0); }
    bool IsColorB() const { return IsMarkedAs(1); }
    bool IsColorC() const { return IsMarkedAs(2); }
    bool IsNoColor() const { return !IsColorA() && !IsColorB() && !IsColorC(); }

    static void ClearAll()
    {
        thread_pochiVMContext->m_astTraverseColorMark += x_inc_value;
    }

private:
    static const uint64_t x_inc_value = 3;  // modify accordingly if you add more mark colors

    void MarkAs(uint64_t color)
    {
        assert(color < x_inc_value);
        m_value = thread_pochiVMContext->m_astTraverseColorMark + color;
    }

    bool IsMarkedAs(uint64_t color) const
    {
        assert(color < x_inc_value);
        return m_value == thread_pochiVMContext->m_astTraverseColorMark + color;
    }

    uint64_t m_value;
};

}   // namespace PochiVM
