// Auto-generated file. Do not edit!
//   Template: src/f32-hswish/scalar.c.in
//   Generator: tools/xngen
//
// Copyright 2019 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>

#include <xnnpack/common.h>
#include <xnnpack/math.h>
#include <xnnpack/vbinary.h>


void xnn_f32_hswish_ukernel__wasm_x4(
    size_t n,
    const float* x,
    float* y,
    const union xnn_f32_hswish_params params[restrict static 1])
{
  assert(n != 0);
  assert(n % sizeof(float) == 0);

  const float vsixth = params->scalar.sixth;
  const float vhalf = params->scalar.half;
  const float vone = params->scalar.one;
  assert(vhalf == 0.5f);
  assert(vone == 1.0f);

  for (; n >= 4 * sizeof(float); n -= 4 * sizeof(float)) {
    const float vx0 = x[0];
    const float vx1 = x[1];
    const float vx2 = x[2];
    const float vx3 = x[3];
    x += 4;

    float vacc0 = vx0 * vsixth + vhalf;
    float vacc1 = vx1 * vsixth + vhalf;
    float vacc2 = vx2 * vsixth + vhalf;
    float vacc3 = vx3 * vsixth + vhalf;

    vacc0 = __builtin_wasm_max_f32(vacc0, 0.0f);
    vacc1 = __builtin_wasm_max_f32(vacc1, 0.0f);
    vacc2 = __builtin_wasm_max_f32(vacc2, 0.0f);
    vacc3 = __builtin_wasm_max_f32(vacc3, 0.0f);

    vacc0 = __builtin_wasm_min_f32(vacc0, vone);
    vacc1 = __builtin_wasm_min_f32(vacc1, vone);
    vacc2 = __builtin_wasm_min_f32(vacc2, vone);
    vacc3 = __builtin_wasm_min_f32(vacc3, vone);

    vacc0 *= vx0;
    vacc1 *= vx1;
    vacc2 *= vx2;
    vacc3 *= vx3;

    y[0] = vacc0;
    y[1] = vacc1;
    y[2] = vacc2;
    y[3] = vacc3;
    y += 4;
  }
  if XNN_UNLIKELY(n != 0) {
    do {
      const float vx = *x++;
      float vacc = vx * vsixth + vhalf;
      vacc = __builtin_wasm_max_f32(vacc, 0.0f);
      vacc = __builtin_wasm_min_f32(vacc, vone);
      vacc = vacc * vx;
      *y++ = vacc;
      n -= sizeof(float);
    } while (n != 0);
  }
}
