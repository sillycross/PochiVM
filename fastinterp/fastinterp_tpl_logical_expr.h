#pragma once

#include "common.h"

namespace PochiVM
{

template<int height>
class FILogicalOperatorTree
{
public:
    static_assert(height > 0);

    constexpr FILogicalOperatorTree() : value(0) {}
    constexpr FILogicalOperatorTree(int value_) : value(value_) {
        TestAssert(0 <= value && value < x_max);
    }

    static constexpr int get_max()
    {
        // '2*': whether has top-level not
        // '1+': if not in the form of 'foo && bar' or 'foo || bar'
        // '2 * size * size': # of possibilities for 'foo && bar' and 'foo || bar'
        //
        constexpr int size = FILogicalOperatorTree<height - 1>::x_max;
        return 2 * (1 + 2 * size * size);
    }

    constexpr static int x_max = get_max();

    static constexpr FILogicalOperatorTree<height> WARN_UNUSED GetSingleton()
    {
        return FILogicalOperatorTree<height> { 0 };
    }

    static constexpr FILogicalOperatorTree<height> WARN_UNUSED GetNot(FILogicalOperatorTree<height> other)
    {
        return FILogicalOperatorTree<height> { other.value ^ 1 };
    }

    static constexpr FILogicalOperatorTree<height> WARN_UNUSED GetAnd(FILogicalOperatorTree<height - 1> lhs,
                                                                      FILogicalOperatorTree<height - 1> rhs)
    {
        int base = 1;
        int encoding = lhs.value * FILogicalOperatorTree<height - 1>::x_max + rhs.value;
        return (base + encoding) * 2;
    }

    static constexpr FILogicalOperatorTree<height> WARN_UNUSED GetOr(FILogicalOperatorTree<height - 1> lhs,
                                                                     FILogicalOperatorTree<height - 1> rhs)
    {
        int base = 1 + FILogicalOperatorTree<height - 1>::x_max * FILogicalOperatorTree<height - 1>::x_max;
        int encoding = lhs.value * FILogicalOperatorTree<height - 1>::x_max + rhs.value;
        return (base + encoding) * 2;
    }

    constexpr bool WARN_UNUSED HasTopLevelNot() const
    {
        return (value & 1) != 0;
    }

    constexpr bool WARN_UNUSED IsSingleton() const
    {
        return value / 2 == 0;
    }

    constexpr bool WARN_UNUSED IsAnd() const
    {
        return 0 < value / 2 && value / 2 <= FILogicalOperatorTree<height - 1>::x_max * FILogicalOperatorTree<height - 1>::x_max;
    }

    constexpr bool WARN_UNUSED IsOr() const
    {
        return value / 2 > FILogicalOperatorTree<height - 1>::x_max * FILogicalOperatorTree<height - 1>::x_max;
    }

    constexpr FILogicalOperatorTree<height - 1> WARN_UNUSED GetLhs() const
    {
        assert(!IsSingleton());
        return ((value / 2 - 1) % (FILogicalOperatorTree<height - 1>::x_max * FILogicalOperatorTree<height - 1>::x_max)) /
                FILogicalOperatorTree<height - 1>::x_max;
    }

    constexpr FILogicalOperatorTree<height - 1> WARN_UNUSED GetRhs() const
    {
        assert(!IsSingleton());
        return ((value / 2 - 1) % (FILogicalOperatorTree<height - 1>::x_max * FILogicalOperatorTree<height - 1>::x_max)) %
                FILogicalOperatorTree<height - 1>::x_max;
    }

    constexpr int WARN_UNUSED GetValue() const { return value; }

private:
    int value;
};

template<>
class FILogicalOperatorTree<0>
{
public:
    constexpr FILogicalOperatorTree() : value(0) {}
    constexpr FILogicalOperatorTree(int value_) : value(value_) {
        TestAssert(0 <= value && value < x_max);
    }

    constexpr static int x_max = 2;

    static constexpr FILogicalOperatorTree<0> WARN_UNUSED GetSingleton()
    {
        return FILogicalOperatorTree<0> { 0 };
    }

    static constexpr FILogicalOperatorTree<0> WARN_UNUSED GetNot(FILogicalOperatorTree<0> other)
    {
        return FILogicalOperatorTree<0> { other.value ^ 1 };
    }

    constexpr bool WARN_UNUSED HasTopLevelNot() const
    {
        return (value & 1) != 0;
    }

    constexpr int WARN_UNUSED GetValue() const { return value; }

private:
    int value;
};

enum class FILogicalOperatorLv2TreeMask
{
    X_END_OF_ENUM = FILogicalOperatorTree<2>::x_max
};

}   // namespace PochiVM
