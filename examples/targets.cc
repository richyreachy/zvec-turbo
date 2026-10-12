// SPDX-License-Identifier: Apache-2.0
#include <turbo/turbo.h>

#include <cstdio>
#include <initializer_list>
int main() {
  using A = turbo::CpuArchType;
  for (A arch : {A::kScalar, A::kSSE2, A::kAVX2, A::kAVX512, A::kAVX512VNNI,
                 A::kAVX512FP16, A::kNEON})
    std::printf("%-14s compiled=%d usable=%d\n", turbo::arch_name(arch),
                turbo::is_arch_compiled(arch), turbo::is_arch_supported(arch));
}
