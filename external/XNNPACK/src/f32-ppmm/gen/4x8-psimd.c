// Auto-generated file. Do not edit!
//   Template: src/f32-ppmm/psimd.c.in
//   Generator: tools/xngen
//
// Copyright 2019 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>

#include <psimd.h>

#include <xnnpack/ppmm.h>


void xnn_f32_ppmm_ukernel_4x8__psimd(
  size_t mr,
  size_t nc,
  size_t kc,
  const float*restrict a,
  const float*restrict w,
  float*restrict c,
  size_t cm_stride,
  size_t cn_stride,
  const union xnn_f32_output_params params[restrict static 1])
{
  assert(mr != 0);
  assert(mr <= 4);
  assert(nc != 0);
  assert(kc != 0);
  assert(kc % sizeof(float) == 0);

  float* c0 = c;
  float* c1 = (float*) ((uintptr_t) c0 + cm_stride);
  if XNN_UNPREDICTABLE(mr < 2) {
    c1 = c0;
  }
  float* c2 = (float*) ((uintptr_t) c1 + cm_stride);
  if XNN_UNPREDICTABLE(mr <= 2) {
    c2 = c1;
  }
  float* c3 = (float*) ((uintptr_t) c2 + cm_stride);
  if XNN_UNPREDICTABLE(mr != 4) {
    c3 = c2;
  }

  do {
    psimd_f32 vacc0x0123 = psimd_load_f32(w);
    psimd_f32 vacc0x4567 = psimd_load_f32(w + 4);
    psimd_f32 vacc1x0123 = vacc0x0123;
    psimd_f32 vacc1x4567 = vacc0x4567;
    psimd_f32 vacc2x0123 = vacc0x0123;
    psimd_f32 vacc2x4567 = vacc0x4567;
    psimd_f32 vacc3x0123 = vacc0x0123;
    psimd_f32 vacc3x4567 = vacc0x4567;
    w += 8;

    size_t k = kc;
    do {
      const psimd_f32 va0123 = psimd_load_f32(a);
      a += 4;

      const psimd_f32 vb0123 = psimd_load_f32(w);
      const psimd_f32 vb4567 = psimd_load_f32(w + 4);
      w += 8;

      const psimd_f32 va0000 = psimd_splat0_f32(va0123);
      const psimd_f32 va1111 = psimd_splat1_f32(va0123);
      const psimd_f32 va2222 = psimd_splat2_f32(va0123);
      const psimd_f32 va3333 = psimd_splat3_f32(va0123);

      vacc0x0123 = psimd_qfma_f32(vacc0x0123, va0000, vb0123);
      vacc1x0123 = psimd_qfma_f32(vacc1x0123, va1111, vb0123);
      vacc2x0123 = psimd_qfma_f32(vacc2x0123, va2222, vb0123);
      vacc3x0123 = psimd_qfma_f32(vacc3x0123, va3333, vb0123);
      vacc0x4567 = psimd_qfma_f32(vacc0x4567, va0000, vb4567);
      vacc1x4567 = psimd_qfma_f32(vacc1x4567, va1111, vb4567);
      vacc2x4567 = psimd_qfma_f32(vacc2x4567, va2222, vb4567);
      vacc3x4567 = psimd_qfma_f32(vacc3x4567, va3333, vb4567);

      k -= sizeof(float);
    } while (k != 0);

    const psimd_f32 vmax = psimd_load_splat_f32(&params->scalar.max);
    vacc0x0123 = psimd_min_f32(vacc0x0123, vmax);
    vacc1x0123 = psimd_min_f32(vacc1x0123, vmax);
    vacc2x0123 = psimd_min_f32(vacc2x0123, vmax);
    vacc3x0123 = psimd_min_f32(vacc3x0123, vmax);
    vacc0x4567 = psimd_min_f32(vacc0x4567, vmax);
    vacc1x4567 = psimd_min_f32(vacc1x4567, vmax);
    vacc2x4567 = psimd_min_f32(vacc2x4567, vmax);
    vacc3x4567 = psimd_min_f32(vacc3x4567, vmax);

    const psimd_f32 vmin = psimd_load_splat_f32(&params->scalar.min);
    vacc0x0123 = psimd_max_f32(vacc0x0123, vmin);
    vacc1x0123 = psimd_max_f32(vacc1x0123, vmin);
    vacc2x0123 = psimd_max_f32(vacc2x0123, vmin);
    vacc3x0123 = psimd_max_f32(vacc3x0123, vmin);
    vacc0x4567 = psimd_max_f32(vacc0x4567, vmin);
    vacc1x4567 = psimd_max_f32(vacc1x4567, vmin);
    vacc2x4567 = psimd_max_f32(vacc2x4567, vmin);
    vacc3x4567 = psimd_max_f32(vacc3x4567, vmin);

    if XNN_LIKELY(nc >= 8) {
      psimd_store_f32(c3, vacc3x0123);
      psimd_store_f32(c3 + 4, vacc3x4567);
      psimd_store_f32(c2, vacc2x0123);
      psimd_store_f32(c2 + 4, vacc2x4567);
      psimd_store_f32(c1, vacc1x0123);
      psimd_store_f32(c1 + 4, vacc1x4567);
      psimd_store_f32(c0, vacc0x0123);
      psimd_store_f32(c0 + 4, vacc0x4567);

      a = (const float*) ((uintptr_t) a - kc * 4);

      c3 = (float*) ((uintptr_t) c3 + cn_stride);
      c2 = (float*) ((uintptr_t) c2 + cn_stride);
      c1 = (float*) ((uintptr_t) c1 + cn_stride);
      c0 = (float*) ((uintptr_t) c0 + cn_stride);

      nc -= 8;
    } else {
      if (nc & 4) {
        psimd_store_f32(c3, vacc3x0123);
        psimd_store_f32(c2, vacc2x0123);
        psimd_store_f32(c1, vacc1x0123);
        psimd_store_f32(c0, vacc0x0123);

        vacc3x0123 = vacc3x4567;
        vacc2x0123 = vacc2x4567;
        vacc1x0123 = vacc1x4567;
        vacc0x0123 = vacc0x4567;

        c3 += 4;
        c2 += 4;
        c1 += 4;
        c0 += 4;
      }
      if (nc & 2) {
        psimd_store2_f32(c3, vacc3x0123);
        psimd_store2_f32(c2, vacc2x0123);
        psimd_store2_f32(c1, vacc1x0123);
        psimd_store2_f32(c0, vacc0x0123);

        vacc3x0123 = psimd_concat_hi_f32(vacc3x0123, vacc3x0123);
        vacc2x0123 = psimd_concat_hi_f32(vacc2x0123, vacc2x0123);
        vacc1x0123 = psimd_concat_hi_f32(vacc1x0123, vacc1x0123);
        vacc0x0123 = psimd_concat_hi_f32(vacc0x0123, vacc0x0123);

        c3 += 2;
        c2 += 2;
        c1 += 2;
        c0 += 2;
      }
      if (nc & 1) {
        psimd_store1_f32(c3, vacc3x0123);
        psimd_store1_f32(c2, vacc2x0123);
        psimd_store1_f32(c1, vacc1x0123);
        psimd_store1_f32(c0, vacc0x0123);
      }

      nc = 0;
    }
  } while (nc != 0);
}
