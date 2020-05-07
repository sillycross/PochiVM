#pragma once

#include "api_base.h"

namespace Ast
{

// A wrapper class similar to Value, except that it holds a AST node of type unknown at C++ build time.
// VT = Variable-Typed
//
class ValueVT
{
public:
    ValueVT(AstNodeBase* ptr)
        : m_ptr(ptr)
    { }

    // Returns true if this expression has type 'typeId'
    //
    bool WARN_UNUSED HasType(TypeId typeId)
    {
        return m_ptr->GetTypeId() == typeId;
    }

    // Currently, the only operation supported is to convert this class to a Value<T, valueType>,
    // which fires an assert if the typename is not correct. In future, we should support
    // operations acting directly on this class.
    //
    template<typename T>
    operator Value<T>() const
    {
        TestAssert(m_ptr->GetTypeId().IsType<T>());
        return Value<T>(m_ptr);
    }

    AstNodeBase* m_ptr;
};

}   // namespace Ast
