#pragma once

#include "ast_expr_base.h"

namespace PochiVM
{

// The operator that static_cast a type to another
//
class AstStaticCastExpr : public AstNodeBase
{
public:
    AstStaticCastExpr(AstNodeBase* operand, TypeId castTo)
        : AstNodeBase(castTo)
        , m_operand(operand)
    {
        TestAssert(m_operand->GetTypeId().MayStaticCastTo(castTo));
    }

    template<typename SrcT, typename DstT, typename Enable = void>
    struct safe_static_cast_helper
    {
        static void execute(SrcT* /*src*/, DstT* /*dst*/)
        {
            TestAssert(false);
        }
    };

    template<typename SrcT, typename DstT>
    struct safe_static_cast_helper<SrcT, DstT, typename std::enable_if<
            AstTypeHelper::may_static_cast<SrcT, DstT>::value
    >::type >
    {
        static void execute(SrcT* src, DstT* dst)
        {
            *dst = static_cast<DstT>(*src);
        }
    };

    template<typename T>
    struct assert_not_nullptr_helper
    {
        static void execute(T* /*value*/) { }
    };

    template<typename T>
    struct assert_not_nullptr_helper<T*>
    {
        static void execute(T** TESTBUILD_ONLY(value))
        {
            TestAssert(*value != nullptr);
        }
    };

    template<typename SrcT, typename DstT>
    void InterpImpl(DstT* out)
    {
        TestAssert((AstTypeHelper::may_static_cast<SrcT, DstT>::value));
        SrcT value;
        m_operand->Interp(&value);
        // We do not allow static_cast if the src value is nullptr
        //
        assert_not_nullptr_helper<SrcT>::execute(&value);
        safe_static_cast_helper<SrcT, DstT>::execute(&value, out);
    }

    GEN_CLASS_METHOD_SELECTOR(SelectImpl, AstStaticCastExpr, InterpImpl, AstTypeHelper::not_cpp_class_or_void_type)

    virtual void SetupInterpImpl() override
    {
        m_interpFn = SelectImpl(m_operand->GetTypeId(), GetTypeId());
    }

    virtual void ForEachChildren(const std::function<void(AstNodeBase*)>& fn) override
    {
        fn(m_operand);
    }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstStaticCastExpr; }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

private:
    AstNodeBase* m_operand;
};

// The operator that reinterpret_cast a type to another
//
class AstReinterpretCastExpr : public AstNodeBase
{
public:
    AstReinterpretCastExpr(AstNodeBase* operand, TypeId castTo)
        : AstNodeBase(castTo)
        , m_operand(operand)
    {
        TestAssert(m_operand->GetTypeId().MayReinterpretCastTo(castTo));
    }

    template<typename SrcT, typename DstT>
    void InterpImpl(DstT* out)
    {
        SrcT value;
        m_operand->Interp(&value);
        *out = reinterpret_cast<DstT>(value);
    }

    GEN_CLASS_METHOD_SELECTOR(SelectImpl, AstReinterpretCastExpr, InterpImpl, AstTypeHelper::pointer_or_uint64_type)

    virtual void SetupInterpImpl() override
    {
        m_interpFn = SelectImpl(m_operand->GetTypeId(), GetTypeId());
    }

    virtual void ForEachChildren(const std::function<void(AstNodeBase*)>& fn) override
    {
        fn(m_operand);
    }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstReinterpretCastExpr; }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

private:
    AstNodeBase* m_operand;
};

}   // namespace PochiVM
