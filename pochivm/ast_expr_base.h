#pragma once

#include "common.h"
#include "ast_enums.h"
#include "ast_type_helper.h"
#include "ast_misc_helper.h"

namespace llvm
{
    class Value;
    class Function;
}   // namespace llvm

namespace PochiVM
{

// The base class of all expressions
// WARNING: All derived classes must have this class as the first base class! Or Interp() breaks!
//
class AstNodeBase : NonCopyable, NonMovable
{
public:
    AstNodeBase(TypeId typeId)
        : m_interpFn(nullptr), m_mark(), m_typeId(typeId)
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

    // For each of the children c of this node, invoke fn(c)
    //
    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) = 0;

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
