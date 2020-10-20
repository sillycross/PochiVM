#include "gtest/gtest.h"

#include "pochivm.h"
#include "test_util_helper.h"
#include "codegen_context.hpp"

using namespace PochiVM;

namespace {

struct Data
{
    uint64_t value;
    Data* next;
};

Data* BuildLinkedList()
{
    Data* head = nullptr;
    for (int i = 0; i < 10000; i++)
    {
        Data* n = new Data();
        n->value = static_cast<uint64_t>(rand());
        n->next = head;
        head = n;
    }
    return head;
}

}   // anonymous namespace

TEST(Sanity, LinkedListChasing)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = uint64_t(*)(void*);

    {
        auto [fn, head] = NewFunction<FnPrototype>("compute_linked_list_sum", "p");
        auto sum = fn.NewVariable<uint64_t>("sum");
        fn.SetBody(
            Declare(sum, Literal<uint64_t>(0)),
            While(ReinterpretCast<uint64_t>(head) != Literal<uint64_t>(0)).Do(
                Assign(sum, sum + *ReinterpretCast<uint64_t*>(head)),
                Assign(head, *ReinterpretCast<void**>(ReinterpretCast<uint64_t>(head) + Literal<uint64_t>(8)))
            ),
            Return(sum)
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();
    thread_pochiVMContext->m_curModule->EmitIR();

    Data* head = BuildLinkedList();
    uint64_t expectedSum = 0;
    {
        Data* p = head;
        while (p != nullptr)
        {
            expectedSum += p->value;
            p = p->next;
        }
    }

    {
        auto interpFn = thread_pochiVMContext->m_curModule->
                               GetDebugInterpGeneratedFunction<FnPrototype>("compute_linked_list_sum");

        uint64_t ret = interpFn(head);
        ReleaseAssert(ret == expectedSum);
    }

    {
        FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
                               GetFastInterpGeneratedFunction<FnPrototype>("compute_linked_list_sum");

        uint64_t ret = interpFn(head);
        ReleaseAssert(ret == expectedSum);
    }

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    {
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("compute_linked_list_sum");
        uint64_t ret = jitFn(head);
        ReleaseAssert(ret == expectedSum);
    }
}

TEST(Sanity, StoreIntoLocalVar)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype1 = void(*)(int*, int);
    {
        auto [fn, addr, value] = NewFunction<FnPrototype1>("store_value");
        fn.SetBody(
            Assign(*addr, value + Literal<int>(233))
        );
    }

    {
        auto [fn, addr, value] = NewFunction<FnPrototype1>("inc_value");
        fn.SetBody(
            Assign(*addr, *addr + value)
        );
    }

    using FnPrototype2 = int(*)(int, int);
    {
        auto [fn, value1, value2] = NewFunction<FnPrototype2>("a_plus_b_plus_233");
        auto ret = fn.NewVariable<int>();
        fn.SetBody(
            Declare(ret),
            Call<FnPrototype1>("store_value", ret.Addr(), value1),
            Call<FnPrototype1>("inc_value", ret.Addr(), value2),
            Return(ret)
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();
    thread_pochiVMContext->m_curModule->EmitIR();

    {
        auto interpFn = thread_pochiVMContext->m_curModule->
                                GetDebugInterpGeneratedFunction<FnPrototype2>("a_plus_b_plus_233");

        ReleaseAssert(interpFn(123, 456) == 123 + 456 + 233);
    }

    {
        auto interpFn = thread_pochiVMContext->m_curModule->
                                GetDebugInterpGeneratedFunction<FnPrototype1>("store_value");

        int x = 29310923;
        interpFn(&x, 101);
        ReleaseAssert(x == 101 + 233);
    }

    {
        auto interpFn = thread_pochiVMContext->m_curModule->
                                GetDebugInterpGeneratedFunction<FnPrototype1>("inc_value");

        int x = 12345;
        interpFn(&x, 543);
        ReleaseAssert(x == 12345 + 543);
    }

    thread_pochiVMContext->m_curModule->PrepareForFastInterp();

    {
        FastInterpFunction<FnPrototype2> interpFn = thread_pochiVMContext->m_curModule->
                                GetFastInterpGeneratedFunction<FnPrototype2>("a_plus_b_plus_233");

        ReleaseAssert(interpFn(123, 456) == 123 + 456 + 233);
    }

    {
        FastInterpFunction<FnPrototype1> interpFn = thread_pochiVMContext->m_curModule->
                                GetFastInterpGeneratedFunction<FnPrototype1>("store_value");

        int x = 29310923;
        interpFn(&x, 101);
        ReleaseAssert(x == 101 + 233);
    }

    {
        FastInterpFunction<FnPrototype1> interpFn = thread_pochiVMContext->m_curModule->
                                GetFastInterpGeneratedFunction<FnPrototype1>("inc_value");

        int x = 12345;
        interpFn(&x, 543);
        ReleaseAssert(x == 12345 + 543);
    }

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    {
        FnPrototype2 jitFn = jit.GetFunction<FnPrototype2>("a_plus_b_plus_233");

        ReleaseAssert(jitFn(123, 456) == 123 + 456 + 233);
    }

    {
        FnPrototype1 jitFn = jit.GetFunction<FnPrototype1>("store_value");

        int x = 29310923;
        jitFn(&x, 101);
        ReleaseAssert(x == 101 + 233);
    }

    {
        FnPrototype1 jitFn = jit.GetFunction<FnPrototype1>("inc_value");

        int x = 12345;
        jitFn(&x, 543);
        ReleaseAssert(x == 12345 + 543);
    }
}

TEST(Sanity, BoolDeref)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = void(*)(bool*, bool);
    {
        auto [fn, addr, value] = NewFunction<FnPrototype>("store_bool");
        fn.SetBody(
                Assign(*addr, value)
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();
    thread_pochiVMContext->m_curModule->EmitIR();

    {
        auto interpFn = thread_pochiVMContext->m_curModule->
                               GetDebugInterpGeneratedFunction<FnPrototype>("store_bool");

        bool x;
        interpFn(&x, true);
        ReleaseAssert(x == true);
        ReleaseAssert(*reinterpret_cast<uint8_t*>(&x) == 1);

        interpFn(&x, false);
        ReleaseAssert(x == false);
        ReleaseAssert(*reinterpret_cast<uint8_t*>(&x) == 0);

        interpFn(&x, true);
        ReleaseAssert(x == true);
        ReleaseAssert(*reinterpret_cast<uint8_t*>(&x) == 1);
    }

    thread_pochiVMContext->m_curModule->PrepareForFastInterp();

    {
        FastInterpFunction<FnPrototype> interpFn = thread_pochiVMContext->m_curModule->
                               GetFastInterpGeneratedFunction<FnPrototype>("store_bool");

        bool x;
        interpFn(&x, true);
        ReleaseAssert(x == true);
        ReleaseAssert(*reinterpret_cast<uint8_t*>(&x) == 1);

        interpFn(&x, false);
        ReleaseAssert(x == false);
        ReleaseAssert(*reinterpret_cast<uint8_t*>(&x) == 0);

        interpFn(&x, true);
        ReleaseAssert(x == true);
        ReleaseAssert(*reinterpret_cast<uint8_t*>(&x) == 1);
    }

    std::string _dst;
    llvm::raw_string_ostream rso(_dst /*target*/);
    thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
    std::string& dump = rso.str();

    AssertIsExpectedOutput(dump);

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    {
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("store_bool");

        bool x;
        jitFn(&x, true);
        ReleaseAssert(x == true);
        ReleaseAssert(*reinterpret_cast<uint8_t*>(&x) == 1);

        jitFn(&x, false);
        ReleaseAssert(x == false);
        ReleaseAssert(*reinterpret_cast<uint8_t*>(&x) == 0);

        jitFn(&x, true);
        ReleaseAssert(x == true);
        ReleaseAssert(*reinterpret_cast<uint8_t*>(&x) == 1);
    }
}
