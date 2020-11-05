#pragma once

#include "fastinterp_helper.h"
#include "generated/fastinterp_library.generated.h"
#include "x86_64_asm_helper.h"
#include "x86_64_populate_nop_instruction_helper.h"
#include "pochivm/codegen_arena_allocator.h"

namespace PochiVM
{

class FastInterpBoilerplateInstance : NonCopyable, NonMovable
{
public:
    friend class FastInterpCodegenEngine;

    void PopulateBoilerplateFnPtrPlaceholder(uint32_t ordinal, FastInterpBoilerplateInstance* value)
    {
        TestAssert(ordinal < m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal && value != nullptr);
        TestAssert(!(m_populatedBoilerplateFnPtrPlaceholderMask & (1ULL << ordinal)));
        TestAssert(m_owner->m_usedBoilerplateFnPtrPlaceholderMask & (1ULL << ordinal));
#ifdef TESTBUILD
        m_populatedBoilerplateFnPtrPlaceholderMask |= (1ULL << ordinal);
#endif
        m_fixupValues[ordinal] = reinterpret_cast<uint64_t>(value);

        if (ordinal == static_cast<uint32_t>(m_owner->m_lastInstructionTailCallOrd))
        {
            TestAssert(m_litcInstanceOrd == static_cast<uint32_t>(-1));
            m_litcInstanceOrd = value->m_ordinalInArray;
            value->m_isContinuationOfAnotherInstance = true;
        }
    }

    void PopulateCppFnPtrPlaceholder(uint32_t ordinal, void* value)
    {
        // Not allowed to hold null pointer
        //
        TestAssert(value != nullptr);
        TestAssert(ordinal < m_owner->m_highestCppFnptrPlaceholderOrdinal);
        TestAssert(!(m_populatedCppFnptrPlaceholderMask & (1ULL << ordinal)));
        TestAssert(m_owner->m_usedCppFnptrPlaceholderMask & (1ULL << ordinal));
#ifdef TESTBUILD
        m_populatedCppFnptrPlaceholderMask |= (1ULL << ordinal);
#endif
        m_fixupValues[m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal + ordinal] = reinterpret_cast<uint64_t>(value);
    }

    template<typename R, typename... Args>
    void PopulateCppFnPtrPlaceholder(uint32_t ordinal, R(*value)(Args...) noexcept)
    {
        PopulateCppFnPtrPlaceholder(ordinal, reinterpret_cast<void*>(value));
    }

    // The constant type must match what is defined in the boilerplate
    //
    template<typename T>
    void PopulateConstantPlaceholder(uint32_t ordinal, T value, [[maybe_unused]] bool silenceZeroWarning = false)
    {
        static_assert(sizeof(T) <= 8 && (std::is_fundamental<T>::value || std::is_pointer<T>::value) &&
                (!std::is_function<typename std::remove_pointer<T>::type>::value), "must be a primitive type");
        // If T is 8-byte long, it is not allowed to pass in 0
        //
        TestAssertImp(!silenceZeroWarning && sizeof(T) == 8, !is_all_underlying_bits_zero(value));
        // It is possible that we populate a constant placeholder that does not exist in boilerplate:
        // the compiler might have optimized it out. For example, comparison of literal >= 0 of unsigned type,
        // which is trivially true so the compiler may have optimized out the constant 'literal'.
        // This should only happen to constant: functions may have side effects so compiler will never optimize it out,
        // and currently we never do function calls inside a condition control flow that may be optimized out.
        //
        TestAssert(ordinal < 64);
        TestAssert(!(m_populatedUInt64PlaceholderMask & (1ULL << ordinal)));
#ifdef TESTBUILD
        m_populatedUInt64PlaceholderMask |= (1ULL << ordinal);
#endif
        // We just allocated enough memory for that many ordinals.
        // If it is a ordinal that has been optimized out by compiler, don't write.
        //
        if (ordinal < m_owner->m_highestUInt64PlaceholderOrdinal)
        {
            union U {
                uint64_t u64v;
                T tv;
            };
            U u;
            u.u64v = 0;
            u.tv = value;
            m_fixupValues[m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal +
                    m_owner->m_highestCppFnptrPlaceholderOrdinal + ordinal] = u.u64v;
        }
    }

    void ReplaceBluePrint(const FastInterpBoilerplateBluePrint* newBluePrint)
    {
        TestAssert(m_owner->m_highestUInt64PlaceholderOrdinal == newBluePrint->m_highestUInt64PlaceholderOrdinal);
        TestAssert(m_owner->m_highestCppFnptrPlaceholderOrdinal == newBluePrint->m_highestCppFnptrPlaceholderOrdinal);
        TestAssert(m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal == newBluePrint->m_highestBoilerplateFnptrPlaceholderOrdinal);
        TestAssert(m_owner->m_usedBoilerplateFnPtrPlaceholderMask == newBluePrint->m_usedBoilerplateFnPtrPlaceholderMask);
        TestAssert(m_owner->m_usedCppFnptrPlaceholderMask == newBluePrint->m_usedCppFnptrPlaceholderMask);
        TestAssert(m_owner->m_usedUInt64PlaceholderMask == newBluePrint->m_usedUInt64PlaceholderMask);
        m_owner = newBluePrint;
    }

    void SetAlignmentLog2(uint16_t log2CodeSectionAlignment)
    {
        TestAssert(log2CodeSectionAlignment <= 6);
        m_log2CodeSectionAlignment = log2CodeSectionAlignment;
    }

private:
    FastInterpBoilerplateInstance(TempArenaAllocator& alloc,
                                  const FastInterpBoilerplateBluePrint* owner,
                                  uint32_t ordinalInArray,
                                  uint16_t log2CodeSectionAlignment)
        : m_owner(owner)
        , m_log2CodeSectionAlignment(log2CodeSectionAlignment)
        , m_codeSectionPaddingRequired(0)
        , m_ordinalInArray(ordinalInArray)
        , m_litcInstanceOrd(static_cast<uint32_t>(-1))
        , m_isContinuationOfAnotherInstance(false)
        , m_populatedRelativeCodeAddress(false)
        , m_shouldStripLITC(false)
#ifdef TESTBUILD
        , m_populatedBoilerplateFnPtrPlaceholderMask(0)
        , m_populatedUInt64PlaceholderMask(0)
        , m_populatedCppFnptrPlaceholderMask(0)
        , m_isInstantiated(false)
#endif
    {
        TestAssert(m_log2CodeSectionAlignment >= x_fastinterp_log2_function_alignment && m_log2CodeSectionAlignment <= 6);
        m_fixupValues = new (alloc) uint64_t[m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal +
                m_owner->m_highestUInt64PlaceholderOrdinal + m_owner->m_highestCppFnptrPlaceholderOrdinal];
        for (uint32_t i = 0; i < m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal; i++)
        {
            m_fixupValues[i] = 0;
        }
    }

    // Called after PlaceBoilerplate() phase which populates the m_relativeCodeAddr of every boilerplate function
    //
    void Materialize(uintptr_t baseAddress)
    {
        TestAssert(!m_isInstantiated && m_populatedRelativeCodeAddress);
        TestAssert(m_relativeCodeAddr % (1 << m_log2CodeSectionAlignment) == 0);
        TestAssert(m_relativeCodeAddr >= m_codeSectionPaddingRequired && m_codeSectionPaddingRequired < (1U << m_log2CodeSectionAlignment));
        TestAssert(baseAddress % 4096 == 0);
        TestAssert(m_populatedBoilerplateFnPtrPlaceholderMask == m_owner->m_usedBoilerplateFnPtrPlaceholderMask);
        TestAssert((m_populatedUInt64PlaceholderMask & m_owner->m_usedUInt64PlaceholderMask) == m_owner->m_usedUInt64PlaceholderMask);
        TestAssert(m_populatedCppFnptrPlaceholderMask == m_owner->m_usedCppFnptrPlaceholderMask);
        TestAssertImp(m_shouldStripLITC, m_litcInstanceOrd != static_cast<uint32_t>(-1));
#ifdef TESTBUILD
        m_isInstantiated = true;
#endif
        // The 'boilerplate' fixups is storing the pointer to the instance.
        // At this point the instance is already placed (its relative code address is known), so fixup the value
        //
        for (uint32_t i = 0; i < m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal; i++)
        {
            FastInterpBoilerplateInstance* instance = reinterpret_cast<FastInterpBoilerplateInstance*>(m_fixupValues[i]);
            if (instance != nullptr)
            {
                TestAssert(instance->m_populatedRelativeCodeAddress);
                m_fixupValues[i] = baseAddress + static_cast<uint64_t>(static_cast<int64_t>(instance->m_relativeCodeAddr));
            }
        }
        uint8_t* trueCodeBaseAddress = reinterpret_cast<uint8_t*>(baseAddress + static_cast<uint64_t>(static_cast<int64_t>(m_relativeCodeAddr)));
        x86_64_populate_NOP_instructions(trueCodeBaseAddress - m_codeSectionPaddingRequired, m_codeSectionPaddingRequired);
        m_owner->MaterializeCodeSection(trueCodeBaseAddress, m_fixupValues, m_shouldStripLITC);
    }

    const FastInterpBoilerplateBluePrint* m_owner;

    uint16_t m_log2CodeSectionAlignment;
    uint16_t m_codeSectionPaddingRequired;

    // Populated in PlaceBoilerplate() phase
    //
    int32_t m_relativeCodeAddr;

    uint32_t m_ordinalInArray;

    // The "last instruction tail call" continuation of this instance
    // If we choose to instantiate 'litc_instance' right after 'instance',
    // we can remove this tail call and make it a fallthrough.
    //
    uint32_t m_litcInstanceOrd;

    // Whether this instance is the LITC continuation of another instance
    //
    bool m_isContinuationOfAnotherInstance;

    // Whether the relative code address has been populated
    //
    bool m_populatedRelativeCodeAddress;

    // True if we should remove the last-instruction tail call when we populate this function
    //
    bool m_shouldStripLITC;

    // An array of length m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal +
    // m_owner->m_highestUInt64PlaceholderOrdinal + m_owner->m_highestCppFnptrPlaceholderOrdinal
    //
    uint64_t* m_fixupValues;

#ifdef TESTBUILD
    uint64_t m_populatedBoilerplateFnPtrPlaceholderMask;
    uint64_t m_populatedUInt64PlaceholderMask;
    uint64_t m_populatedCppFnptrPlaceholderMask;
    bool m_isInstantiated;
#endif
};

class AstFunction;

// A owning generated program.
// When this class is destructed, the program is gone.
// So it is undefined behavior if this class is destructed while the program is still running.
//
class FastInterpGeneratedProgram : NonCopyable, NonMovable
{
public:
    friend class FastInterpCodegenEngine;

    ~FastInterpGeneratedProgram()
    {
        munmap(m_addr, m_length);
    }

    void* GetGeneratedFunctionAddress(AstFunction* fn)
    {
        auto it = m_fnEntryPoint.find(fn);
        if (it == m_fnEntryPoint.end()) {
            return nullptr;
        } else {
            TestAssert(it->second != nullptr);
            return it->second;
        }
    }

private:
    FastInterpGeneratedProgram(void* addr, size_t length, std::unordered_map<AstFunction*, void*>&& fnEntryPoint)
        : m_addr(addr), m_length(length), m_fnEntryPoint(std::move(fnEntryPoint))
    {
        TestAssert(m_addr != nullptr && reinterpret_cast<uintptr_t>(m_addr) % 4096 == 0 && m_length % 4096 == 0);
    }

    void* m_addr;
    size_t m_length;
    std::unordered_map<AstFunction*, void*> m_fnEntryPoint;
};

class FastInterpCodegenEngine : NonCopyable, NonMovable
{
public:
    FastInterpCodegenEngine()
    {
        Reset();
    }

    void Reset()
    {
        m_functionEntryPoint.clear();
        m_allBoilerplateInstances.clear();
        m_boilerplateFnEntryPointPlaceholders.clear();
        m_fastInterpFnPtrFixList.clear();
        m_boilerplateAlloc.Reset();
#ifdef TESTBUILD
        m_materialized = false;
#endif
    }

    FastInterpBoilerplateInstance* WARN_UNUSED InstantiateBoilerplate(
            const FastInterpBoilerplateBluePrint* boilerplate,
            size_t log2CodeSectionAlignment = x_fastinterp_log2_function_alignment)
    {
        TestAssert(log2CodeSectionAlignment <= 6);
        FastInterpBoilerplateInstance* inst = new (m_boilerplateAlloc) FastInterpBoilerplateInstance(
                    m_boilerplateAlloc,
                    boilerplate,
                    static_cast<uint32_t>(m_allBoilerplateInstances.size()),
                    static_cast<uint16_t>(log2CodeSectionAlignment));
        m_allBoilerplateInstances.push_back(inst);
        return inst;
    }

    void PopulateBoilerplateFnPtrPlaceholderAsFunctionEntryPoint(FastInterpBoilerplateInstance* inst, AstFunction* fn, uint32_t ord)
    {
        m_boilerplateFnEntryPointPlaceholders.push_back(std::make_pair(inst, std::make_pair(fn, ord)));
    }

    void RegisterGeneratedFunctionEntryPoint(AstFunction* fn, FastInterpBoilerplateInstance* inst);

    void TestOnly_RegisterUnitTestFunctionEntryPoint(
            FastInterpTypeId returnType, bool isNoExcept, uintptr_t fakeId, FastInterpBoilerplateInstance* inst);

    FastInterpBoilerplateInstance* WARN_UNUSED GetGeneratedFunctionEntryPoint(AstFunction* fn)
    {
        auto it = m_functionEntryPoint.find(fn);
        if (it == m_functionEntryPoint.end()) {
            return nullptr;
        } else {
            return it->second.first;
        }
    }

    void AppendFnPtrFixList(AstFunction* fn, FastInterpBoilerplateInstance* inst)
    {
        m_fastInterpFnPtrFixList.push_back(std::make_pair(fn, inst));
    }

    // Materialize the generated program. All placeholders must have been populated.
    // If anything failed, return nullptr.
    // After calling this function, this class no longer needs to exist.
    //
    std::unique_ptr<FastInterpGeneratedProgram> WARN_UNUSED Materialize();

private:
    static void InvalidateInstructionCache(const void* addr, size_t len);

    std::vector<std::pair<AstFunction*, FastInterpBoilerplateInstance*>> m_fastInterpFnPtrFixList;
    std::unordered_map<AstFunction*, std::pair<FastInterpBoilerplateInstance*, FastInterpBoilerplateInstance*> > m_functionEntryPoint;
    std::vector<FastInterpBoilerplateInstance*> m_allBoilerplateInstances;
    std::vector<std::pair<FastInterpBoilerplateInstance*, std::pair<AstFunction*, uint32_t>>> m_boilerplateFnEntryPointPlaceholders;
    TempArenaAllocator m_boilerplateAlloc;
#ifdef TESTBUILD
    bool m_materialized;
#endif
};

}   // namespace PochiVM
