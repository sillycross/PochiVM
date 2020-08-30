#pragma once

#include "common.h"

namespace PochiVM
{

struct LLVMCodegenContext;

class AutoThreadLLVMCodegenContext
{
public:
    AutoThreadLLVMCodegenContext();
    ~AutoThreadLLVMCodegenContext();

private:
    LLVMCodegenContext* m_contextPtr;
};
#define AutoLLVMCodegenContext(...) static_assert(false, "Wrong use of 'auto'-pattern!");

inline thread_local LLVMCodegenContext* thread_llvmContext = nullptr;

}   // namespace PochiVM
