// SPDX-License-Identifier: Apache-2.0
#include <turbo/fast_scan.h>
#include <turbo/rotation.h>
#include <turbo/turbo.h>

#include <cmath>
int main() {
  const float a[] = {1, 2, 3}, b[] = {3, 2, 1};
  auto k = turbo::get_distance_kernels(turbo::MetricType::kSquaredEuclidean,
                                       turbo::DataType::kFp32,
                                       turbo::QuantizeType::kFp32);
  float result = 0;
  if (!k.dist) return 1;
  k.dist(a, b, 3, &result);
  if (result != 8) return 2;
  turbo::FhtRotation rotation(3);
  float rotated[3], restored[3];
  rotation.apply(a, rotated);
  rotation.inverse(rotated, restored);
  for (int i = 0; i < 3; ++i)
    if (std::abs(restored[i] - a[i]) > 1e-4f) return 3;
  return turbo::fast_scan_packed_block_size(3) == 64 ? 0 : 4;
}
