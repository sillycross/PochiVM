#include "fastinterp_codegen_helper.h"
#include "llvm/Support/Memory.h"

namespace PochiVM
{

void FastInterpCodegenEngine::InvalidateInstructionCache(const void* addr, size_t len)
{
    llvm::sys::Memory::InvalidateInstructionCache(addr, len);
}

}   // namespace PochiVM
