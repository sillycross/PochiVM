#include "common.h"
#include "pochivm_context.h"

namespace PochiVM
{

namespace ReflectionHelper
{
class RawFnTypeNamesInfo;
}

// The __pochivm_report_info__ hook is hooked up by dump_symbols in build_runtime_lib
// In PochiVM main libaray it should never be called. Give it a definition just to make it links.
//
void NO_RETURN __pochivm_report_info__(ReflectionHelper::RawFnTypeNamesInfo*)
{
    ReleaseAssert(false);
}

// Defintion for 'thread_pochiVMFastInterpOutstandingExceptionPtr'
//
thread_local std::exception_ptr thread_pochiVMFastInterpOutstandingExceptionPtr;

}   // namespace PochiVM
