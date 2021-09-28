// Auto-generated file. Do not edit!
//   Template: src/f32-vbinary/vopc-psimd.c.in
//   Generator: tools/xngen
//
// Copyright 2019 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>

#include <psimd.h>

#include <xnnpack/common.h>
#include <xnnpack/vbinary.h>


void xnn_f32_vaddc_ukernel__psimd_x8(
    size_t n,
    const float* a,
    const float* b,
    float* y,
    const union xnn_f32_output_params params[restrict static 1])
{
  assert(n != 0);
  assert(n % sizeof(float) == 0);

  const psimd_f32 vy_min = psimd_load_splat_f32(&params->scalar.min);
  const psimd_f32 vy_max = psimd_load_splat_f32(&params->scalar.max);

  const psimd_f32 vb = psimd_load_splat_f32(b);
  for (; n >= 8 * sizeof(float); n -= 8 * sizeof(float)) {
    const psimd_f32 va0123 = psimd_load_f32(a);
    const psimd_f32 va4567 = psimd_load_f32(a + 4);
    a += 8;

    psimd_f32 vy0123 = psimd_add_f32(va0123, vb);
    psimd_f32 vy4567 = psimd_add_f32(va4567, vb);

    vy0123 = psimd_max_f32(vy0123, vy_min);
    vy4567 = psimd_max_f32(vy4567, vy_min);

    vy0123 = psimd_min_f32(vy0123, vy_max);
    vy4567 = psimd_min_f32(vy4567, vy_max);

    psimd_store_f32(y, vy0123);
    psimd_store_f32(y + 4, vy4567);
    y += 8;
  }
  for (; n >= 4 * sizeof(float); n -= 4 * sizeof(float)) {
    const psimd_f32 va0123 = psimd_load_f32(a);
    a += 4;

    psimd_f32 vy0123 = psimd_add_f32(va0123, vb);
    vy0123 = psimd_max_f32(vy0123, vy_min);
    vy0123 = psimd_min_f32(vy0123, vy_max);
    psimd_store_f32(y, vy0123);
    y += 4;
  }
  if XNN_UNLIKELY(n != 0) {
    const psimd_f32 va0123 = psimd_load_f32(a);

    psimd_f32 vy0123 = psimd_add_f32(va0123, vb);
    vy0123 = psimd_max_f32(vy0123, vy_min);
    vy0123 = psimd_min_f32(vy0123, vy_max);
    if (n & (2 * sizeof(float))) {
      psimd_store2_f32(y, vy0123);
      vy0123 = psimd_concat_hi_f32(vy0123, vy0123);
      y += 2;
    }
    if (n & (1 * sizeof(float))) {
      psimd_store1_f32(y, vy0123);
    }
  }
}
