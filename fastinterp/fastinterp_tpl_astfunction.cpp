#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_astfunction.h"
#include "fastinterp_tpl_common.hpp"
#include "pochivm/interp_control_signal.h"

namespace PochiVM
{

// Must always inline for correctness, same reason as GetLocalVarAddress
//
template<FIFunctionNumStatements numStmtsEnum,
         FIFunctionStmtsMayReturnMask mayReturnMaskEnum>
static void __attribute__((__always_inline__)) FIFunctionBodyImplInternal() noexcept
{
    constexpr int numStmts = static_cast<int>(numStmtsEnum);
    constexpr int mayReturnMask = static_cast<int>(mayReturnMaskEnum);

#define EXECUTE_STMT(stmtOrd, placeholderOrd)                                                                      \
    if constexpr(numStmts > (stmtOrd))                                                                             \
    {                                                                                                              \
        if constexpr((mayReturnMask & (1 << (stmtOrd))) != 0)                                                      \
        {                                                                                                          \
            INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholderOrd, InterpControlSignal(*)() noexcept);      \
            if (BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholderOrd() != InterpControlSignal::None)                   \
            {                                                                                                      \
                return;                                                                                            \
            }                                                                                                      \
        }                                                                                                          \
        else                                                                                                       \
        {                                                                                                          \
            INTERNAL_DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER(placeholderOrd, void(*)() noexcept);                     \
            BOILERPLATE_FNPTR_PLACEHOLDER_ ## placeholderOrd();                                                    \
        }                                                                                                          \
    }

    EXECUTE_STMT(0, 0)
    EXECUTE_STMT(1, 1)
    EXECUTE_STMT(2, 2)
    EXECUTE_STMT(3, 3)
    EXECUTE_STMT(4, 4)
    EXECUTE_STMT(5, 5)
    EXECUTE_STMT(6, 6)
    EXECUTE_STMT(7, 7)
    EXECUTE_STMT(8, 8)
    EXECUTE_STMT(9, 9)
    static_assert(static_cast<int>(FIFunctionNumStatements::X_END_OF_ENUM) == 10 + 1);

#undef EXECUTE_STMT
}

// A generated function
//
struct FIFunctionImpl
{
    template<bool isNoExcept,
             FIFunctionNumStatements numStmtsEnum,
             FIFunctionStmtsMayReturnMask mayReturnMaskEnum>
    static constexpr bool cond()
    {
        constexpr int numStmts = static_cast<int>(numStmtsEnum);
        constexpr int mayReturnMask = static_cast<int>(mayReturnMaskEnum);
        if (mayReturnMask >= (1 << numStmts)) { return false; }
        return true;
    }

    // Prototype is void(*)() if noexcept, or bool(*)() if may throw (and returns true if it throws)
    // BoilerplateFn placeholder 0 - n: the function stmts
    //
    template<bool isNoExcept,
             FIFunctionNumStatements numStmtsEnum,
             FIFunctionStmtsMayReturnMask mayReturnMaskEnum>
    static typename std::conditional<isNoExcept, void, bool>::type f() noexcept
    {
        if constexpr(isNoExcept)
        {
            FIFunctionBodyImplInternal<numStmtsEnum, mayReturnMaskEnum>();
        }
        else
        {
            void** oldEhTarget = __pochivm_thread_fastinterp_context.m_ehTarget;
            builtin_sjlj_env_t ehTarget;
            if (__builtin_setjmp(ehTarget) == 0)
            {
                __pochivm_thread_fastinterp_context.m_ehTarget = ehTarget;
                FIFunctionBodyImplInternal<numStmtsEnum, mayReturnMaskEnum>();
                __pochivm_thread_fastinterp_context.m_ehTarget = oldEhTarget;
                return false;
            }
            else
            {
                // A C++ exception is thrown out.
                // At this point our soft emulator has called the destructor sequence,
                // and saved the exception as a std::exception_ptr to thread context
                //
                __pochivm_thread_fastinterp_context.m_ehTarget = oldEhTarget;
                return true;
            }
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateBoolMetaVar("isNoExcept"),
                    CreateEnumMetaVar<FIFunctionNumStatements::X_END_OF_ENUM>("numStmts"),
                    CreateEnumMetaVar<FIFunctionStmtsMayReturnMask::X_END_OF_ENUM>("mayReturnMask")
        );
    }
};

}   // namespace PochiVM

// build_fast_interp_lib.cpp JIT entry point
//
extern "C"
void __pochivm_build_fast_interp_library__()
{
    using namespace PochiVM;
    RegisterBoilerplate<FIFunctionImpl>();
}
