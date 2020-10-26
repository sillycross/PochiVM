#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP

#include "fastinterp_tpl_common.hpp"
#include "fastinterp_tpl_operandshape.hpp"
#include "fastinterp_tpl_return_type.h"

namespace PochiVM
{

struct FIInlinedReturnImpl
{
    template<typename ReturnType>
    static constexpr bool cond()
    {
        if (std::is_same<ReturnType, void>::value) { return false; }
        if (std::is_pointer<ReturnType>::value && !std::is_same<ReturnType, void*>::value) { return false; }
        return true;
    }

    template<typename ReturnType,
             typename IndexType,
             bool isNoExcept,
             FIOperandShapeCategory osc>
    static constexpr bool cond()
    {
        if constexpr(!std::is_same<ReturnType, void>::value)
        {
            if (!FIOperandShapeCategoryHelper::cond<ReturnType, IndexType, osc>()) { return false; }
        }
        return true;
    }

    template<typename ReturnType,
             typename IndexType,
             bool isNoExcept,
             FIOperandShapeCategory osc>
    static FIReturnType<ReturnType, isNoExcept> f(uintptr_t stackframe) noexcept
    {
        ReturnType ret = FIOperandShapeCategoryHelper::get_0_1<ReturnType, IndexType, osc>(stackframe);
        return FIReturnValueHelper::GetForRet<ReturnType, isNoExcept>(ret);
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateTypeMetaVar("returnType"),
                    CreateTypeMetaVar("indexType"),
                    CreateBoolMetaVar("isNoExcept"),
                    CreateEnumMetaVar<FIOperandShapeCategory::X_END_OF_ENUM>("osc")
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
    RegisterBoilerplate<FIInlinedReturnImpl>(FIAttribute::NoContinuation);
}
