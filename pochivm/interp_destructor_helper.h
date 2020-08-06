#pragma once

#include "pochivm_context.h"
#include "ast_expr_base.h"
#include "ast_variable.h"
#include "generated/pochivm_runtime_cpp_typeinfo.generated.h"

namespace PochiVM
{

// Get the destructor's metadata for a CPP class from TypeId
// If the destructor is not registered, return nullptr
//
inline const CppFunctionMetadata* GetDestructorMetadata(TypeId typeId)
{
    using ConstCppFnMdPtr = const CppFunctionMetadata*;
#define F(...) DestructorCppFnMetadata<__VA_ARGS__>::value,
    static constexpr ConstCppFnMdPtr x_cppClassDestructorsByOrdinal[AstTypeHelper::x_num_cpp_class_types + 1] = {
        FOR_EACH_CPP_CLASS_TYPE
        nullptr /*dummy value for bad CPP type */
    };
#undef F
    TestAssert(typeId.IsCppClassType());
    uint64_t ord = typeId.GetCppClassTypeOrdinal();
    assert(ord < AstTypeHelper::x_num_cpp_class_types);
    return x_cppClassDestructorsByOrdinal[ord];
}

// Destruct a local variable in interp mode
//
inline void InterpDestructLocalVariableHelper(AstVariable* var)
{
    const CppFunctionMetadata* md = GetDestructorMetadata(var->GetTypeId().RemovePointer());
    // md is nullptr if the destructor for this CPP class type is not registered
    // However, this should never happen at this stage. It should have been blocked by the frontend.
    //
    TestAssert(md != nullptr);
    // 'addr' is the address of the local variable
    //
    void* addr;
    var->Interp(&addr /*out*/);
    // interp functions take a parameter array of pointers to the actual parameters.
    // In our case, the actual parameter is 'addr'. So we should populate the array with '&addr'.
    //
    void* paramsArray[1];
    paramsArray[0] = &addr;
    assert(md->m_returnType.IsVoid() && md->m_numParams == 1 && md->m_paramTypes[0] == var->GetTypeId() && !md->m_isUsingSret);
    md->m_interpFn(nullptr /*ret*/, paramsArray);
}

// Push a new scope into the scope stack when this class is constructed,
// destruct all variables in the scope stack and pop the scope when this class goes out of scope.
//
struct AutoInterpExecutionScope
{
    AutoInterpExecutionScope()
    {
        thread_pochiVMContext->m_interpScopeStack.push_back(std::vector<AstVariable*>());
    }

    ~AutoInterpExecutionScope()
    {
        assert(thread_pochiVMContext->m_interpScopeStack.size() > 0);
        for (AstVariable* var : thread_pochiVMContext->m_interpScopeStack.back())
        {
            InterpDestructLocalVariableHelper(var);
        }
        thread_pochiVMContext->m_interpScopeStack.pop_back();
    }
};

}   // namespace PochiVM
