#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_function_alignment.h"
#include "fastinterp_tpl_stackframe_category.h"
#include "fastinterp_tpl_return_type.h"

namespace PochiVM
{

// Call a generated function
//
struct FICallExprImpl
{
    template<typename ReturnType>
    static constexpr bool cond()
    {
        if (std::is_pointer<ReturnType>::value && !std::is_same<ReturnType, void*>::value) { return false; }
        return true;
    }

    template<typename ReturnType,
             bool spillReturnValue>
    static constexpr bool cond()
    {
        if (std::is_same<void, ReturnType>::value && spillReturnValue) { return false; }
        return true;
    }

    template<typename ReturnType,
             bool spillReturnValue,
             bool isCalleeNoExcept,
             FIStackframeSizeCategory stackframeSizeCategoryEnum>
    static constexpr bool cond()
    {
        return true;
    }

    template<typename T>
    using WorkaroundVoidType = typename std::conditional<std::is_same<T, void>::value, void*, T>::type;

    // Unlike most of the other operators, this operator allows no OpaqueParams.
    // GHC has no callee-saved registers, all registers are invalidated after a call.
    // Therefore, it is always a waste to have register-pinned opaque parameters:
    // they must be pushed to stack and then popped in order to be passed to the continuation,
    // so it is cheaper to have spilled them to memory at the very beginning.
    //
    // Placeholder rules:
    // boilerplate placeholder 1: call expression
    // constant placeholder 0: spill location, if spillReturnValue
    //
    template<typename ReturnType,
             bool spillReturnValue,
             bool isCalleeNoExcept,
             FIStackframeSizeCategory stackframeSizeCategoryEnum>
    static void f(uintptr_t stackframe) noexcept
    {
        constexpr int newStackframeSize = FIStackframeSizeCategoryHelper::internal_get_stackframe_size(
                    static_cast<int>(stackframeSizeCategoryEnum));
        alignas(x_fastinterp_function_stack_alignment) uint8_t newStackframe[newStackframeSize];

        [[maybe_unused]] WorkaroundVoidType<FIReturnType<ReturnType, isCalleeNoExcept>> returnValue;

        if constexpr(std::is_same<FIReturnType<ReturnType, isCalleeNoExcept>, void>::value)
        {
            // "noescape" is required, otherwise the compiler may assume that "newStackframe" could escape the function,
            // preventing tail call optimization on our continuation.
            //
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_1_NO_TAILCALL(
                        FIReturnType<ReturnType, isCalleeNoExcept>(*)(uintptr_t, __attribute__((__noescape__)) uint8_t*) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_1(stackframe, newStackframe);
        }
        else
        {
            // "noescape" is required, otherwise the compiler may assume that "newStackframe" could escape the function,
            // preventing tail call optimization on our continuation.
            //
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_1_NO_TAILCALL(
                        FIReturnType<ReturnType, isCalleeNoExcept>(*)(uintptr_t, __attribute__((__noescape__)) uint8_t*) noexcept);
            returnValue = BOILERPLATE_FNPTR_PLACEHOLDER_1(stackframe, newStackframe);
        }

        if constexpr(std::is_same<ReturnType, void>::value)
        {
            if constexpr(isCalleeNoExcept)
            {
                DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t) noexcept);
                BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe);
            }
            else
            {
                DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, uint64_t) noexcept);
                BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, FIReturnValueHelper::HasException<ReturnType>(returnValue));
            }
        }
        else if constexpr(spillReturnValue)
        {
            DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
            *GetLocalVarAddress<ReturnType>(stackframe, CONSTANT_PLACEHOLDER_0) =
                    FIReturnValueHelper::GetReturnValue<ReturnType, isCalleeNoExcept>(returnValue);

            if constexpr(isCalleeNoExcept)
            {
                DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t) noexcept);
                BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe);
            }
            else
            {
                DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, uint64_t) noexcept);
                BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, FIReturnValueHelper::HasException<ReturnType>(returnValue));
            }
        }
        else
        {
            ReturnType ret = FIReturnValueHelper::GetReturnValue<ReturnType, isCalleeNoExcept>(returnValue);
            if constexpr(isCalleeNoExcept)
            {
                DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, ReturnType) noexcept);
                BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, ret);
            }
            else
            {
                DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, ReturnType, uint64_t) noexcept);
                BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, ret, FIReturnValueHelper::HasException<ReturnType>(returnValue));
            }
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("returnType"),
                    CreateBoolMetaVar("spillReturnValue"),
                    CreateBoolMetaVar("isCalleeNoExcept"),
                    CreateEnumMetaVar<FIStackframeSizeCategory::X_END_OF_ENUM>("stackframeSizeCategory")
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
    RegisterBoilerplate<FICallExprImpl>();
}
