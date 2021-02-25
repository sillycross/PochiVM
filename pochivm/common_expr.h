#pragma once

#include "ast_expr_base.h"
#include "ast_mem2reg.h"

namespace PochiVM
{

// The operator that dereference a pointer
// T* -> T
//
class AstDereferenceExpr : public AstNodeBase
{
public:
    AstDereferenceExpr(AstNodeBase* operand)
        : AstNodeBase(AstNodeType::AstDereferenceExpr, operand->GetTypeId().RemovePointer())
        , m_operand(operand)
    {
        TestAssert(m_operand->GetTypeId().IsPointerType());
        // cannot dereference a cpp_class* or void*, those do not make sense
        //
        TestAssert(!GetTypeId().IsCppClassType() && !GetTypeId().IsVoid());
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override final;

    template<typename T>
    void InterpImpl(T* out)
    {
        T* src;
        m_operand->DebugInterp(&src);
        *out = *src;
    }

    GEN_CLASS_METHOD_SELECTOR(SelectImpl, AstDereferenceExpr, InterpImpl, AstTypeHelper::not_cpp_class_or_void_type)

    virtual void SetupDebugInterpImpl() override final
    {
        m_debugInterpFn = SelectImpl(GetTypeId());
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*&)> fn) override final
    {
        fn(m_operand);
    }

    virtual FastInterpSnippet WARN_UNUSED PrepareForFastInterp(FISpillLocation spillLoc) override final;

    virtual void FastInterpSetupSpillLocation() override final;

    AstNodeBase* GetOperand() const
    {
        return m_operand;
    }

private:
    AstNodeBase* m_operand;
};

// A primitive type literal value known at codegen time
//
class AstLiteralExpr : public AstNodeBase
{
public:
    AstLiteralExpr(TypeId typeId, void* valuePtr)
        : AstNodeBase(AstNodeType::AstLiteralExpr, typeId)
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

    virtual void SetupDebugInterpImpl() override final
    {
        m_debugInterpFn = SelectImpl(GetTypeId());
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*&)> /*fn*/) override final { }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override final;

    bool IsAllBitsZero() const
    {
        if (GetTypeId().Size() == 1)
        {
            return m_as_uint8_t == 0;
        }
        else if (GetTypeId().Size() == 2)
        {
            return m_as_uint16_t == 0;
        }
        else if (GetTypeId().Size() == 4)
        {
            return m_as_uint32_t == 0;
        }
        else
        {
            TestAssert(GetTypeId().Size() == 8);
            return m_as_uint64_t == 0;
        }
    }

    virtual FastInterpSnippet WARN_UNUSED PrepareForFastInterp(FISpillLocation spillLoc) override final;
    virtual void FastInterpSetupSpillLocation() override final { }

    uint64_t GetAsU64()
    {
        return m_as_uint64_t;
    }

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

    void InitLiteralValue(TypeId typeId, void* valuePtr)
    {
        m_as_uint64_t = 0;
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

class AstExceptionAddressPlaceholder : public AstNodeBase
{
public:
    AstExceptionAddressPlaceholder(TypeId exnType)
        : AstNodeBase(AstNodeType::AstExceptionAddressPlaceholder, exnType.AddPointer())
        , m_llvmValueSet(false)
        , m_debuginterpValueSet(false)
        , m_fastInterpStackOffsetSet(false)
    { }

    void DebugInterpImpl(void** out)
    {
        TestAssert(m_debuginterpValueSet);
        *out = m_debugInterpValue;
    }

    // SetDebugInterpValue may be called more than once
    //
    void SetDebugInterpValue(void* value)
    {
        m_debuginterpValueSet = true;
        m_debugInterpValue = value;
    }

    void SetLLVMValue(llvm::Value* value);

    void SetFastInterpValue(uint64_t offset)
    {
        TestAssert(!m_fastInterpStackOffsetSet);
        m_fastInterpStackOffsetSet = true;
        m_fastInterpStackOffset = offset;
    }

    virtual void SetupDebugInterpImpl() override final
    {
        m_debugInterpFn = AstTypeHelper::GetClassMethodPtr(&AstExceptionAddressPlaceholder::DebugInterpImpl);
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*&)> /*fn*/) override final { }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override final;

    virtual FastInterpSnippet WARN_UNUSED PrepareForFastInterp(FISpillLocation /*spillLoc*/) override final;

    virtual void FastInterpSetupSpillLocation() override final { }

private:
    llvm::Value* m_llvmValue;
    void* m_debugInterpValue;
    uint64_t m_fastInterpStackOffset;
    bool m_llvmValueSet;
    bool m_debuginterpValueSet;
    bool m_fastInterpStackOffsetSet;
};

// Assign expression: *dst = src
//
class AstAssignExpr : public AstNodeBase
{
public:
    AstAssignExpr(AstNodeBase* dst, AstNodeBase* src)
        : AstNodeBase(AstNodeType::AstAssignExpr, TypeId::Get<void>())
        , m_fiInlineShape(FIShape::INVALID), m_isLhsMem2RegVar(false), m_dst(dst), m_src(src), m_mem2regDst(nullptr)
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

    AstNodeBase* GetSrc() const { return m_src; }
    AstNodeBase* GetDst() const { return m_dst; }

    virtual void SetupDebugInterpImpl() override final
    {
        m_debugInterpFn = SelectImpl(m_src->GetTypeId());
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*&)> fn) override final
    {
        fn(m_src);
        fn(m_dst);
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override final;

    virtual FastInterpSnippet WARN_UNUSED PrepareForFastInterp(FISpillLocation spillLoc) override final;
    virtual void FastInterpSetupSpillLocation() override final;

    void SetLhsMem2Reg(AstRegisterCachedVariableExpr* expr)
    {
        TestAssert(!m_isLhsMem2RegVar && expr->m_variable == m_dst);
        m_isLhsMem2RegVar = true;
        m_mem2regDst = expr;
    }

    bool IsLhsMem2Reg() const { return m_isLhsMem2RegVar; }

private:

    enum FIShape : int16_t
    {
        INVALID,
        INLINE_ARITH,
        INLINE_BOTH,
        INLINE_LHS,
        INLINE_RHS,
        OUTLINE
    };

    FIShape m_fiInlineShape;
    bool m_fiIsLhsSpill;

    // Whether the LHS has been transformed to be a mem2reg variable
    //
    bool m_isLhsMem2RegVar;

    AstNodeBase* m_dst;
    AstNodeBase* m_src;

    AstRegisterCachedVariableExpr* m_mem2regDst;
};

class AstPointerArithmeticExpr : public AstNodeBase
{
public:
    AstPointerArithmeticExpr(AstNodeBase* base, AstNodeBase* index, bool isAddition)
        : AstNodeBase(AstNodeType::AstPointerArithmeticExpr, base->GetTypeId())
        , m_base(base), m_index(index), m_isAddition(isAddition)
    {
        TestAssert(base->GetTypeId().IsPointerType() && !(base->GetTypeId() == TypeId::Get<void*>()));
        TestAssert(index->GetTypeId().IsPrimitiveIntType());
    }

    template<typename BaseType, typename IndexType>
    void InterpImpl([[maybe_unused]] BaseType* out)
    {
        constexpr bool cond = std::is_pointer<BaseType>::value && !std::is_same<BaseType, void*>::value &&
                AstTypeHelper::is_primitive_int_type<IndexType>::value;
        TestAssert(cond);
        if constexpr(cond)
        {
            BaseType base;
            m_base->DebugInterp(&base);
            IndexType index;
            m_index->DebugInterp(&index);
            if (m_isAddition) {
                *out = base + index;
            } else {
                *out = base - index;
            }
        }
    }

    GEN_CLASS_METHOD_SELECTOR(SelectImpl, AstPointerArithmeticExpr, InterpImpl, AstTypeHelper::not_cpp_class_or_void_type)

    virtual void SetupDebugInterpImpl() override final
    {
        m_debugInterpFn = SelectImpl(m_base->GetTypeId(), m_index->GetTypeId());
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*&)> fn) override final
    {
        fn(m_base);
        fn(m_index);
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override final;

    virtual FastInterpSnippet WARN_UNUSED PrepareForFastInterp(FISpillLocation spillLoc) override final;
    virtual void FastInterpSetupSpillLocation() override final;

    AstNodeBase* m_base;
    AstNodeBase* m_index;
    bool m_isAddition;
    bool m_fiIsBaseSpill;
};

// A NULL pointer known at codegen time
//
class AstNullptrExpr : public AstNodeBase
{
public:
    AstNullptrExpr(TypeId typeId)
        : AstNodeBase(AstNodeType::AstNullptrExpr, typeId)
    {
        TestAssert(typeId.IsPointerType());
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override final;

    virtual void SetupDebugInterpImpl() override final
    {
        TestAssert(false && "unimplemented");
    }

    virtual FastInterpSnippet WARN_UNUSED PrepareForFastInterp(FISpillLocation /*spillLoc*/) override final
    {
        ReleaseAssert(false && "unimplemented");
    }

    virtual void FastInterpSetupSpillLocation() override final
    {
        ReleaseAssert(false && "unimplemented");
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*&)> /*fn*/) override final { }
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
        : AstNodeBase(AstNodeType::AstTrashPtrExpr, typeId)
    {
        TestAssert(typeId.IsPointerType());
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override final;

    virtual void SetupDebugInterpImpl() override final
    {
        TestAssert(false && "unimplemented");
    }

    virtual FastInterpSnippet WARN_UNUSED PrepareForFastInterp(FISpillLocation /*spillLoc*/) override final
    {
        ReleaseAssert(false && "unimplemented");
    }

    virtual void FastInterpSetupSpillLocation() override final
    {
        ReleaseAssert(false && "unimplemented");
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*&)> /*fn*/) override final { }
};

// Converts a rvalue to a temporary reference, so it matches the expectation of a C++ function
// Internally it just creates an alloca, put the rvalue there, and returns the address
//
class AstRvalueToConstPrimitiveRefExpr : public AstNodeBase
{
public:
    AstRvalueToConstPrimitiveRefExpr(AstNodeBase* operand)
        : AstNodeBase(AstNodeType::AstRvalueToConstPrimitiveRefExpr, operand->GetTypeId().AddPointer())
        , m_operand(operand)
    {
        TestAssert(operand->GetTypeId().IsPrimitiveType() || operand->GetTypeId().IsPointerType());
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override final;

    template<typename T>
    void InterpImpl(T** out)
    {
        T* addr = reinterpret_cast<T*>(thread_pochiVMContext->m_debugInterpStackFrameBase + m_debugInterpOffset);
        m_operand->DebugInterp(addr);
        *out = addr;
    }

    GEN_CLASS_METHOD_SELECTOR(SelectImpl, AstRvalueToConstPrimitiveRefExpr, InterpImpl, AstTypeHelper::not_cpp_class_or_void_type)

    virtual void SetupDebugInterpImpl() override final
    {
        m_debugInterpFn = SelectImpl(GetTypeId());
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*&)> fn) override final
    {
        fn(m_operand);
    }

    virtual void FastInterpSetupSpillLocation() override final;

    virtual FastInterpSnippet WARN_UNUSED PrepareForFastInterp(FISpillLocation /*spillLoc*/) override final
    {
        // This AstNode is only used to handle C++ const primitive reference parameter.
        // The call expression will directly check m_operand, so execution should never reach here
        //
        ReleaseAssert(false && "unexpected use of AstRvalueToConstPrimitiveRefExpr");
    }

    AstNodeBase* m_operand;
    uint32_t m_debugInterpOffset;
};

}   // namespace PochiVM
