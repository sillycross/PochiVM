#include "error_context.h"
#include "pochivm_context.h"
#include "codegen_context.hpp"

#include "pochivm.hpp"

thread_local LLVMCodegenContext* thread_llvmContext = nullptr;

thread_local ErrorContext* thread_errorContext = nullptr;

thread_local Ast::PochiVMContext* thread_pochiVMContext = nullptr;
