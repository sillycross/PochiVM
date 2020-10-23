#include "destructor_helper.h"
#include "pochivm.hpp"
#include "scoped_variable_manager.h"

namespace PochiVM
{

using namespace llvm;

void ScopedVariableManager::EmitIRDestructAllVariablesUntilScope(AstNodeBase* boundaryScope)
{
    TestAssert(m_operationMode == OperationMode::LLVM);
    TestAssert(!thread_llvmContext->m_isCursorAtDummyBlock);
    auto rit = m_scopeStack.rbegin();
    while (rit != m_scopeStack.rend())
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
    TestAssertImp(boundaryScope != nullptr, rit != m_scopeStack.rend() && rit->first == boundaryScope);
    TestAssertImp(boundaryScope == nullptr, rit == m_scopeStack.rend());
}

}   // namespace PochiVM
