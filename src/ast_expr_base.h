#pragma once

#include "common.h"
#include "ast_type_helper.h"
#include "ast_misc_helper.h"

namespace llvm
{
    class Value;
    class Function;
}   // namespace llvm

namespace Ast
{

enum class AstNodeType
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
    AstBlock,
    AstScope,
    AstIfStatement,
    AstWhileLoop,
    AstForLoop,
    AstBreakOrContinueStmt,
    AstCallExpr,
    AstReturnStmt
};

// The base class of all expressions
// WARNING: All derived classes must have this class as the first base class! Or Interp() breaks!
//
class AstNodeBase : NonCopyable, NonMovable
{
public:
    AstNodeBase(TypeId typeId)
        : m_interpFn(nullptr), m_mark(), m_typeId(typeId)
    { }
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

    // For each of the children c of this node, invoke fn(c)
    //
    virtual void ForEachChildren(const std::function<void(AstNodeBase*)>& fn) = 0;

    // Returns the AstNodeType of this node
    //
    virtual AstNodeType GetAstNodeType() const = 0;

    // Set up m_interpFn
    //
    virtual void SetupInterpImpl() = 0;

    TypeId GetTypeId() const { return m_typeId; }

    AstTraverseColorMark& GetColorMark() { return m_mark; }

    void Interp(void* out /*out*/)
    {
        using InterpFnPrototype = void(*)(void* /*this*/, void* /*out, returnValue*/);
        // This relies on that all derived class inherit this class as the first base class
        // so that static_cast<AstNodeBase*>(derived) == reinterpret_cast<AstNodeBase*>(derived)
        //
        reinterpret_cast<InterpFnPrototype>(m_interpFn)(this, out);
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
    void* m_interpFn;

    // Helper color mark for AST traversal
    //
    AstTraverseColorMark m_mark;

    // The TypeId of the underlying type of this node.
    //
    TypeId m_typeId;
};

// Traverse AST tree, invoking a custom lambda on each node. Example:
//   auto fn = [](AstNodeBase* cur, AstNodeBase* parent, const std::function<void(void)>& Recurse) {
//       DoSomething(cur);
//       Recurse();     // Recurse on all children. If not called, no recursion on subtree of cur happens.
//   }
//   TraverseAstTree(root, fn);
//
using TraverseAstTreeFn = std::function<void(AstNodeBase* /*cur*/,
                                             AstNodeBase* /*parent*/,
                                             const std::function<void(void)>& /*Recurse*/)>;

inline void TraverseAstTree(AstNodeBase* root, const TraverseAstTreeFn& fn)
{
    std::function<void(AstNodeBase*)> RecurseFactory = [&fn, &RecurseFactory](AstNodeBase* p) {
        p->ForEachChildren([p, &fn, &RecurseFactory](AstNodeBase* c) {
            fn(c, p, std::bind(RecurseFactory, c));
        });
    };
    fn(root, nullptr, std::bind(RecurseFactory, root));
}

#if 0
// Base class of all C++-type expressions with known type of T
// All operations with return type of T will inherit this class
//
// For primitive types and pointer types, isLValue may be true or false
// For C++ classes, isLValue is always true: since we only allow function prototype
// with primitive parameters and return values, RValue classes will never show up.
//
// LValue can always implicitly cast to RValue, except C++ classes which we should
// never use its RValue.
//
template<typename T, AstValueType valueType>
class AstExpr : AstExprBase<valueType>
{
    // Only whitelisted types (defined by specializations blow) are allowed
    //
    static_assert(sizeof(T) == 0, "Bad type T. Add to for_each_xop_type.h?");
};

// Whitelist 'void' type, RValue only
//
template<> class AstExpr<void, RValue> : public AstExprBase<RValue>
{
public:
    virtual TypeId GetTypeId() const override
    {
        return AstTypeHelper::GetTypeId<void>::value;
    }
    // Specialized version of EmitIR(), for void type only
    //
    virtual llvm::Value* WARN_UNUSED EmitIR() override;
};

// Whitelist all primitive types
//
#define F(type) \
template<AstValueType valueType>                                                     \
class AstExpr<type, valueType> : public AstExprBase<valueType> {                     \
public:                                                                              \
    virtual TypeId GetTypeId() const override {                                      \
        return AstTypeHelper::GetTypeId<type>::value;                                \
    }                                                                                \
};
FOR_EACH_PRIMITIVE_TYPE
#undef F

// Whitelist all C++ class types, but LValues only
//
#define CLASS(type) \
template<> class AstExpr<type, LValue> : public AstExprBase<LValue> {                \
public:                                                                              \
    virtual TypeId GetTypeId() const override {                                      \
        return AstTypeHelper::GetTypeId<type>::value;                                \
    }                                                                                \
};
#define MEMBER(v, type)
#define METHOD(v, ret, param, attr)
#define ENDCLASS
#include "for_each_xop_type.h"
#undef CLASS
#undef MEMBER
#undef METHOD
#undef ENDCLASS

// Whitelist all pointer types
//
template<typename T, AstValueType valueType>
class AstExpr<T*, valueType> : public AstExprBase<valueType>
{
public:
    virtual TypeId GetTypeId() const override
    {
        return AstTypeHelper::GetTypeId<T*>::value;
    }
};

#endif

}   // namespace Ast
