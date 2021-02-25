#include "mem2reg_pass.h"
#include "function_proto.h"

namespace PochiVM
{

struct Mem2RegVariableWeightStat
{
    // A weighted count of how many times it is used
    // We assume 'if' branch has 50% chance to be taken (except loop condition)
    // The initial weight will be 2^32, each nested 'if' decreasing it by half
    //
    uint64_t m_weight;

    // Whether this variable is initiated before entering the region.
    // In that case, we need to load its initial value from its memory address at the beginning.
    //
    bool m_initiatedBeforeRegion;

    // Whether this variable outlives the region
    // In that case, we need to writeback its final value to its memory address in the end if it could be modified.
    //
    bool m_outlivesRegion;

    // true if the variable might be modified in the region.
    // Only useful if 'm_outlivesRegion' is true.
    //
    bool m_isPotentiallyModified;

    Mem2RegVariableWeightStat()
        : m_weight(0)
        , m_initiatedBeforeRegion(true)
        , m_outlivesRegion(true)
        , m_isPotentiallyModified(false)
    { }
};

struct Mem2RegVariableUsageList
{
    Mem2RegVariableWeightStat m_stat;
    std::vector<AstNodeBase**> m_loads;
    std::vector<AstAssignExpr*> m_stores;
};

static void ApplyMem2RegPassForRegion(AstFunction* func, AstNodeBase* root)
{
    std::unordered_map<AstVariable*, Mem2RegVariableUsageList> usageList;

    {
        int ifDepth = 0;
        auto insertAndUpdateVariableWeight = [&](AstVariable* var, bool updateWeight = true) -> std::pair<bool, Mem2RegVariableUsageList*>
        {
            if (!var->IsEligibleForMem2Reg()) { return std::make_pair(false, nullptr); }
            Mem2RegVariableUsageList& list = usageList[var];
            if (updateWeight && ifDepth <= 32)
            {
                list.m_stat.m_weight += 1ULL << (32 - ifDepth);
            }
            return std::make_pair(true, &list);
        };

        auto registerVariableLoad = [&](AstVariable* var, AstNodeBase** usage)
        {
            auto r = insertAndUpdateVariableWeight(var);
            if (!r.first) { return; }
            Mem2RegVariableUsageList* list = r.second;
            list->m_loads.push_back(usage);
        };

        auto registerVariableStore = [&](AstVariable* var, AstAssignExpr* usage)
        {
            auto r = insertAndUpdateVariableWeight(var);
            if (!r.first) { return; }
            Mem2RegVariableUsageList* list = r.second;
            list->m_stores.push_back(usage);
            list->m_stat.m_isPotentiallyModified = true;
        };

        auto registerVariableDeclare = [&](AstVariable* var, bool isInForLoopInitBlock = false)
        {
            auto r = insertAndUpdateVariableWeight(var, false /*updateWeight*/);
            if (!r.first) { return; }
            Mem2RegVariableUsageList* list = r.second;
            // The variable is declared inside the region or for-loop init-block.
            // So we know that it does not outlive the region.
            //
            list->m_stat.m_outlivesRegion = false;
            // If we are in for-loop init-block, the variable does not outlive the region,
            // but is initiated before the region.
            //
            if (!isInForLoopInitBlock)
            {
                list->m_stat.m_initiatedBeforeRegion = false;
            }
        };

        auto traverseFn = [&](AstNodeBase*& cur,
                              AstNodeBase* /*parent*/,
                              FunctionRef<void(void)> Recurse)
        {
            if (cur->GetAstNodeType() == AstNodeType::AstDereferenceExpr)
            {
                AstDereferenceExpr* derefExpr = assert_cast<AstDereferenceExpr*>(cur);
                if (derefExpr->GetOperand()->GetAstNodeType() == AstNodeType::AstVariable)
                {
                    registerVariableLoad(assert_cast<AstVariable*>(derefExpr->GetOperand()), &cur);
                }
            }
            else if (cur->GetAstNodeType() == AstNodeType::AstDereferenceVariableExpr)
            {
                AstDereferenceVariableExpr* derefExpr = assert_cast<AstDereferenceVariableExpr*>(cur);
                registerVariableLoad(derefExpr->GetOperand(), &cur);
            }
            else if (cur->GetAstNodeType() == AstNodeType::AstAssignExpr)
            {
                AstAssignExpr* assignExpr = assert_cast<AstAssignExpr*>(cur);
                if (assignExpr->GetDst()->GetAstNodeType() == AstNodeType::AstVariable)
                {
                    registerVariableStore(assert_cast<AstVariable*>(assignExpr->GetDst()), assignExpr);
                }
            }
            else if (cur->GetAstNodeType() == AstNodeType::AstDeclareVariable)
            {
                AstDeclareVariable* declVar = assert_cast<AstDeclareVariable*>(cur);
                registerVariableDeclare(declVar->m_variable);
            }
            else if (cur->GetAstNodeType() == AstNodeType::AstIfStatement)
            {
                ifDepth++;
            }

            Recurse();

            if (cur->GetAstNodeType() == AstNodeType::AstIfStatement)
            {
                ifDepth--;
            }
        };

        if (root->GetAstNodeType() == AstNodeType::AstForLoop)
        {
            // For for-loop, we should not traverse the init-block for counting weights,
            // but we should still traverse the init-block for variable declarations,
            // since it's a really common case that loop induction variable is declared in init-block.
            // Knowing it does not outlive the region saves an useless writeback in the end.
            //
            AstForLoop* forLoop = assert_cast<AstForLoop*>(root);
            TraverseAstTreeByRef(forLoop->GetCondClause(), traverseFn);
            TraverseAstTreeByRef(forLoop->GetBody(), traverseFn);
            TraverseAstTreeByRef(forLoop->GetStepBlock(), traverseFn);

            auto initBlockTraverseFn = [&](AstNodeBase*& cur,
                                           AstNodeBase* /*parent*/,
                                           FunctionRef<void(void)> Recurse)
            {
                if (cur->GetAstNodeType() == AstNodeType::AstDeclareVariable)
                {
                    AstDeclareVariable* declVar = assert_cast<AstDeclareVariable*>(cur);
                    registerVariableDeclare(declVar->m_variable, true /*isInForLoopInitBlock*/);
                }
                Recurse();
            };
            TraverseAstTreeByRef(forLoop->GetInitBlock(), initBlockTraverseFn);
        }
        else
        {
            // For while-loop, or the whole function (in that case it is an AstScope),
            // we can just traverse the whole tree
            //
            TestAssert(root->GetAstNodeType() == AstNodeType::AstWhileLoop ||
                       root->GetAstNodeType() == AstNodeType::AstScope);
            TraverseAstTreeByRef(root, traverseFn);
        }

        TestAssert(ifDepth == 0);
    }

    // Now, choose the variables with highest weight for mem2reg.
    // std::nth_element partition the list to put the smallest first, so we need to inverse the weight factor.
    //
    std::vector<std::pair<uint64_t, AstVariable*>> intList, floatList;

    for (auto it = usageList.begin(); it != usageList.end(); it++)
    {
        AstVariable* var = it->first;
        TestAssert(!var->GetTypeId().RemovePointer().IsCppClassType());
        if (var->GetTypeId().RemovePointer().IsFloatingPoint())
        {
            floatList.push_back(std::make_pair(std::numeric_limits<uint64_t>::max() - it->second.m_stat.m_weight, var));
        }
        else
        {
            intList.push_back(std::make_pair(std::numeric_limits<uint64_t>::max() - it->second.m_stat.m_weight, var));
        }
    }

    if (intList.size() > x_mem2reg_max_integral_vars)
    {
        std::nth_element(intList.begin(), intList.begin() + (x_mem2reg_max_integral_vars - 1), intList.end());
        intList.resize(x_mem2reg_max_integral_vars);
    }
    if (floatList.size() > x_mem2reg_max_floating_vars)
    {
        std::nth_element(floatList.begin(), floatList.begin() + (x_mem2reg_max_floating_vars - 1), floatList.end());
        floatList.resize(x_mem2reg_max_floating_vars);
    }

    // Execute mem2reg by rewriting all loads and stores in AST,
    // and add records to appropriately initiate and writeback variables as needed.
    //
    auto executeMem2Reg = [&](AstVariable* var, size_t ordinal)
    {
        TestAssert(var->IsEligibleForMem2Reg() && usageList.count(var));
        Mem2RegVariableUsageList& list = usageList[var];
        // Rewrite all loads and stores
        //
        AstRegisterCachedVariableExpr* mem2regVar = new AstRegisterCachedVariableExpr(var, static_cast<int>(ordinal));
        // PrepareForDebugInterp() may have been executed. So set up the debug interp function.
        //
        mem2regVar->SetupDebugInterpImpl();
        for (AstNodeBase** loadExprAddr : list.m_loads)
        {
            *loadExprAddr = mem2regVar;
        }
        for (AstAssignExpr* assignExpr : list.m_stores)
        {
            assignExpr->SetLhsMem2Reg(mem2regVar);
        }
        // Setup initiation/writeback record as needed
        // We only need to do this for loops
        //
        if (root->GetAstNodeType() == AstNodeType::AstWhileLoop ||
            root->GetAstNodeType() == AstNodeType::AstForLoop)
        {
            Mem2RegEligibleRegion* region;
            if (root->GetAstNodeType() == AstNodeType::AstWhileLoop)
            {
                region = assert_cast<AstWhileLoop*>(root);
            }
            else
            {
                region = assert_cast<AstForLoop*>(root);
            }

            bool needInitRecord = list.m_stat.m_initiatedBeforeRegion;
            if (needInitRecord)
            {
                region->m_mem2RegInitList.push_back(mem2regVar);
            }
            bool needWritebackRecord = list.m_stat.m_outlivesRegion && list.m_stat.m_isPotentiallyModified;
            if (needWritebackRecord)
            {
                region->m_mem2RegWritebackList.push_back(mem2regVar);
            }
        }
        else
        {
            TestAssert(root->GetAstNodeType() == AstNodeType::AstScope);
            func->AddSpecialMem2RegVariable(mem2regVar);
        }
    };

    if (root->GetAstNodeType() == AstNodeType::AstScope)
    {
        func->SetAsSpecialMem2RegWholeRegion();
    }

    for (size_t i = 0; i < intList.size(); i++)
    {
        executeMem2Reg(intList[i].second, i);
    }
    for (size_t i = 0; i < floatList.size(); i++)
    {
        executeMem2Reg(floatList[i].second, i);
    }
}

void ApplyMem2RegPass(AstFunction* func)
{
    // All loops that contain no nested loops or call expressions.
    // Those are candidate regions to apply mem2reg.
    // Each candidate region is either AstForLoop, or AstWhileLoop, or AstFunction
    //
    std::vector<AstNodeBase*> candidateRegions;

    {
        // Figure out candidateRegions and all candidate variables
        // The candidate variables are those variables whose address is never taken.
        //

        // The current stack of loops.
        // The 'bool' is true if it contains no nested loops and no call expressions.
        //
        std::vector<std::pair<AstNodeBase*, bool>> loopStack;
        loopStack.push_back(std::make_pair(nullptr, true));

        auto traverseFn = [&](AstNodeBase* cur,
                              AstNodeBase* parent,
                              FunctionRef<void(void)> Recurse)
        {
            if (cur->GetAstNodeType() == AstNodeType::AstVariable)
            {
                TestAssert(parent != nullptr);
                AstVariable* var = assert_cast<AstVariable*>(cur);
                if (var->IsEligibleForMem2Reg())
                {
                    // If the variable address is NOT used for a direct load or store,
                    // its address is significant. We must not apply mem2reg to it.
                    //
                    bool ok = false;
                    if (parent->GetAstNodeType() == AstNodeType::AstDereferenceExpr)
                    {
                        ok = true;
                    }
                    else if (parent->GetAstNodeType() == AstNodeType::AstDereferenceVariableExpr)
                    {
                        ok = true;
                    }
                    else if (parent->GetAstNodeType() == AstNodeType::AstDeclareVariable)
                    {
                        // AstDeclareVariable statement also traverses the AstVariable,
                        // which is neutral on its own
                        //
                        TestAssert(assert_cast<AstDeclareVariable*>(parent)->m_variable == var);
                        ok = true;
                    }
                    else if (parent->GetAstNodeType() == AstNodeType::AstAssignExpr)
                    {
                        // Only good if the variable appeared directly as the left side of the assign expr
                        //
                        AstAssignExpr* assignExpr = assert_cast<AstAssignExpr*>(parent);
                        if (cur == assignExpr->GetDst())
                        {
                            TestAssert(cur != assignExpr->GetSrc());
                            ok = true;
                        }
                    }
                    if (!ok)
                    {
                        var->SetNotEligibleForMem2Reg();
                    }
                }
            }
            else if (cur->GetAstNodeType() == AstNodeType::AstWhileLoop ||
                     cur->GetAstNodeType() == AstNodeType::AstForLoop)
            {
                loopStack.back().second = false;
                loopStack.push_back(std::make_pair(cur, true));
            }
            else if (cur->GetAstNodeType() == AstNodeType::AstCallExpr)
            {
                loopStack.back().second = false;
            }
            else if (cur->GetAstNodeType() == AstNodeType::AstDeclareVariable)
            {
                AstDeclareVariable* declVar = assert_cast<AstDeclareVariable*>(cur);
                if (declVar->m_variable->GetTypeId().RemovePointer().IsCppClassType())
                {
                    // If the variable is a CPP object, its destructor will be a function call.
                    //
                    loopStack.back().second = false;
                }
            }

            Recurse();

            if (cur->GetAstNodeType() == AstNodeType::AstWhileLoop ||
                cur->GetAstNodeType() == AstNodeType::AstForLoop)
            {
                TestAssert(loopStack.size() > 0 && loopStack.back().first == cur);
                if (loopStack.back().second)
                {
                    candidateRegions.push_back(cur);
                }
                loopStack.pop_back();
            }
        };

        func->TraverseFunctionBody(traverseFn);

        TestAssert(loopStack.size() == 1 && loopStack.back().first == nullptr);
        if (loopStack.back().second)
        {
            TestAssert(candidateRegions.size() == 0);
            candidateRegions.push_back(func->GetFunctionBody());
        }
    }

    // Now, for each candidate region, apply mem2reg pass
    //
    for (AstNodeBase* region : candidateRegions)
    {
        ApplyMem2RegPassForRegion(func, region);
    }
}

}   // namespace PochiVM
