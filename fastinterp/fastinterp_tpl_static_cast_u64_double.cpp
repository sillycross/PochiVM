#define POCHIVM_INSIDE_FASTINTERP_TPL_CPP
#define FASTINTERP_TPL_USE_MEDIUM_MCMODEL

#include "fastinterp_tpl_common.hpp"

#include <immintrin.h>
#include <emmintrin.h>

namespace PochiVM
{

struct FIStaticCastU64DoubleImpl
{
    template<bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP>
    static constexpr bool cond()
    {
        if (!FIOpaqueParamsHelper::CanPush(numOIP)) { return false; }
        if (!spillOutput)
        {
            if (!FIOpaqueParamsHelper::CanPush(numOFP)) { return false; }
        }
        return true;
    }

    // Placeholder rules:
    // constant placeholder 0: spill location, if spilled
    //
    template<bool spillOutput,
             FINumOpaqueIntegralParams numOIP,
             FINumOpaqueFloatingParams numOFP,
             typename... OpaqueParams>
    static void f(uintptr_t stackframe, OpaqueParams... opaqueParams, uint64_t qa) noexcept
    {
        double result;

        // Cast from uint64_t to double needs constant table
        // We don't support that relocation, and we don't have time to fix it now.
        // So just manually implement it.
        //
        {
            // constant table: 4985484788626358272, 4841369599423283200, 4985484787499139072
            //
            // movq    %rdi, %xmm1
            // punpckldq       .LCPI0_0(%rip), %xmm1
            // subpd   .LCPI0_1(%rip), %xmm1
            // movapd  %xmm1, %xmm0
            // unpckhpd        %xmm1, %xmm0
            // addsd   %xmm1, %xmm0
            //
            DEFINE_CONSTANT_PLACEHOLDER_1(double*);
            double* magic_cst = CONSTANT_PLACEHOLDER_1;
            double v0 = cxx2a_bit_cast<double>(qa);
            __m128d v1 = { v0, 0 };
            __m128d v2 { magic_cst[0], 0 };
            __m128d v3 = _mm_castsi128_pd(_mm_unpacklo_epi32(_mm_castpd_si128(v1), _mm_castpd_si128(v2)));
            __m128d v4 { magic_cst[1], magic_cst[2] };
            __m128d v5 = _mm_sub_pd(v3, v4);
            __m128d v6 = _mm_unpackhi_pd(v5, v5);
            __m128d v7 = _mm_add_sd(v5, v6);
            result = _mm_cvtsd_f64(v7);
        }

        if constexpr(!spillOutput)
        {
            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams..., double) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams..., result);
        }
        else
        {
            DEFINE_INDEX_CONSTANT_PLACEHOLDER_0;
            *GetLocalVarAddress<double>(stackframe, CONSTANT_PLACEHOLDER_0) = result;

            DEFINE_BOILERPLATE_FNPTR_PLACEHOLDER_0(void(*)(uintptr_t, OpaqueParams...) noexcept);
            BOILERPLATE_FNPTR_PLACEHOLDER_0(stackframe, opaqueParams...);
        }
    }

    static auto metavars()
    {
        return CreateMetaVarList(
                    CreateBoolMetaVar("spillOutput"),
                    CreateOpaqueIntegralParamsLimit(),
                    CreateOpaqueFloatParamsLimit()
        );
    }
};

}   // namespace PochiVM

// build_fast_interp_lib.cpp JIT entry point
//
extern "C"
void __pochivm_build_fast_interp_library__()
{
    using namespace PochiVM;
    RegisterBoilerplate<FIStaticCastU64DoubleImpl>(FIAttribute::CodeModelMedium);
}
