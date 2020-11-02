#pragma once

// Internal utility classes. One should always include "fastinterp.h" instead, not this file.
//

#include "pochivm/common.h"
#include "pochivm/cxx2a_bit_cast_helper.h"
#include "fastinterp_function_alignment.h"
#include "x86_64_asm_helper.h"
#include "unaligned_memaccess_helper.h"
#include "x86_64_rewrite_jmp_jcc_instruction_helper.h"

namespace PochiVM
{

// Utility classes for fast interp.
// Basic workflow:
//
//    const FastInterpBoilerplateBluePrint* boilerplate;
//    boilerplate = FastInterpBoilerplateLibaray<AstNodeType>::SelectBoilerplateBluePrint(...);
//        Selects a boilerplate for 'AstNodeType', using a list of parameters as specified in 'fastinterp_tpl.cpp'
//
//    FastInterpCodegenEngine engine;
//        The manager class to build a FastInterp program
//
//    FastInterpBoilerplateInstance* instance;
//    instance = engine.InstantiateBoilerplate(const FastInterpBoilerplateBluePrint* boilerplate);
//        Put a copy of 'boilerplate' into the generated program, returns reference to the instantiation of the boilerplate.
//
//    engine.RegisterGeneratedFunctionEntryPoint(AstFunction* fn, FastInterpBoilerplateInstance* instance);
//        Register 'instance' as the entry point for AstFunction 'fn'.
//
//    instance->PopulateBoilerplateFnPtrPlaceholder(...)
//    instance->PopulateCppFnPtrPlaceholder(...)
//    instance->PopulateConstantPlaceholder(...)
//        Populate the placeholders in the boilerplate instantiation with concrete values.
//
//    std::unique_ptr<FastInterpGeneratedProgram> generatedProgram;
//    generatedProgram = engine.Materialize();
//        After all boilerplates are instantiated and all placeholders of all instantiations are populated,
//        use the above code to materialize the generated program.
//
//    void* entryPoint = generatedProgram->GetGeneratedFunctionAddress(AstFunction* fn);
//        Returns the function pointer for boilerplate instance registered in 'RegisterGeneratedFunctionEntryPoint'.
//

struct FastInterpSymbolFixupRecord
{
    constexpr FastInterpSymbolFixupRecord()
        : m_offset(0), m_ordinalIntoPlaceholderArray(static_cast<uint32_t>(-1))
    { }

    constexpr FastInterpSymbolFixupRecord(uint32_t offset, uint32_t ord)
        : m_offset(offset), m_ordinalIntoPlaceholderArray(ord)
    { }

    uint32_t m_offset;
    uint32_t m_ordinalIntoPlaceholderArray;
};

class FastInterpBoilerplateBluePrint : NonCopyable, NonMovable
{
public:
    friend class FastInterpBoilerplateInstance;

    size_t GetCodeSectionLength() const
    {
        return m_contentLength;
    }

#ifdef TESTBUILD
    const char* TestOnly_GetSymbolName() const
    {
        return m_symbolName;
    }
#endif

protected:
    constexpr FastInterpBoilerplateBluePrint(
            const uint8_t* content
          , uint32_t contentLength
          , const uint32_t* addr32FixupArray
          , uint32_t addr32FixupArrayLength
          , const FastInterpSymbolFixupRecord* symbol32FixupArray
          , uint32_t symbol32FixupArrayLength
          , const FastInterpSymbolFixupRecord* symbol64FixupArray
          , uint32_t symbol64FixupArrayLength
          , uint16_t highestBoilerplateFnptrPlaceholderOrdinal
          , uint16_t highestCppFnptrPlaceholderOrdinal
          , uint16_t highestUInt64PlaceholderOrdinal
          , int lastInstructionTailCallOrd
          , uint32_t jmp32ArrayLength
          , const uint32_t* jmp32Offsets
          , uint32_t jcc32ArrayLength
          , const uint32_t* jcc32Offsets
#ifdef TESTBUILD
          , uint64_t usedBoilerplateFnPtrPlaceholderMask
          , uint64_t usedCppFnptrPlaceholderMask
          , uint64_t usedUInt64PlaceholderMask
          , const char* symbolName
#endif
    )
        : m_content(content)
        , m_addr32FixupArray(addr32FixupArray)
        , m_symbol32FixupArray(symbol32FixupArray)
        , m_symbol64FixupArray(symbol64FixupArray)
        , m_jmp32OffsetArray(jmp32Offsets)
        , m_jcc32OffsetArray(jcc32Offsets)
        , m_contentLength(contentLength)
        , m_addr32FixupArrayLength(addr32FixupArrayLength)
        , m_symbol32FixupArrayLength(symbol32FixupArrayLength)
        , m_symbol64FixupArrayLength(symbol64FixupArrayLength)
        , m_highestBoilerplateFnptrPlaceholderOrdinal(highestBoilerplateFnptrPlaceholderOrdinal)
        , m_highestCppFnptrPlaceholderOrdinal(highestCppFnptrPlaceholderOrdinal)
        , m_highestUInt64PlaceholderOrdinal(highestUInt64PlaceholderOrdinal)
        , m_lastInstructionTailCallOrd(lastInstructionTailCallOrd)
        , m_jmp32ArrayLength(jmp32ArrayLength)
        , m_jcc32ArrayLength(jcc32ArrayLength)
#ifdef TESTBUILD
        , m_usedBoilerplateFnPtrPlaceholderMask(usedBoilerplateFnPtrPlaceholderMask)
        , m_usedCppFnptrPlaceholderMask(usedCppFnptrPlaceholderMask)
        , m_usedUInt64PlaceholderMask(usedUInt64PlaceholderMask)
        , m_symbolName(symbolName)
#endif
    { }

private:
    void MaterializeCodeSection(uint8_t* destAddr, uint64_t* fixupValues, bool shouldStripLITC) const
    {
        TestAssert(reinterpret_cast<uint64_t>(destAddr) % x_fastinterp_function_alignment == 0);
        uint32_t trueContentLength = m_contentLength;
        if (shouldStripLITC)
        {
            TestAssert(trueContentLength >= x86_64_rip_relative_jmp_instruction_len &&
                       m_content[trueContentLength - x86_64_rip_relative_jmp_instruction_len] == x86_64_jmp_instruction_opcode);
            trueContentLength -= x86_64_rip_relative_jmp_instruction_len;
        }
        memcpy(destAddr, m_content, trueContentLength);

        {
            uint32_t limit = m_addr32FixupArrayLength;
            if (shouldStripLITC)
            {
                TestAssert(limit > 0 && m_addr32FixupArray[limit - 1] == trueContentLength + x86_64_jmp_opcode_num_bytes);
                limit--;
            }
            uint32_t addend = static_cast<uint32_t>(-static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uint64_t>(destAddr))));
            for (uint32_t i = 0; i < limit; i++)
            {
                TestAssert(m_addr32FixupArray[i] + sizeof(uint32_t) <= trueContentLength);
                UnalignedAddAndWriteback<uint32_t>(destAddr + m_addr32FixupArray[i], addend);
            }
        }

        {
            uint32_t limit = m_symbol32FixupArrayLength;
            if (shouldStripLITC)
            {
                TestAssert(limit > 0 && m_symbol32FixupArray[limit - 1].m_offset == trueContentLength + x86_64_jmp_opcode_num_bytes);
                limit--;
            }
            for (uint32_t i = 0; i < limit; i++)
            {
                TestAssert(m_symbol32FixupArray[i].m_offset + sizeof(uint32_t) <= trueContentLength);
                uint32_t addend = static_cast<uint32_t>(fixupValues[m_symbol32FixupArray[i].m_ordinalIntoPlaceholderArray]);
                UnalignedAddAndWriteback<uint32_t>(destAddr + m_symbol32FixupArray[i].m_offset, addend);
            }
        }

        for (uint32_t i = 0; i < m_symbol64FixupArrayLength; i++)
        {
            TestAssert(m_symbol64FixupArray[i].m_offset + sizeof(uint64_t) <= trueContentLength);
            uint64_t addend = static_cast<uint64_t>(fixupValues[m_symbol64FixupArray[i].m_ordinalIntoPlaceholderArray]);
            UnalignedAddAndWriteback<uint64_t>(destAddr + m_symbol64FixupArray[i].m_offset, addend);
        }

        // Rewrite jmp and jcc instructions if possible
        //
        {
            uint32_t limit = m_jmp32ArrayLength;
            if (shouldStripLITC)
            {
                TestAssert(limit > 0 && m_jmp32OffsetArray[limit - 1] == trueContentLength + x86_64_jmp_opcode_num_bytes);
                limit--;
            }
            for (uint32_t i = 0; i < limit; i++)
            {
                x86_64_try_rewrite_jmp_instruction(destAddr + m_jmp32OffsetArray[i]);
            }
        }

        {
            for (uint32_t i = 0; i < m_jcc32ArrayLength; i++)
            {
                x86_64_try_rewrite_jcc_instruction(destAddr + m_jcc32OffsetArray[i]);
            }
        }
    }

    // The pre-fixup binary content
    //
    const uint8_t* m_content;

    // The list of positions that needs to fix up by "-fn_addr", 32bit
    //
    const uint32_t* m_addr32FixupArray;

    // The list of positions that needs to fix up by "+value", 32bit
    //
    const FastInterpSymbolFixupRecord* m_symbol32FixupArray;

    // The list of positions that needs to fix up by "+value", 64bit
    //
    const FastInterpSymbolFixupRecord* m_symbol64FixupArray;

    const uint32_t* m_jmp32OffsetArray;
    const uint32_t* m_jcc32OffsetArray;

    uint32_t m_contentLength;
    uint32_t m_addr32FixupArrayLength;
    uint32_t m_symbol32FixupArrayLength;
    uint32_t m_symbol64FixupArrayLength;

    uint16_t m_highestBoilerplateFnptrPlaceholderOrdinal;
    uint16_t m_highestCppFnptrPlaceholderOrdinal;
    uint16_t m_highestUInt64PlaceholderOrdinal;
    int m_lastInstructionTailCallOrd;
    uint32_t m_jmp32ArrayLength;
    uint32_t m_jcc32ArrayLength;

#ifdef TESTBUILD
    // bitmask denoting which placeholder ordinals are used, for test assertion purpose
    //
    uint64_t m_usedBoilerplateFnPtrPlaceholderMask;
    uint64_t m_usedCppFnptrPlaceholderMask;
    uint64_t m_usedUInt64PlaceholderMask;
    // The symbol name of the original function that generated this boilerplate
    //
    const char* m_symbolName;
#endif
};

struct FastInterpBoilerplateSelectionHashTableEntry
{
    constexpr FastInterpBoilerplateSelectionHashTableEntry()
        : m_value(nullptr), m_fingerprint(static_cast<uint32_t>(-1))
    { }

    constexpr FastInterpBoilerplateSelectionHashTableEntry(const FastInterpBoilerplateBluePrint* value, uint32_t fingerprint)
        : m_value(value), m_fingerprint(fingerprint)
    { }

    const FastInterpBoilerplateBluePrint* m_value;
    uint32_t m_fingerprint;
};

// const FastInterpBoilerplateBluePrint* FastInterpBoilerplateLibrary<T>::SelectBoilerplateBluePrint(...)
//    select a boilerplate for AstNodeType based on compile-time specialization parameters
//
// This class is defined in generated header
//

}   // namespace PochiVM
