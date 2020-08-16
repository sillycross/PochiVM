#pragma once

#include "common.h"
#include "generated/pochivm_runtime_cpp_typeinfo.generated.h"

namespace llvm {
class BasicBlock;
}   // namespace LLVM

namespace PochiVM
{

// Exception handling helpers
//
// The high-level structure for exception-handling looks like this:
//     alloca { i8*, i32 } _cur_exception_obj_and_type
//     ...
//     invoke ... normal_dest, eh_landingpad_1 ...
// normal_dest:
//     ...
// eh_landingpad_1:
//     %1 = landingpad {i8*, i32} cleanup catch ...
//     store %1, _cur_exception_obj_and_type
//     br eh_dtor_tree_1
// eh_dtor_tree_1:  (multiple entry points, but always converge to eh_catch)
//     ... call destructors ...
//     br eh_catch
// eh_catch:   (all exceptions enters here in the end)
//     %2 = load _cur_exception_obj_and_type
//     ( ...handle exception using data from %2, or resume %2 if no catch clause exists )
//     _cur_exception_obj_and_type may be overwritten in the logic of handling exception,
//     if the logic contains a nested try-catch clause
//

inline bool IsTypeRegisteredForThrownFromGeneratedCode(TypeId typeId)
{
     return valid_for_ast_throw_stmt_helper::query(typeId);
}

// Return true if there is no need to setup a landing pad for potentially-throwing function.
// This is possible if we are not in a try-catch block and there is no destructor to call.
//
bool WARN_UNUSED IsNoLandingPadNeeded();

// Emit a basic block that the control flow should branch to
// if there is an exception thrown right at the current program position.
// IRBuilder IP is unchanged before and after the call.
//
llvm::BasicBlock* WARN_UNUSED EmitEHLandingPadForCurrentPosition();

}   // namespace PochiVM
