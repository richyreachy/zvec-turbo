// SPDX-License-Identifier: Apache-2.0
#include "support/cpu_features.h"
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#elif defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif
#if defined(__aarch64__) && defined(__linux__)
#include <asm/hwcap.h>
#include <sys/auxv.h>
#elif defined(__aarch64__) && defined(__APPLE__)
#include <sys/sysctl.h>
#endif

namespace turbo::detail {
CpuFlags decode_x86(uint32_t ecx1, uint32_t edx1, uint32_t ebx7, uint32_t ecx7,
                    uint32_t edx7, uint64_t xcr0) {
  CpuFlags f;
  f.SSE2 = (edx1 & (1u << 26)) != 0;
  const uint32_t avx_os = (1u << 26) | (1u << 27) | (1u << 28);
  const bool ymm = (ecx1 & avx_os) == avx_os && (xcr0 & 6) == 6;
  f.F16C = ymm && (ecx1 & (1u << 29));
  // These bundles match the exact per-TU compiler flags in CMakeLists.txt.
  f.AVX2 = ymm && (ebx7 & (1u << 5)) && (ecx1 & (1u << 12)) && f.F16C;
  const bool zmm = f.AVX2 && (xcr0 & 0xe6) == 0xe6;
  f.AVX512BW = zmm && (ebx7 & (1u << 30));
  f.AVX512DQ = zmm && (ebx7 & (1u << 17));
  f.AVX512F = zmm && (ebx7 & (1u << 16)) && (ebx7 & (1u << 31)) && f.AVX512BW &&
              f.AVX512DQ;
  f.AVX512_VNNI = f.AVX512F && (ecx7 & (1u << 11));
  f.AVX512_FP16 = f.AVX512F && (edx7 & (1u << 23));
  return f;
}

static CpuFlags detect() {
  CpuFlags f;
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
  int r[4]{};
  __cpuidex(r, 0, 0);
  const int max_leaf = r[0];
  if (max_leaf < 1) return f;
  __cpuidex(r, 1, 0);
  const auto c1 = static_cast<uint32_t>(r[2]);
  const auto d1 = static_cast<uint32_t>(r[3]);
  uint64_t xcr0 = 0;
  if ((c1 & (3u << 26)) == (3u << 26)) xcr0 = _xgetbv(0);
  uint32_t b7 = 0, c7 = 0, d7 = 0;
  if (max_leaf >= 7) {
    __cpuidex(r, 7, 0);
    b7 = r[1];
    c7 = r[2];
    d7 = r[3];
  }
  f = decode_x86(c1, d1, b7, c7, d7, xcr0);
#elif defined(__x86_64__) || defined(__i386__)
  uint32_t a = 0, b = 0, c1 = 0, d1 = 0;
  if (!__get_cpuid(1, &a, &b, &c1, &d1)) return f;
  uint64_t xcr0 = 0;
  if ((c1 & (3u << 26)) == (3u << 26)) {
    uint32_t lo, hi;
    __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
    xcr0 = (static_cast<uint64_t>(hi) << 32) | lo;
  }
  uint32_t b7 = 0, c7 = 0, d7 = 0;
  if (__get_cpuid_max(0, nullptr) >= 7) __cpuid_count(7, 0, a, b7, c7, d7);
  f = decode_x86(c1, d1, b7, c7, d7, xcr0);
#elif defined(__aarch64__) || defined(_M_ARM64)
  f.NEON = true;  // AArch64 baseline
#if defined(__linux__) && defined(HWCAP_ASIMDHP)
  f.FP16 = (getauxval(AT_HWCAP) & HWCAP_ASIMDHP) != 0;
#elif defined(__APPLE__)
  int supported = 0;
  size_t size = sizeof(supported);
  f.FP16 = sysctlbyname("hw.optional.arm.FEAT_FP16", &supported, &size, nullptr,
                        0) == 0 &&
           supported != 0;
#endif
#endif
  return f;
}
const CpuFlags &CpuFeatures::get() {
  static const CpuFlags flags = detect();
  return flags;
}
}  // namespace turbo::detail
