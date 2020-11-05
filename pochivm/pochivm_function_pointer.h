#pragma once

#include "common.h"
#include "fastinterp/fastinterp_tpl_return_type.h"
#include "fastinterp/fastinterp_tpl_stackframe_category.h"

namespace PochiVM
{

class AstFunction;

extern thread_local std::exception_ptr thread_pochiVMFastInterpOutstandingExceptionPtr;

template<typename T>
class FastInterpFunction
{
    static_assert(sizeof(T) == 0, "T must be a C function pointer");
};

template<typename R, typename... Args>
class FastInterpFunction<R(*)(Args...) noexcept>
{
public:
    FastInterpFunction() : m_fnPtr(nullptr) {}
    FastInterpFunction(void* fnPtr, uint32_t stackframeSize)
        : m_fnPtr(fnPtr), m_stackframeSize(stackframeSize)
    { }

    explicit operator bool() const { return m_fnPtr != nullptr; }

    operator std::function<R(Args...)>() const
    {
        return [*this](Args... args) -> R
        {
            return (*this)(args...);
        };
    }

    R operator()(Args... args) const noexcept
    {
        TestAssert(m_fnPtr != nullptr);
        uintptr_t sf = reinterpret_cast<uintptr_t>(alloca(m_stackframeSize));
        PopulateParams(sf + 8, args...);
        return reinterpret_cast<R(*)(uintptr_t) noexcept>(m_fnPtr)(sf);
    }

private:
    static void PopulateParams(uintptr_t /*sf*/) noexcept { }

    template<typename T, typename... TArgs>
    static void PopulateParams(uintptr_t sf, T arg, TArgs... more) noexcept
    {
        *reinterpret_cast<T*>(sf) = arg;
        PopulateParams(sf + 8, more...);
    }

    void* m_fnPtr;
    uint32_t m_stackframeSize;
};

template<typename R, typename... Args>
class FastInterpFunction<R(*)(Args...)>
{
public:
    FastInterpFunction() : m_fnPtr(nullptr) {}
    FastInterpFunction(void* fnPtr, uint32_t stackframeSize, bool isNoExcept)
        : m_fnPtr(fnPtr), m_stackframeSize(stackframeSize), m_isNoExcept(isNoExcept)
    { }

    explicit operator bool() const { return m_fnPtr != nullptr; }

    operator std::function<R(Args...)>() const
    {
        return [*this](Args... args) -> R
        {
            return (*this)(args...);
        };
    }

    R operator()(Args... args) const
    {
        TestAssert(m_fnPtr != nullptr);
        uintptr_t sf = reinterpret_cast<uintptr_t>(alloca(m_stackframeSize));
        PopulateParams(sf + 8, args...);
        if (m_isNoExcept)
        {
            return reinterpret_cast<R(*)(uintptr_t) noexcept>(m_fnPtr)(sf);
        }
        else
        {
            TestAssert(!thread_pochiVMFastInterpOutstandingExceptionPtr);
            Auto(TestAssert(!thread_pochiVMFastInterpOutstandingExceptionPtr));

            using RetOrExn = FIReturnType<R, false /*isNoExcept*/>;
            RetOrExn r = reinterpret_cast<RetOrExn(*)(uintptr_t) noexcept>(m_fnPtr)(sf);
            if (unlikely(FIReturnValueHelper::HasException<R>(r)))
            {
                // Stupid: std::exception_ptr is a shared-ownership smart pointer.
                // We have to manually null it so the exception won't leak.
                // However, std::rethrow_exception will NOT null the pointer.
                // So we have to copy the pointer to a local variable, then null the global-variable pointer,
                // then call std::rethrow_exception. Now when the local variable goes out of scope,
                // the refcount is properly decremented..
                //
                TestAssert(thread_pochiVMFastInterpOutstandingExceptionPtr);
                std::exception_ptr eptr = thread_pochiVMFastInterpOutstandingExceptionPtr;
                thread_pochiVMFastInterpOutstandingExceptionPtr = nullptr;
                // The 'eptr' is a local var, and after rethrow_exception,
                // it goes out of scope so the exception refcount is decremented.
                //
                std::rethrow_exception(eptr);
            }
            else
            {
                if constexpr(!std::is_same<R, void>::value)
                {
                    return FIReturnValueHelper::GetReturnValue<R, false /*isNoExcept*/>(r);
                }
            }
        }
    }

private:
    static void PopulateParams(uintptr_t /*sf*/) noexcept { }

    template<typename T, typename... TArgs>
    static void PopulateParams(uintptr_t sf, T arg, TArgs... more) noexcept
    {
        *reinterpret_cast<T*>(sf) = arg;
        PopulateParams(sf + 8, more...);
    }

    void* m_fnPtr;
    uint32_t m_stackframeSize;
    bool m_isNoExcept;
};

void DebugInterpCallFunctionPointerFromCppImpl(AstFunction* fn, uintptr_t paramsAndRet, size_t numArgs);

class GeneratedFunctionPointerImpl
{
public:
    enum class Kind
    {
        LLVM_MODE,
        FAST_INTERP_MODE,
        DEBUG_INTERP_MODE
    };

    GeneratedFunctionPointerImpl(uintptr_t control) noexcept
        : m_control(control)
    { }

    template<bool isNoExcept, typename R, typename... Args>
    R Call(Args... args) const noexcept(isNoExcept)
    {
        if (GetType() == 0)
        {
            // LLVM mode, this is just the function pointer, call directly
            //
            if constexpr(isNoExcept)
            {
                return reinterpret_cast<R(*)(Args...) noexcept>(m_control)(args...);
            }
            else
            {
                return reinterpret_cast<R(*)(Args...)>(m_control)(args...);
            }
        }
        else if (GetType() == 1)
        {
            // FastInterp mode
            //
            uint32_t sfSize = FIStackframeSizeCategoryHelper::GetSize(
                        static_cast<FIStackframeSizeCategory>(GetStackFrameCategory()));

            if constexpr(isNoExcept)
            {
                TestAssert(IsNoExcept());
                return FastInterpFunction<R(*)(Args...) noexcept>(
                            reinterpret_cast<void*>(GetPointer()), sfSize)(args...);
            }
            else
            {
                return FastInterpFunction<R(*)(Args...)>(
                            reinterpret_cast<void*>(GetPointer()), sfSize, IsNoExcept())(args...);
            }
        }
        else
        {
            TestAssert(GetType() == 2);
            AstFunction* fn = reinterpret_cast<AstFunction*>(GetPointer());
            uintptr_t sf = reinterpret_cast<uintptr_t>(alloca(8 * (sizeof...(Args) + 1)));
            PopulateParams(sf + 8, args...);
            DebugInterpCallFunctionPointerFromCppImpl(fn, sf, sizeof...(Args));
            if constexpr(!std::is_same<R, void>::value)
            {
                return *reinterpret_cast<R*>(sf);
            }
        }
    }

    static uintptr_t GetControlValueForFastInterpFn(AstFunction* fn, uintptr_t generatedFnAddress);
    static uintptr_t GetControlValueForDebugInterpFn(AstFunction* fn);

private:
    static void PopulateParams(uintptr_t /*sf*/) noexcept { }

    template<typename T, typename... TArgs>
    static void PopulateParams(uintptr_t sf, T arg, TArgs... more) noexcept
    {
        *reinterpret_cast<T*>(sf) = arg;
        PopulateParams(sf + 8, more...);
    }

    static_assert(FIStackframeSizeCategoryHelper::x_num_categories < (1 << 13));

    // 2 bit: m_type
    // 0 = LLVM Mode function pointer
    // 1 = FastInterp Mode function pointer
    // 2 = DebugInterp Mode function pointer
    //
    // 1 bit: m_isNoExcept
    // If FastInterp Mode, whether the function is noexcept
    // 0 if not FastInterp Mode
    //
    // 13 bits: m_fiStackframeCategory
    // If FastInterp Mode, the stack frame size category
    // 0 if not FastInterp Mode
    //
    // 48 bits: the pointer
    //
    uint64_t m_control;

    uint64_t GetType() const { return m_control >> 62; }
    bool IsNoExcept() const { TestAssert(GetType() == 1); return ((m_control >> 61) & 1); }
    uint64_t GetStackFrameCategory() const { TestAssert(GetType() == 1); return (m_control >> 48) & ((1ULL << 13) - 1); }
    uint64_t GetPointer() const { return m_control & ((1ULL << 48) - 1); }
};
static_assert(sizeof(GeneratedFunctionPointerImpl) == 8);

template<typename T>
class GeneratedFunctionPointer;

template<typename R, typename... Args>
class GeneratedFunctionPointer<R(*)(Args...) noexcept>
{
public:
    GeneratedFunctionPointer(uintptr_t control) noexcept
        : m_control(control)
    { }

    R operator()(Args... args) noexcept
    {
        return GeneratedFunctionPointerImpl(m_control).Call<true /*isNoExcept*/, R, Args...>(args...);
    }

private:
    uint64_t m_control;
};

template<typename R, typename... Args>
class GeneratedFunctionPointer<R(*)(Args...)>
{
public:
    GeneratedFunctionPointer(uintptr_t control) noexcept
        : m_control(control)
    { }

    R operator()(Args... args)
    {
        return GeneratedFunctionPointerImpl(m_control).Call<false /*isNoExcept*/, R, Args...>(args...);
    }

private:
    uint64_t m_control;
};

}   //namespace PochiVM
