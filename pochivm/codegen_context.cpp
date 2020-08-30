#include "codegen_context.hpp"

namespace PochiVM
{

AutoThreadLLVMCodegenContext::AutoThreadLLVMCodegenContext()
{
    TestAssert(thread_llvmContext == nullptr);
    m_contextPtr = new LLVMCodegenContext();
    ReleaseAssert(m_contextPtr != nullptr);
    thread_llvmContext = m_contextPtr;
}

AutoThreadLLVMCodegenContext::~AutoThreadLLVMCodegenContext()
{
    TestAssert(thread_llvmContext == m_contextPtr);
    delete m_contextPtr;
    thread_llvmContext = nullptr;
}

}   // namespace PochiVM
