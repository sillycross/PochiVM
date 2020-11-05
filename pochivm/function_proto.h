#pragma once

#include "ast_expr_base.h"
#include "lang_constructs.h"
#include "error_context.h"
#include "bitcode_data.h"
#include "fastinterp/fastinterp_tpl_return_type.h"
#include "pochivm_function_pointer.h"

#include "generated/pochivm_runtime_cpp_typeinfo.generated.h"

namespace llvm
{

class BasicBlock;
class AllocaInst;
class Module;
class LLVMContext;

namespace orc {
class ThreadSafeModule;
}   // namespace orc

}   // namespace llvm

namespace PochiVM
{

namespace internal
{

template<typename T>
void AstFunctionInterpStoreParamsHelper(T* dst, AstNodeBase* src)
{
    T value;
    src->DebugInterp(&value);
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
        , m_logicalOpSuffixOrdinal(0)
        , m_debugInterpStackFrameSize(static_cast<uint32_t>(-1))
        , m_debugInterpStoreParamFns()
        , m_debugInterpStoreRetValFn(nullptr)
        , m_llvmEntryBlock(nullptr)
        , m_isNoExcept(false)
        , m_fastInterpStackFrameSize(static_cast<uint32_t>(-1))
        , m_fastInterpCppEntryPoint(nullptr)
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
    void TraverseFunctionBody(FunctionRef<void(AstNodeBase* /*cur*/,
                                               AstNodeBase* /*parent*/,
                                               FunctionRef<void(void)> /*Recurse*/)> fn)
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
    uint32_t GetNextLogicalOpSuffix() { return m_logicalOpSuffixOrdinal++; }

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

    void SetIsNoExcept(bool isNoExcept)
    {
        m_isNoExcept = isNoExcept;
    }

    bool GetIsNoExcept() const
    {
        return m_isNoExcept;
    }

    uint32_t GetFastInterpStackFrameSize() const
    {
        TestAssert(m_fastInterpStackFrameSize != static_cast<uint32_t>(-1));
        return m_fastInterpStackFrameSize;
    }

    // Below are methods for interp mode
    //
    // Interp execute the function
    //
    void DebugInterp(AstNodeBase** params, void* returnValueOut)
    {
        // Allocate space for the new stack frame
        //
        assert(m_debugInterpStackFrameSize != static_cast<uint32_t>(-1) && m_debugInterpStackFrameSize > 0);
        void* stackFrameBase = alloca(m_debugInterpStackFrameSize);

        // Store parameters into stack frame
        // The parameter expressions must be evaluated in the OLD stack frame,
        // so it is important we not switch to the new stack frame now.
        // However, their results should be stored into corrresponding addresses in new stack frame.
        //
        assert(m_debugInterpStoreParamFns.size() == m_params.size());
        for (size_t i = 0; i < GetNumParams(); i++)
        {
            DebugInterpSetParam(reinterpret_cast<uintptr_t>(stackFrameBase), i, params[i]);
        }

        // Switch to the new stack frame, saving the old one
        //
        uintptr_t oldStackFrameBase = thread_pochiVMContext->m_debugInterpStackFrameBase;
        thread_pochiVMContext->m_debugInterpStackFrameBase = reinterpret_cast<uintptr_t>(stackFrameBase);

        // Save the old variable scope stack, and switch to a new empty scope stack.
        //
        std::vector<std::vector<AstVariable*>> oldScopeStack;
        oldScopeStack.swap(thread_pochiVMContext->m_debugInterpScopeStack);

        // Execute function
        //
        InterpControlSignal ics = InterpControlSignal::None;
        m_body->DebugInterp(&ics);

        // The scope stack must end up empty after the function's execution
        //
        TestAssert(thread_pochiVMContext->m_debugInterpScopeStack.size() == 0);

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
        m_debugInterpStoreRetValFn(returnValueOut /*dst*/, stackFrameBase /*src*/);

        // Restore stack frame and variable scope stack
        //
        thread_pochiVMContext->m_debugInterpStackFrameBase = oldStackFrameBase;
        thread_pochiVMContext->m_debugInterpScopeStack.swap(oldScopeStack);
    }

    // Set up various information needed for interp execution
    //
    void PrepareForDebugInterp();

    void PrepareForFastInterp();

    void SetFastInterpCppEntryPoint(void* entryPoint)
    {
        TestAssert(m_fastInterpCppEntryPoint == nullptr && entryPoint != nullptr);
        m_fastInterpCppEntryPoint = entryPoint;
    }

    void* GetFastInterpCppEntryPoint() const
    {
        return m_fastInterpCppEntryPoint;
    }

private:

    void DebugInterpSetParam(uintptr_t newStackFrameBase, size_t i, AstNodeBase* param)
    {
        assert(i < GetNumParams() && param->GetTypeId() == GetParamType(i));
        uintptr_t addr = newStackFrameBase + m_params[i]->GetDebugInterpOffset();
        m_debugInterpStoreParamFns[i](reinterpret_cast<void*>(addr), param);
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
    uint32_t m_logicalOpSuffixOrdinal;

    // Interp data
    //
    // Stack frame size needed to store the temp data
    //
    uint32_t m_debugInterpStackFrameSize;
    // function pointer for storing each parameter on invocation
    //
    using _StoreParamsFn = void(*)(void*, AstNodeBase*);
    std::vector<_StoreParamsFn> m_debugInterpStoreParamFns;
    // function pointer for retrieving return value and store to variable
    //
    using _StoreRetValFn = void(*)(void*, void*);
    _StoreRetValFn m_debugInterpStoreRetValFn;

    // llvm data
    //
    llvm::BasicBlock* m_llvmEntryBlock;

    bool m_isNoExcept;
    uint32_t m_fastInterpStackFrameSize;
    void* m_fastInterpCppEntryPoint;
};

namespace internal
{

template<typename R, typename... Args>
struct DebugInterpCallFunctionImplImpl
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
        memoryOwner->SetupDebugInterpImpl();
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
        fn->DebugInterp(params, ret);
    }
};

// avoid compile error due to declaring a void type variable
//
template<typename R, typename... Args>
struct DebugInterpCallFunctionImpl
{
    static R call(AstFunction* fn, Args... args)
    {
        R ret;
        DebugInterpCallFunctionImplImpl<R, Args...>::call_impl(fn, &ret, args...);
        return ret;
    }
};

template<typename... Args>
struct DebugInterpCallFunctionImpl<void, Args...>
{
    static void call(AstFunction* fn, Args... args)
    {
        DebugInterpCallFunctionImplImpl<void, Args...>::call_impl(fn, nullptr, args...);
    }
};

}   // namespace internal

template<typename T>
class DebugInterpFunction
{
    static_assert(sizeof(T) == 0, "T must be a C function pointer");
};

template<typename R, typename... Args>
class DebugInterpFunction<R(*)(Args...) noexcept>
{
public:
    DebugInterpFunction() : m_fn(nullptr) { }
    DebugInterpFunction(AstFunction* fn) : m_fn(fn) { }

    explicit operator bool() const { return m_fn != nullptr; }

    // Important that this function is marked noexcept, so if an exception is thrown out, std::terminate is called
    //
    R operator()(Args... args) const noexcept
    {
        TestAssert(m_fn != nullptr);
        return internal::DebugInterpCallFunctionImpl<R, Args...>::call(m_fn, args...);
    }

private:
    AstFunction* m_fn;
};

template<typename R, typename... Args>
class DebugInterpFunction<R(*)(Args...)>
{
public:
    DebugInterpFunction() : m_fn(nullptr) { }
    DebugInterpFunction(AstFunction* fn) : m_fn(fn) { }

    explicit operator bool() const { return m_fn != nullptr; }

    R operator()(Args... args) const
    {
        TestAssert(m_fn != nullptr);
        if (m_fn->GetIsNoExcept())
        {
            return NoExceptExecutor(args...);
        }
        else
        {
            return internal::DebugInterpCallFunctionImpl<R, Args...>::call(m_fn, args...);
        }
    }

private:
    // Important that this function is marked noexcept, so if an exception is thrown out, std::terminate is called
    //
    R NoExceptExecutor(Args... args) const noexcept
    {
        return internal::DebugInterpCallFunctionImpl<R, Args...>::call(m_fn, args...);
    }

    AstFunction* m_fn;
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
        , m_llvmContext(nullptr)
        , m_llvmModule(nullptr)
#ifdef TESTBUILD
        , m_validated(false)
        , m_debugInterpPrepared(false)
        , m_fastInterpPrepared(false)
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

    void PrepareForDebugInterp()
    {
        TestAssert(!m_debugInterpPrepared);
#ifdef TESTBUILD
        m_debugInterpPrepared = true;
#endif
        AstTraverseColorMark::ClearAll();
        for (auto iter = m_functions.begin(); iter != m_functions.end(); iter++)
        {
            AstFunction* fn = iter->second;
            fn->PrepareForDebugInterp();
        }
    }

    void PrepareForFastInterp();

    void EmitIR();
    void OptimizeIR(int optLevel);
    void OptimizeIRIfNotDebugMode(int optLevel);

    // Called after emitting IR and optionally optimizing IR.
    // Transfer ownership of m_llvmContext and m_llvmModule to the returned llvm::ThreadSafeModule object.
    //
    llvm::orc::ThreadSafeModule GetThreadSafeModule();

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
        assert(m_llvmModule != nullptr);
        return m_llvmModule;
    }

    // Get a callable object that invokes a generated function in debug interp mode
    //
    // T must be a C-style function pointer that matches the prototype of the generated function
    // Return the callable object that calls the generated function in interp mode,
    //    or an object that contextually evaluates to false if no function with given name exist.
    //
    // Example:
    //    using FnType = void(*)(int, double);
    //    auto fn = GetGeneratedFunctionInterpMode<FnType>("generated_fn_name");
    //    fn(12, 3.45);
    //
    template<typename T>
    DebugInterpFunction<T> GetDebugInterpGeneratedFunction(const std::string& name)
    {
        assert(m_debugInterpPrepared);
        AstFunction* fn = GetAstFunction(name);
        if (fn == nullptr)
        {
            return DebugInterpFunction<T>();
        }
        TestAssert(FastInterpCallFunction<T>::check_prototype_ok(fn));
        return DebugInterpFunction<T>(fn);
    }

    // T must be a C style function pointer
    // returns FastInterpFunction which contextually converts to false if function not found
    //
    template<typename T>
    FastInterpFunction<T> GetFastInterpGeneratedFunction(const std::string& name)
    {
        assert(m_fastInterpPrepared);
        AstFunction* fn = GetAstFunction(name);
        if (fn == nullptr)
        {
            return FastInterpFunction<T>();
        }
        TestAssert(FastInterpCallFunction<T>::check_prototype_ok(fn));
        return FastInterpCallFunction<T>::get(fn);
    }

    // Check that the function with specified name exists and its prototype matches T
    // T must be a C-style function pointer
    //
    template<typename T>
    bool WARN_UNUSED CheckFunctionExistsAndPrototypeMatches(const std::string& name)
    {
        AstFunction* fn = GetAstFunction(name);
        CHECK(fn != nullptr);
        CHECK(FastInterpCallFunction<T>::check_prototype_ok(fn));
        return true;
    }

private:

    template<typename T>
    struct FastInterpCallFunction
    {
        static_assert(sizeof(T) == 0, "T must be a std::function type!");
    };

    template<typename R, typename... Args>
    struct FastInterpCallFunction<R(*)(Args...)>
    {
        using FnInfo = AstTypeHelper::function_type_helper<R(*)(Args...)>;

        static bool WARN_UNUSED check_prototype_ok(AstFunction* fn)
        {
            CHECK(FnInfo::numArgs == fn->GetNumParams());
            CHECK(TypeId::Get<R>() == fn->GetReturnType());
            for (size_t i = 0; i < FnInfo::numArgs; i++)
            {
                CHECK(fn->GetParamType(i) == FnInfo::argTypeId[i]);
            }
            return true;
        }

        static FastInterpFunction<R(*)(Args...)> get(AstFunction* fn)
        {
            return FastInterpFunction<R(*)(Args...)>(
                        fn->GetFastInterpCppEntryPoint(),
                        fn->GetFastInterpStackFrameSize(),
                        fn->GetIsNoExcept());
        }
    };

    template<typename R, typename... Args>
    struct FastInterpCallFunction<R(*)(Args...) noexcept>
    {
        using FnInfo = AstTypeHelper::function_type_helper<R(*)(Args...) noexcept>;

        static bool WARN_UNUSED check_prototype_ok(AstFunction* fn)
        {
            CHECK(FnInfo::numArgs == fn->GetNumParams());
            CHECK(TypeId::Get<R>() == fn->GetReturnType());
            for (size_t i = 0; i < FnInfo::numArgs; i++)
            {
                CHECK(fn->GetParamType(i) == FnInfo::argTypeId[i]);
            }
            CHECK(fn->GetIsNoExcept());
            return true;
        }

        static FastInterpFunction<R(*)(Args...) noexcept> get(AstFunction* fn)
        {
            return FastInterpFunction<R(*)(Args...) noexcept>(
                        fn->GetFastInterpCppEntryPoint(),
                        fn->GetFastInterpStackFrameSize());
        }
    };

    std::string m_moduleName;
    // We are using std::map (not std::unordered_map) so that the iteration order is deterministic.
    // This is required to make tests happy (so IR dump output is deterministic).
    // The perf hit should be small. TODO: consider use std::unordered_map in release?
    //
#ifdef TESTBUILD
    std::map<std::string, AstFunction*> m_functions;
#else
    std::unordered_map<std::string, AstFunction*> m_functions;
#endif
    llvm::LLVMContext* m_llvmContext;
    llvm::Module* m_llvmModule;
#ifdef TESTBUILD
    bool m_validated;
    bool m_debugInterpPrepared;
    bool m_fastInterpPrepared;
    bool m_irEmitted;
    bool m_irOptimized;
#endif
};

// A function pointer to a generated function invocable from C++ code
// This operator gives an opaque uintptr_t value.
// From the C++ code one can write 'GeneratedFunctionPointer<FnPrototype>(value)(args...)'
// to invoke the generated function
//
class AstGeneratedFunctionPointerExpr : public AstNodeBase
{
public:
    AstGeneratedFunctionPointerExpr(const std::string& fnName)
        : AstNodeBase(AstNodeType::AstGeneratedFunctionPointerExpr, TypeId::Get<uintptr_t>())
        , m_fnName(fnName)
    { }

    void InterpImpl(uintptr_t* result)
    {
        *result = m_debugInterpResult;
    }

    virtual void SetupDebugInterpImpl() override final
    {
        m_callTarget = thread_pochiVMContext->m_curModule->GetAstFunction(m_fnName);
        m_debugInterpResult = GeneratedFunctionPointerImpl::GetControlValueForDebugInterpFn(m_callTarget);
        m_debugInterpFn = AstTypeHelper::GetClassMethodPtr(&AstGeneratedFunctionPointerExpr::InterpImpl);
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override final;
    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> /*fn*/) override final {}
    virtual FastInterpSnippet WARN_UNUSED PrepareForFastInterp(FISpillLocation spillLoc) override final;
    virtual void FastInterpSetupSpillLocation() override final { }

    const std::string& GetFnName() { return m_fnName; }

private:
    std::string m_fnName;
    AstFunction* m_callTarget;
    uintptr_t m_debugInterpResult;
};

// Call a generated function
//
class AstCallExpr : public AstNodeBase
{
public:
    AstCallExpr(const std::string& name,
                const std::vector<AstNodeBase*>& params,
                TypeId returnType)
        : AstNodeBase(AstNodeType::AstCallExpr, returnType)
        , m_fnName(name)
        , m_params(params)
        , m_isCppFunction(false)
        , m_cppFunctionMd(nullptr)
        , m_interpFunction(nullptr)
        , m_debugInterpStoreParamFns()
        , m_sretAddress(nullptr)
        , m_fastInterpSretVar(nullptr)
        , m_fastInterpSpillNewSfAddrAt(static_cast<uint32_t>(-1))
    { }

    AstCallExpr(const CppFunctionMetadata* cppFunctionMd,
                const std::vector<AstNodeBase*>& params)
        : AstNodeBase(AstNodeType::AstCallExpr, cppFunctionMd->m_returnType)
        , m_fnName()
        , m_params(params)
        , m_isCppFunction(true)
        , m_cppFunctionMd(cppFunctionMd)
        , m_interpFunction(nullptr)
        , m_debugInterpStoreParamFns()
        , m_sretAddress(nullptr)
        , m_fastInterpSretVar(nullptr)
        , m_fastInterpSpillNewSfAddrAt(static_cast<uint32_t>(-1))
    {
        assert(m_cppFunctionMd != nullptr);
        TestAssert(params.size() == static_cast<size_t>(m_cppFunctionMd->m_numParams));
#ifdef TESTBUILD
        for (size_t i = 0; i < params.size(); i++)
        {
            TestAssert(m_cppFunctionMd->m_paramTypes[i] == m_params[i]->GetTypeId());
        }
#endif
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override final;

    void InterpImplGeneratedFunction(void* out)
    {
        m_interpFunction->DebugInterp(m_params.data(), out);
    }

    void InterpImplCppFunction(void* out)
    {
        size_t numParams = m_cppFunctionMd->m_numParams;
        if (m_cppFunctionMd->m_isUsingSret)
        {
            numParams++;
        }
        void** params = reinterpret_cast<void**>(alloca(numParams * sizeof(void*)));
        void** curParam = params;
        if (m_cppFunctionMd->m_isUsingSret)
        {
            *curParam = out;
            curParam++;
        }
        for (size_t i = 0; i < m_cppFunctionMd->m_numParams; i++)
        {
            TestAssert(m_cppFunctionMd->m_paramTypes[i].Size() <= sizeof(void*));
            m_debugInterpStoreParamFns[i](curParam, m_params[i]);
            curParam++;
        }
        // If the function is using sret or returns void,
        // we should never need to write into 'out'. Make it nullptr to catch any bugs.
        //
        if (m_cppFunctionMd->m_isUsingSret || m_cppFunctionMd->m_returnType.IsVoid())
        {
            out = nullptr;
        }
        m_cppFunctionMd->m_debugInterpFn(out /*ret*/, params);
    }

    // Validate that all caller types matches corresponding callee types
    //
    bool WARN_UNUSED ValidateSignature()
    {
        if (!m_isCppFunction)
        {
            AstFunction* fn = thread_pochiVMContext->m_curModule->GetAstFunction(m_fnName);
            CHECK_REPORT_ERR(fn != nullptr, "Call to undefined function %s", m_fnName.c_str());
            CHECK_ERR(fn->CheckParamTypes(m_params));
            CHECK_REPORT_ERR(fn->GetReturnType() == GetTypeId(),
                             "Call to function %s: wrong return type, expects %s got %s",
                             m_fnName.c_str(), fn->GetReturnType().Print().c_str(), GetTypeId().Print().c_str());
        }
        else
        {
            // Shouldn't really fail here since the API is generated, but just to be safe..
            //
            CHECK_REPORT_ERR(m_params.size() == m_cppFunctionMd->m_numParams,
                             "Call to function %s: wrong number of parameters, expects %d got %d",
                             m_cppFunctionMd->m_bitcodeData->m_symbolName, int(m_params.size()), int(m_cppFunctionMd->m_numParams));
            for (size_t i = 0; i < m_params.size(); i++)
            {
                CHECK_REPORT_ERR(m_params[i]->GetTypeId() == m_cppFunctionMd->m_paramTypes[i],
                                 "Call to function %s: parameter %d has wrong type, expects %s got %s",
                                 m_cppFunctionMd->m_bitcodeData->m_symbolName, int(i),
                                 m_cppFunctionMd->m_paramTypes[i].Print().c_str(),
                                 m_params[i]->GetTypeId().Print().c_str());
            }
            CHECK_REPORT_ERR(m_cppFunctionMd->m_returnType == GetTypeId(),
                             "Call to function %s: wrong return type, expects %s got %s",
                             m_cppFunctionMd->m_bitcodeData->m_symbolName,
                             m_cppFunctionMd->m_returnType.Print().c_str(), GetTypeId().Print().c_str());
        }
        RETURN_TRUE;
    }

    bool IsCppFunction() const
    {
        return m_isCppFunction;
    }

    const CppFunctionMetadata* GetCppFunctionMetadata() const
    {
        assert(IsCppFunction());
        return m_cppFunctionMd;
    }

    const std::vector<AstNodeBase*>& GetParams() const
    {
        return m_params;
    }

    void SetSretAddress(llvm::Value* address);

    void SetFastInterpSretVariable(AstVariable* variable)
    {
        TestAssert(m_isCppFunction && m_cppFunctionMd->m_isUsingSret);
        TestAssert(m_fastInterpSretVar == nullptr && variable != nullptr);
        TestAssert(m_cppFunctionMd->m_returnType.AddPointer() == variable->GetTypeId());
        m_fastInterpSretVar = variable;
    }

    virtual void SetupDebugInterpImpl() override final
    {
        if (!m_isCppFunction)
        {
            m_interpFunction = thread_pochiVMContext->m_curModule->GetAstFunction(m_fnName);
            TestAssert(m_interpFunction != nullptr);
            m_debugInterpFn = AstTypeHelper::GetClassMethodPtr(&AstCallExpr::InterpImplGeneratedFunction);
        }
        else
        {
            for (size_t i = 0; i < m_cppFunctionMd->m_numParams; i++)
            {
                _StoreParamsFn storeParamsFn = reinterpret_cast<_StoreParamsFn>(
                        internal::AstFunctionInterpStoreParamsSelector(m_cppFunctionMd->m_paramTypes[i]));
                m_debugInterpStoreParamFns.push_back(storeParamsFn);
            }
            m_debugInterpFn = AstTypeHelper::GetClassMethodPtr(&AstCallExpr::InterpImplCppFunction);
        }
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) override final
    {
        for (auto it = m_params.begin(); it != m_params.end(); it++)
        {
            fn(*it);
        }
    }

    virtual FastInterpSnippet WARN_UNUSED PrepareForFastInterp(FISpillLocation spillLoc) override final;
    virtual void FastInterpSetupSpillLocation() override final;

    void FastInterpFixStackFrameSize(AstFunction* target);

private:
    std::string m_fnName;
    std::vector<AstNodeBase*> m_params;
    bool m_isCppFunction;
    const CppFunctionMetadata* m_cppFunctionMd;
    // Function to call, only populated in interp mode and for non-cpp function
    //
    AstFunction* m_interpFunction;
    // Interp mode storeParamHelper helper.
    // Only populated for CPP function!
    // The non-CPP function's interp mode storeParamHelper belongs to the AstFunction class.
    //
    using _StoreParamsFn = void(*)(void*, AstNodeBase*);
    std::vector<_StoreParamsFn> m_debugInterpStoreParamFns;
    // In LLVM mode, for function using sret, the position to which its return value shall be stored
    //
    llvm::Value* m_sretAddress;
    AstVariable* m_fastInterpSretVar;
    FISpillLocation m_fastInterpSpillLoc;
    uint32_t m_fastInterpSpillNewSfAddrAt;
    FastInterpBoilerplateInstance* m_fastInterpInst;
};

class AstDeclareVariable : public AstNodeBase
{
public:
    AstDeclareVariable(AstVariable* variable)
        : AstNodeBase(AstNodeType::AstDeclareVariable, TypeId::Get<void>())
        , m_assignExpr(nullptr)
        , m_callExpr(nullptr)
        , m_variable(variable)
        , m_isCtor(false)
    {
        TestAssert(m_variable->GetTypeId().IsPointerType());
    }

    AstDeclareVariable(AstVariable* variable, AstAssignExpr* assignExpr)
        : AstNodeBase(AstNodeType::AstDeclareVariable, TypeId::Get<void>())
        , m_assignExpr(assignExpr)
        , m_callExpr(nullptr)
        , m_variable(variable)
        , m_isCtor(false)
    {
        TestAssert(m_assignExpr->GetValueType().AddPointer() == m_variable->GetTypeId());
        TestAssert(assert_cast<AstVariable*>(m_assignExpr->GetDst()) == m_variable);
    }

    AstDeclareVariable(AstVariable* variable, AstCallExpr* callExpr, bool isCtor)
        : AstNodeBase(AstNodeType::AstDeclareVariable, TypeId::Get<void>())
        , m_assignExpr(nullptr)
        , m_callExpr(callExpr)
        , m_variable(variable)
        , m_isCtor(isCtor)
    {
        TestAssertImp(!isCtor, m_callExpr->GetTypeId().AddPointer() == m_variable->GetTypeId());
        TestAssertImp(isCtor,
                      m_callExpr->GetTypeId() == TypeId::Get<void>() &&
                      m_callExpr->IsCppFunction() &&
                      m_callExpr->GetCppFunctionMetadata()->m_numParams > 0 &&
                      m_callExpr->GetCppFunctionMetadata()->m_paramTypes[0] == m_variable->GetTypeId() &&
                      m_callExpr->GetParams()[0] == m_variable);
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override final;

    void InterpImpl(void* /*out*/)
    {
        if (m_assignExpr != nullptr)
        {
            m_assignExpr->DebugInterp(nullptr /*out*/);
        }
        else if (m_callExpr != nullptr)
        {
            if (!m_isCtor)
            {
                void* addr;
                // 'addr' now points to the storing address of the variable
                //
                m_variable->DebugInterp(&addr /*out*/);
                // evaluate the call expression, which will construct the return value in-place at 'addr'
                //
                m_callExpr->DebugInterp(addr /*out*/);
            }
            else
            {
                // For ctor, return value is void, and the address of the variable
                // has been placed on the first parameter
                //
                m_callExpr->DebugInterp(nullptr /*out*/);
            }
        }
        // If the variable being declared is a CPP class, we should push it into the current variable scope.
        // When we leave the scope, the destructor will be called.
        //
        if (m_variable->GetTypeId().RemovePointer().IsCppClassType())
        {
            assert(thread_pochiVMContext->m_debugInterpScopeStack.size() > 0);
            thread_pochiVMContext->m_debugInterpScopeStack.back().push_back(m_variable);
        }
    }

    virtual void SetupDebugInterpImpl() override final
    {
        m_debugInterpFn = AstTypeHelper::GetClassMethodPtr(&AstDeclareVariable::InterpImpl);
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) override final
    {
        if (m_assignExpr != nullptr) { fn(m_assignExpr); }
        if (m_callExpr != nullptr) { fn(m_callExpr); }
        fn(m_variable);
    }

    virtual FastInterpSnippet WARN_UNUSED PrepareForFastInterp(FISpillLocation spillLoc) override final;
    virtual void FastInterpSetupSpillLocation() override final;

    // An assign statement for primitive type variable initialization.
    //
    AstAssignExpr* m_assignExpr;
    // A call expression for non-primitive type variable initialization
    //
    AstCallExpr* m_callExpr;
    AstVariable* m_variable;
    bool m_isCtor;
};

class AstReturnStmt : public AstNodeBase
{
public:
    AstReturnStmt(AstNodeBase* retVal)
        : AstNodeBase(AstNodeType::AstReturnStmt, TypeId::Get<void>())
        , m_retVal(retVal)
    {
        TestAssertImp(m_retVal != nullptr,
                      m_retVal->GetTypeId().IsPrimitiveType() || m_retVal->GetTypeId().IsPointerType());
    }

    virtual llvm::Value* WARN_UNUSED EmitIRImpl() override final;

    template<typename T>
    void InterpImpl(InterpControlSignal* out)
    {
        m_retVal->DebugInterp(reinterpret_cast<void*>(thread_pochiVMContext->m_debugInterpStackFrameBase));
        assert(*out == InterpControlSignal::None);
        *out = InterpControlSignal::Return;
    }

    GEN_CLASS_METHOD_SELECTOR(SelectImpl, AstReturnStmt, InterpImpl, AstTypeHelper::primitive_or_pointer_type)

    void InterpImplVoid(InterpControlSignal* out)
    {
        assert(*out == InterpControlSignal::None);
        *out = InterpControlSignal::Return;
    }

    virtual void SetupDebugInterpImpl() override final
    {
        if (m_retVal == nullptr)
        {
            m_debugInterpFn = AstTypeHelper::GetClassMethodPtr(&AstReturnStmt::InterpImplVoid);
        }
        else
        {
            m_debugInterpFn = SelectImpl(m_retVal->GetTypeId());
        }
    }

    virtual void ForEachChildren(FunctionRef<void(AstNodeBase*)> fn) override final
    {
        if (m_retVal != nullptr)
        {
            fn(m_retVal);
        }
    }

    virtual FastInterpSnippet WARN_UNUSED PrepareForFastInterp(FISpillLocation spillLoc) override final;
    virtual void FastInterpSetupSpillLocation() override final;

    AstNodeBase* m_retVal;
};

inline void AstFunction::PrepareForDebugInterp()
{
    assert(m_debugInterpStackFrameSize == static_cast<uint32_t>(-1));
    // Setup m_debugInterpStoreParamFns and m_debugInterpStoreRetValFn
    //
    for (size_t i = 0; i < GetNumParams(); i++)
    {
        _StoreParamsFn storeParamsFn = reinterpret_cast<_StoreParamsFn>(
                    internal::AstFunctionInterpStoreParamsSelector(GetParamType(i)));
        m_debugInterpStoreParamFns.push_back(storeParamsFn);
    }
    m_debugInterpStoreRetValFn = reinterpret_cast<_StoreRetValFn>(
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
        cur->SetupDebugInterpImpl();
        AstNodeType nodeType = cur->GetAstNodeType();
        if (nodeType == AstNodeType::AstVariable)
        {
            AstVariable* v = assert_cast<AstVariable*>(cur);
            // pad to natural alignment min(8, storageSize)
            //
            size = up_align(size, std::min(8U, v->GetStorageSize()));
            v->SetDebugInterpOffset(size);
            size += v->GetStorageSize();
        }
        else if (nodeType == AstNodeType::AstRvalueToConstPrimitiveRefExpr)
        {
            AstRvalueToConstPrimitiveRefExpr* v = assert_cast<AstRvalueToConstPrimitiveRefExpr*>(cur);
            uint32_t storageSize = static_cast<uint32_t>(v->GetTypeId().RemovePointer().Size());
            size = up_align(size, std::min(8U, storageSize));
            v->m_debugInterpOffset = size;
            size += storageSize;
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
    auto traverseFn = [&](AstNodeBase* cur,
                          AstNodeBase* /*parent*/,
                          FunctionRef<void(void)> Recurse)
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
    m_debugInterpStackFrameSize = size;
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
    bool isInsideForLoopInitBlock = false;
    bool isInsideForLoopStepBlock = false;

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
    auto traverseFn = [&](AstNodeBase* cur,
                          AstNodeBase* parent,
                          FunctionRef<void(void)> Recurse)
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

        if (nodeType == AstNodeType::AstGeneratedFunctionPointerExpr)
        {
            // Check the function exists
            //
            AstGeneratedFunctionPointerExpr* c = assert_cast<AstGeneratedFunctionPointerExpr*>(cur);
            const std::string& fnName = c->GetFnName();
            if (thread_pochiVMContext->m_curModule->GetAstFunction(fnName) == nullptr)
            {
                REPORT_ERR("Function %s: Attempted to take generated function address of non-existent function %s",
                           m_name.c_str(), fnName.c_str());
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

        if (isInsideForLoopInitBlock || isInsideForLoopStepBlock)
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

        if (isInsideForLoopStepBlock)
        {
            // Inside for-loop step-block, we additionally disallow declaring variables,
            // since it is unclear what those variables' lifetime would be.
            //
            if (nodeType == AstNodeType::AstDeclareVariable)
            {
                REPORT_ERR("Function %s: use of 'DeclareVariable' statement inside for-loop step-block is unsupported",
                           m_name.c_str());
                success = false;
                return;
            }
        }

        if (nodeType == AstNodeType::AstBlock &&
            parent != nullptr && parent->GetAstNodeType() == AstNodeType::AstForLoop)
        {
            AstForLoop* forLoop = assert_cast<AstForLoop*>(parent);
            TestAssert(cur == forLoop->GetInitBlock() || cur == forLoop->GetStepBlock());
            TestAssert(!isInsideForLoopInitBlock && !isInsideForLoopStepBlock);
            if (cur == forLoop->GetInitBlock())
            {
                isInsideForLoopInitBlock = true;
            }
            else
            {
                isInsideForLoopStepBlock = true;
            }
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
            TestAssert(isInsideForLoopInitBlock || isInsideForLoopStepBlock);
            TestAssert(!isInsideForLoopInitBlock || !isInsideForLoopStepBlock);
            isInsideForLoopInitBlock = false;
            isInsideForLoopStepBlock = false;
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

}   // namespace PochiVM
