#pragma once

#include "ast_expr_base.h"
#include "destructor_helper.h"
#include "exception_helper.h"
#include "function_proto.h"

namespace PochiVM
{

// Throw an exception
//
class AstThrowStmt : public AstNodeBase
{
public:
    AstThrowStmt(AstNodeBase* operand, bool isCtor, bool isLValueObject)
        : AstNodeBase(TypeId::Get<void>())
        , m_operand(operand)
        , m_isCtor(isCtor)
        , m_isLValueObject(isLValueObject)
    {
        TestAssert(!(m_isCtor && m_isLValueObject));
        if (m_isCtor)
        {
            TestAssert(m_operand->GetAstNodeType() == AstNodeType::AstCallExpr);
            TestAssert(m_operand->GetTypeId() == TypeId::Get<void>());
            // The operand is a CallExpr for constructor, the TypeId of the
            // exception is the first parameter (which is where the value is constructed)'s type without pointer
            //
            AstCallExpr* callExpr = assert_cast<AstCallExpr*>(m_operand);
            TestAssert(callExpr->GetParams().size() > 0);
            TestAssert(callExpr->GetParams()[0]->GetTypeId().IsPointerType());
            m_exceptionTypeId = callExpr->GetParams()[0]->GetTypeId().RemovePointer();
            TestAssert(m_exceptionTypeId != TypeId::Get<void>());
        }
        else if (m_isLValueObject)
        {
            // This expression is like 'throw obj' where obj is a LValue to a CPP class
            // We have to distinguish this case with the 'isCtor' case because otherwise,
            // we would have to call copy constructor twice (instead of once as one would expect in C++) in interp mode
            //
            // The operand is the CallExpr for the copy constructor.
            //
            TestAssert(m_operand->GetAstNodeType() == AstNodeType::AstCallExpr);
            TestAssert(m_operand->GetTypeId() == TypeId::Get<void>());
            AstCallExpr* callExpr = assert_cast<AstCallExpr*>(m_operand);
            // A copy constructor should take exactly two parameters (the address into which copied
            // object is constructed, and the address of original object), which has same type.
            //
            TestAssert(callExpr->GetParams().size() == 2);
            TestAssert(callExpr->GetParams()[0]->GetTypeId() == callExpr->GetParams()[1]->GetTypeId());
            TestAssert(callExpr->GetParams()[0]->GetTypeId().IsPointerType());
            m_exceptionTypeId = callExpr->GetParams()[0]->GetTypeId().RemovePointer();
            TestAssert(m_exceptionTypeId.IsCppClassType());
            TestAssert(IsCppClassCopyConstructible(m_exceptionTypeId));
        }
        else
        {
            // Otherwise, it is either a fundamental-type value, or a CPP class returned from a function
            // Its type is just the exception type.
            //
            m_exceptionTypeId = m_operand->GetTypeId();
            TestAssert(m_exceptionTypeId != TypeId::Get<void>());
        }
        TestAssert(IsTypeRegisteredForThrownFromGeneratedCode(m_exceptionTypeId));
        TestAssertImp(m_exceptionTypeId.IsCppClassType(),
                      GetDestructorMetadata(m_exceptionTypeId) != nullptr);
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) override
    {
        fn(m_operand);
    }

    virtual AstNodeType GetAstNodeType() const override
    {
        return AstNodeType::AstThrowStmt;
    }

    template<typename T>
    void InterpImpl(void* /*out*/)
    {
        if (m_isLValueObject)
        {
            T* addr;
            // The address of the LValue object is the second parameter to the copy-constructor callExpr
            //
            AstCallExpr* callExpr = assert_cast<AstCallExpr*>(m_operand);
            callExpr->GetParams()[1]->Interp(&addr /*out*/);
            throw *addr;
        }
        else
        {
            T* addr = reinterpret_cast<T*>(alloca(sizeof(T)));
            if (!m_isCtor)
            {
                // If m_operand is not a CallExpr for constructor, just evaluate it and we will get what we want
                //
                m_operand->Interp(addr /*out*/);
            }
            else
            {
                // CallExpr for constructor is special: it returns void, and the address to construct
                // the object is placed at the first operand. We do not know the address until now.
                // So the CallExpr was taking a AstLiteral parameter as a placeholder, and we rewrite its value now.
                //
                AstCallExpr* callExpr = assert_cast<AstCallExpr*>(m_operand);
                AstLiteralExpr* literalExpr = assert_cast<AstLiteralExpr*>(callExpr->GetParams()[0]);
                assert(literalExpr->GetTypeId() == m_exceptionTypeId.AddPointer());
                literalExpr->ResetPointerValue(addr);
                m_operand->Interp(nullptr /*out*/);
            }
            // At this time the exception object is constructed.
            //
            Auto(
                // If T is a CPP class, we need to call its destructor, after the exception is thrown out.
                //
                if (DestructorCppFnMetadata<T>::value != nullptr)
                {
                    InterpCallDestructorHelper(DestructorCppFnMetadata<T>::value, addr);
                }
            );
            // Throw out the exception.
            // This always invokes the copy constructor for non-fundamental type T,
            // but this is allowed by standard (C++17 guaranteed copy elision does not apply to throw)
            //
            // This is also why we had to special-case the LValue object case, otherwise we would be
            // first copying the LValue object to a temporary (addr) by copy constructor, then invoke
            // the copy constructor the second time when we throw, which is against the standard.
            //
            throw *addr;
        }
    }

    template<typename T>
    struct select_impl_helper
    {
        static void* get()
        {
            return AstTypeHelper::GetClassMethodPtr(&AstThrowStmt::InterpImpl<T>);
        }
    };

    virtual void SetupInterpImpl() override
    {
        m_interpFn = select_impl_based_on_exception_type<select_impl_helper>::get(m_exceptionTypeId);
        TestAssert(m_interpFn != nullptr);
    }

    AstNodeBase* m_operand;
    TypeId m_exceptionTypeId;
    bool m_isCtor;
    bool m_isLValueObject;
};

}   // namespace PochiVM
