#pragma once

#include "common.h"
#include "interp_control_signal.h"
#include "ast_type_helper.h"
#include "ast_misc_helper.h"
#include "fastinterp/fastinterp_spill_location.h"
#include "fastinterp/fastinterp_snippet.h"

namespace llvm
{
    class Value;
    class Function;
}   // namespace llvm

namespace PochiVM
{

class AstNodeType
{
public:
    enum _EnumType
    {
        AstArithmeticExpr,
        AstComparisonExpr,
        AstStaticCastExpr,
        AstReinterpretCastExpr,
        AstDereferenceExpr,
        AstLiteralExpr,
        AstAssignExpr,
        AstNullptrExpr,
        AstTrashPtrExpr,
        AstVariable,
        AstDeclareVariable,
        AstDereferenceVariableExpr,
        AstBlock,
        AstScope,
        AstIfStatement,
        AstWhileLoop,
        AstForLoop,
        AstBreakOrContinueStmt,
        AstCallExpr,
        AstReturnStmt,
        AstLogicalAndOrExpr,
        AstLogicalNotExpr,
        AstThrowStmt,
        AstRvalueToConstPrimitiveRefExpr,
        AstExceptionAddressPlaceholder,
        AstPointerArithmeticExpr,
        AstGeneratedFunctionPointerExpr
    };

    AstNodeType() {}
    AstNodeType(AstNodeType::_EnumType e) : m_value(e) {}
    explicit operator int() const { return m_value; }
    bool operator==(const AstNodeType::_EnumType& other) const { return m_value == other; }
    bool operator!=(const AstNodeType::_EnumType& other) const { return !(*this == other); }
    bool operator==(const AstNodeType& other) const { return m_value == other.m_value; }
    bool operator!=(const AstNodeType& other) const { return !(*this == other); }

    const char* ToString() const
    {
        switch(m_value)
        {
        case AstNodeType::AstArithmeticExpr: return "AstArithmeticExpr";
        case AstNodeType::AstComparisonExpr: return "AstComparisonExpr";
        case AstNodeType::AstStaticCastExpr: return "AstStaticCastExpr";
        case AstNodeType::AstReinterpretCastExpr: return "AstReinterpretCastExpr";
        case AstNodeType::AstDereferenceExpr: return "AstDereferenceExpr";
        case AstNodeType::AstLiteralExpr: return "AstLiteralExpr";
        case AstNodeType::AstAssignExpr: return "AstAssignExpr";
        case AstNodeType::AstNullptrExpr: return "AstNullptrExpr";
        case AstNodeType::AstTrashPtrExpr: return "AstTrashPtrExpr";
        case AstNodeType::AstVariable: return "AstVariable";
        case AstNodeType::AstDeclareVariable: return "AstDeclareVariable";
        case AstNodeType::AstDereferenceVariableExpr: return "AstDereferenceVariableExpr";
        case AstNodeType::AstBlock: return "AstBlock";
        case AstNodeType::AstScope: return "AstScope";
        case AstNodeType::AstIfStatement: return "AstIfStatement";
        case AstNodeType::AstWhileLoop: return "AstWhileLoop";
        case AstNodeType::AstForLoop: return "AstForLoop";
        case AstNodeType::AstBreakOrContinueStmt: return "AstBreakOrContinueStmt";
        case AstNodeType::AstCallExpr: return "AstCallExpr";
        case AstNodeType::AstReturnStmt: return "AstReturnStmt";
        case AstNodeType::AstLogicalAndOrExpr: return "AstLogicalAndOrExpr";
        case AstNodeType::AstLogicalNotExpr: return "AstLogicalNotExpr";
        case AstNodeType::AstThrowStmt: return "AstThrowStmt";
        case AstNodeType::AstRvalueToConstPrimitiveRefExpr: return "AstRvalueToConstPrimitiveRefExpr";
        case AstNodeType::AstExceptionAddressPlaceholder: return "AstExceptionAddressPlaceholder";
        case AstNodeType::AstPointerArithmeticExpr: return "AstPointerArithmeticExpr";
        case AstNodeType::AstGeneratedFunctionPointerExpr: return "AstGeneratedFunctionPointerExpr";
        }
        __builtin_unreachable();
    }

private:
    _EnumType m_value;
};

// The base class of all expressions
// WARNING: All derived classes must have this class as the first base class! Or DebugInterp() breaks!
//
class AstNodeBase : NonCopyable, NonMovable
{
public:
    AstNodeBase(AstNodeType nodeType, TypeId typeId)
        : m_debugInterpFn(nullptr), m_mark(), m_nodeType(nodeType), m_typeId(typeId)
    {
        TestAssert(!typeId.IsInvalid());
    }
    virtual ~AstNodeBase() {}

    // TODO: outdated
    // EmitIR() is the wrapper which handles the case that the node (which must be variable
    // node, const variable node or literal node) gets used multiple times in AST.
    // An m_llvmValue corresponds to a temporary in LLVM IR
    // If it has been generated, we should not generate it again. This is a correctness issue, e.g.:
    //   When the user writes: auto x = FooExpr(); auto y = x * x;
    //     the correct generated IR is %1 = call Foo(), %ret = %1 * %1
    //   But if the user writes auto y = FooExpr() * FooExpr();
    //     the correct generated IR is %1 = call Foo(), %2 = call Foo(), %ret = %1 * %2
    //
    // So when we generate IR for an AST node, we will populate m_llvmValue
    // And when we see m_llvmValue has been populated, we should simply return its value,
    // not generating the IR again.
    //
    // Note that in order to have easily reasonable correctness of control flow, we require users to declare an
    // const var by DeclareConst(...) for reusing RValue expressions, which must also be the first
    // occurrance of the AST node and must only be used inside the scope the const var is alive.
    // Those requirements are enforced by a pass of semantics check before emitting IR.
    //
    // The only exception is GetTypeId().IsVoid() case. A 'void' return value corresponds to
    // no return value in LLVM IR. In that case attempting to 'refer' to the value is a bug:
    // it indicates that the user built the AST tree illegally and we fire an assert.
    //
    // This function also asserts that the generated llvm::Value has the expected type.
    //
    llvm::Value* WARN_UNUSED EmitIR();

    // EmitIRImpl() is the derived-class-supplemented function which contains the real logic to generate IR.
    // Since EmitIR() has done the type asserts, the implementation do not need to do it again.
    //
    virtual llvm::Value* WARN_UNUSED EmitIRImpl() = 0;

    virtual FastInterpSnippet WARN_UNUSED PrepareForFastInterp(FISpillLocation spillLoc) = 0;

    virtual void FastInterpSetupSpillLocation() = 0;

    // For each of the children c of this node, invoke fn(c)
    //
    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) = 0;

    // Returns the AstNodeType of this node
    //
    AstNodeType GetAstNodeType() const
    {
        return m_nodeType;
    }

    // Set up m_debugInterpFn
    //
    virtual void SetupDebugInterpImpl() = 0;

    TypeId GetTypeId() const { return m_typeId; }

    AstTraverseColorMark& GetColorMark() { return m_mark; }

    void DebugInterp(void* out /*out*/)
    {
        using InterpFnPrototype = void(*)(void* /*this*/, void* /*out, returnValue*/);
        // This relies on that all derived class inherit this class as the first base class
        // so that static_cast<AstNodeBase*>(derived) == reinterpret_cast<AstNodeBase*>(derived)
        //
        reinterpret_cast<InterpFnPrototype>(m_debugInterpFn)(this, out);
    }

protected:
    // Function pointer for interp execution of this node
    //
    // For statements with a return value, it always point to some class member function with
    // prototype void(T* /*out*/) where T is the return type of the statement.
    //
    // For language constructs (e.g. block/scope/for/while), the prototype is void(InterpControlSignal* /*out*/)
    // where the output is set correspondingly if a control flow redirection statement (Break/Continue/Return)
    // is executed within.
    //
    // For statements without a return value and is impossible to execute control flow redirection statements
    // (e.g. Assign) the prototype is void(void*) with the parameter being ignored.
    //
    // Since they only differ in pointer types, all interp implementations can be called using InterpFnPrototype.
    //
    void* m_debugInterpFn;

    // Helper color mark for AST traversal
    //
    AstTraverseColorMark m_mark;

    // The type of this node
    //
    AstNodeType m_nodeType;

    // The TypeId of the underlying type of this node.
    //
    TypeId m_typeId;
};

// Traverse AST tree, invoking a custom lambda on each node. Example:
//   auto fn = [](AstNodeBase* cur, AstNodeBase* parent, FunctionRef<void(void)> Recurse) {
//       DoSomething(cur);
//       Recurse();     // Recurse on all children. If not called, no recursion on subtree of cur happens.
//   }
//   TraverseAstTree(root, fn);
//
inline void TraverseAstTree(AstNodeBase* root,
                            FunctionRef<void(AstNodeBase* /*cur*/,
                                             AstNodeBase* /*parent*/,
                                             FunctionRef<void(void)> /*Recurse*/)> fn)
{
    // Syntax is a bit stupid to create a FunctionRef that captures itself..
    //
    FunctionRef<void(AstNodeBase*)> RecurseFactory;
    auto _lambda = [&fn, &RecurseFactory](AstNodeBase* p) {
        p->ForEachChildren([p, &fn, &RecurseFactory](AstNodeBase* c) {
            fn(c, p, [&RecurseFactory, c]() {
                RecurseFactory(c);
            });
        });
    };
    RecurseFactory = _lambda;

    fn(root, nullptr, [&RecurseFactory, root]() {
        RecurseFactory(root);
    });
}

}   // namespace PochiVM
