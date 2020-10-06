#pragma once

#include "fastinterp_helper.h"
#include "generated/fastinterp_library.generated.h"
#include "x86_64_asm_helper.h"

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

    template<typename R, typename... Args>
    void PopulateCppFnPtrPlaceholder(uint32_t ordinal, R(*value)(Args...) noexcept)
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
        m_dataValues[m_owner->m_cppFnPtrPlaceholderOrdinalToId[ordinal]] = reinterpret_cast<uint64_t>(value);
        m_fixupValues[m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal + ordinal] = static_cast<uint64_t>(
                    m_relativeDataAddr + m_owner->m_cppFnPtrPlaceholderOrdinalToId[ordinal] * sizeof(uint64_t));
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

private:
    FastInterpBoilerplateInstance(const FastInterpBoilerplateBluePrint* owner,
                                  uintptr_t relativeDataAddr,
                                  uint32_t ordinalInArray)
        : m_relativeDataAddr(relativeDataAddr)
        , m_owner(owner)
        , m_ordinalInArray(ordinalInArray)
        , m_litcInstanceOrd(static_cast<uint32_t>(-1))
        , m_isContinuationOfAnotherInstance(false)
        , m_populatedRelativeCodeAddress(false)
        , m_shouldStripLITC(false)
        , m_isInstantiated(false)
#ifdef TESTBUILD
        , m_populatedBoilerplateFnPtrPlaceholderMask(0)
        , m_populatedUInt64PlaceholderMask(0)
        , m_populatedCppFnptrPlaceholderMask(0)
#endif
    {
        m_fixupValues = new uint64_t[m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal +
                m_owner->m_highestUInt64PlaceholderOrdinal + m_owner->m_highestCppFnptrPlaceholderOrdinal];
        for (uint32_t i = 0; i < m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal; i++)
        {
            m_fixupValues[i] = 0;
        }
        m_dataValues = new uint64_t[m_owner->GetDataSectionLength() / sizeof(uint64_t)];
    }

    // Called after PlaceBoilerplate() phase which populates the m_relativeCodeAddr of every boilerplate function
    //
    void Materialize(uintptr_t baseAddress)
    {
        TestAssert(!m_isInstantiated && m_populatedRelativeCodeAddress);
        TestAssert(baseAddress % x_fastinterp_function_alignment == 0);
        TestAssert(m_populatedBoilerplateFnPtrPlaceholderMask == m_owner->m_usedBoilerplateFnPtrPlaceholderMask);
        TestAssert((m_populatedUInt64PlaceholderMask & m_owner->m_usedUInt64PlaceholderMask) == m_owner->m_usedUInt64PlaceholderMask);
        TestAssert(m_populatedCppFnptrPlaceholderMask == m_owner->m_usedCppFnptrPlaceholderMask);
        TestAssertImp(m_shouldStripLITC, m_litcInstanceOrd != static_cast<uint32_t>(-1));
        m_isInstantiated = true;
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
        // The 'cpp_fn' fixups are all relative pointers to the final baseAddress
        // Now the base address is known, add it so we get the true pointer
        //
        for (uint32_t i = m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal;
             i < m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal + m_owner->m_highestCppFnptrPlaceholderOrdinal; i++)
        {
            m_fixupValues[i] += baseAddress;
        }
        uint8_t* trueCodeBaseAddress = reinterpret_cast<uint8_t*>(baseAddress + static_cast<uint64_t>(static_cast<int64_t>(m_relativeCodeAddr)));
        uint8_t* trueDataBaseAddress = reinterpret_cast<uint8_t*>(baseAddress + m_relativeDataAddr);
        memcpy(trueDataBaseAddress, m_dataValues, m_owner->GetDataSectionLength());
        m_owner->MaterializeCodeSection(trueCodeBaseAddress, m_fixupValues, m_shouldStripLITC);
    }

    uintptr_t m_relativeDataAddr;

    const FastInterpBoilerplateBluePrint* m_owner;

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

    // Whether this instance has been instantiated
    //
    bool m_isInstantiated;

    // An array of length m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal +
    // m_owner->m_highestUInt64PlaceholderOrdinal + m_owner->m_highestCppFnptrPlaceholderOrdinal
    //
    uint64_t* m_fixupValues;

    // An array of length m_owner->GetDataSectionLength / sizeof(uint64_t)
    //
    uint64_t* m_dataValues;

#ifdef TESTBUILD
    uint64_t m_populatedBoilerplateFnPtrPlaceholderMask;
    uint64_t m_populatedUInt64PlaceholderMask;
    uint64_t m_populatedCppFnptrPlaceholderMask;
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
        : m_dataSectionLength(0)
#ifdef TESTBUILD
        , m_materialized(false)
#endif
    { }

    FastInterpBoilerplateInstance* WARN_UNUSED InstantiateBoilerplate(const FastInterpBoilerplateBluePrint* boilerplate)
    {
        // Data section grows down from base address,
        // its data section range is [-(oldDSLength + thisDsLength), -oldDsLength)
        //
        m_dataSectionLength += boilerplate->GetDataSectionLength();
        uintptr_t relativeDataSectionBase = static_cast<uintptr_t>(-static_cast<ssize_t>(m_dataSectionLength));
        FastInterpBoilerplateInstance* inst = new FastInterpBoilerplateInstance(
                    boilerplate, relativeDataSectionBase, static_cast<uint32_t>(m_allBoilerplateInstances.size()));
        m_allBoilerplateInstances.push_back(inst);
        return inst;
    }

    void RegisterGeneratedFunctionEntryPoint(AstFunction* fn, FastInterpBoilerplateInstance* inst, bool isNoExcept)
    {
        TestAssert(!m_functionEntryPoint.count(fn));
        FastInterpBoilerplateInstance* cdeclWrapper = InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FICdeclInterfaceImpl>::SelectBoilerplateBluePrint(
                        isNoExcept ? TypeId::Get<void>().GetDefaultFastInterpTypeId() : TypeId::Get<bool>().GetDefaultFastInterpTypeId()));
        cdeclWrapper->PopulateBoilerplateFnPtrPlaceholder(0, inst);
        m_functionEntryPoint[fn] = std::make_pair(inst, cdeclWrapper);
    }

    void TestOnly_RegisterUnitTestFunctionEntryPoint(TypeId returnType, uintptr_t fakeId, FastInterpBoilerplateInstance* inst)
    {
        AstFunction* fn = reinterpret_cast<AstFunction*>(fakeId);
        TestAssert(!m_functionEntryPoint.count(fn));
        FastInterpBoilerplateInstance* cdeclWrapper = InstantiateBoilerplate(
                    FastInterpBoilerplateLibrary<FICdeclInterfaceImpl>::SelectBoilerplateBluePrint(
                        returnType.GetDefaultFastInterpTypeId()));
        cdeclWrapper->PopulateBoilerplateFnPtrPlaceholder(0, inst);
        m_functionEntryPoint[fn] = std::make_pair(inst, cdeclWrapper);
    }

    FastInterpBoilerplateInstance* WARN_UNUSED GetGeneratedFunctionEntryPoint(AstFunction* fn)
    {
        auto it = m_functionEntryPoint.find(fn);
        if (it == m_functionEntryPoint.end()) {
            return nullptr;
        } else {
            return it->second.first;
        }
    }

    // Materialize the generated program. All placeholders must have been populated.
    // If anything failed, return nullptr.
    // After calling this function, this class no longer needs to exist.
    //
    std::unique_ptr<FastInterpGeneratedProgram> WARN_UNUSED Materialize()
    {
        TestAssert(!m_materialized);
#ifdef TESTBUILD
        m_materialized = true;
#endif

        // Phase 1: place all code sections. Code section grows up from base address.
        //
        // The complication is that we want to eliminate tail calls as much as possible.
        // One can think of this as a graph in which each node has exactly 0 or 1 outcoming edge,
        // and we want to order the nodes, to maximize the # of neighboring nodes in our sequence
        // that has an edge between them in the graph.
        //
        // It is easy to prove that the following greedy algorithm produces an optimal ordering:
        // Repeat: pick a node with no inbound-degree, chase its outlink until we reach an end or a visited node.
        //
        int32_t codeSectionLength = 0;
        for (size_t i = 0; i < m_allBoilerplateInstances.size(); i++)
        {
            FastInterpBoilerplateInstance* instance = m_allBoilerplateInstances[i];
            if (instance->m_populatedRelativeCodeAddress || instance->m_isContinuationOfAnotherInstance)
            {
                continue;
            }
            while (true)
            {
                instance->m_relativeCodeAddr = codeSectionLength;
                codeSectionLength += static_cast<int32_t>(instance->m_owner->GetCodeSectionLength());
                instance->m_populatedRelativeCodeAddress = true;
                if (instance->m_litcInstanceOrd == static_cast<uint32_t>(-1))
                {
                    break;
                }
                else
                {
                    TestAssert(instance->m_litcInstanceOrd < m_allBoilerplateInstances.size());
                    FastInterpBoilerplateInstance* nextInstance = m_allBoilerplateInstances[instance->m_litcInstanceOrd];
                    TestAssert(nextInstance->m_isContinuationOfAnotherInstance);
                    if (nextInstance->m_populatedRelativeCodeAddress)
                    {
                        break;
                    }
                    instance->m_shouldStripLITC = true;
                    instance = nextInstance;
                    codeSectionLength -= x86_64_rip_relative_jmp_instruction_len;
                }
            }
        }

#ifdef TESTBUILD
        // Just to make sure we have placed every placeholder
        //
        for (size_t i = 0; i < m_allBoilerplateInstances.size(); i++)
        {
            TestAssert(m_allBoilerplateInstances[i]->m_populatedRelativeCodeAddress);
        }
#endif

        // Phase 2: allocate the actual memory, and materialize everything.
        //
        size_t dataSectionSize = (m_dataSectionLength + 4095) / 4096 * 4096;
        size_t codeSectionSize = static_cast<size_t>((codeSectionLength + 4095) / 4096 * 4096);
        size_t mmapLength = dataSectionSize + codeSectionSize;
        void* mmapResult = mmap(nullptr, mmapLength, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
        if (mmapResult == MAP_FAILED) {
            return std::unique_ptr<FastInterpGeneratedProgram>(nullptr);
        }

        bool success = false;
        Auto(
            if (!success) {
                munmap(mmapResult, mmapLength);
            }
        );

        uintptr_t baseAddress = reinterpret_cast<uintptr_t>(mmapResult) + dataSectionSize;
        for (FastInterpBoilerplateInstance* instance : m_allBoilerplateInstances)
        {
            instance->Materialize(baseAddress);
        }

        std::unordered_map<AstFunction*, void*> entryPointMap;
        for (auto it = m_functionEntryPoint.begin(); it != m_functionEntryPoint.end(); it++)
        {
            TestAssert(!entryPointMap.count(it->first));
            entryPointMap[it->first] = reinterpret_cast<void*>(
                        baseAddress + static_cast<uint64_t>(static_cast<int64_t>(it->second.second->m_relativeCodeAddr)));
        }

        {
            int r = mprotect(mmapResult, dataSectionSize, PROT_READ);
            if (r != 0) {
                return std::unique_ptr<FastInterpGeneratedProgram>(nullptr);
            }
        }

        {
            int r = mprotect(reinterpret_cast<void*>(baseAddress), codeSectionSize, PROT_READ | PROT_EXEC);
            if (r != 0) {
                return std::unique_ptr<FastInterpGeneratedProgram>(nullptr);
            }
        }

        InvalidateInstructionCache(reinterpret_cast<void*>(baseAddress), codeSectionSize);

        std::unique_ptr<FastInterpGeneratedProgram> ret(new FastInterpGeneratedProgram(mmapResult, mmapLength, std::move(entryPointMap)));
        success = true;
        return ret;
    }

private:
    static void InvalidateInstructionCache(const void* addr, size_t len);

    std::unordered_map<AstFunction*, std::pair<FastInterpBoilerplateInstance*, FastInterpBoilerplateInstance*> > m_functionEntryPoint;
    std::vector<FastInterpBoilerplateInstance*> m_allBoilerplateInstances;
    size_t m_dataSectionLength;
#ifdef TESTBUILD
    bool m_materialized;
#endif
};

}   // namespace PochiVM
