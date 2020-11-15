#pragma once

#include "fastinterp_codegen_helper.h"
#include "pochivm/function_proto.h"
#include "pochivm/pochivm_function_pointer.h"

namespace PochiVM
{

inline void FastInterpCodegenEngine::RegisterGeneratedFunctionEntryPoint(AstFunction* fn, FastInterpBoilerplateInstance* inst)
{
    TestAssert(!m_functionEntryPoint.count(fn));
    FastInterpBoilerplateInstance* cdeclWrapper = InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICdeclInterfaceImpl>::SelectBoilerplateBluePrint(
                    fn->GetReturnType().GetDefaultFastInterpTypeId(),
                    fn->GetIsNoExcept()));
    cdeclWrapper->PopulateBoilerplateFnPtrPlaceholder(0, inst);
    m_functionEntryPoint[fn] = std::make_pair(inst, cdeclWrapper);
}

inline void FastInterpCodegenEngine::TestOnly_RegisterUnitTestFunctionEntryPoint(
        FastInterpTypeId returnType, bool isNoExcept, uintptr_t fakeId, FastInterpBoilerplateInstance* inst)
{
    AstFunction* fn = reinterpret_cast<AstFunction*>(fakeId);
    TestAssert(!m_functionEntryPoint.count(fn));
    FastInterpBoilerplateInstance* cdeclWrapper = InstantiateBoilerplate(
                FastInterpBoilerplateLibrary<FICdeclInterfaceImpl>::SelectBoilerplateBluePrint(
                    returnType, isNoExcept));
    cdeclWrapper->PopulateBoilerplateFnPtrPlaceholder(0, inst);
    m_functionEntryPoint[fn] = std::make_pair(inst, cdeclWrapper);
}

inline std::unique_ptr<FastInterpGeneratedProgram> WARN_UNUSED FastInterpCodegenEngine::Materialize()
{
    TestAssert(!m_materialized);
#ifdef TESTBUILD
    m_materialized = true;
#endif

    // Phase 1: populate all AstFunction placeholders. Their boilerplate entry point is now all known.
    //
    for (size_t index = 0; index < m_boilerplateFnEntryPointPlaceholders.size(); index++)
    {
        FastInterpBoilerplateInstance* inst = m_boilerplateFnEntryPointPlaceholders[index].first;
        AstFunction* func = m_boilerplateFnEntryPointPlaceholders[index].second.first;
        uint32_t ord = m_boilerplateFnEntryPointPlaceholders[index].second.second;
        TestAssert(m_functionEntryPoint.count(func));
        // We are calling from boilerplate code, so we should use the GHC convention one
        //
        FastInterpBoilerplateInstance* target = m_functionEntryPoint[func].first;
        inst->PopulateBoilerplateFnPtrPlaceholder(ord, target);
    }

    // Phase 2: place all code sections. Code section grows up from base address.
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

        int padding = codeSectionLength & ((1 << instance->m_log2CodeSectionAlignment) - 1);
        if (padding != 0) { padding = (1 << instance->m_log2CodeSectionAlignment) - padding; }
        codeSectionLength += padding;
        instance->m_relativeCodeAddr = codeSectionLength;
        instance->m_codeSectionPaddingRequired = static_cast<uint16_t>(padding);
        codeSectionLength += static_cast<int32_t>(instance->m_owner->GetCodeSectionLength());
        instance->m_populatedRelativeCodeAddress = true;
        instance->m_shouldStripLITC = false;
    }

#ifdef TESTBUILD
    // Just to make sure we have placed every boilerplate instance
    //
    for (size_t i = 0; i < m_allBoilerplateInstances.size(); i++)
    {
        TestAssert(m_allBoilerplateInstances[i]->m_populatedRelativeCodeAddress);
    }
#endif

    // Phase 3: allocate the actual memory, and materialize everything.
    //
    size_t mmapLength = static_cast<size_t>((codeSectionLength + 4095) / 4096 * 4096);
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

    uintptr_t baseAddress = reinterpret_cast<uintptr_t>(mmapResult);

    std::unordered_map<AstFunction*, void*> entryPointMap;
    for (auto it = m_functionEntryPoint.begin(); it != m_functionEntryPoint.end(); it++)
    {
        TestAssert(!entryPointMap.count(it->first));
        entryPointMap[it->first] = reinterpret_cast<void*>(
                    baseAddress + static_cast<uint64_t>(static_cast<int64_t>(it->second.second->m_relativeCodeAddr)));
    }

    // Only now we know the true baseAddress, we can compute the values for function pointers.
    // Populate these placeholders.
    //
    for (auto iter = m_fastInterpFnPtrFixList.begin();
         iter != m_fastInterpFnPtrFixList.end(); iter++)
    {
        AstFunction* fn = iter->first;
        // This 'inst' is always a FILiteralMcMediumImpl, so we should write to placeholder 1
        //
        FastInterpBoilerplateInstance* inst = iter->second;

        TestAssert(entryPointMap.count(fn));
        void* addr = entryPointMap.find(fn)->second;

        uint64_t controlValue = GeneratedFunctionPointerImpl::GetControlValueForFastInterpFn(fn, reinterpret_cast<uint64_t>(addr));
        inst->PopulateConstantPlaceholder<uint64_t>(1, controlValue);
    }

    // Now all the placeholders are populated, materialize everything.
    //
    for (FastInterpBoilerplateInstance* instance : m_allBoilerplateInstances)
    {
        instance->Materialize(baseAddress);
    }

    // TODO: consider only do this in test build
    // TLB shootdown is a bit costly
    //
    {
        int r = mprotect(reinterpret_cast<void*>(baseAddress), mmapLength, PROT_READ | PROT_EXEC);
        if (r != 0) {
            return std::unique_ptr<FastInterpGeneratedProgram>(nullptr);
        }
    }

    InvalidateInstructionCache(reinterpret_cast<void*>(baseAddress), mmapLength);

    std::unique_ptr<FastInterpGeneratedProgram> ret(new FastInterpGeneratedProgram(mmapResult, mmapLength, std::move(entryPointMap)));
    success = true;
    return ret;
}

}   // namespace PochiVM
