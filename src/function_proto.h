#pragma once

#include "ast_expr_base.h"
#include "lang_constructs.h"
#include "error_context.h"

namespace llvm
{

class BasicBlock;
class AllocaInst;
class Module;

}   // namespace llvm

namespace Ast
{

namespace internal
{

template<typename T>
void AstFunctionInterpStoreParamsHelper(T* dst, AstNodeBase* src)
{
    T value;
    src->Interp(&value);
    *dst = value;
}

GEN_FUNCTION_SELECTOR(AstFunctionInterpStoreParamsSelector,
                      AstFunctionInterpStoreParamsHelper,
                      AstTypeHelper::primitive_or_pointer_type)

}   // namespace internal

// A generated function
//
class AstFunction : NonCopyable, NonMovable
{
private:
    // Only AstModule may use constructor
    //
    AstFunction(const std::string& name)
        : m_params()
        , m_returnType()
        , m_name(name)
        , m_body(new AstScope())
        , m_generatedPrototype(nullptr)
        , m_varSuffixOrdinal(0)
        , m_ifStmtSuffixOrdinal(0)
        , m_whileLoopSuffixOrdinal(0)
        , m_forLoopSuffixOrdinal(0)
        , m_interpStackFrameSize(static_cast<uint32_t>(-1))
        , m_interpStoreParamFns()
        , m_interpStoreRetValFn(nullptr)
        , m_llvmEntryBlock(nullptr)
    { }

public:
    friend class AstModule;

    // Below are methods to codegen this function itself
    //
    // Emit the function definition into m_generatedPrototype field
    // We need to emit all the function definitions before generating any function body,
    // otherwise cross invocation (A calling B and B calling A) would inevitably fail.
    //
    void EmitDefinition();

    // Emit the function body
    //
    void EmitIR();

    // Traverse the function body
    // The parameter list is not traversed
    //
    void TraverseFunctionBody(const TraverseAstTreeFn& fn)
    {
        TraverseAstTree(m_body, fn);
    }

    // Below are methods useful to codegen calls to this function
    //
    // Returns if all the param types match, sets error if not
    //
    bool WARN_UNUSED CheckParamTypes(const std::vector<AstNodeBase*>& params) const
    {
        CHECK_REPORT_ERR(params.size() == m_params.size(),
                         "Call to function %s: wrong number of parameters, expects %d got %d",
                         m_name.c_str(), int(m_params.size()), int(params.size()));
        for (size_t i = 0; i < m_params.size(); i++)
        {
            CHECK_REPORT_ERR(params[i]->GetTypeId() == GetParamType(i),
                             "Call to function %s: parameter %d has wrong type, expects %s got %s",
                             m_name.c_str(), int(i),
                             GetParamType(i).Print().c_str(),
                             params[i]->GetTypeId().Print().c_str());
        }
        return true;
    }

    size_t GetNumParams() const { return m_params.size(); }

    const std::string& GetName() const { return m_name; }

    TypeId GetReturnType() const { return m_returnType; }

    TypeId GetParamType(size_t i) const
    {
        assert(i < m_params.size());
        return m_params[i]->GetTypeId().RemovePointer();
    }

    llvm::Function* GetGeneratedPrototype() const
    {
        assert(m_generatedPrototype != nullptr);
        return m_generatedPrototype;
    }

    llvm::BasicBlock* GetEntryBlock() const
    {
        assert(m_llvmEntryBlock != nullptr);
        return m_llvmEntryBlock;
    }

    // Validate the semantics of the function
    // Set error message on failure
    //
    bool WARN_UNUSED Validate();

    // Below are methods used by AstFunctionBuilder to build the function
    //
    const std::vector<AstVariable*>& GetParamsVector() const
    {
        return m_params;
    }

    uint32_t GetNextVarSuffix() { return m_varSuffixOrdinal++; }
    uint32_t GetNextIfStmtSuffix() { return m_ifStmtSuffixOrdinal++; }
    uint32_t GetNextWhileLoopSuffix() { return m_whileLoopSuffixOrdinal++; }
    uint32_t GetNextForLoopSuffix() { return m_forLoopSuffixOrdinal++; }

    void AddParam(TypeId type, const char* name)
    {
        m_params.push_back(new AstVariable(
                               type.AddPointer(), this /*owner*/, GetNextVarSuffix(), name));
    }

    void SetParamName(size_t i, const char* name)
    {
        assert(i < m_params.size());
        m_params[i]->SetVarName(name);
    }

    AstScope* GetFunctionBody() const
    {
        return m_body;
    }

    void SetFunctionBody(AstBlock* body)
    {
        assert(m_body->IsEmpty());
        m_body->Append(body);
    }

    void SetReturnType(TypeId returnType)
    {
        assert(m_returnType.IsInvalid() && !returnType.IsInvalid());
        m_returnType = returnType;
    }

    // Below are methods for interp mode
    //
    // Interp execute the function
    //
    void Interp(AstNodeBase** params, void* returnValueOut)
    {
        // Allocate space for the new stack frame
        //
        assert(m_interpStackFrameSize != static_cast<uint32_t>(-1) && m_interpStackFrameSize > 0);
        void* stackFrameBase = alloca(m_interpStackFrameSize);

        // Store parameters into stack frame
        // The parameter expressions must be evaluated in the OLD stack frame,
        // so it is important we not switch to the new stack frame now.
        // However, their results should be stored into corrresponding addresses in new stack frame.
        //
        assert(m_interpStoreParamFns.size() == m_params.size());
        for (size_t i = 0; i < GetNumParams(); i++)
        {
            InterpSetParam(reinterpret_cast<uintptr_t>(stackFrameBase), i, params[i]);
        }

        // Switch to the new stack frame, saving the old one
        //
        uintptr_t oldStackFrameBase = thread_pochiVMContext->m_interpStackFrameBase;
        thread_pochiVMContext->m_interpStackFrameBase = reinterpret_cast<uintptr_t>(stackFrameBase);

        // Execute function
        //
        InterpControlSignal ics = InterpControlSignal::None;
        m_body->Interp(&ics);

        // The control signal must not be continue/break: they should never reach here
        //
        TestAssert(ics != InterpControlSignal::Continue && ics != InterpControlSignal::Break);

        // If the function has a return value, control signal must be 'return',
        // or it indicates no return value has been set
        //
        TestAssertImp(!m_returnType.IsVoid(), ics == InterpControlSignal::Return);

        // Retrieve return value if exists
        // The return value is always stored at stackFrameBase
        //
        m_interpStoreRetValFn(returnValueOut /*dst*/, stackFrameBase /*src*/);

        // Restore stack frame
        //
        thread_pochiVMContext->m_interpStackFrameBase = oldStackFrameBase;
    }

    // Set up various information needed for interp execution
    //
    void PrepareForInterp();

private:

    void InterpSetParam(uintptr_t newStackFrameBase, size_t i, AstNodeBase* param)
    {
        assert(i < GetNumParams() && param->GetTypeId() == GetParamType(i));
        uintptr_t addr = newStackFrameBase + m_params[i]->GetInterpOffset();
        m_interpStoreParamFns[i](reinterpret_cast<void*>(addr), param);
    }

    // Function parameters in LLVM are RValues. However, we make them LValues here,
    // same as normal variables. In LLVM, we emit a header block to assign RValue parameters
    // to these LValues (same as what clang++ does). In interp mode, these are already stored
    // on the stack with a valid address, so we don't have do to anything.
    //
    std::vector<AstVariable*> m_params;
    TypeId m_returnType;
    std::string m_name;
    AstScope* m_body;
    llvm::Function* m_generatedPrototype;
    // ordinal suffix for various constructs in this function
    //
    uint32_t m_varSuffixOrdinal;
    uint32_t m_ifStmtSuffixOrdinal;
    uint32_t m_whileLoopSuffixOrdinal;
    uint32_t m_forLoopSuffixOrdinal;

    // Interp data
    //
    // Stack frame size needed to store the temp data
    //
    uint32_t m_interpStackFrameSize;
    // function pointer for storing each parameter on invocation
    //
    using _StoreParamsFn = void(*)(void*, AstNodeBase*);
    std::vector<_StoreParamsFn> m_interpStoreParamFns;
    // function pointer for retrieving return value and store to variable
    //
    using _StoreRetValFn = void(*)(void*, void*);
    _StoreRetValFn m_interpStoreRetValFn;

    // llvm data
    //
    llvm::BasicBlock* m_llvmEntryBlock;
};

// A module, consists of a list of functions
// TODO: support generated types
//
class AstModule : NonCopyable, NonMovable
{
public:
    AstModule(const std::string& name)
        : m_moduleName(name)
        , m_functions()
        , m_llvmModule(nullptr)
#ifdef TESTBUILD
        , m_validated(false)
        , m_interpPrepared(false)
        , m_irEmitted(false)
        , m_irOptimized(false)
#endif
    { }

    AstFunction* NewAstFunction(const std::string& name)
    {
        // TODO: this should throw
        TestAssert(!m_functions.count(name));
        AstFunction* ret = new AstFunction(name);
        m_functions[name] = ret;
        return ret;
    }

    AstFunction* GetAstFunction(const std::string& name)
    {
        auto it = m_functions.find(name);
        if (it == m_functions.end())
        {
            return nullptr;
        }
        else
        {
            return it->second;
        }
    }

    void PrepareForInterp()
    {
        TestAssert(!m_interpPrepared);
#ifdef TESTBUILD
        m_interpPrepared = true;
#endif
        AstTraverseColorMark::ClearAll();
        for (auto iter = m_functions.begin(); iter != m_functions.end(); iter++)
        {
            AstFunction* fn = iter->second;
            fn->PrepareForInterp();
        }
    }

    void EmitIR();
    void OptimizeIR();

    bool WARN_UNUSED Validate()
    {
        TestAssert(!m_validated);
#ifdef TESTBUILD
        m_validated = true;
#endif
        assert(!thread_errorContext->HasError());
        AstTraverseColorMark::ClearAll();
        for (auto iter = m_functions.begin(); iter != m_functions.end(); iter++)
        {
            AstFunction* fn = iter->second;
            CHECK_ERR(fn->Validate());
        }
        RETURN_TRUE;
    }

    llvm::Module* GetBuiltLLVMModule() const
    {
        assert(m_llvmModule);
        return m_llvmModule;
    }

private:
    template<typename T> struct InterpCallFunction;

public:
    // Get a std::function object that invokes a generated function in interp mode
    //
    // T must be a std::function that matches the prototype of the generated function
    // Return the std::function object that calls the generated function in interp mode,
    //    or nullptr if no function with given name exist.
    //
    // Example:
    //    using FnType = std::function<void(int, double)>;
    //    FnType fn = GetGeneratedFunctionInterpMode<FnType>("generated_fn_name");
    //    fn(12, 3.45);
    //
    template<typename T>
    T GetGeneratedFunctionInterpMode(const std::string& name)
    {
        assert(m_interpPrepared);
        AstFunction* fn = GetAstFunction(name);
        if (fn == nullptr)
        {
            return nullptr;
        }
        InterpCallFunction<T>::assert_prototype_ok(fn);
        return InterpCallFunction<T>::get(fn);
    }

private:

    template<typename R, typename... Args>
    struct InterpCallFunctionImplImpl
    {
        static const size_t numArgs = sizeof...(Args);

        static void construct_params(AstLiteralExpr* /*memoryOwner*/)
        { }

        template<typename F, typename... T>
        static void construct_params(AstLiteralExpr* memoryOwner,
                                     F first, T... args)
        {
            // Construct AstLiteralExpr by in-place new
            //
            new (memoryOwner) AstLiteralExpr(TypeId::Get<F>(), &first);
            memoryOwner->SetupInterpImpl();
            construct_params(memoryOwner + 1, args...);
        }

        static void call_impl(AstFunction* fn, R* ret /*out*/, Args... args)
        {
            // It is important that we not use any CodeGenAllocator
            //
            // Avoid alloca(0)
            //
            size_t arrSize = numArgs;
            if (arrSize < 1) { arrSize = 1; }
            // Construct the parameter vector
            // Intentionally uninitialized, we will use in-place new to individually initialize in construct_params
            //
            AstLiteralExpr* paramsMem = reinterpret_cast<AstLiteralExpr*>(alloca(sizeof(AstLiteralExpr) * arrSize));
            construct_params(paramsMem, args...);

            AstNodeBase** params = reinterpret_cast<AstNodeBase**>(alloca(sizeof(AstNodeBase*) * arrSize));
            for (size_t i = 0; i < numArgs; i++) { params[i] = paramsMem + i; }

            // Interp the function, fill in return value at 'ret'
            //
            fn->Interp(params, ret);
        }
    };

    // avoid compile error due to declaring a void type variable
    //
    template<typename R, typename... Args>
    struct InterpCallFunctionImpl
    {
        static R call(AstFunction* fn, Args... args)
        {
            R ret;
            InterpCallFunctionImplImpl<R, Args...>::call_impl(fn, &ret, args...);
            return ret;
        }
    };

    template<typename... Args>
    struct InterpCallFunctionImpl<void, Args...>
    {
        static void call(AstFunction* fn, Args... args)
        {
            InterpCallFunctionImplImpl<void, Args...>::call_impl(fn, nullptr, args...);
        }
    };

    template<typename T>
    struct InterpCallFunction
    {
        static_assert(sizeof(T) == 0, "T must be a std::function type!");
    };

    template<typename R, typename... Args>
    struct InterpCallFunction<std::function<R(Args...)>>
    {
        using FnInfo = AstTypeHelper::function_type_helper<std::function<R(Args...)>>;

        static void assert_prototype_ok(AstFunction* fn)
        {
            TestAssert(FnInfo::numArgs == fn->GetNumParams());
            TestAssert(TypeId::Get<R>() == fn->GetReturnType());
            for (size_t i = 0; i < FnInfo::numArgs; i++)
            {
                TestAssert(fn->GetParamType(i) == FnInfo::argTypeId[i]);
            }
            std::ignore = fn;
        }

        static std::function<R(Args...)> get(AstFunction* fn)
        {
            return [fn](Args... args) -> R {
                return InterpCallFunctionImpl<R, Args...>::call(fn, args...);
            };
        }
    };

    std::string m_moduleName;
    // We are using std::map (not std::unordered_map) so that the iteration order is deterministic.
    // This is required to make tests happy (so IR dump output is deterministic).
    // The perf hit should be small. TODO: consider use std::unordered_map in release?
    //
    std::map<std::string, AstFunction*> m_functions;
    llvm::Module* m_llvmModule;
#ifdef TESTBUILD
    bool m_validated;
    bool m_interpPrepared;
    bool m_irEmitted;
    bool m_irOptimized;
#endif
};

// Call a generated function
//
class AstCallExpr : public AstNodeBase
{
public:
    AstCallExpr(const std::string& name,
                const std::vector<AstNodeBase*>& params,
                TypeId returnType)
        : AstNodeBase(returnType), m_fnName(name), m_params(params)
    { }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    void InterpImpl(void* out)
    {
        m_interpFunction->Interp(m_params.data(), out);
    }

    // Validate that all caller types matches corresponding callee types
    //
    bool WARN_UNUSED ValidateSignature()
    {
        AstFunction* fn = thread_pochiVMContext->m_curModule->GetAstFunction(m_fnName);
        CHECK_REPORT_ERR(fn != nullptr, "Call to undefined function %s", m_fnName.c_str());
        CHECK_ERR(fn->CheckParamTypes(m_params));
        CHECK_REPORT_ERR(fn->GetReturnType() == GetTypeId(),
                         "Call to function %s: wrong return type, expects %s got %s",
                         m_fnName.c_str(), fn->GetReturnType().Print().c_str(), GetTypeId().Print().c_str());
        RETURN_TRUE;
    }

    virtual void SetupInterpImpl() override
    {
        m_interpFunction = thread_pochiVMContext->m_curModule->GetAstFunction(m_fnName);
        TestAssert(m_interpFunction != nullptr);
        m_interpFn = AstTypeHelper::GetClassMethodPtr(&AstCallExpr::InterpImpl);
    }

    virtual void ForEachChildren(const std::function<void(AstNodeBase*)>& fn) override
    {
        for (auto it = m_params.begin(); it != m_params.end(); it++)
        {
            fn(*it);
        }
    }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstCallExpr; }

private:
    std::string m_fnName;
    std::vector<AstNodeBase*> m_params;
    // Function to call, only populated in interp mode
    //
    AstFunction* m_interpFunction;
};

class AstReturnStmt : public AstNodeBase
{
public:
    AstReturnStmt(AstNodeBase* retVal)
        : AstNodeBase(TypeId::Get<void>())
        , m_retVal(retVal)
    {
        TestAssertImp(m_retVal != nullptr,
                      m_retVal->GetTypeId().IsPrimitiveType() || m_retVal->GetTypeId().IsPointerType());
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override;

    template<typename T>
    void InterpImpl(InterpControlSignal* out)
    {
        m_retVal->Interp(reinterpret_cast<void*>(thread_pochiVMContext->m_interpStackFrameBase));
        assert(*out == InterpControlSignal::None);
        *out = InterpControlSignal::Return;
    }

    GEN_CLASS_METHOD_SELECTOR(SelectImpl, AstReturnStmt, InterpImpl, AstTypeHelper::primitive_or_pointer_type)

    void InterpImplVoid(InterpControlSignal* out)
    {
        assert(*out == InterpControlSignal::None);
        *out = InterpControlSignal::Return;
    }

    virtual void SetupInterpImpl() override
    {
        if (m_retVal == nullptr)
        {
            m_interpFn = AstTypeHelper::GetClassMethodPtr(&AstReturnStmt::InterpImplVoid);
        }
        else
        {
            m_interpFn = SelectImpl(m_retVal->GetTypeId());
        }
    }

    virtual void ForEachChildren(const std::function<void(AstNodeBase*)>& fn) override
    {
        if (m_retVal != nullptr)
        {
            fn(m_retVal);
        }
    }

    virtual AstNodeType GetAstNodeType() const override { return AstNodeType::AstReturnStmt; }

    AstNodeBase* m_retVal;
};

inline void AstFunction::PrepareForInterp()
{
    assert(m_interpStackFrameSize == static_cast<uint32_t>(-1));
    // Setup m_interpStoreParamFns and m_interpStoreRetValFn
    //
    for (size_t i = 0; i < GetNumParams(); i++)
    {
        _StoreParamsFn storeParamsFn = reinterpret_cast<_StoreParamsFn>(
                    internal::AstFunctionInterpStoreParamsSelector(GetParamType(i)));
        m_interpStoreParamFns.push_back(storeParamsFn);
    }
    m_interpStoreRetValFn = reinterpret_cast<_StoreRetValFn>(
                AstTypeHelper::void_safe_store_value_selector(GetReturnType()));

    // Compute stack frame size, and offsets for various variables
    //
    uint32_t size = 0;
    // Allocate space for return value
    //
    if (!m_returnType.IsVoid())
    {
        size += m_returnType.Size();
    }
    auto up_align = [](uint32_t n, uint32_t align)
    {
        if (n % align == 0) return n;
        return n / align * align + align;
    };
    auto interpSetupFn = [&](AstNodeBase* cur)
    {
        assert(cur->GetColorMark().IsNoColor());
        cur->GetColorMark().MarkColorA();
        cur->SetupInterpImpl();
        AstNodeType nodeType = cur->GetAstNodeType();
        if (nodeType == AstNodeType::AstVariable)
        {
            AstVariable* v = assert_cast<AstVariable*>(cur);
            // pad to natural alignment min(8, storageSize)
            //
            size = up_align(size, std::min(8U, v->GetStorageSize()));
            v->SetInterpOffset(size);
            size += v->GetStorageSize();
        }
    };
    // Allocate space for parameters
    // We need to do these specially, becuase AST traverse would only allocate space
    // for them if they were used. But space for parameters need to be allocated
    // even if unused because caller always write into there.
    // TODO: sort by size to save space
    //
    for (size_t i = 0; i < m_params.size(); i++)
    {
        interpSetupFn(m_params[i]);
    }
    // Allocate space for variables, and do per-node interp preparation
    //
    TraverseAstTreeFn traverseFn = [&](AstNodeBase* cur,
                                       AstNodeBase* /*parent*/,
                                       const std::function<void(void)>& Recurse)
    {
        if (cur->GetColorMark().IsColorA())
        {
            return;
        }
        interpSetupFn(cur);
        Recurse();
    };
    TraverseFunctionBody(traverseFn);

    // avoid alloca(0)
    //
    size = std::max(8U, size);
    size = up_align(size, 8);
    m_interpStackFrameSize = size;
}

inline bool WARN_UNUSED AstFunction::Validate()
{
    TestAssert(!thread_errorContext->HasError());
    // ColorA: on the dfs path. ColorB: visited, not on dfs path.
    //
    // Variable scope checking: checks that each variable is declared
    // before use, is declared only once, and is used only when it is alive.
    //
    // For declaration check, we use node color mark. For aliveness check,
    // we associate each variable with the scope it belongs to.
    //

    // The scope in which each var is declared. The AstNodeBase* may be AstScope* or
    // AstForLoop* (for-loop is possible because its header block may declare variable
    // and those go out of scope as for-loop do) or nullptr (for function params)
    //
    std::unordered_map<AstVariable*, AstNodeBase*> varScope;

    // The stack of scopes in current dfs path.
    // Each scope may be AstScope* or AstForLoop*, same as above.
    //
    std::vector<AstNodeBase*> scopeStack;

    // The stack of loops in current dfs path.
    //
    std::vector<AstNodeBase*> loopStack;

    // Whether we are inside for-loop init-block or step-block
    //
    bool isInsideForLoopInitOrStepBlock = false;

    // First of all, all parameters are valid to use at all time.
    //
    for (size_t i = 0; i < m_params.size(); i++)
    {
        CHECK_REPORT_ERR(m_params[i]->GetColorMark().IsNoColor(),
                         "Function %s: Internal bug?", m_name.c_str());
        m_params[i]->GetColorMark().MarkColorB();
        varScope[m_params[i]] = nullptr;
    }

    // Reachablity analysis
    //
    // When encountering a break/continue/return statement, everything below is unreachable and
    //   nothing can reset it except end of control-block (end of then-clause/for-loop etc).
    //
    // When encountering a if statement, the reachability of code after the if-statement
    //   is determined by the smaller reachability value of the then-clause and else-clause.
    //
    // If any statement is hit with reachability != _REACHABLE, we report an error of unreachable code.
    //
    enum _Reachability
    {
        _REACHABLE = 0,
        // break has higher value than continue:
        // break makes for-loop step-clause unreachable while continue does not.
        //
        _CONTINUE = 1,
        _BREAK = 2,
        // Currently _RETURN and _BREAK has no difference in their power.
        // But if we support do {..} while(cond) syntax in future, there would be a difference.
        // (end of do-while loop resets _BREAK but not _RETURN since loop body is always executed.
        //
        _RETURN = 3
    };

    bool success = true;
    _Reachability reachability = _REACHABLE;
    TraverseAstTreeFn traverseFn = [&](AstNodeBase* cur,
                                       AstNodeBase* parent,
                                       const std::function<void(void)>& Recurse)
    {
        TestAssertIff(thread_errorContext->HasError(), !success);
        if (!success) { return; }
        AstNodeType nodeType = cur->GetAstNodeType();
        if (!cur->GetColorMark().IsNoColor())
        {
            if (cur->GetColorMark().IsColorA())
            {
                REPORT_ERR("Function %s: cycle in AST tree detected", m_name.c_str());
                success = false;
                return;
            }
            // We only allow reusing a very limited number of AST nodes.
            // All other nodes should only have one parent in AST tree.
            //
            if (nodeType != AstNodeType::AstVariable &&
                nodeType != AstNodeType::AstLiteralExpr &&
                nodeType != AstNodeType::AstNullptrExpr &&
                nodeType != AstNodeType::AstTrashPtrExpr &&
                nodeType != AstNodeType::AstDereferenceVariableExpr)
            {
                REPORT_ERR("Function %s: illegal reuse of AST node (AstNodeType = %s)",
                           m_name.c_str(), nodeType.ToString());
                success = false;
                return;
            }
            if (nodeType == AstNodeType::AstDereferenceVariableExpr)
            {
                // For DereferenceVariableExpr, even if we allow them to be reused,
                // we still need to validate that the variable it is dereferencing
                // has not gone not out of scope, at each time this node is reused.
                //
                Recurse();
            }
            if (nodeType == AstNodeType::AstVariable)
            {
                AstVariable* v = assert_cast<AstVariable*>(cur);
                if (v->GetFunctionOwner() != this)
                {
                    REPORT_ERR("Function %s: use of local variable %s_%u belonging to another function %s",
                               m_name.c_str(), v->GetVarNameNoSuffix(), v->GetVarSuffix(), v->GetFunctionOwner()->m_name.c_str());
                    success = false;
                    return;
                }
                if (!varScope.count(v))
                {
                    // This should not happen. If it is visited, it should be in varScope
                    //
                    TestAssert(false);
                    REPORT_ERR("Function %s: internal bug? variable %s_%u",
                               m_name.c_str(), v->GetVarNameNoSuffix(), v->GetVarSuffix());
                    success = false;
                    return;
                }
                AstNodeBase* scope = varScope[v];
                // if scope == nullptr, those are function parameters, no aliveness checks needed
                //
                if (scope != nullptr)
                {
                    if (scope->GetColorMark().IsColorB())
                    {
                        REPORT_ERR("Function %s: use of out-of-scope variable %s_%u",
                                   m_name.c_str(), v->GetVarNameNoSuffix(), v->GetVarSuffix());
                        success = false;
                        return;
                    }
                    else
                    {
                        TestAssert(scope->GetColorMark().IsColorA());
                    }
                }
            }
            return;
        }

        if (nodeType == AstNodeType::AstVariable)
        {
            AstVariable* v = assert_cast<AstVariable*>(cur);
            if (v->GetFunctionOwner() != this)
            {
                REPORT_ERR("Function %s: use of local variable %s_%u belonging to another function %s",
                           m_name.c_str(), v->GetVarNameNoSuffix(), v->GetVarSuffix(), v->GetFunctionOwner()->m_name.c_str());
                success = false;
                return;
            }
            else
            {
                REPORT_ERR("Function %s: use of undeclared variable %s_%u",
                           m_name.c_str(), v->GetVarNameNoSuffix(), v->GetVarSuffix());
                success = false;
                return;
            }
        }

        cur->GetColorMark().MarkColorA();

        _Reachability thenClauseReachability;
        if (nodeType == AstNodeType::AstScope &&
            parent != nullptr && parent->GetAstNodeType() == AstNodeType::AstIfStatement)
        {
            AstIfStatement* ifStmt = assert_cast<AstIfStatement*>(parent);
            if (cur == ifStmt->GetElseClause())
            {
                // We are traversing the else-clause of a if-statement.
                // The if-statement is reachable (otherwise an error has been reported),
                // so the else-clause is always reachable.
                // We backup the then-clause reachablity, and the reachability value of code
                // right after the if-statement is the minimum of then-clause and else-clause.
                //
                thenClauseReachability = reachability;
                reachability = _REACHABLE;
            }
            else
            {
                // We are traversing the then-clause. Since the if-statement is reachable,
                // there is no reason things become unreachable here.
                //
                assert(cur == ifStmt->GetThenClause());
                assert(reachability == _REACHABLE);
            }
        }

        // Ignore unreachable empty blocks/scopes. Those are no-ops so harmless.
        //
        if (reachability != _REACHABLE && nodeType != AstNodeType::AstBlock && nodeType != AstNodeType::AstScope)
        {
            // This statement is unreachable! Report error.
            //
            REPORT_ERR("Function %s: unreachable statement (AstNodeType = %s) due to an earlier %s statement",
                       m_name.c_str(), nodeType.ToString(),
                       (reachability == _BREAK ? "Break" : (reachability == _CONTINUE ? "Continue" : "Return")));
            success = false;
            return;
        }

        if (nodeType == AstNodeType::AstDeclareVariable)
        {
            // We ignore stupid corner cases like Declare(v, v + 1)
            // Those could be handled by LLVM codegen logic and interp logic with no issue
            //
            AstDeclareVariable* d = assert_cast<AstDeclareVariable*>(cur);
            AstVariable* v = d->m_variable;
            if (v->GetFunctionOwner() != this)
            {
                REPORT_ERR("Function %s: use of local variable %s_%u belonging to another function %s",
                           m_name.c_str(), v->GetVarNameNoSuffix(), v->GetVarSuffix(), v->GetFunctionOwner()->m_name.c_str());
                success = false;
                return;
            }
            if (varScope.count(v))
            {
                REPORT_ERR("Function %s: re-declaration of variable %s_%u",
                           m_name.c_str(), v->GetVarNameNoSuffix(), v->GetVarSuffix());
                success = false;
                return;
            }
            TestAssert(v->GetColorMark().IsNoColor());
            TestAssert(scopeStack.size() > 0 && scopeStack.back()->GetColorMark().IsColorA());
            varScope[v] = scopeStack.back();
            v->GetColorMark().MarkColorB();
        }

        if (nodeType == AstNodeType::AstScope || nodeType == AstNodeType::AstForLoop)
        {
            scopeStack.push_back(cur);
        }

        if (nodeType == AstNodeType::AstForLoop || nodeType == AstNodeType::AstWhileLoop)
        {
            loopStack.push_back(cur);
        }

        if (nodeType == AstNodeType::AstCallExpr)
        {
            // Check function invocation has matched types
            //
            AstCallExpr* c = assert_cast<AstCallExpr*>(cur);
            if (!c->ValidateSignature())
            {
                assert(thread_errorContext->HasError());
                success = false;
                return;
            }
        }

        if (nodeType == AstNodeType::AstReturnStmt)
        {
            // Check return value matches function prototype
            //
            AstReturnStmt* r = assert_cast<AstReturnStmt*>(cur);
            if (r->m_retVal == nullptr)
            {
                if (!GetReturnType().IsVoid())
                {
                    REPORT_ERR("Function %s: return type does not match function prototype. "
                               "Return statement returned void, expects %s",
                               m_name.c_str(), GetReturnType().Print().c_str());
                    success = false;
                    return;
                }
            }
            else
            {
                if (GetReturnType().IsVoid())
                {
                    REPORT_ERR("Function %s: returning a void expression is not supported. "
                               "Instead of 'Return(VoidExpr())', use 'VoidExpr(); Return();'",
                               m_name.c_str());
                    success = false;
                    return;
                }
                if (r->m_retVal->GetTypeId() != GetReturnType())
                {
                    REPORT_ERR("Function %s: return type does not match function prototype. "
                               "Return statement returned %s, expects %s",
                               m_name.c_str(),
                               r->m_retVal->GetTypeId().Print().c_str(),
                               GetReturnType().Print().c_str());
                    success = false;
                    return;
                }
            }
        }

        if (nodeType == AstNodeType::AstBreakOrContinueStmt)
        {
            if (loopStack.size() == 0)
            {
                AstBreakOrContinueStmt* b = assert_cast<AstBreakOrContinueStmt*>(cur);
                REPORT_ERR("Function %s: use of '%s' statement while not in a loop",
                           m_name.c_str(), (b->IsBreakStatement() ? "Break" : "Continue"));
                success = false;
                return;
            }
        }

        if (isInsideForLoopInitOrStepBlock)
        {
            // Inside for-loop init-block or step-block,
            // we disallow break/continue/return statement, or further nested loops
            //
            if (nodeType == AstNodeType::AstBreakOrContinueStmt)
            {
                AstBreakOrContinueStmt* b = assert_cast<AstBreakOrContinueStmt*>(cur);
                REPORT_ERR("Function %s: use of '%s' statement inside for-loop init-block or step-block is unsupported",
                           m_name.c_str(), (b->IsBreakStatement() ? "Break" : "Continue"));
                success = false;
                return;
            }
            if (nodeType == AstNodeType::AstReturnStmt)
            {
                REPORT_ERR("Function %s: use of 'Return' statement inside for-loop init-block or step-block is unsupported",
                           m_name.c_str());
                success = false;
                return;
            }
            if (nodeType == AstNodeType::AstForLoop || nodeType == AstNodeType::AstWhileLoop)
            {
                REPORT_ERR("Function %s: use of %s inside a for-loop init-block or step-block is unsupported",
                           m_name.c_str(), (nodeType == AstNodeType::AstForLoop ? "for-loop" : "while-loop"));
                success = false;
                return;
            }
        }

        if (nodeType == AstNodeType::AstBlock &&
            parent != nullptr && parent->GetAstNodeType() == AstNodeType::AstForLoop)
        {
#ifdef TESTBUILD
            AstForLoop* forLoop = assert_cast<AstForLoop*>(parent);
            TestAssert(cur == forLoop->GetInitBlock() || cur == forLoop->GetStepBlock());
#endif
            TestAssert(!isInsideForLoopInitOrStepBlock);
            isInsideForLoopInitOrStepBlock = true;
        }

        AssertImp(reachability != _REACHABLE,
                  nodeType == AstNodeType::AstBlock || nodeType == AstNodeType::AstScope);

        Recurse();

        AssertIff(!success, thread_errorContext->HasError());
        if (!success)
        {
            return;
        }

        if (nodeType == AstNodeType::AstBlock &&
            parent != nullptr && parent->GetAstNodeType() == AstNodeType::AstForLoop)
        {
            TestAssert(isInsideForLoopInitOrStepBlock);
            isInsideForLoopInitOrStepBlock = false;
        }

        // Update reachablity flag to reflect the reachablity status of the code right after this statement.
        //
        if (nodeType == AstNodeType::AstScope &&
            parent != nullptr && parent->GetAstNodeType() == AstNodeType::AstIfStatement)
        {
            AstIfStatement* ifStmt = assert_cast<AstIfStatement*>(parent);
            if (cur == ifStmt->GetElseClause())
            {
                // We just finished traversing the else-clause. The reachability status of code right
                // after the if-statement is the minimum of then-clause and else-clause.
                //
                reachability = std::min(reachability, thenClauseReachability);
            }
            else
            {
                // We just finished traversing the then-clause.
                // If the if-statement has else-clause, we will traverse it next,
                // and the logic in the branch above will update the reachability.
                //
                // However, if the if-statement does not have an else-clause,
                // then we are responsible for updating the reachability, and the code
                // after the if-statement is always reachable.
                //
                assert(cur == ifStmt->GetThenClause());
                if (ifStmt->GetElseClause() == nullptr)
                {
                    reachability = _REACHABLE;
                }
            }
        }

        if (nodeType == AstNodeType::AstReturnStmt)
        {
            reachability = _RETURN;
        }

        if (nodeType == AstNodeType::AstBreakOrContinueStmt)
        {
            AstBreakOrContinueStmt* stmt = assert_cast<AstBreakOrContinueStmt*>(cur);
            if (stmt->IsBreakStatement())
            {
                reachability = _BREAK;
            }
            else
            {
                reachability = _CONTINUE;
            }
        }

        // There is a difference between Break and Continue:
        // Break makes the step-block of for-loop unreachable while Continue does not.
        // So we should reset the Continue reachablity flag at the end of the for-loop body,
        // while the Break reachablity flag at the end of the for loop.
        //
        if (reachability == _CONTINUE && nodeType == AstNodeType::AstScope &&
            parent != nullptr && parent->GetAstNodeType() == AstNodeType::AstForLoop)
        {
            AstForLoop* forLoop = assert_cast<AstForLoop*>(parent);
            if (cur == forLoop->GetBody())
            {
                reachability = _REACHABLE;
            }
        }

        if (nodeType == AstNodeType::AstScope || nodeType == AstNodeType::AstForLoop)
        {
            scopeStack.pop_back();
        }

        if (nodeType == AstNodeType::AstForLoop || nodeType == AstNodeType::AstWhileLoop)
        {
            loopStack.pop_back();
            // We are at the end of the loop.
            // Reset reachability flag, since it is always possible that the loop body is not executed
            //
            // For-loop continue flag should have been reset at end of body,
            // and we disallow use of continue/break/return inside for-loop step-block
            //
            AssertImp(nodeType == AstNodeType::AstForLoop, reachability != _CONTINUE);
            reachability = _REACHABLE;
        }

        TestAssert(cur->GetColorMark().IsColorA());
        cur->GetColorMark().MarkColorB();
    };

    TraverseFunctionBody(traverseFn);

    TestAssertIff(thread_errorContext->HasError(), !success);
    return success;
}

}   // namespace Ast
