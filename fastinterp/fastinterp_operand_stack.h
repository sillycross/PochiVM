#pragma once

#include "pochivm/common.h"
#include "simple_constexpr_power_helper.h"
#include "fastinterp_tpl_opaque_params.h"
#include "pochivm/ast_type_helper.h"
#include "fastinterp_spill_location.h"

namespace PochiVM
{

struct FITempOperand
{
    FITempOperand(uint32_t operandSize)
        : m_operandSize(operandSize), m_spillLoc()
    {
        TestAssert(m_operandSize <= 8 && math::is_power_of_2(static_cast<int>(operandSize)));
    }

    uint32_t m_operandSize;
    FISpillLocation m_spillLoc;
};

class FIStackFramePlanner
{
public:
    FIStackFramePlanner()
    {
        Reset(8);
    }

    void Reset(uint32_t startOffset)
    {
        TestAssert(startOffset % 8 == 0);
        m_baseOffset = startOffset;
        m_topOffset = startOffset;
        m_maxOffsetEver = startOffset;
        m_freeList.clear();
        m_oldAddress.clear();
    }

    // Get a 8-byte temporary space
    //
    uint32_t GetTemp()
    {
        if (!m_freeList.empty())
        {
            uint32_t result = m_freeList.back();
            m_freeList.pop_back();
            return result;
        }
        else
        {
            TestAssert(m_topOffset % 8 == 0);
            uint32_t result = m_topOffset;
            m_topOffset += 8;
            UpdateMaxOffsetEver();
            return result;
        }
    }

    void FreeTemp(uint32_t offset)
    {
        TestAssert(offset % 8 == 0 && m_baseOffset <= offset && offset < m_topOffset);
        m_freeList.push_back(offset);
    }

    bool IsNoTempInUse() const
    {
        uint32_t bottom = UpAlign(m_baseOffset, 8);
        TestAssert(bottom <= m_topOffset);
        TestAssert((m_topOffset - bottom) % 8 == 0);
        size_t count = (m_topOffset - bottom) / 8;
        TestAssert(m_freeList.size() <= count);
#ifdef TESTBUILD
        // Just to sanity check that the elements in freeList are distinct
        //
        std::vector<bool> m_used;
        m_used.resize(count, false);
        for (size_t i = 0; i < m_freeList.size(); i++)
        {
            TestAssert(bottom <= m_freeList[i] && m_freeList[i] < m_topOffset);
            TestAssert((m_freeList[i] - bottom) % 8 == 0);
            uint32_t offset = (m_freeList[i] - bottom) / 8;
            TestAssert(0 <= offset && offset < count && !m_used[offset]);
            m_used[offset] = true;
        }
#endif
        return count == m_freeList.size();
    }

    uint32_t GetLocalVar(uint32_t size, uint32_t alignment)
    {
        TestAssert(IsNoTempInUse());
        m_oldAddress.push_back(m_baseOffset);
        m_baseOffset = UpAlign(m_baseOffset, alignment);
        uint32_t result = m_baseOffset;
        m_baseOffset += size;
        m_topOffset = UpAlign(m_baseOffset, 8);
        m_freeList.clear();
        UpdateMaxOffsetEver();
        return result;
    }

    void PopLocalVar()
    {
        TestAssert(IsNoTempInUse());
        TestAssert(!m_oldAddress.empty());
        m_baseOffset = m_oldAddress.back();
        m_oldAddress.pop_back();
        m_topOffset = UpAlign(m_baseOffset, 8);
        m_freeList.clear();
    }

    uint32_t GetFinalStackFrameSize() const
    {
        return m_maxOffsetEver;
    }

    static uint32_t UpAlign(uint32_t value, uint32_t alignment)
    {
        return (value + alignment - 1) / alignment * alignment;
    }

private:
    void UpdateMaxOffsetEver()
    {
        m_maxOffsetEver = std::max(m_maxOffsetEver, m_topOffset);
    }

    uint32_t m_baseOffset;
    uint32_t m_topOffset;
    uint32_t m_maxOffsetEver;
    std::vector<uint32_t> m_freeList;
    std::vector<uint32_t> m_oldAddress;
};

class FITempOperandStack
{
public:
    FITempOperandStack(FIStackFramePlanner* sfPlanner, size_t nospillLimit)
        : m_sfPlanner(sfPlanner), m_nospillLimit(nospillLimit)
    {
        Reset();
    }

    void Reset()
    {
        m_stack.clear();
        m_firstNoSpill = 0;
    }

    bool IsEmpty() const { return m_stack.empty(); }

    void Push(uint32_t operandSize, bool spill)
    {
        TestAssertImp(spill && m_stack.size() > 0, !m_stack.back().m_spillLoc.IsNoSpill());
        m_stack.push_back(FITempOperand(operandSize));
        if (spill)
        {
            SpillUpTo(m_stack.size());
        }
        else if (m_stack.size() > m_nospillLimit)
        {
            SpillUpTo(m_stack.size() - m_nospillLimit);
        }
    }

    FISpillLocation Peek() const
    {
        TestAssert(!IsEmpty());
        return m_stack.back().m_spillLoc;
    }

    FISpillLocation Pop()
    {
        TestAssert(!IsEmpty());
        FISpillLocation spillLoc = m_stack.back().m_spillLoc;
        m_stack.pop_back();
        if (m_firstNoSpill > m_stack.size()) { m_firstNoSpill--; assert(m_firstNoSpill == m_stack.size()); }
        if (!spillLoc.IsNoSpill())
        {
            m_sfPlanner->FreeTemp(static_cast<uint32_t>(spillLoc.GetSpillLocation()));
        }
        return spillLoc;
    }

    void ForceSpillAll()
    {
        SpillUpTo(m_stack.size());
        TestAssert(m_firstNoSpill == m_stack.size());
    }

    size_t GetNumNoSpill() const
    {
        TestAssert(m_firstNoSpill <= m_stack.size());
        size_t result = m_stack.size() - m_firstNoSpill;
        TestAssert(result <= m_nospillLimit);
        return result;
    }

private:
    void SpillUpTo(size_t bound)
    {
        while (m_firstNoSpill < bound)
        {
            m_stack[m_firstNoSpill].m_spillLoc.SetSpillLocation(m_sfPlanner->GetTemp());
            m_firstNoSpill++;
        }
    }

    FIStackFramePlanner* m_sfPlanner;
    std::vector<FITempOperand> m_stack;
    size_t m_firstNoSpill;
    size_t m_nospillLimit;
};

class FIStackFrameManager
{
public:
    FIStackFrameManager()
        : m_planner()
        , m_integralOperandStack(&m_planner, x_fastinterp_max_integral_params)
        , m_floatOperandStack(&m_planner, x_fastinterp_max_floating_point_params)
    { }

    void Reset(uint32_t startOffset)
    {
        m_planner.Reset(startOffset);
        m_integralOperandStack.Reset();
        m_floatOperandStack.Reset();
#ifdef TESTBUILD
        m_tempStack.clear();
        m_localVarStack.clear();
#endif
    }

    void PushTemp(TypeId typeId, bool spill = false)
    {
        TestAssert(!typeId.IsCppClassType() && !typeId.IsVoid());
#ifdef TESTBUILD
        m_tempStack.push_back(typeId);
#endif
        if (typeId.IsFloatingPoint())
        {
            m_floatOperandStack.Push(static_cast<uint32_t>(typeId.Size()), spill);
        }
        else
        {
            m_integralOperandStack.Push(static_cast<uint32_t>(typeId.Size()), spill);
        }
    }

    FISpillLocation WARN_UNUSED PopTemp(TypeId typeId)
    {
        TestAssert(m_tempStack.size() > 0 && m_tempStack.back() == typeId);
#ifdef TESTBUILD
        m_tempStack.pop_back();
#endif
        if (typeId.IsFloatingPoint())
        {
            return m_floatOperandStack.Pop();
        }
        else
        {
            return m_integralOperandStack.Pop();
        }
    }

    FISpillLocation WARN_UNUSED PeekTopTemp(TypeId typeId) const
    {
        TestAssert(m_tempStack.size() > 0 && m_tempStack.back() == typeId);
        if (typeId.IsFloatingPoint())
        {
            return m_floatOperandStack.Peek();
        }
        else
        {
            return m_integralOperandStack.Peek();
        }
    }

    bool WARN_UNUSED CanReserveWithoutSpill(TypeId typeId) const
    {
        if (typeId.IsFloatingPoint())
        {
            return m_floatOperandStack.GetNumNoSpill() < x_fastinterp_max_floating_point_params;
        }
        else
        {
            return m_integralOperandStack.GetNumNoSpill() < x_fastinterp_max_integral_params;
        }
    }

    void ReserveTemp(TypeId typeId)
    {
        PushTemp(typeId);
        FISpillLocation result = PopTemp(typeId);
        TestAssert(result.IsNoSpill());
        std::ignore = result;
    }

    uint64_t WARN_UNUSED PushLocalVar(TypeId typeId)
    {
        TestAssert(!typeId.IsVoid());
        TestAssert(m_tempStack.size() == 0);
#ifdef TESTBUILD
        m_localVarStack.push_back(typeId);
#endif
        // TODO: support custom CPP class alignment
        //
        uint32_t alignment = typeId.IsCppClassType() ? 8 : static_cast<uint32_t>(typeId.Size());
        return m_planner.GetLocalVar(static_cast<uint32_t>(typeId.Size()), alignment);
    }

    void PopLocalVar([[maybe_unused]] TypeId typeId)
    {
        TestAssert(m_localVarStack.size() > 0 && m_localVarStack.back() == typeId);
        TestAssert(m_tempStack.size() == 0);
#ifdef TESTBUILD
        m_localVarStack.pop_back();
#endif
        m_planner.PopLocalVar();
    }

    void ForceSpillAll()
    {
        m_integralOperandStack.ForceSpillAll();
        m_floatOperandStack.ForceSpillAll();
    }

    FINumOpaqueIntegralParams GetNumNoSpillIntegral() const
    {
        return static_cast<FINumOpaqueIntegralParams>(m_integralOperandStack.GetNumNoSpill());
    }

    FINumOpaqueFloatingParams GetNumNoSpillFloat() const
    {
        return static_cast<FINumOpaqueFloatingParams>(m_floatOperandStack.GetNumNoSpill());
    }

    void AssertEmpty() const
    {
        TestAssert(m_localVarStack.size() == 0 && m_tempStack.size() == 0);
    }

    void AssertNoTemp() const
    {
        TestAssert(m_tempStack.size() == 0);
    }

    uint32_t GetFinalStackFrameSize() const
    {
        TestAssert(m_localVarStack.size() == 0 && m_tempStack.size() == 0);
        return m_planner.GetFinalStackFrameSize();
    }

private:
    FIStackFramePlanner m_planner;
    FITempOperandStack m_integralOperandStack;
    FITempOperandStack m_floatOperandStack;
#ifdef TESTBUILD
    std::vector<TypeId> m_tempStack;
    std::vector<TypeId> m_localVarStack;
#endif
};

}   // namespace PochiVM
