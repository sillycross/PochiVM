#pragma once

#include "pochivm/common.h"
#include "pochivm/constexpr_array_concat_helper.h"
#include "fastinterp_function_alignment.h"

namespace PochiVM
{

// It seems like LLVM has a codegen bug when we use GHC convention and variable-sized alloca.
// So here we simply pick a list of exponentially-growing stack frame size category,
// and up-align a stack frame size to the nearest category.
// This workarounds the bug with variable-sized alloca, and also improves performance.
//

namespace FIStackframeSizeCategoryHelper
{
    // The growth factor is chosen to be 1.1
    //
    constexpr int x_numerator = 11;
    constexpr int x_denominator = 10;

    // Maximum supported stack frame size (for a single function): 2101231792 bytes
    //
    constexpr int x_num_categories = 178;

    constexpr int internal_get_stackframe_size(int num)
    {
        TestAssert(0 <= num && num < x_num_categories);
        int64_t result = 16;
        for (int i = 0; i < num; i++)
        {
            result = result * x_numerator / x_denominator;
            result = (result + x_fastinterp_function_stack_alignment - 1) /
                    x_fastinterp_function_stack_alignment * x_fastinterp_function_stack_alignment;
        }
        TestAssert(result <= 0x7fffffff);
        return static_cast<int>(result);
    }

    template<int n>
    struct compute_array_helper
    {
        static constexpr std::array<int, n> get()
        {
            if constexpr(n == 0)
            {
                return std::array<int, 0>{};
            }
            else
            {
                return AstTypeHelper::constexpr_std_array_concat(
                            compute_array_helper<n-1>::get(), std::array<int, 1>{ internal_get_stackframe_size(n - 1) });
            }
        }
    };

    constexpr std::array<int, x_num_categories> x_size_list = compute_array_helper<x_num_categories>::get();
    static_assert(x_size_list[x_num_categories - 1] == 2101231792);

}   // namespace FIStackframeSizeCategoryHelper

enum class FIStackframeSizeCategory
{
    X_END_OF_ENUM = FIStackframeSizeCategoryHelper::x_num_categories
};

enum class FICallExprParamOrd
{
    FIRST_NON_INLINE_PARAM_ORD = 100,
    X_END_OF_ENUM
};

namespace FIStackframeSizeCategoryHelper
{
    constexpr FIStackframeSizeCategory SelectCategory(size_t neededSize)
    {
        ReleaseAssert(neededSize <= x_size_list[x_num_categories - 1] && "generated function local stackframe too large (>1.95GB)");
        int l = 0, r = x_num_categories - 1;
        while (l != r)
        {
            int mid = (l + r) / 2;
            if (x_size_list[static_cast<size_t>(mid)] >= static_cast<int>(neededSize))
            {
                r = mid;
            }
            else
            {
                l = mid + 1;
            }
        }
        assert(static_cast<size_t>(x_size_list[static_cast<size_t>(r)]) >= neededSize);
        assert(r == 0 || static_cast<size_t>(x_size_list[static_cast<size_t>(r) - 1]) < neededSize);
        return static_cast<FIStackframeSizeCategory>(r);
    }

    inline uint32_t GetSize(FIStackframeSizeCategory category)
    {
        TestAssert(static_cast<int>(category) < x_num_categories);
        return static_cast<uint32_t>(x_size_list[static_cast<size_t>(category)]);
    }

}   // namespace FIStackframeSizeCategoryHelper
}   // namespace PochiVM
