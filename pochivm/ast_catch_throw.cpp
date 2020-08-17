#include "ast_catch_throw.h"
#include "pochivm.hpp"

namespace PochiVM
{

using namespace llvm;

Value* WARN_UNUSED NO_RETURN AstThrowStmt::EmitIRImpl()
{
    // TODO: implement
    ReleaseAssert(false);
}

}   // namespace PochiVM
