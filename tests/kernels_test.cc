// SPDX-License-Identifier: Apache-2.0
#include <turbo/fast_scan.h>
#include <turbo/rotation.h>
#include <turbo/turbo.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

#include "support/cpu_features.h"
using namespace turbo;
static int checks = 0;
#define CHECK(x)                                                   \
  do {                                                             \
    ++checks;                                                      \
    if (!(x)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); \
      std::exit(1);                                                \
    }                                                              \
  } while (0)
static void close(float a, float b, float tolerance = 2e-4f) {
  CHECK(std::isfinite(a) && std::abs(a - b) <= tolerance * (1 + std::abs(b)));
}
static const std::array<CpuArchType, 9> arches = {
    CpuArchType::kScalar,     CpuArchType::kAuto,   CpuArchType::kSSE2,
    CpuArchType::kAVX2,       CpuArchType::kAVX512, CpuArchType::kAVX512VNNI,
    CpuArchType::kAVX512FP16, CpuArchType::kNEON,   CpuArchType::kSVE};

static void distances() {
  for (size_t n :
       {0, 1, 3, 7, 15, 16, 17, 31, 32, 33, 63, 128, 129, 257, 768}) {
    std::vector<float> a(n + 1), b(n + 1);
    double ip = 0, l2 = 0;
    for (size_t i = 0; i < n; ++i) {
      a[i] = static_cast<float>(static_cast<int>(i % 13) - 6) / 13;
      b[i] = static_cast<float>(static_cast<int>(i % 7) - 3) / 7;
      ip += static_cast<double>(a[i]) * b[i];
      double delta = static_cast<double>(a[i]) - b[i];
      l2 += delta * delta;
    }
    for (auto m : {MetricType::kSquaredEuclidean, MetricType::kInnerProduct,
                   MetricType::kCosine}) {
      const float expected = static_cast<float>(
          m == MetricType::kSquaredEuclidean
              ? l2
              : (m == MetricType::kInnerProduct ? -ip : 1 - ip));
      for (auto arch : arches) {
        auto k =
            get_distance_kernels(m, DataType::kFp32, QuantizeType::kFp32, arch);
        if (!k.dist) {
          CHECK(!k.batch);
          continue;
        }
        CHECK(k.batch && is_arch_supported(k.arch));
        if (arch != CpuArchType::kAuto) CHECK(k.arch == arch);
        float result = 123;
        k.dist(a.data(), b.data(), n, &result);
        close(result, expected);
        const void* ptrs[] = {a.data(), a.data(), a.data(), a.data(), a.data()};
        float values[5] = {};
        k.batch(ptrs, b.data(), 5, n, values, nullptr);
        for (float value : values) close(value, expected);
        result = 123;
        k.batch(ptrs, b.data(), 0, n, &result, nullptr);
        CHECK(result == 123);
      }
    }
  }
}
static void half_precision() {
  std::vector<uint16_t> bits(65536), roundtrip(65536);
  std::vector<float> values(65536);
  for (size_t i = 0; i < bits.size(); ++i) bits[i] = static_cast<uint16_t>(i);
  fp16_to_fp32(bits.data(), bits.size(), values.data());
  fp32_to_fp16(values.data(), values.size(), roundtrip.data());
  for (size_t i = 0; i < bits.size(); ++i) {
    if ((i & 0x7c00) == 0x7c00 && (i & 1023))
      CHECK(std::isnan(values[i]));
    else
      CHECK(roundtrip[i] == bits[i]);
  }
  const float ties[] = {1.00048828125f, 1.00146484375f, 65504.f,
                        70000.f,        0x1p-24f,       0x1p-25f};
  uint16_t output[6]{};
  fp32_to_fp16(ties, 6, output);
  CHECK(output[0] == 0x3c00 && output[1] == 0x3c02 && output[2] == 0x7bff);
  CHECK(output[3] == 0x7c00 && output[4] == 1 && output[5] == 0);
  for (size_t n : {1, 7, 16, 31, 128, 129}) {
    std::vector<float> a(n, 1000), b(n, -1000);
    std::vector<uint16_t> ah(n), bh(n);
    fp32_to_fp16(a.data(), n, ah.data());
    fp32_to_fp16(b.data(), n, bh.data());
    for (auto arch : arches) {
      auto k = get_distance_kernels(MetricType::kSquaredEuclidean,
                                    DataType::kFp16, QuantizeType::kRaw, arch);
      if (!k.dist) continue;
      float distance = 0;
      k.dist(ah.data(), bh.data(), n, &distance);
      close(distance, 4000000.f * n);
    }
  }
  for (size_t n : {1, 7, 31, 128, 129}) {
    std::vector<float> a(n), b(n);
    std::vector<uint16_t> ah(n), bh(n);
    for (size_t i = 0; i < n; ++i) {
      a[i] = (static_cast<int>(i % 9) - 4) / 32.f;
      b[i] = (static_cast<int>(i % 7) - 3) / 32.f;
    }
    fp32_to_fp16(a.data(), n, ah.data());
    fp32_to_fp16(b.data(), n, bh.data());
    for (auto metric : {MetricType::kSquaredEuclidean,
                        MetricType::kInnerProduct, MetricType::kCosine}) {
      auto ref = get_distance_kernels(
          metric, DataType::kFp16, QuantizeType::kFp16, CpuArchType::kScalar);
      CHECK(ref.dist);
      float expected = 0;
      ref.dist(ah.data(), bh.data(), n, &expected);
      for (auto arch : arches) {
        auto k = get_distance_kernels(metric, DataType::kFp16,
                                      QuantizeType::kFp16, arch);
        if (!k.dist) continue;
        float out = 0;
        k.dist(ah.data(), bh.data(), n, &out);
        close(out, expected, 0.01f);
        const void* points[] = {ah.data(), ah.data(), ah.data()};
        float batch[3]{};
        k.batch(points, bh.data(), 3, n, batch, nullptr);
        for (float v : batch) close(v, expected, 0.01f);
      }
    }
  }
}
static void integer_distances() {
  for (size_t n : {8, 24, 32, 72, 128})
    for (auto type : {DataType::kInt4, DataType::kInt8}) {
      const size_t bytes = type == DataType::kInt4 ? n / 2 : n;
      const size_t tail = type == DataType::kInt4 ? 16 : 20;
      const size_t dim = type == DataType::kInt4 ? n + 32 : n + 20;
      std::vector<uint32_t> a((bytes + tail) / 4), b(a.size());
      auto *ac = reinterpret_cast<uint8_t*>(a.data()),
           *bc = reinterpret_cast<uint8_t*>(b.data());
      float suma = 0, sumb = 0, sqra = 0, sqrb = 0, l2 = 0, ip = 0;
      for (size_t i = 0; i < n; ++i) {
        int x = static_cast<int>(i % 13) - 6, y = static_cast<int>(i % 7) - 3;
        if (type == DataType::kInt4) {
          ac[i / 2] |= (x & 15) << ((i % 2) * 4);
          bc[i / 2] |= (y & 15) << ((i % 2) * 4);
        } else {
          ac[i] = static_cast<uint8_t>(x);
          bc[i] = static_cast<uint8_t>(y);
        }
        suma += x;
        sumb += y;
        sqra += x * x;
        sqrb += y * y;
        l2 += (x - y) * (x - y);
        ip += x * y;
      }
      float am[] = {1, 0, suma, sqra}, bm[] = {1, 0, sumb, sqrb};
      std::memcpy(ac + bytes, am, 16);
      std::memcpy(bc + bytes, bm, 16);
      if (type == DataType::kInt8) {
        int32_t ia = static_cast<int32_t>(suma),
                ib = static_cast<int32_t>(sumb);
        std::memcpy(ac + bytes + 16, &ia, 4);
        std::memcpy(bc + bytes + 16, &ib, 4);
      }
      for (auto metric :
           {MetricType::kSquaredEuclidean, MetricType::kInnerProduct})
        for (auto arch : arches) {
          auto k =
              get_distance_kernels(metric, type, QuantizeType::kRecord, arch);
          if (!k.dist) continue;
          const float expected =
              metric == MetricType::kSquaredEuclidean ? l2 : -ip;
          float value = 0;
          k.dist(a.data(), b.data(), dim, &value);
          close(value, expected);
          auto query = b;
          if (k.preprocess) k.preprocess(query.data(), dim);
          const void* points[] = {a.data(), a.data(), a.data()};
          float batch[3]{};
          k.batch(points, query.data(), 3, dim, batch, nullptr);
          for (float v : batch) close(v, expected);
        }
    }
  for (size_t n : {1, 7, 31, 128, 129}) {
    std::vector<uint8_t> a(n, 255), b(n, 0);
    for (auto arch : arches) {
      auto k = get_distance_kernels(MetricType::kSquaredEuclidean,
                                    DataType::kUint8, QuantizeType::kRaw, arch);
      if (!k.dist) continue;
      float value = 0;
      k.dist(a.data(), b.data(), n, &value);
      close(value, 65025.f * n);
    }
  }
}
static void pq() {
  for (size_t m : {1, 3, 8, 17, 32})
    for (auto type : {DataType::kInt4, DataType::kInt8}) {
      const size_t centers = type == DataType::kInt4 ? 16 : 256;
      std::vector<uint8_t> code(type == DataType::kInt4 ? (m + 1) / 2 : m, 0);
      std::vector<float> lut(m * centers), table(m * centers * centers);
      float expected = 0, symmetric = 0;
      for (size_t i = 0; i < lut.size(); ++i)
        lut[i] = static_cast<float>(i % 53) / 8;
      for (size_t i = 0; i < table.size(); ++i)
        table[i] = static_cast<float>(i % 31) / 4;
      for (size_t i = 0; i < m; ++i) {
        const uint8_t c = static_cast<uint8_t>((i * 7 + 1) % centers);
        if (centers == 16)
          code[i / 2] |= c << ((i % 2) * 4);
        else
          code[i] = c;
        expected += lut[i * centers + c];
        symmetric += table[i * centers * centers + c * centers + c];
      }
      for (auto arch : arches) {
        auto k = get_pq_kernels(type, QuantizeType::kPQ, arch);
        if (!k.asymmetric_distance) continue;
        float d = 0;
        k.asymmetric_distance(code.data(), lut.data(), m, &d);
        close(d, expected);
        k.symmetric_distance(code.data(), code.data(), table.data(), m, &d);
        close(d, symmetric);
        const void* codes[] = {code.data(), code.data(), code.data(),
                               code.data(), code.data()};
        float batch[5]{};
        k.batch_asymmetric_distance(codes, lut.data(), 5, m, batch, nullptr);
        for (float v : batch) close(v, expected);
      }
    }
  for (size_t m : {1, 3, 8, 17, 64}) {
    const size_t stride = (m + 1) / 2;
    std::vector<uint8_t> codes(32 * stride, 0), lut(m * 16);
    for (size_t v = 0; v < 32; ++v)
      for (size_t j = 0; j < m; ++j)
        codes[v * stride + j / 2] |= ((v + j) % 16) << ((j % 2) * 4);
    for (size_t i = 0; i < lut.size(); ++i)
      lut[i] = static_cast<uint8_t>((i * 13) % 256);
    std::vector<uint8_t> packed(fast_scan_packed_block_size(m)),
        plut(fast_scan_packed_lut_size(m));
    fast_scan_pack_codes(codes.data(), 32, stride, m, packed.data());
    fast_scan_pack_lut(lut.data(), m, plut.data());
    for (auto arch : arches) {
      auto k = get_pq_kernels(DataType::kInt4, QuantizeType::kPQFast, arch);
      if (!k.fast_scan) continue;
      int32_t out[32];
      k.fast_scan(packed.data(), plut.data(), m, out);
      for (size_t v = 0; v < 32; ++v) {
        int expected = 0;
        for (size_t j = 0; j < m; ++j) expected += lut[j * 16 + (v + j) % 16];
        CHECK(out[v] == expected);
      }
    }
  }
}
static void rotations() {
  for (size_t n : {1, 3, 8, 31, 64, 129}) {
    FhtRotation r(n, 42);
    std::vector<float> input(n), expected(n), out(n), back(n);
    for (size_t i = 0; i < n; ++i) input[i] = static_cast<float>(i % 11) - 5;
    r.apply(input.data(), expected.data(), CpuArchType::kScalar);
    for (auto arch : arches) {
      if (!get_rotator_kernels(RotateType::kFht, arch).rotate) continue;
      r.apply(input.data(), out.data(), arch);
      r.inverse(out.data(), back.data(), arch);
      for (size_t i = 0; i < n; ++i) {
        close(out[i], expected[i]);
        close(back[i], input[i]);
      }
    }
  }
  const float matrix[] = {0, 1, 0, 1, 0, 0, 0, 0, -1}, input[] = {1, 2, 3};
  for (auto arch : arches) {
    auto k = get_rotator_kernels(RotateType::kOpq, arch);
    if (!k.rotate) continue;
    float out[3], back[3];
    k.rotate(input, out, 3, 3, const_cast<float*>(matrix));
    close(out[0], 2);
    close(out[1], 1);
    close(out[2], -3);
    k.unrotate(out, back, 3, 3, const_cast<float*>(matrix));
    for (int i = 0; i < 3; ++i) close(back[i], input[i]);
  }
}
static void dispatch_contract() {
  for (auto arch : arches) {
    if (arch == CpuArchType::kAuto || is_arch_supported(arch)) continue;
    CHECK(!get_distance_func(MetricType::kInnerProduct, DataType::kFp32,
                             QuantizeType::kFp32, arch));
    CHECK(!get_pq_kernels(DataType::kInt4, QuantizeType::kPQFast, arch)
               .fast_scan);
    CHECK(!get_rotator_kernels(RotateType::kFht, arch).rotate);
  }
  CHECK(!get_pq_kernels(DataType::kFp32).asymmetric_distance);
  CHECK(!get_pq_kernels(DataType::kInt8, QuantizeType::kPQFast).fast_scan);
  CHECK(!get_rotator_kernels(static_cast<RotateType>(999)).rotate);
  CHECK(!get_distance_func(MetricType::kUnknown, DataType::kFp32,
                           QuantizeType::kFp32));
  CHECK(!get_uniform_quantize_func(DataType::kFp32));
  CHECK(!get_convert_func(DataType::kUnknown));
  const uint32_t all = 0xffffffffu;
  auto f = detail::decode_x86(all, all, all, all, all, 0xe6);
  CHECK(f.AVX2 && f.AVX512F && f.AVX512_VNNI && f.AVX512_FP16);
  f = detail::decode_x86(all & ~(1u << 27), all, all, all, all, 0xe6);
  CHECK(f.SSE2 && !f.AVX2 && !f.AVX512F);
  f = detail::decode_x86(all, all, all, all, all, 6);
  CHECK(f.AVX2 && !f.AVX512F);
  f = detail::decode_x86(all, all, all, all, all, 2);
  CHECK(!f.AVX2 && !f.F16C);
  f = detail::decode_x86(all, all, all & ~(1u << 30), all, all, 0xe6);
  CHECK(!f.AVX512F && !f.AVX512_VNNI);
}
int main() {
  distances();
  half_precision();
  integer_distances();
  pq();
  rotations();
  dispatch_contract();
  std::printf("PASS %d checks (only usable ISA kernels execute)\n", checks);
}
