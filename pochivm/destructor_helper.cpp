#include "destructor_helper.h"
#include "pochivm.hpp"

namespace PochiVM
{

using namespace llvm;

void EmitIRDestructAllVariablesUntilScope(AstNodeBase* boundaryScope)
{
    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    auto rit = thread_llvmContext->m_scopeStack.rbegin();
    while (rit != thread_llvmContext->m_scopeStack.rend())
    {
        const std::vector<DestructorIREmitter*>& vec = rit->second;
        for (auto rit2 = vec.rbegin(); rit2 != vec.rend(); rit2++)
        {
            DestructorIREmitter* e = *rit2;
            e->EmitDestructorIR();
        }
        if (rit->first == boundaryScope)
        {
            break;
        }
        rit++;
    }
    TestAssertImp(boundaryScope != nullptr, rit != thread_llvmContext->m_scopeStack.rend() && rit->first == boundaryScope);
    TestAssertImp(boundaryScope == nullptr, rit == thread_llvmContext->m_scopeStack.rend());
}

}   // namespace PochiVM
