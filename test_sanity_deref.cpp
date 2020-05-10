#include "gtest/gtest.h"

#include "pochivm.h"

using namespace Ast;

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

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = std::function<uint64_t(void*)>;

    {
        auto [fn, head] = NewFunction<FnPrototype>("compute_linked_list_sum");
        auto sum = fn.NewVariable<uint64_t>();
        fn.SetBody(
            Declare(sum, Literal<uint64_t>(0)),
            While(head.ReinterpretCast<uint64_t>() != Literal<uint64_t>(0)).Do(
                Assign(sum, sum + head.ReinterpretCast<uint64_t*>().Deref()),
                Assign(head, (head.ReinterpretCast<uint64_t>() + Literal<uint64_t>(8))
                                 .ReinterpretCast<void**>().Deref())
            ),
            Return(sum)
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    FnPrototype interpFn = thread_pochiVMContext->m_curModule->
                           GetGeneratedFunctionInterpMode<FnPrototype>("compute_linked_list_sum");

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

    uint64_t ret = interpFn(head);
    ReleaseAssert(ret == expectedSum);
}

TEST(Sanity, StoreIntoLocalVar)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype1 = std::function<void(int*, int)>;
    {
        auto [fn, addr, value] = NewFunction<FnPrototype1>("store_value");
        fn.SetBody(
            (value + Literal<int>(233)).StoreIntoAddress(addr)
        );
    }

    {
        auto [fn, addr, value] = NewFunction<FnPrototype1>("inc_value");
        fn.SetBody(
            (addr.Deref() + value).StoreIntoAddress(addr)
        );
    }

    using FnPrototype2 = std::function<int(int, int)>;
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
    thread_pochiVMContext->m_curModule->PrepareForInterp();

    {
        FnPrototype2 interpFn = thread_pochiVMContext->m_curModule->
                                GetGeneratedFunctionInterpMode<FnPrototype2>("a_plus_b_plus_233");

        ReleaseAssert(interpFn(123, 456) == 123 + 456 + 233);
    }

    {
        FnPrototype1 interpFn = thread_pochiVMContext->m_curModule->
                                GetGeneratedFunctionInterpMode<FnPrototype1>("store_value");

        int x = 29310923;
        interpFn(&x, 101);
        ReleaseAssert(x == 101 + 233);
    }

    {
        FnPrototype1 interpFn = thread_pochiVMContext->m_curModule->
                                GetGeneratedFunctionInterpMode<FnPrototype1>("inc_value");

        int x = 12345;
        interpFn(&x, 543);
        ReleaseAssert(x == 12345 + 543);
    }
}
