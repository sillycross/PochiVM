#pragma once

#include "ast_expr_base.h"
#include "common_expr.h"
#include "pochivm_context.h"

namespace llvm {
class AllocaInst;
}   // namespace llvm

namespace PochiVM
{

class AstFunction;

class DestructorIREmitter
{
public:
    virtual ~DestructorIREmitter() {}
    virtual void EmitDestructorIR() = 0;
};

class AstVariable : public AstNodeBase, public DestructorIREmitter
{
public:
    AstVariable(TypeId typeId, AstFunction* owner, uint32_t varnameSuffix, const char* name = "var")
        : AstNodeBase(typeId)
        , m_varname(name)
        , m_functionOwner(owner)
        , m_llvmValue(nullptr)
        , m_varnameSuffix(varnameSuffix)
        , m_storageSize(static_cast<uint32_t>(typeId.RemovePointer().Size()))
        , m_interpOffset(static_cast<uint32_t>(-1))
    {
        TestAssert(GetTypeId().IsPointerType());
    }

    void SetVarName(const char* newName)
    {
        m_varname = newName;
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    // Emit the LLVM IR code that calls the destructor for this variable
    // May only be called if the variable is a CPP class type
    //
    virtual void EmitDestructorIR() override;

    template<typename T>
    void InterpImpl(T* out)
    {
        *out = reinterpret_cast<T>(thread_pochiVMContext->m_interpStackFrameBase + m_interpOffset);
    }

    GEN_CLASS_METHOD_SELECTOR(SelectImpl, AstVariable, InterpImpl, std::is_pointer)

    virtual void SetupInterpImpl() override
    {
        m_interpFn = SelectImpl(GetTypeId());
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> /*fn*/) override { }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstVariable; }

    const char* GetVarNameNoSuffix() const { return m_varname; }
    uint32_t GetVarSuffix() const { return m_varnameSuffix; }
    AstFunction* GetFunctionOwner() const { return m_functionOwner; }
    uint32_t GetStorageSize() const { return m_storageSize; }

    void SetInterpOffset(uint32_t offset)
    {
        assert(m_interpOffset == static_cast<uint32_t>(-1) && offset != static_cast<uint32_t>(-1));
        m_interpOffset = offset;
    }

    uint32_t GetInterpOffset() const
    {
        assert(m_interpOffset != static_cast<uint32_t>(-1));
        return m_interpOffset;
    }

private:
    // name of the variable
    //
    const char* m_varname;
    // The function in which this variable is declared
    //
    AstFunction* m_functionOwner;
    // The llvm AllocaInst corresponding to this variable,
    // populated by AstDeclareVariable if it is a local var, or by AstFunction if it is a parameter
    //
    llvm::AllocaInst* m_llvmValue;
    // The variable name suffix for printing
    //
    uint32_t m_varnameSuffix;
    // The storage size of this variable
    // This is useful for C++ class variables
    //
    uint32_t m_storageSize;
    // The offset in stackframe that stores the value of this variable, in interp mode
    //
    uint32_t m_interpOffset;
};

}   // namespace PochiVM
