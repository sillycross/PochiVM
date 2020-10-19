#include "gtest/gtest.h"

#include "pochivm.h"
#include "test_util_helper.h"

using namespace PochiVM;

namespace {

bool SideEffect(bool r, int32_t* dst, uint64_t offset)
{
    *dst += 1;
    dst[offset] = *dst;
    return r;
}

using SimulationFn = std::function<bool(int32_t*, bool*)>;

std::tuple<Value<bool>, SimulationFn>
Build(Value<int32_t*> dst, Value<bool>* vars, int numVars, uint64_t& cnt)
{
    using SideEffectFn = bool(*)(bool, int32_t*, uint64_t);

    cnt++;
    uint64_t ord = cnt;
    int chance = 49;    // on average 100 clauses
    if (ord != 1 && (rand() % 100 >= chance || ord > 500))
    {
        int dice = rand() % numVars;
        Value<bool> ret = Call<SideEffectFn>("f", vars[dice], dst, Literal<uint64_t>(ord));
        SimulationFn fn = [dice, ord](int32_t* _dst, bool* _vars) -> bool {
            return SideEffect(_vars[dice], _dst, ord);
        };
         return std::make_tuple(ret, fn);
    }
    else
    {
        auto [lhs, _fnLhs] = Build(dst, vars, numVars, cnt);
        auto [rhs, _fnRhs] = Build(dst, vars, numVars, cnt);
        bool dice1 = (rand() % 1000 < 500);
        bool dice2 = (rand() % 1000 < 500);
        bool dice3 = (rand() % 1000 < 500);
        Value<bool> lhs2 = dice1 ? (lhs) : (!lhs);
        Value<bool> rhs2 = dice2 ? (rhs) : (!rhs);
        Value<bool> param = dice3 ? (lhs2 && rhs2) : (lhs2 || rhs2);
        SimulationFn fnLhs = _fnLhs;
        SimulationFn fnRhs = _fnRhs;
        SimulationFn fn = [dice1, dice2, dice3, fnLhs, fnRhs, ord](int32_t* _dst, bool* _vars) -> bool {
            bool l = fnLhs(_dst, _vars);
            if (!dice1) { l = !l; }
            if ((dice3 && l) || (!dice3 && !l))
            {
                bool r = fnRhs(_dst, _vars);
                if (!dice2) { r = !r; }
                l = r;
            }
            return SideEffect(l, _dst, ord);
        };
        Value<bool> ret = Call<SideEffectFn>("f", param, dst, Literal<uint64_t>(ord));
        return std::make_tuple(ret, fn);
    }
}

const static int x_numVars = 10;
using GeneratedFn = bool(*)(int32_t*, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool) noexcept;

void CheckOnce()
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using SideEffectFn = bool(*)(bool, int32_t*, uint64_t) noexcept;

    {
        auto [fn, r, dst, offset] = NewFunction<SideEffectFn>("f", "r", "dst", "offset");
        fn.SetBody(
            Assign(*dst, *dst + Literal<int32_t>(1)),
            Assign(*ReinterpretCast<int32_t*>(ReinterpretCast<uint64_t>(dst) + offset * Literal<uint64_t>(4)), *dst),
            Return(r)
        );
    }

    SimulationFn simFn;
    uint64_t cnt = 0;
    {
        auto [fn, dst, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9] = NewFunction<GeneratedFn>("testFn");
        Value<bool> arr[x_numVars] = {a0, a1, a2, a3, a4, a5, a6, a7, a8, a9};
        auto [r, _simFn] = Build(dst, arr, x_numVars, cnt);
        fn.SetBody(Return(r));
        simFn = _simFn;
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    thread_pochiVMContext->m_curModule->PrepareForDebugInterp();
    thread_pochiVMContext->m_curModule->PrepareForFastInterp();
    thread_pochiVMContext->m_curModule->EmitIR();
    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    auto interpFn = thread_pochiVMContext->m_curModule->
                           GetDebugInterpGeneratedFunction<GeneratedFn>("testFn");

    FastInterpFunction<GeneratedFn> fastinterpFn = thread_pochiVMContext->m_curModule->
                GetFastInterpGeneratedFunction<GeneratedFn>("testFn");

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);
    GeneratedFn jitFn = jit.GetFunction<GeneratedFn>("testFn");

    int32_t* d1 = new int32_t[cnt + 1];
    Auto(delete [] d1);
    int32_t* d2 = new int32_t[cnt + 1];
    Auto(delete [] d2);
    int32_t* d3 = new int32_t[cnt + 1];
    Auto(delete [] d3);
    int32_t* d4 = new int32_t[cnt + 1];
    Auto(delete [] d4);

    auto CompareResults = [&simFn, &interpFn, &jitFn, &fastinterpFn, d1, d2, d3, d4, cnt](bool* input)
    {
        memset(d1, 0, sizeof(int32_t) * (cnt + 1));
        memset(d2, 0, sizeof(int32_t) * (cnt + 1));
        memset(d3, 0, sizeof(int32_t) * (cnt + 1));
        memset(d4, 0, sizeof(int32_t) * (cnt + 1));
        bool r1 = simFn(d1, input);
        bool r2 = interpFn(d2, input[0], input[1], input[2], input[3], input[4], input[5], input[6], input[7], input[8], input[9]);
        bool r3 = jitFn(d3, input[0], input[1], input[2], input[3], input[4], input[5], input[6], input[7], input[8], input[9]);
        bool r4 = fastinterpFn(d4, input[0], input[1], input[2], input[3], input[4], input[5], input[6], input[7], input[8], input[9]);
        ReleaseAssert(r1 == r2);
        ReleaseAssert(r1 == r3);
        ReleaseAssert(r1 == r4);
        for (uint64_t i = 0; i <= cnt; i++)
        {
            ReleaseAssert(d1[i] == d2[i]);
            ReleaseAssert(d1[i] == d3[i]);
            ReleaseAssert(d1[i] == d4[i]);
        }
    };

    for (uint64_t bitMask = 0; bitMask < (1ULL << x_numVars); bitMask++)
    {
        bool input[x_numVars];
        uint64_t x = bitMask;
        for (int i = 0; i < x_numVars; i++)
        {
            input[i] = (x % 2 == 1);
            x /= 2;
        }
        CompareResults(input);
    }
}

}   // anonymous namespace

TEST(SanityIrCodeDump, LogicalOp)
{
    AutoThreadPochiVMContext apv;
    AutoThreadErrorContext arc;
    AutoThreadLLVMCodegenContext alc;

    thread_pochiVMContext->m_curModule = new AstModule("test");

    using FnPrototype = bool(*)(int32_t*, bool, bool);
    using SideEffectFn = bool(*)(bool, int32_t*, uint64_t);

    {
        auto [fn, r, dst, offset] = NewFunction<SideEffectFn>("f", "r", "dst", "offset");
        fn.SetBody(
            Assign(*dst, *dst + Literal<int32_t>(1)),
            Assign(*ReinterpretCast<int32_t*>(ReinterpretCast<uint64_t>(dst) + offset * Literal<uint64_t>(4)), *dst),
            Return(r)
        );
    }

    {
        auto [fn, out, a, b] = NewFunction<FnPrototype>("logical_and", "out", "a", "b");
        fn.SetBody(
                Return(Call<SideEffectFn>("f", a, out, Literal<uint64_t>(1)) &&
                       Call<SideEffectFn>("f", b, out, Literal<uint64_t>(2)))
        );
    }

    {
        auto [fn, out, a, b] = NewFunction<FnPrototype>("logical_or", "out", "a", "b");
        fn.SetBody(
                Return(Call<SideEffectFn>("f", a, out, Literal<uint64_t>(1)) ||
                       Call<SideEffectFn>("f", b, out, Literal<uint64_t>(2)))
        );
    }

    {
        auto [fn, out, a, b] = NewFunction<FnPrototype>("logical_fn_3", "out", "a", "b");
        fn.SetBody(
                Return(!Call<SideEffectFn>("f", a, out, Literal<uint64_t>(1)) ||
                       Call<SideEffectFn>("f", b, out, Literal<uint64_t>(2)))
        );
    }

    {
        auto [fn, out, a, b] = NewFunction<FnPrototype>("logical_fn_4", "out", "a", "b");
        fn.SetBody(
                Return((!Call<SideEffectFn>("f", a, out, Literal<uint64_t>(1)) &&
                        Call<SideEffectFn>("f", b, out, Literal<uint64_t>(2))) ||
                       Call<SideEffectFn>("f", a || !b, out, Literal<uint64_t>(3)))
        );
    }

    {
        auto [fn, out, a, b] = NewFunction<FnPrototype>("logical_fn_5", "out", "a", "b");
        fn.SetBody(
                Return(Call<SideEffectFn>("f",
                                          Call<SideEffectFn>("f", a, out, Literal<uint64_t>(1)) ||
                                          Call<SideEffectFn>("f", b, out, Literal<uint64_t>(2)),
                                          out, Literal<uint64_t>(3)) &&
                       !Call<SideEffectFn>("f",
                                           Call<SideEffectFn>("f", a, out, Literal<uint64_t>(4)) &&
                                           Call<SideEffectFn>("f", b, out, Literal<uint64_t>(5)),
                                           out, Literal<uint64_t>(6))
                )
        );
    }

    ReleaseAssert(thread_pochiVMContext->m_curModule->Validate());
    ReleaseAssert(!thread_errorContext->HasError());
    thread_pochiVMContext->m_curModule->EmitIR();

    {
        std::string _dst;
        llvm::raw_string_ostream rso(_dst /*target*/);
        thread_pochiVMContext->m_curModule->GetBuiltLLVMModule()->print(rso, nullptr);
        std::string& dump = rso.str();

        AssertIsExpectedOutput(dump);
    }

    thread_pochiVMContext->m_curModule->OptimizeIRIfNotDebugMode();

    SideEffectFn f = [](bool r, int32_t* dst, uint64_t offset) -> bool {
        *dst += 1;
        dst[offset] = *dst;
        return r;
    };

    auto checkOne = [](const FnPrototype& fn, const std::function<std::remove_pointer_t<FnPrototype>>& gold, bool a, bool b)
    {
        const int sz = 7;
        int32_t d1[sz]; memset(d1, 0, sizeof(int32_t) * sz);
        int32_t d2[sz]; memset(d2, 0, sizeof(int32_t) * sz);
        bool r1 = fn(d1, a, b);
        bool r2 = gold(d2, a, b);
        ReleaseAssert(r1 == r2);
        for (int i = 0; i < sz; i++)
        {
            ReleaseAssert(d1[i] == d2[i]);
        }
    };

    auto check = [checkOne](const FnPrototype& fn, const std::function<std::remove_pointer_t<FnPrototype>>& gold) {
        checkOne(fn, gold, false, false);
        checkOne(fn, gold, false, true);
        checkOne(fn, gold, true, false);
        checkOne(fn, gold, true, true);
    };

    SimpleJIT jit;
    jit.SetModule(thread_pochiVMContext->m_curModule);

    {
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("logical_and");
        auto gold = [f](int32_t* dst, bool a, bool b) ->bool {
            return f(a, dst, 1) && f(b, dst, 2);
        };
        check(jitFn, gold);
    }

    {
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("logical_or");
        auto gold = [f](int32_t* dst, bool a, bool b) ->bool {
            return f(a, dst, 1) || f(b, dst, 2);
        };
        check(jitFn, gold);
    }

    {
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("logical_fn_3");
        auto gold = [f](int32_t* dst, bool a, bool b) ->bool {
            return !f(a, dst, 1) || f(b, dst, 2);
        };
        check(jitFn, gold);
    }

    {
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("logical_fn_4");
        auto gold = [f](int32_t* dst, bool a, bool b) ->bool {
            return (!f(a, dst, 1) && f(b, dst, 2)) || (f(a || !b, dst, 3));
        };
        check(jitFn, gold);
    }

    {
        FnPrototype jitFn = jit.GetFunction<FnPrototype>("logical_fn_5");
        auto gold = [f](int32_t* dst, bool a, bool b) ->bool {
            return f(f(a, dst, 1) || f(b, dst, 2), dst, 3) && !f(f(a, dst, 4) && f(b, dst, 5), dst, 6);
        };
        check(jitFn, gold);
    }
}

TEST(Sanity, LogicalOp)
{
    for (int i = 0; i < 100; i++)
    {
        CheckOnce();
    }
}
