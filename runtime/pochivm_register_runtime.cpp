// DO NOT INCLUDE ADDITIONAL HEADERS
// Put the includes in 'pochivm_runtime_headers.h', not here.
//
#include "pochivm_runtime_headers.h"
#include "src/pochivm_reflection_helper.h"

__attribute__((__optnone__))    // make sure link-time optimization won't optimize these out as dead code
static void RegisterRuntimeLibrary()
{
    using namespace Ast;

    // ****************************************
    // Register the list of functions callable from generated code, using the APIs below.
    //
    //    void RegisterFreeFn<function pointer>()
    //    void RegisterMemberFn<member function pointer>()
    //    void RegisterStaticMemberFn<member function pointer>()
    //
    // ****************************************

    RegisterMemberFn<&TestClassA::GetY>();
    RegisterMemberFn<&TestClassA::SetY>();
    RegisterMemberFn<&TestClassA::GetXPlusY>();
    RegisterMemberFn<&TestClassA::PushVec>();
    RegisterMemberFn<&TestClassA::GetSize>();
    RegisterMemberFn<&TestClassA::SortVector>();
    RegisterMemberFn<&TestClassA::GetVectorSum>();
    RegisterMemberFn<&TestClassA::GetStringY>();

    RegisterMemberFn<&TestClassB::SetA>();
    RegisterMemberFn<&TestClassB::GetA>();
    RegisterMemberFn<&TestClassB::SetAp>();
    RegisterMemberFn<&TestClassB::GetAp>();

    RegisterFreeFn<&FreeFunctionAPlusB>();
    RegisterFreeFn<&FreeFunctionStoreValue>();
    RegisterFreeFn<&FreeFunctionPrintFile>();

    RegisterFreeFn<&FreeFunctionTemplated<int, 1>>();
    RegisterFreeFn<&FreeFunctionTemplated<int, 2>>();
    RegisterFreeFn<&FreeFunctionTemplated<int, 3>>();
    RegisterFreeFn<&FreeFunctionTemplated<double, 1>>();
    RegisterFreeFn<&FreeFunctionTemplated<double, 5>>();

    RegisterFreeFn<static_cast<int(*)(int)>(&FreeFunctionOverloaded<int, int, int>)>();
    RegisterFreeFn<static_cast<int(*)(int)>(&FreeFunctionOverloaded<int, int, double>)>();
    RegisterFreeFn<static_cast<int(*)(int)>(&FreeFunctionOverloaded<double, int, int>)>();
    RegisterFreeFn<static_cast<int(*)(int)>(&FreeFunctionOverloaded<int, double, double>)>();

    RegisterFreeFn<static_cast<int(*)(int, int)>(&FreeFunctionOverloaded<int, int, int>)>();
    RegisterFreeFn<static_cast<int(*)(int, int)>(&FreeFunctionOverloaded<int, double, int>)>();
    RegisterFreeFn<static_cast<int(*)(int, int)>(&FreeFunctionOverloaded<double, double, int>)>();
    RegisterFreeFn<static_cast<int(*)(int, int)>(&FreeFunctionOverloaded<double, double, double>)>();

    RegisterFreeFn<static_cast<double(*)(double)>(&FreeFunctionOverloaded)>();
}

// DO NOT MODIFY
// dump_symbols.cpp JIT entry point
//
extern "C" __attribute__((__optnone__, __used__))
void __pochivm_register_runtime_library__()
{
    RegisterRuntimeLibrary();
}
