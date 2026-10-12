// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <turbo/turbo.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>
namespace turbo {
// Owns the aligned context required by the low-level FHT kernels. This class
// has value semantics. Its in-memory representation is not a storage format.
class FhtRotation {
 public:
  explicit FhtRotation(size_t dim, uint32_t seed = 0) : dim_(dim) {
    static_assert(sizeof(size_t) == 8, "FHT requires a 64-bit target");
    if (dim == 0 || dim > static_cast<size_t>(std::numeric_limits<int>::max()))
      throw std::invalid_argument("FHT dimension must be in [1, INT_MAX]");
    const size_t offset = (dim + 7) / 8;
    size_t trunc = 1;
    while (trunc <= dim / 2) trunc *= 2;
    storage_.resize((24 + 4 * offset + 7) / 8);
    auto *bytes = reinterpret_cast<uint8_t *>(storage_.data());
    const float factor = 1.0f / std::sqrt(static_cast<float>(trunc));
    std::memcpy(bytes, &offset, 8);
    std::memcpy(bytes + 8, &trunc, 8);
    std::memcpy(bytes + 16, &factor, 4);
    std::mt19937 gen(seed);
    for (size_t i = 0; i < 4 * offset; ++i)
      bytes[24 + i] = static_cast<uint8_t>(gen());
  }
  size_t dimension() const { return dim_; }
  void apply(const float *input, float *output,
             CpuArchType arch = CpuArchType::kAuto) const {
    auto k = get_rotator_kernels(RotateType::kFht, arch);
    if (!k.rotate)
      throw std::invalid_argument("requested FHT ISA is unavailable");
    k.rotate(input, output, dim_, dim_, context());
  }
  void inverse(const float *input, float *output,
               CpuArchType arch = CpuArchType::kAuto) const {
    auto k = get_rotator_kernels(RotateType::kFht, arch);
    if (!k.unrotate)
      throw std::invalid_argument("requested FHT ISA is unavailable");
    k.unrotate(input, output, dim_, dim_, context());
  }

 private:
  void *context() const { return const_cast<uint64_t *>(storage_.data()); }
  size_t dim_;
  std::vector<uint64_t> storage_;
};
}  // namespace turbo
