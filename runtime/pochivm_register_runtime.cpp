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
    // While you may write any logic you like, keep in mind that you will get a segfault if you try to
    // call functions or access global values which implementations reside in other CPP files.
    // E.g. suppose implementation of function 'f' is not in this file.
    //      'auto fnPtr = &f' or 'RegisterFreeFn<&f>()' is OK since it only takes address of the symbol
    //      (which does not require knowledge of its implementation). But 'f()' will give you a segfault.
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
    RegisterMemberFn<&TestClassB::TestBool>();
    RegisterMemberFn<&TestClassB::TestBoolStar>();

    RegisterFreeFn<&FreeFunctionAPlusB>();
    RegisterFreeFn<&FreeFunctionStoreValue>();
    RegisterFreeFn<&FreeFunctionPrintFile>();
    RegisterFreeFn<&FreeFunctionConcatStr>();

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
