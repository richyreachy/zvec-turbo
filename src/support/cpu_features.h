// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
namespace turbo::detail {
struct CpuFlags {
  bool SSE2 = false, AVX2 = false, AVX512F = false;
  bool AVX512BW = false, AVX512DQ = false, AVX512_VNNI = false;
  bool AVX512_FP16 = false, F16C = false, NEON = false, FP16 = false;
};
// Kept separate from CPUID acquisition so OS-state rejection can be tested.
CpuFlags decode_x86(uint32_t ecx1, uint32_t edx1, uint32_t ebx7, uint32_t ecx7,
                    uint32_t edx7, uint64_t xcr0);
struct CpuFeatures {
  static const CpuFlags &get();
};
}  // namespace turbo::detail
