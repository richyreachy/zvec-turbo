// SPDX-License-Identifier: Apache-2.0
#include <turbo/turbo.h>

#include <cstdio>
int main() {
  const float a[] = {1, 2, 3}, b[] = {3, 2, 1};
  auto kernels = turbo::get_distance_kernels(
      turbo::MetricType::kSquaredEuclidean, turbo::DataType::kFp32,
      turbo::QuantizeType::kFp32);
  if (!kernels.dist) return 1;
  float value = 0;
  kernels.dist(a, b, 3, &value);
  std::printf("Turbo %s: squared L2 = %.1f (%s)\n", turbo::version(), value,
              turbo::arch_name(kernels.arch));
  return value == 8 ? 0 : 1;
}
