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

inline void InterpCallDestructorHelper(const CppFunctionMetadata* md, void* addr) noexcept
{
    // interp functions take a parameter array of the actual parameters.
    // In our case, the actual parameter is 'addr'.
    //
    void* paramsArray[1];
    paramsArray[0] = addr;
    assert(md->m_returnType.IsVoid() && md->m_numParams == 1 && !md->m_isUsingSret);
    md->m_debugInterpFn(nullptr /*ret*/, paramsArray);
}

// Destruct a local variable in interp mode
//
inline void InterpDestructLocalVariableHelper(AstVariable* var) noexcept
{
    const CppFunctionMetadata* md = GetDestructorMetadata(var->GetTypeId().RemovePointer());
    // md is nullptr if the destructor for this CPP class type is not registered
    // However, this should never happen at this stage. It should have been blocked by the frontend.
    //
    TestAssert(md != nullptr);
    // 'addr' is the address of the local variable
    //
    void* addr;
    var->DebugInterp(&addr /*out*/);
    assert(md->m_numParams == 1 && md->m_paramTypes[0] == var->GetTypeId());
    InterpCallDestructorHelper(md, addr);
}

// Push a new scope into the scope stack when this class is constructed,
// destruct all variables in the scope stack and pop the scope when this class goes out of scope.
//
struct AutoInterpExecutionScope
{
    AutoInterpExecutionScope()
    {
        thread_pochiVMContext->m_debugInterpScopeStack.push_back(std::vector<AstVariable*>());
    }

    ~AutoInterpExecutionScope()
    {
        assert(thread_pochiVMContext->m_debugInterpScopeStack.size() > 0);
        // Call destructors in reverse order of the variables are declared
        //
        const std::vector<AstVariable*>& vec = thread_pochiVMContext->m_debugInterpScopeStack.back();
        for (auto rit = vec.rbegin(); rit != vec.rend(); rit++)
        {
            AstVariable* var = *rit;
            InterpDestructLocalVariableHelper(var);
        }
        thread_pochiVMContext->m_debugInterpScopeStack.pop_back();
    }
};

// Emit IR that calls destructors in reverse order of declaration for all variables declared,
// until scope 'boundaryScope' (inclusive). When boundaryScope == nullptr, destructs everything.
//
void EmitIRDestructAllVariablesUntilScope(AstNodeBase* boundaryScope);

}   // namespace PochiVM
