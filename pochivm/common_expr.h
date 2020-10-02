#pragma once

#include "ast_expr_base.h"

namespace PochiVM
{

// The operator that dereference a pointer
// T* -> T
//
class AstDereferenceExpr : public AstNodeBase
{
public:
    AstDereferenceExpr(AstNodeBase* operand)
        : AstNodeBase(operand->GetTypeId().RemovePointer())  
        , m_operand(operand)
    {
        TestAssert(m_operand->GetTypeId().IsPointerType());
        // cannot dereference a cpp_class* or void*, those do not make sense
        //
        TestAssert(!GetTypeId().IsCppClassType() && !GetTypeId().IsVoid());
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    template<typename T>
    void InterpImpl(T* out)
    {
        T* src;
        m_operand->DebugInterp(&src);
        *out = *src;
    }

    GEN_CLASS_METHOD_SELECTOR(SelectImpl, AstDereferenceExpr, InterpImpl, AstTypeHelper::not_cpp_class_or_void_type)

    virtual void SetupDebugInterpImpl() override
    {
        m_debugInterpFn = SelectImpl(GetTypeId());
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) override
    {
        fn(m_operand);
    }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstDereferenceExpr; }

private:
    AstNodeBase* m_operand;
};

// A primitive type literal value known at codegen time
//
class AstLiteralExpr : public AstNodeBase
{
public:
    AstLiteralExpr(TypeId typeId, void* valuePtr)
        : AstNodeBase(typeId)
        , m_useHijackedLLVMValue(false)
        , m_hijackedLLVMValue(nullptr)
    {
        TestAssert(typeId.IsPrimitiveType() || typeId.IsPointerType());
        InitLiteralValue(typeId, valuePtr);
    }

    template<typename T>
    void InterpImpl(T* out)
    {
        *out = *reinterpret_cast<T*>(m_as_rawdata);
    }

    GEN_CLASS_METHOD_SELECTOR(SelectImpl, AstLiteralExpr, InterpImpl, AstTypeHelper::primitive_or_pointer_type)

    virtual void SetupDebugInterpImpl() override
    {
        m_debugInterpFn = SelectImpl(GetTypeId());
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> /*fn*/) override { }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstLiteralExpr; }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    // A hacky method used only by 'throw' statement's interp execution
    //
    void ResetPointerValue(void* value)
    {
        TestAssert(GetTypeId().IsPointerType());
        m_as_voidstar = value;
    }

    // A hacky method used only by 'throw' statement's LLVM codegen
    //
    void HijackPointerValueLLVM(llvm::Value* value);

private:
    // Stores the literal value with a union of all possible primitive types
    //
    union
    {
        char m_as_rawdata[8];
        void* m_as_voidstar;
#define F(type) type m_as_ ## type;
FOR_EACH_PRIMITIVE_TYPE
#undef F
    };

    bool m_useHijackedLLVMValue;
    llvm::Value* m_hijackedLLVMValue;

    void InitLiteralValue(TypeId typeId, void* valuePtr)
    {
#define F(type) if (typeId.IsType<type>()) { m_as_ ## type = *reinterpret_cast<type*>(valuePtr); return; }
FOR_EACH_PRIMITIVE_TYPE
#undef F
        if (typeId.IsPointerType()) { m_as_voidstar = *reinterpret_cast<void**>(valuePtr); return; }
        TestAssert(false);
    }

    // For primitive int type, return a static cast to uint64_t (as LLVM interface expects)
    //
    uint64_t StaticCastIntTypeValueToUInt64()
    {
        TypeId typeId = GetTypeId();
        assert(typeId.IsPrimitiveIntType());
#define F(type) if (typeId.IsType<type>()) { return static_cast<uint64_t>(m_as_ ## type); }
FOR_EACH_PRIMITIVE_INT_TYPE
#undef F
        TestAssert(false);
        __builtin_unreachable();
    }

    float GetFloat()
    {
        assert(GetTypeId().IsFloat());
        return m_as_float;
    }

    double GetDouble()
    {
        assert(GetTypeId().IsDouble());
        return m_as_double;
    }
};

// Assign expression: *dst = src
//
class AstAssignExpr : public AstNodeBase
{
public:
    AstAssignExpr(AstNodeBase* dst, AstNodeBase* src)
        : AstNodeBase(TypeId::Get<void>())
        , m_dst(dst), m_src(src)
    {
        TestAssert(m_src->GetTypeId().IsPrimitiveType() || m_src->GetTypeId().IsPointerType());
        TestAssert(m_dst->GetTypeId() == m_src->GetTypeId().AddPointer());
    }

    TypeId GetValueType() const
    {
        return m_src->GetTypeId();
    }

    template<typename SrcT>
    void InterpImpl(void* /*out*/)
    {
        SrcT src;
        SrcT* dst;
        m_src->DebugInterp(&src);
        m_dst->DebugInterp(&dst);
        *dst = src;
    }

    GEN_CLASS_METHOD_SELECTOR(SelectImpl, AstAssignExpr, InterpImpl, AstTypeHelper::primitive_or_pointer_type)

    AstNodeBase* GetDst() const { return m_dst; }

    virtual void SetupDebugInterpImpl() override
    {
        m_debugInterpFn = SelectImpl(m_src->GetTypeId());
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) override
    {
        fn(m_src);
        fn(m_dst);
    }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstAssignExpr; }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

private:
    AstNodeBase* m_dst;
    AstNodeBase* m_src;
};

// A NULL pointer known at codegen time
//
class AstNullptrExpr : public AstNodeBase
{
public:
    AstNullptrExpr(TypeId typeId)
        : AstNodeBase(typeId)
    {
        TestAssert(typeId.IsPointerType());
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    virtual void SetupDebugInterpImpl() override
    {
        TestAssert(false && "unimplemented");
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> /*fn*/) override { }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstNullptrExpr; }
};

// A TRASH pointer known at codegen time
// Internally this is just reinterpret_cast<T*>(0x1). Using a trash pointer in any way
// (including attempting to check whether it is a trash pointer) is undefined behavior.
//
// The main use of this is to provide a value to a param that will never be used,
// while allowing static_cast to act on it without firing an assert. e.g.:
//
// void B(bool shouldWrite, Base* writeTarget) { if (shouldWrite) writeTarget->x = 1; }
// void A(bool shouldWrite, Derived* writeTarget) { B(shouldWrite, writeTarget.StaticCast<Base*>()); }
//
// One might want to call A(false, nullptr), but that would fire the assert that
// static_cast cannot work on NULL pointers. We know this is actually a false alarm
// because 'writeTarget' will not be used as long as 'shouldWrite' is false.
// We can circumvent that by doing A(false, TrashPtr).
//
class AstTrashPtrExpr : public AstNodeBase
{
public:
    AstTrashPtrExpr(TypeId typeId)
        : AstNodeBase(typeId)
    {
        TestAssert(typeId.IsPointerType());
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    virtual void SetupDebugInterpImpl() override
    {
        TestAssert(false && "unimplemented");
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> /*fn*/) override { }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstTrashPtrExpr; }
};

// Converts a rvalue to a temporary reference, so it matches the expectation of a C++ function
// Internally it just creates an alloca, put the rvalue there, and returns the address
//
class AstRvalueToConstPrimitiveRefExpr : public AstNodeBase
{
public:
    AstRvalueToConstPrimitiveRefExpr(AstNodeBase* operand)
        : AstNodeBase(operand->GetTypeId().AddPointer())
        , m_operand(operand)
    {
        TestAssert(operand->GetTypeId().IsPrimitiveType() || operand->GetTypeId().IsPointerType());
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    template<typename T>
    void InterpImpl(T** out)
    {
        T* addr = reinterpret_cast<T*>(thread_pochiVMContext->m_debugInterpStackFrameBase + m_debugInterpOffset);
        m_operand->DebugInterp(addr);
        *out = addr;
    }

    GEN_CLASS_METHOD_SELECTOR(SelectImpl, AstRvalueToConstPrimitiveRefExpr, InterpImpl, AstTypeHelper::not_cpp_class_or_void_type)

    virtual void SetupDebugInterpImpl() override
    {
        m_debugInterpFn = SelectImpl(GetTypeId());
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) override
    {
        fn(m_operand);
    }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstRvalueToConstPrimitiveRefExpr; }

    AstNodeBase* m_operand;
    uint32_t m_debugInterpOffset;
};

}   // namespace PochiVM
