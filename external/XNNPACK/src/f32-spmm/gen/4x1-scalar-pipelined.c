// Auto-generated file. Do not edit!
//   Template: src/f32-spmm/scalar-pipelined.c.in
//   Generator: tools/xngen
//
// Copyright 2019 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>

#include <xnnpack/math.h>
#include <xnnpack/spmm.h>


void xnn_f32_spmm_ukernel_4x1__scalar_pipelined(
    uint32_t m,
    uint32_t n,
    const float*restrict a,
    const float*restrict weights,
    const int32_t*restrict widx_dmap,
    const uint32_t*restrict nidx_nnzmap,
    float*restrict c,
    const union xnn_f32_output_params params[restrict static 1])
{
  assert(m != 0);

  const float vmin = params->scalar.min;
  const float vmax = params->scalar.max;
  size_t i = m;
  while XNN_LIKELY(i >= 4) {
    const float*restrict w = weights;
    const int32_t* dmap = widx_dmap;
    const uint32_t* nnzmap = nidx_nnzmap;
    float vw = *w++;
    intptr_t diff = *dmap++;
    float va0 = a[0];
    float va1 = a[1];
    float va2 = a[2];
    float va3 = a[3];
    size_t j = n;
    do {
      uint32_t nnz = *nnzmap++;
      float vacc0 = vw;
      float vacc1 = vw;
      float vacc2 = vw;
      float vacc3 = vw;
      vw = *w++;
      if XNN_LIKELY(nnz != 0) {
        do {
          vacc0 += va0 * vw;
          vacc1 += va1 * vw;
          vacc2 += va2 * vw;
          vacc3 += va3 * vw;
          a = (const float*restrict) ((uintptr_t) a + (uintptr_t) diff);

          diff = *dmap++;
          vw = *w++;
          va0 = a[0];
          va1 = a[1];
          va2 = a[2];
          va3 = a[3];
        } while (--nnz != 0);
      }
      float vout0 = math_min_f32(vacc0, vmax);
      float vout1 = math_min_f32(vacc1, vmax);
      float vout2 = math_min_f32(vacc2, vmax);
      float vout3 = math_min_f32(vacc3, vmax);
      vout0 = math_max_f32(vout0, vmin);
      vout1 = math_max_f32(vout1, vmin);
      vout2 = math_max_f32(vout2, vmin);
      vout3 = math_max_f32(vout3, vmin);
      c[0] = vout0;
      c[1] = vout1;
      c[2] = vout2;
      c[3] = vout3;
      c += m;
    } while (--j != 0);
    c -= m * n;
    c += 4;
    a += 4;
    i -= 4;
  }
  if XNN_UNLIKELY(i != 0) {
    if (i & 2) {
      const float*restrict w = weights;
      const int32_t* dmap = widx_dmap;
      const uint32_t* nnzmap = nidx_nnzmap;
      float vw = *w++;
      intptr_t diff = *dmap++;
      float va0 = a[0];
      float va1 = a[1];
      size_t j = n;
      do {
        uint32_t nnz = *nnzmap++;
        float vacc0 = vw;
        float vacc1 = vw;
        vw = *w++;
        if XNN_LIKELY(nnz != 0) {
          do {
            vacc0 += va0 * vw;
            vacc1 += va1 * vw;
            a = (const float*restrict) ((uintptr_t) a + (uintptr_t) diff);

            diff = *dmap++;
            vw = *w++;
            va0 = a[0];
            va1 = a[1];
          } while (--nnz != 0);
        }
        float vout0 = math_min_f32(vacc0, vmax);
        float vout1 = math_min_f32(vacc1, vmax);
        vout0 = math_max_f32(vout0, vmin);
        vout1 = math_max_f32(vout1, vmin);
        c[0] = vout0;
        c[1] = vout1;
        c += m;
      } while (--j != 0);
      c -= m * n;
      c += 2;
      a += 2;
    }
    if (i & 1) {
      const float*restrict w = weights;
      const int32_t* dmap = widx_dmap;
      const uint32_t* nnzmap = nidx_nnzmap;
      float vw = *w++;
      intptr_t diff = *dmap++;
      float va0 = a[0];
      size_t j = n;
      do {
        uint32_t nnz = *nnzmap++;
        float vacc0 = vw;
        vw = *w++;
        if XNN_LIKELY(nnz != 0) {
          do {
            vacc0 += va0 * vw;
            a = (const float*restrict) ((uintptr_t) a + (uintptr_t) diff);

            diff = *dmap++;
            vw = *w++;
            va0 = a[0];
          } while (--nnz != 0);
        }
        float vout0 = math_min_f32(vacc0, vmax);
        vout0 = math_max_f32(vout0, vmin);
        c[0] = vout0;
        c += m;
      } while (--j != 0);
      c -= m * n;
      c += 1;
      a += 1;
    }
  }
}
