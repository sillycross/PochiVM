#pragma once

#include "common.h"
#include "ast_expr_base.h"
#include "ast_variable.h"
#include "fastinterp/fastinterp_mem2reg_helper.h"

namespace PochiVM
{

// Same as a AstDereferenceVariableExpr, except that this is a variable that
// has been put into register by mem2reg pass
//
class AstRegisterCachedVariableExpr : public AstNodeBase
{
public:
    AstRegisterCachedVariableExpr(AstVariable* var, int regOrdinal)
        : AstNodeBase(AstNodeType::AstRegisterCachedVariableExpr, var->GetTypeId().RemovePointer())
        , m_variable(var), m_regOrdinal(regOrdinal)
    {
        TestAssert(!var->GetTypeId().RemovePointer().IsCppClassType() && !var->GetTypeId().RemovePointer().IsVoid());
        TestAssertImp(var->GetTypeId().RemovePointer().IsFloatingPoint(), static_cast<size_t>(regOrdinal) < x_mem2reg_max_floating_vars);
        TestAssertImp(!var->GetTypeId().RemovePointer().IsFloatingPoint(), static_cast<size_t>(regOrdinal) < x_mem2reg_max_integral_vars);
    }

    template<typename T>
    void InterpImpl(T* out)
    {
        T* src;
        m_variable->DebugInterp(&src);
        *out = *src;
    }

    GEN_CLASS_METHOD_SELECTOR(SelectImpl,
                              AstRegisterCachedVariableExpr,
                              InterpImpl,
                              AstTypeHelper::not_cpp_class_or_void_type)

    virtual void SetupDebugInterpImpl() override final
    {
        m_debugInterpFn = SelectImpl(GetTypeId());
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*&)> fn) override final
    {
        fn(*reinterpret_cast<AstNodeBase**>(&m_variable));
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override final;

    virtual FastInterpSnippet WARN_UNUSED PrepareForFastInterp(FISpillLocation spillLoc) override final;
    virtual void FastInterpSetupSpillLocation() override final { }

    AstVariable* m_variable;
    int m_regOrdinal;
};

class Mem2RegEligibleRegion
{
public:
    std::vector<AstRegisterCachedVariableExpr*> m_mem2RegInitList;
    std::vector<AstRegisterCachedVariableExpr*> m_mem2RegWritebackList;
};

FastInterpSnippet WARN_UNUSED FIMem2RegGenerateInitLogic(const std::vector<AstRegisterCachedVariableExpr*>& list);
FastInterpSnippet WARN_UNUSED FIMem2RegGenerateWritebackLogic(const std::vector<AstRegisterCachedVariableExpr*>& list);

}   // namespace PochiVM
