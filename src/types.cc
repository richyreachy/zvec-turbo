// SPDX-License-Identifier: Apache-2.0
#include <turbo/turbo.h>

#include "support/float_helper.h"
namespace turbo {
void fp32_to_fp16(const float *input, size_t count, uint16_t *output) {
  detail::FloatHelper::ToFP16(input, count, output);
}
void fp16_to_fp32(const uint16_t *input, size_t count, float *output) {
  detail::FloatHelper::ToFP32(input, count, output);
}
}  // namespace turbo
