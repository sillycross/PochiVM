#include "error_context.h"
#include "pochivm_context.h"
#include "codegen_context.hpp"

namespace PochiVM
{

thread_local LLVMCodegenContext* thread_llvmContext = nullptr;

thread_local ErrorContext* thread_errorContext = nullptr;

thread_local PochiVMContext* thread_pochiVMContext = nullptr;

namespace ReflectionHelper
{
class RawFnTypeNamesInfo;
}

void NO_RETURN __pochivm_report_info__(ReflectionHelper::RawFnTypeNamesInfo*)
{
    ReleaseAssert(false);
}

}
