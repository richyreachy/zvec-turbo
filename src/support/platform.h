// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#if defined(__aarch64__) || defined(_M_ARM64)
#define TURBO_ARM64 1
#endif
#if defined(__ARM_NEON) || defined(_M_ARM64)
#include <arm_neon.h>
#define TURBO_ARM64_NEON 1
#endif
#if defined(__aarch64__) && defined(__ARM_NEON)
#define TURBO_ARM64_GNU_LIKE 1
#endif
#if defined(_MSC_VER)
#include <intrin.h>
#define turbo_force_inline __forceinline
#else
#define turbo_force_inline inline __attribute__((always_inline))
#endif
#define turbo_assert_with(condition, message) assert((condition) && (message))
inline void turbo_prefetch(const void *p) {
  if (!p) return;
#if defined(__GNUC__) || defined(__clang__)
  __builtin_prefetch(p, 0, 3);
#elif defined(_M_X64) || defined(_M_IX86)
  _mm_prefetch(static_cast<const char *>(p), _MM_HINT_T0);
#endif
}
