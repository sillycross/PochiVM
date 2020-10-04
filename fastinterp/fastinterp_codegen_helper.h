#pragma once

#include "fastinterp_helper.h"
#include "generated/fastinterp_library.generated.h"

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
        m_fixupValues[ordinal] = reinterpret_cast<uint64_t>(value->m_relativeCodeAddr);
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
        m_fixupValues[m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal + ordinal] = reinterpret_cast<uint64_t>(
                    m_relativeDataAddr + m_owner->m_cppFnPtrPlaceholderOrdinalToId[ordinal] * sizeof(uint64_t));
    }

    // The constant type must match what is defined in the boilerplate
    //
    template<typename T>
    void PopulateConstantPlaceholder(uint32_t ordinal, T value)
    {
        static_assert(sizeof(T) <= 8 && (std::is_fundamental<T>::value || std::is_pointer<T>::value) &&
                (!std::is_function<typename std::remove_pointer<T>::type>::value), "must be a primitive type");
        // If T is 8-byte long, it is not allowed to pass in 0
        //
        TestAssertImp(sizeof(T) == 8, !is_all_underlying_bits_zero(value));
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
                                  uintptr_t relativeCodeAddr,
                                  uintptr_t relativeDataAddr)
        : m_relativeCodeAddr(relativeCodeAddr)
        , m_relativeDataAddr(relativeDataAddr)
        , m_owner(owner)
#ifdef TESTBUILD
        , m_populatedBoilerplateFnPtrPlaceholderMask(0)
        , m_populatedUInt64PlaceholderMask(0)
        , m_populatedCppFnptrPlaceholderMask(0)
        , m_isMaterialized(false)
#endif
    {
        m_fixupValues = new uint64_t[m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal +
                m_owner->m_highestUInt64PlaceholderOrdinal + m_owner->m_highestCppFnptrPlaceholderOrdinal];
        m_dataValues = new uint64_t[m_owner->GetDataSectionLength() / sizeof(uint64_t)];
    }

    void Materialize(uintptr_t baseAddress)
    {
        TestAssert(!m_isMaterialized);
        TestAssert(baseAddress % x_fastinterp_function_alignment == 0);
        TestAssert(m_populatedBoilerplateFnPtrPlaceholderMask == m_owner->m_usedBoilerplateFnPtrPlaceholderMask);
        TestAssert((m_populatedUInt64PlaceholderMask & m_owner->m_usedUInt64PlaceholderMask) == m_owner->m_usedUInt64PlaceholderMask);
        TestAssert(m_populatedCppFnptrPlaceholderMask == m_owner->m_usedCppFnptrPlaceholderMask);
#ifdef TESTBUILD
        m_isMaterialized = true;
#endif
        // The 'boilerplate' and 'cpp_fn' fixups are all relative pointers to the final baseAddress
        // Now the base address is known, add it so we get the true pointer
        //
        for (uint32_t i = 0; i < m_owner->m_highestBoilerplateFnptrPlaceholderOrdinal + m_owner->m_highestCppFnptrPlaceholderOrdinal; i++)
        {
            m_fixupValues[i] += baseAddress;
        }
        uint8_t* trueCodeBaseAddress = reinterpret_cast<uint8_t*>(baseAddress + m_relativeCodeAddr);
        uint8_t* trueDataBaseAddress = reinterpret_cast<uint8_t*>(baseAddress + m_relativeDataAddr);
        memcpy(trueDataBaseAddress, m_dataValues, m_owner->GetDataSectionLength());
        m_owner->MaterializeCodeSection(trueCodeBaseAddress, m_fixupValues);
    }

    uintptr_t m_relativeCodeAddr;
    uintptr_t m_relativeDataAddr;

    const FastInterpBoilerplateBluePrint* m_owner;

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
    bool m_isMaterialized;
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
        : m_codeSectionLength(0)
        , m_dataSectionLength(0)
#ifdef TESTBUILD
        , m_materialized(false)
#endif
    { }

    FastInterpBoilerplateInstance* WARN_UNUSED InstantiateBoilerplate(const FastInterpBoilerplateBluePrint* boilerplate)
    {
        // Code section grows up from base address
        //
        TestAssert(m_codeSectionLength % x_fastinterp_function_alignment == 0);
        uintptr_t relativeCodeSectionBase = m_codeSectionLength;
        m_codeSectionLength += boilerplate->GetCodeSectionLength();
        // Data section grows down from base address,
        // its data section range is [-(oldDSLength + thisDsLength), -oldDsLength)
        //
        m_dataSectionLength += boilerplate->GetDataSectionLength();
        uintptr_t relativeDataSectionBase = static_cast<uintptr_t>(-static_cast<ssize_t>(m_dataSectionLength));
        FastInterpBoilerplateInstance* inst = new FastInterpBoilerplateInstance(boilerplate, relativeCodeSectionBase, relativeDataSectionBase);
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

        size_t dataSectionSize = (m_dataSectionLength + 4095) / 4096 * 4096;
        size_t codeSectionSize = (m_codeSectionLength + 4095) / 4096 * 4096;
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
            entryPointMap[it->first] = reinterpret_cast<void*>(baseAddress + it->second.second->m_relativeCodeAddr);
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
    size_t m_codeSectionLength;
    size_t m_dataSectionLength;
#ifdef TESTBUILD
    bool m_materialized;
#endif
};

}   // namespace PochiVM
