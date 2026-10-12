// SPDX-License-Identifier: Apache-2.0
#include "support/float_helper.h"

#include <cstring>
namespace turbo::detail {
float FloatHelper::ToFP32(uint16_t h) {
  const uint32_t sign = static_cast<uint32_t>(h & 0x8000) << 16;
  uint32_t exp = (h >> 10) & 31, mant = h & 1023, bits;
  if (exp == 0) {
    if (!mant)
      bits = sign;
    else {
      int e = -14;
      while ((mant & 1024) == 0) {
        mant <<= 1;
        --e;
      }
      bits =
          sign | (static_cast<uint32_t>(e + 127) << 23) | ((mant & 1023) << 13);
    }
  } else if (exp == 31)
    bits = sign | 0x7f800000u | (mant << 13);
  else
    bits = sign | ((exp + 112) << 23) | (mant << 13);
  float out;
  std::memcpy(&out, &bits, sizeof(out));
  return out;
}
uint16_t FloatHelper::ToFP16(float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  const uint16_t sign = static_cast<uint16_t>((bits >> 16) & 0x8000);
  const uint32_t exponent = (bits >> 23) & 255, mantissa = bits & 0x7fffff;
  if (exponent == 255) return sign | (mantissa ? 0x7e00 : 0x7c00);
  const int e = static_cast<int>(exponent) - 127 + 15;
  if (e >= 31) return sign | 0x7c00;
  if (e < -10) return sign;
  if (e <= 0) {
    const uint32_t m = mantissa | 0x800000;
    const unsigned shift = static_cast<unsigned>(14 - e);
    const uint32_t half = 1u << (shift - 1);
    const uint32_t rounded = (m + half - 1 + ((m >> shift) & 1)) >> shift;
    return static_cast<uint16_t>(sign | rounded);
  }
  const uint32_t rounded = mantissa + 0xfff + ((mantissa >> 13) & 1);
  return static_cast<uint16_t>(
      sign | ((static_cast<uint32_t>(e) << 10) + (rounded >> 13)));
}
void FloatHelper::ToFP32(const uint16_t *a, size_t n, float *out) {
  for (size_t i = 0; i < n; ++i) out[i] = ToFP32(a[i]);
}
void FloatHelper::ToFP32(const uint16_t *a, size_t n, float norm, float *out) {
  for (size_t i = 0; i < n; ++i) out[i] = ToFP32(a[i]) / norm;
}
void FloatHelper::ToFP16(const float *a, size_t n, uint16_t *out) {
  for (size_t i = 0; i < n; ++i) out[i] = ToFP16(a[i]);
}
void FloatHelper::ToFP16(const float *a, size_t n, float norm, uint16_t *out) {
  for (size_t i = 0; i < n; ++i) out[i] = ToFP16(a[i] / norm);
}
}  // namespace turbo::detail
