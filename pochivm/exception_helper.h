#pragma once

#include "common.h"
#include "generated/pochivm_runtime_cpp_typeinfo.generated.h"

namespace llvm {
class BasicBlock;
}   // namespace LLVM

namespace PochiVM
{

// Exception handling helpers
//
// The high-level structure for exception-handling looks like this:
//     alloca { i8*, i32 } _cur_exception_obj_and_type
//     ...
//     invoke ... normal_dest, eh_landingpad_1 ...
// normal_dest:
//     ...
// eh_landingpad_1:
//     %1 = landingpad {i8*, i32} cleanup catch ...
//     store %1, _cur_exception_obj_and_type
//     br eh_dtor_tree_1
// eh_dtor_tree_1:  (multiple entry points, but always converge to eh_catch)
//     ... call destructors ...
//     br eh_catch
// eh_catch:   (all exceptions enters here in the end)
//     %2 = load _cur_exception_obj_and_type
//     ( ...handle exception using data from %2, or resume %2 if no catch clause exists )
//     _cur_exception_obj_and_type may be overwritten in the logic of handling exception,
//     if the logic contains a nested try-catch clause
//

namespace internal
{

template<template<typename> class S, typename T, typename Enable = void>
struct valid_typeid_insertion_helper
{
    static void insert(std::unordered_map<TypeId, void*>& /*unused*/) {}
};
template<template<typename> class S, typename T>
struct valid_typeid_insertion_helper<S, T, typename std::enable_if<(
    std::is_same<typename ReflectionHelper::recursive_remove_cv<T>::type, T>::value)>::type>
{
    static void insert(std::unordered_map<TypeId, void*>& out)
    {
        TypeId t = TypeId::Get<T>();
        TestAssert(!out.count(t));
        out[t] = S<T>::get();
    }
};

}   // namespace internal

// Select a instantiation of a templated function based on exception type. 'S' must provide
//     static void* S<Type>::get()
// The function select_impl_based_on_exception_type<S>::get(TypeId typeId)
// returns S<(typeId's C++ type)>::get() if typeId is an exception type, or nullptr if not.
//
template<template<typename> class S>
class select_impl_based_on_exception_type
{
    static std::unordered_map<TypeId, void*> get_hash_map();
    static inline const std::unordered_map<TypeId, void*> resultMap = get_hash_map();

public:
    static void* get(TypeId typeId)
    {
        auto it = resultMap.find(typeId);
        if (it == resultMap.end())
        {
            return nullptr;
        }
        else
        {
            return it->second;
        }
    }
};

namespace internal
{

template<typename T>
struct is_expection_registered_for_thrown_helper
{
    static void* get() { return reinterpret_cast<void*>(1); }
};

template<typename T>
struct get_typeinfo_object_symbol_name_helper
{
    static void* get()
    {
        // Remove const only for API compatibility (the select_impl_based_on_exception_type API
        // only accepts void* return value), we will cast it back to const char* later.
        //
        return const_cast<char*>(AstTypeHelper::typeinfo_object_symbol_name<T>::value);
    }
};

}   // namespace internal

inline bool WARN_UNUSED IsTypeRegisteredForThrownFromGeneratedCode(TypeId typeId)
{
    void* r = select_impl_based_on_exception_type<internal::is_expection_registered_for_thrown_helper>::get(typeId);
    return r != nullptr;
}

inline const char* WARN_UNUSED GetStdTypeInfoObjectSymbolName(TypeId typeId)
{
    TestAssert(IsTypeRegisteredForThrownFromGeneratedCode(typeId));
    void* r = select_impl_based_on_exception_type<internal::get_typeinfo_object_symbol_name_helper>::get(typeId);
    TestAssert(r != nullptr);
    return reinterpret_cast<const char*>(r);
}

// Return true if there is no need to setup a landing pad for potentially-throwing function.
// This is possible if we are not in a try-catch block and there is no destructor to call.
//
bool WARN_UNUSED IsNoLandingPadNeeded();

// Emit a basic block that the control flow should branch to
// if there is an exception thrown right at the current program position.
// IRBuilder IP is unchanged before and after the call.
//
llvm::BasicBlock* WARN_UNUSED EmitEHLandingPadForCurrentPosition();

}   // namespace PochiVM

// Expand macro FOR_EACH_EXCEPTION_TYPE in root namespace to prevent potential type resolution conflict
//
template<template<typename> class S>
std::unordered_map<PochiVM::TypeId, void*> PochiVM::select_impl_based_on_exception_type<S>::get_hash_map()
{
    std::unordered_map<PochiVM::TypeId, void*> ret;
#define F(...) PochiVM::internal::valid_typeid_insertion_helper<S, __VA_ARGS__>::insert(ret);
    FOR_EACH_EXCEPTION_TYPE
#undef F
    return ret;
}
