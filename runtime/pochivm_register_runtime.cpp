// DO NOT INCLUDE ADDITIONAL HEADERS
// Put the includes in 'pochivm_runtime_headers.h', not here.
//
#include "pochivm_runtime_headers.h"
#include "pochivm/pochivm_reflection_helper.h"

__attribute__((__optnone__))    // make sure link-time optimization won't optimize these out as dead code
static void RegisterRuntimeLibrary()
{
    using namespace PochiVM;

    // ****************************************
    // Register the list of functions callable from generated code, using the APIs below.
    //
    //    void RegisterFreeFn<function pointer>()
    //    void RegisterMemberFn<member function pointer>()
    //    void RegisterStaticMemberFn<member function pointer>()
    //    void RegisterConstructor<ClassName, ArgTypeNames...>()
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

    RegisterMemberFn<&TestLargeClass::F1>();
    RegisterMemberFn<&TestLargeClass::F2>();
    RegisterMemberFn<&TestLargeClass::F3>();
    RegisterMemberFn<&TestLargeClass::F4>();
    RegisterMemberFn<&TestLargeClass::F5>();

    RegisterMemberFn<&TestSmallClass::F1>();
    RegisterMemberFn<&TestSmallClass::F2>();
    RegisterMemberFn<&TestSmallClass::F3>();
    RegisterMemberFn<&TestSmallClass::F4>();
    RegisterMemberFn<&TestSmallClass::F5>();
    RegisterMemberFn<&TestSmallClass::F6>();
    RegisterMemberFn<&TestSmallClass::F7>();

    RegisterMemberFn<&TestSmallClass::G1>();
    RegisterMemberFn<&TestSmallClass::G2>();
    RegisterMemberFn<&TestSmallClass::G3>();
    RegisterMemberFn<&TestSmallClass::G4>();
    RegisterMemberFn<&TestSmallClass::G5>();
    RegisterMemberFn<&TestSmallClass::G6>();
    RegisterMemberFn<&TestSmallClass::G7>();

    RegisterStaticMemberFn<&TestSmallClass::S1>();
    RegisterStaticMemberFn<&TestSmallClass::S2>();

    RegisterMemberFn<static_cast<void(TestSmallClass::*)(int)>(&TestSmallClass::Overloaded)>();
    RegisterMemberFn<static_cast<void(TestSmallClass::*)(double)>(&TestSmallClass::Overloaded)>();
    RegisterMemberFn<static_cast<void(TestSmallClass::*)(TestSmallClass) const>(&TestSmallClass::Overloaded)>();

    RegisterFreeFn<&FreeFnTestSmallClass1>();
    RegisterFreeFn<&FreeFnTestSmallClass2>();
    RegisterFreeFn<&FreeFnTestSmallClass3>();
    RegisterFreeFn<&FreeFnTestSmallClass4>();
    RegisterFreeFn<&FreeFnTestSmallClass5>();

    RegisterFreeFn<&FreeFnTestLargeClass1>();
    RegisterFreeFn<&FreeFnTestLargeClass2>();
    RegisterFreeFn<&FreeFnTestLargeClass3>();
    RegisterFreeFn<&FreeFnTestLargeClass4>();

    RegisterFreeFn<&FreeFnRecursive>();
    RegisterFreeFn<&FreeFnRecursive2>();
    RegisterFreeFn<&FreeFnRecursive3>();

    RegisterFreeFn<&TestCornerCases::BoolParamTest1>();
    RegisterFreeFn<&TestCornerCases::BoolParamTest2>();
    RegisterFreeFn<&TestCornerCases::VoidStarParamTest1>();
    RegisterFreeFn<&TestCornerCases::VoidStarParamTest2>();

    RegisterStaticMemberFn<&TestNonTrivialConstructor::Create>();
    RegisterMemberFn<&TestNonTrivialConstructor::GetValue>();

    RegisterStaticMemberFn<&TestNonTrivialCopyConstructor::Fn>();

    RegisterConstructor<std::vector<int>>();
    RegisterConstructor<std::vector<int>, size_t /*count*/>();
    RegisterConstructor<std::vector<int>, size_t /*count*/, int /*value*/>();
    RegisterConstructor<std::vector<int>, const std::vector<int>& /*other*/>();

    RegisterFreeFn<&CopyVectorInt>();

    RegisterConstructor<TestConstructor1>();
    RegisterMemberFn<&TestConstructor1::GetValue>();

    RegisterConstructor<TestConstructor2, int>();
    RegisterMemberFn<&TestConstructor2::GetValue>();

    RegisterConstructor<TestDestructor1, int, int*>();

    RegisterConstructor<TestDestructor2, CtorDtorOrderRecorder*, int>();
    RegisterMemberFn<&CtorDtorOrderRecorder::Push>();

    RegisterFreeFn<&TestStaticVarInFunction>();
    RegisterFreeFn<&TestConstantWithSignificantAddress>();
    RegisterFreeFn<&TestConstantWithInsignificantAddress>();
    RegisterFreeFn<&StringInterningQuirkyBehavior>();

    RegisterFreeFn<&TestNoExceptButThrows>();
}

// DO NOT MODIFY
// dump_symbols.cpp JIT entry point
//
extern "C" __attribute__((__optnone__, __used__))
void __pochivm_register_runtime_library__()
{
    RegisterRuntimeLibrary();
}
