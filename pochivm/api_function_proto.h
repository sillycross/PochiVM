#pragma once

#include "api_base.h"
#include "function_proto.h"
#include "api_lang_constructs.h"

#include "generated/pochivm_runtime_cpp_typeinfo.generated.h"
#include "generated/pochivm_runtime_headers.generated.h"

namespace PochiVM
{

class Function;

namespace internal
{

template<typename T, size_t n>
struct params_user_tuple_internal
{
    using paramType = typename AstTypeHelper::function_type_helper<T>::template ArgType<n-1>;

    using type = AstTypeHelper::tuple_concat_type<
        typename params_user_tuple_internal<T, n-1>::type,
        std::tuple < Variable< paramType > >
    >;

    static type build(const Function& fn, const std::vector<AstVariable*>& params);
};

template<typename T>
struct params_user_tuple_internal<T, 0>
{
    using type = std::tuple<Function>;

    static type build(const Function& fn, const std::vector<AstVariable*>& params);
};

template<typename T>
struct params_user_tuple
{
    static const size_t numArgs = AstTypeHelper::function_type_helper<T>::numArgs;

    using type = typename params_user_tuple_internal<T, numArgs>::type;

    static type build(const Function& fn, const std::vector<AstVariable*>& params);
};

}   // namespace internal

// Given C-style function pointer or std::function T with prototype RetType(Param1Type, Param2Type...),
// FunctionAndParamsTuple is std::tuple<Function, Variable<Param1Type>, Variable<Param2Type> ...>
//
template<typename T>
using FunctionAndParamsTuple = typename internal::params_user_tuple<T>::type;

class Function
{
public:
    template<typename T, typename... Targs>
    friend FunctionAndParamsTuple<T> NewFunction(const std::string& fnName, Targs... paramNames);

    void SetReturnType(TypeId type)
    {
        assert(!m_prototypeFrozen);
        m_ptr->SetReturnType(type);
    }

    void SetParamName(size_t i, const char* name)
    {
        m_ptr->SetParamName(i, name);
    }

    void SetIsNoExcept(bool isNoExcept)
    {
        assert(!m_prototypeFrozen);
        m_ptr->SetIsNoExcept(isNoExcept);
    }

    template<typename T>
    Variable<T> NewVariable(const char* name = "var")
    {
        return Variable<T>(new AstVariable(TypeId::Get<T*>(), m_ptr, m_ptr->GetNextVarSuffix(), name));
    }

    template<typename... T>
    void SetBody(T... args)
    {
        m_ptr->SetFunctionBody(Block(args...).GetPtr());
    }

    Scope GetBody()
    {
        return Scope(m_ptr->GetFunctionBody());
    }

    AstFunction* GetPtr() const { return m_ptr; }

private:
    Function(AstFunction* ptr)
        : m_ptr(ptr)
#ifndef NDEBUG
        , m_prototypeFrozen(false)
#endif
    {}

    void AddParamUnnamed(TypeId type)
    {
        assert(!m_prototypeFrozen);
        m_ptr->AddParam(type, "param");
    }

    void FreezePrototype()
    {
#ifndef NDEBUG
        m_prototypeFrozen = true;
#endif
    }

    AstFunction* const m_ptr;
#ifndef NDEBUG
    bool m_prototypeFrozen;
#endif
};

namespace internal
{

template<typename T, size_t n>
typename params_user_tuple_internal<T, n>::type
params_user_tuple_internal<T, n>::build(const Function& fn, const std::vector<AstVariable*>& params)
{
    return std::tuple_cat(params_user_tuple_internal<T, n-1>::build(fn, params),
                          std::tuple<Variable<paramType>>(Variable<paramType>(params[n-1])));
}

template<typename T>
typename params_user_tuple_internal<T, 0>::type
params_user_tuple_internal<T, 0>::build(const Function& fn, const std::vector<AstVariable*>& /*params*/)
{
    return std::tuple<Function>(fn);
}

template<typename T>
typename params_user_tuple<T>::type
params_user_tuple<T>::build(const Function& fn, const std::vector<AstVariable*>& params)
{
    TestAssert(params.size() == numArgs);
    return params_user_tuple_internal<T, numArgs>::build(fn, params);
}

inline void NewFunctionSetParamNamesHelper(Function& fn, size_t i)
{
    TestAssert(fn.GetPtr()->GetNumParams() == i);
    std::ignore = fn;
    std::ignore = i;
}

template<typename... Targs>
void NewFunctionSetParamNamesHelper(Function& fn, size_t i, const char* name, Targs... paramNames)
{
    fn.SetParamName(i, name);
    NewFunctionSetParamNamesHelper(fn, i+1, paramNames...);
}

}   // namespace internal

// Usage:
//   auto [ fn, param1, param2 ... ] = NewFunction<Prototype>(fnName, [nameParam1, nameParam2...])
//   Prototype must be a C-style function pointer
//   Parameter names are optional, but if specified, must specify for all parameters.
//
// Example:
//   using FnType = int(*)(int);
//   auto [fn, param] = NewFunction<FnType>("find_nth_prime", "n");
//
template<typename T, typename... Targs>
FunctionAndParamsTuple<T> NewFunction(const std::string& fnName, Targs... paramNames)
{
    static_assert(AstTypeHelper::is_function_prototype<T>::value,
                  "T must be a C-style function pointer type");

    using FnInfo = AstTypeHelper::function_type_helper<T>;
    static_assert(sizeof...(Targs) == 0 || sizeof...(Targs) == FnInfo::numArgs,
                  "Must specify either no param name at all, or all of the param names");

    Function fn(thread_pochiVMContext->m_curModule->NewAstFunction(fnName));
    size_t numArgs = FnInfo::numArgs;
    for (size_t i = 0; i < numArgs; i++)
    {
        fn.AddParamUnnamed(FnInfo::argTypeId[i]);
    }
    if (sizeof...(Targs) > 0)
    {
        internal::NewFunctionSetParamNamesHelper(fn, 0, paramNames...);
    }
    fn.SetReturnType(FnInfo::returnTypeId);
    fn.SetIsNoExcept(AstTypeHelper::is_noexcept_function_prototype<T>::value);
    fn.FreezePrototype();
    return internal::params_user_tuple<T>::build(fn, fn.GetPtr()->GetParamsVector());
}

// Declare a variable, with initialization expression
//
template<typename T>
Value<void> Declare(const Variable<T>& var, const Value<T>& value)
{
    if constexpr(AstTypeHelper::is_primitive_type<T>::value || std::is_pointer<T>::value)
    {
        return Value<void>(new AstDeclareVariable(
                               var.__pochivm_var_ptr,
                               new AstAssignExpr(var.__pochivm_var_ptr, value.__pochivm_value_ptr)));
    }
    else
    {
        // non-primitive type, rhs must be a CallExpr:
        // this is the only way to get a Value<T> for non-primitive T in current design
        //
        static_assert(AstTypeHelper::is_cpp_class_type<T>::value,
                      "T must be a CPP class type registered in pochivm_register_runtime.cpp");
        AstCallExpr* callExpr = assert_cast<AstCallExpr*>(value.__pochivm_value_ptr);
        return Value<void>(new AstDeclareVariable(var.__pochivm_var_ptr, callExpr, false /*isCtor*/));
    }
}

namespace internal
{

inline AstCallExpr* GetCallExprFromConstructor(AstNodeBase* addr, const ConstructorParamInfo& ctorParams)
{
    TestAssert(ctorParams.m_constructorMd->m_numParams > 0);
    TestAssert(ctorParams.m_constructorMd->m_paramTypes[0] == addr->GetTypeId());
    TestAssert(ctorParams.m_params.size() == ctorParams.m_constructorMd->m_numParams - 1);
#ifdef TESTBUILD
    for (size_t i = 0; i < ctorParams.m_params.size(); i++)
    {
        TestAssert(ctorParams.m_params[i]->GetTypeId() == ctorParams.m_constructorMd->m_paramTypes[i+1]);
    }
#endif
    std::vector<AstNodeBase*> params;
    params.reserve(ctorParams.m_constructorMd->m_numParams);
    params.push_back(addr);
    params.insert(params.end(), ctorParams.m_params.begin(), ctorParams.m_params.end());
    return new AstCallExpr(ctorParams.m_constructorMd, params);
}

}   // namespace internal

// Declare a variable, with constructor initialization
//
template<typename T>
Value<void> Declare(const Variable<T>& var, const Constructor<T>& constructorParams)
{
    static_assert(std::is_base_of<ConstructorParamInfo, Constructor<T>>::value,
                  "Constructor<T> is supposed to be derived from ConstructorParamInfo");
    const ConstructorParamInfo& ctorParams = constructorParams;
    static_assert(AstTypeHelper::is_cpp_class_type<T>::value, "must be a cpp class type");
    TestAssert(ctorParams.m_constructorMd->m_paramTypes[0] == TypeId::Get<T>().AddPointer());
    return Value<void>(new AstDeclareVariable(var.__pochivm_var_ptr,
                                              internal::GetCallExprFromConstructor(var.__pochivm_var_ptr, ctorParams),
                                              true /*isCtor*/));
}

// Declare a variable, with default constructor (for objects) or uninitialized (for primitive types)
//
template<typename T>
Value<void> Declare(const Variable<T>& var)
{
    if constexpr(AstTypeHelper::is_primitive_type<T>::value || std::is_pointer<T>::value)
    {
        return Value<void>(new AstDeclareVariable(var.__pochivm_var_ptr));
    }
    else
    {
        static_assert(AstTypeHelper::is_default_ctor_registered<T>::value,
                      "Cannot default-construct a non-primitive type variable with no registered default constructor. "
                      "You must either register a default constructor in pochivm_register_runtime.cpp, "
                      "or call a custom constructor with parameters, or call a function that returns such type.");
        return Declare<T>(var, Constructor<T>());
    }
}

// Declare a variable, with constant value initialization
//
template<typename T>
Value<void> Declare(const Variable<T>& var, T value)
{
    static_assert(AstTypeHelper::is_primitive_type<T>::value,
                  "may only constant-initialize primitive type variable");
    return Value<void>(new AstDeclareVariable(
                           var.__pochivm_var_ptr,
                           new AstAssignExpr(var.__pochivm_var_ptr, Literal<T>(value).__pochivm_value_ptr)));
}

namespace internal
{

template<typename T>
struct typesafe_populate_parameters_helper
{
    static const size_t numArgs = AstTypeHelper::function_type_helper<T>::numArgs;

    template<size_t i>
    using ArgType = AstTypeHelper::function_arg_type<T, i>;

    template<size_t n>
    static void populate_internal(std::vector<AstNodeBase*>& out)
    {
        static_assert(n == 0, "wrong number of arguments supplied to call expression");
        assert(out.size() == numArgs);
        std::ignore = out;
    }

    template<size_t n, typename... Targs>
    static void populate_internal(std::vector<AstNodeBase*>& out,
                           Value< ArgType<numArgs-n> > first,
                           Targs... args)
    {
        static_assert(sizeof...(Targs) == n - 1, "wrong number of arguments supplied to call expression");
        out.push_back(first.__pochivm_value_ptr);
        populate_internal<n-1>(out, args...);
    }

    template<typename... Targs>
    static std::vector<AstNodeBase*> populate(Targs... args)
    {
        static_assert(sizeof...(Targs) == numArgs, "wrong number of arguments supplied to call expression");
        std::vector<AstNodeBase*> ret;
        populate_internal<numArgs>(ret /*out*/, args...);
        return ret;
    }
};

template<typename T, typename Enable = void>
struct call_expr_construct_helper
{
    static_assert(sizeof(T) == 0,
                  "T must be either the return-type of the function, or the prototype of the function");
};

template<typename T>
struct call_expr_construct_helper<T, typename std::enable_if<
    (std::is_void<T>::value || AstTypeHelper::is_primitive_type<T>::value || std::is_pointer<T>::value)
    && (!AstTypeHelper::is_function_prototype<T>::value)
>::type >
{
    using ReturnType = T;

    static void populate_helper(std::vector<AstNodeBase*>& /*out*/)
    { }

    template<typename F, typename... Targs>
    static void populate_helper(std::vector<AstNodeBase*>& out, Value<F> first, Targs... args)
    {
        out.push_back(first.__pochivm_value_ptr);
        populate_helper(out, args...);
    }

    // The user specified the return type only.
    // Parameter check will not be performed
    //
    template<typename... Targs>
    static Value<ReturnType> call(const std::string& fnName, Targs... args)
    {
        std::vector<AstNodeBase*> params = populate_helper(args...);
        return Value<ReturnType>(new AstCallExpr(fnName, params, TypeId::Get<ReturnType>()));
    }
};

template<typename T>
struct call_expr_construct_helper<T, typename std::enable_if<
    AstTypeHelper::is_function_prototype<T>::value
>::type >
{
    using ReturnType = AstTypeHelper::function_return_type<T>;

    // The user specified the function prototype. Do full param type checking
    //
    template<typename... Targs>
    static Value<ReturnType> call(const std::string& fnName, Targs... args)
    {
        std::vector<AstNodeBase*> params = typesafe_populate_parameters_helper<T>::populate(args...);
        return Value<ReturnType>(new AstCallExpr(fnName, params, TypeId::Get<ReturnType>()));
    }
};

}   // namespace internal

// Usage:
// (1) auto r = Call<FnPrototype>(params...)
//     Fires static_assert if any of the parameter types do not match.
//     Implicit cast (integer widening) on param types are supported.
//
// (2) auto r = Call<ReturnType>(params...)
//     No static typechecking on parameter types. Fires a runtime assert if there is a mismatch.
//
template<typename T, typename... Targs>
Value<typename internal::call_expr_construct_helper<T>::ReturnType> Call(const std::string& fnName, Targs... args)
{
    return internal::call_expr_construct_helper<T>::call(fnName, args...);
}

template<typename T, typename... Targs>
Value<typename internal::call_expr_construct_helper<T>::ReturnType> Call(Function fn, Targs... args)
{
    return internal::call_expr_construct_helper<T>::call(fn.GetPtr()->GetName(), args...);
}

// Return(): a return statement, with no return value
//
inline Value<void> Return()
{
    return Value<void>(new AstReturnStmt(nullptr));
}

// Return(expr): a return statement with a non-void return value
//
template<typename T>
Value<void> Return(const Value<T>& val)
{
    static_assert(!std::is_same<T, void>::value, "Cannot return a void expression. Use Return() instead.");
    return Value<void>(new AstReturnStmt(val.__pochivm_value_ptr));
}

inline Value<void> Return(bool val)
{
    return Value<void>(new AstReturnStmt(new AstLiteralExpr(TypeId::Get<bool>(), &val)));
}

// CallDestructor<T>(T* ptr): manually call the destructor to destruct the object at 'ptr'
//
template<typename T>
Value<void> CallDestructor(const Value<T*>& val)
{
    static_assert(is_destructor_registered<T>::value, "Destructor for T is not registered. Register in pochivm_register_runtime.cpp?");
    return Value<void>(new AstCallExpr(DestructorCppFnMetadata<T>::value, { val.__pochivm_value_ptr }));
}

inline void NewModule(const std::string& name)
{
    thread_pochiVMContext->m_curModule = new AstModule(name);
}

}   // namespace PochiVM
