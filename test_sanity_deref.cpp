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
