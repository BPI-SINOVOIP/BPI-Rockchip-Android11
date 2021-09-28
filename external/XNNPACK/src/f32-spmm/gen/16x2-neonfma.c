// Auto-generated file. Do not edit!
//   Template: src/f32-spmm/neon-blocked.c.in
//   Generator: tools/xngen
//
// Copyright 2019 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>

#include <arm_neon.h>

#include <xnnpack/spmm.h>


void xnn_f32_spmm_ukernel_16x2__neonfma(
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

  const float32x4_t vmin = vld1q_dup_f32(&params->scalar.min);
  const float32x4_t vmax = vld1q_dup_f32(&params->scalar.max);
  size_t i = m;
  while XNN_LIKELY(i >= 16) {
    const float*restrict w = weights;
    const int32_t* dmap = widx_dmap;
    const uint32_t* nnzmap = nidx_nnzmap;
    size_t j = n;
    while (j >= 2) {
      uint32_t nnz = *nnzmap++;
      float32x4_t vacc0123c0 = vld1q_dup_f32(w); w += 1;
      float32x4_t vacc4567c0 = vacc0123c0;
      float32x4_t vacc89ABc0 = vacc0123c0;
      float32x4_t vaccCDEFc0 = vacc0123c0;
      float32x4_t vacc0123c1 = vld1q_dup_f32(w); w += 1;
      float32x4_t vacc4567c1 = vacc0123c1;
      float32x4_t vacc89ABc1 = vacc0123c1;
      float32x4_t vaccCDEFc1 = vacc0123c1;
      if XNN_LIKELY(nnz != 0) {
        do {
          const intptr_t diff = *dmap++;
          const float32x4_t va0123 = vld1q_f32(a);
          const float32x4_t va4567 = vld1q_f32(a + 4);
          const float32x4_t va89AB = vld1q_f32(a + 8);
          const float32x4_t vaCDEF = vld1q_f32(a + 12);
          __builtin_prefetch(a + 16);
          a = (const float*restrict) ((uintptr_t) a + (uintptr_t) diff);
          const float32x2_t vb = vld1_f32(w); w += 2;

          vacc0123c0 = vfmaq_lane_f32(vacc0123c0, va0123, vb, 0);
          vacc4567c0 = vfmaq_lane_f32(vacc4567c0, va4567, vb, 0);
          vacc89ABc0 = vfmaq_lane_f32(vacc89ABc0, va89AB, vb, 0);
          vaccCDEFc0 = vfmaq_lane_f32(vaccCDEFc0, vaCDEF, vb, 0);
          vacc0123c1 = vfmaq_lane_f32(vacc0123c1, va0123, vb, 1);
          vacc4567c1 = vfmaq_lane_f32(vacc4567c1, va4567, vb, 1);
          vacc89ABc1 = vfmaq_lane_f32(vacc89ABc1, va89AB, vb, 1);
          vaccCDEFc1 = vfmaq_lane_f32(vaccCDEFc1, vaCDEF, vb, 1);
        } while (--nnz != 0);
      }
      float32x4_t vout0123c0 = vminq_f32(vacc0123c0, vmax);
      float32x4_t vout4567c0 = vminq_f32(vacc4567c0, vmax);
      float32x4_t vout89ABc0 = vminq_f32(vacc89ABc0, vmax);
      float32x4_t voutCDEFc0 = vminq_f32(vaccCDEFc0, vmax);
      float32x4_t vout0123c1 = vminq_f32(vacc0123c1, vmax);
      float32x4_t vout4567c1 = vminq_f32(vacc4567c1, vmax);
      float32x4_t vout89ABc1 = vminq_f32(vacc89ABc1, vmax);
      float32x4_t voutCDEFc1 = vminq_f32(vaccCDEFc1, vmax);

      vout0123c0 = vmaxq_f32(vout0123c0, vmin);
      vout4567c0 = vmaxq_f32(vout4567c0, vmin);
      vout89ABc0 = vmaxq_f32(vout89ABc0, vmin);
      voutCDEFc0 = vmaxq_f32(voutCDEFc0, vmin);
      vout0123c1 = vmaxq_f32(vout0123c1, vmin);
      vout4567c1 = vmaxq_f32(vout4567c1, vmin);
      vout89ABc1 = vmaxq_f32(vout89ABc1, vmin);
      voutCDEFc1 = vmaxq_f32(voutCDEFc1, vmin);

      vst1q_f32(c + 0 * m + 0, vout0123c0);
      vst1q_f32(c + 0 * m + 4, vout4567c0);
      vst1q_f32(c + 0 * m + 8, vout89ABc0);
      vst1q_f32(c + 0 * m + 12, voutCDEFc0);
      vst1q_f32(c + 1 * m + 0, vout0123c1);
      vst1q_f32(c + 1 * m + 4, vout4567c1);
      vst1q_f32(c + 1 * m + 8, vout89ABc1);
      vst1q_f32(c + 1 * m + 12, voutCDEFc1);
      c += 2 * m;
      j -= 2;
    }

    // clean up loop, fall back to nr=1
    if XNN_UNLIKELY(j != 0) {
      do {
        uint32_t nnz = *nnzmap++;
        float32x4_t vacc0123 = vld1q_dup_f32(w); w += 1;
        float32x4_t vacc4567 = vacc0123;
        float32x4_t vacc89AB = vacc0123;
        float32x4_t vaccCDEF = vacc0123;
        if XNN_LIKELY(nnz != 0) {
          do {
            const intptr_t diff = *dmap++;
            const float32x4_t va0123 = vld1q_f32(a);
            const float32x4_t va4567 = vld1q_f32(a + 4);
            const float32x4_t va89AB = vld1q_f32(a + 8);
            const float32x4_t vaCDEF = vld1q_f32(a + 12);
            a = (const float*restrict) ((uintptr_t) a + (uintptr_t) diff);
            const float32x4_t vb = vld1q_dup_f32(w); w += 1;
            vacc0123 = vfmaq_f32(vacc0123, va0123, vb);
            vacc4567 = vfmaq_f32(vacc4567, va4567, vb);
            vacc89AB = vfmaq_f32(vacc89AB, va89AB, vb);
            vaccCDEF = vfmaq_f32(vaccCDEF, vaCDEF, vb);
          } while (--nnz != 0);
        }
        float32x4_t vout0123 = vminq_f32(vacc0123, vmax);
        float32x4_t vout4567 = vminq_f32(vacc4567, vmax);
        float32x4_t vout89AB = vminq_f32(vacc89AB, vmax);
        float32x4_t voutCDEF = vminq_f32(vaccCDEF, vmax);

        vout0123 = vmaxq_f32(vout0123, vmin);
        vout4567 = vmaxq_f32(vout4567, vmin);
        vout89AB = vmaxq_f32(vout89AB, vmin);
        voutCDEF = vmaxq_f32(voutCDEF, vmin);

        vst1q_f32(c + 0, vout0123);
        vst1q_f32(c + 4, vout4567);
        vst1q_f32(c + 8, vout89AB);
        vst1q_f32(c + 12, voutCDEF);
        c += m;
        j -= 1;
      } while (j != 0);
    }
    c -= m * n;
    c += 16;
    a += 16;
    i -= 16;
  }
  if XNN_UNLIKELY(i != 0) {
    if (i & 8) {
      const float*restrict w = weights;
      const int32_t* dmap = widx_dmap;
      const uint32_t* nnzmap = nidx_nnzmap;
      size_t j = n;
      while (j >= 2) {
        uint32_t nnz = *nnzmap++;
        float32x4_t vacc0123c0 = vld1q_dup_f32(w); w += 1;
        float32x4_t vacc4567c0 = vacc0123c0;
        float32x4_t vacc0123c1 = vld1q_dup_f32(w); w += 1;
        float32x4_t vacc4567c1 = vacc0123c1;
        if XNN_LIKELY(nnz != 0) {
          do {
            const intptr_t diff = *dmap++;
            const float32x4_t va0123 = vld1q_f32(a);
            const float32x4_t va4567 = vld1q_f32(a + 4);
            a = (const float*restrict) ((uintptr_t) a + (uintptr_t) diff);
            const float32x2_t vb = vld1_f32(w); w += 2;

            vacc0123c0 = vfmaq_lane_f32(vacc0123c0, va0123, vb, 0);
            vacc4567c0 = vfmaq_lane_f32(vacc4567c0, va4567, vb, 0);
            vacc0123c1 = vfmaq_lane_f32(vacc0123c1, va0123, vb, 1);
            vacc4567c1 = vfmaq_lane_f32(vacc4567c1, va4567, vb, 1);
          } while (--nnz != 0);
        }
        float32x4_t vout0123c0 = vminq_f32(vacc0123c0, vmax);
        float32x4_t vout4567c0 = vminq_f32(vacc4567c0, vmax);
        float32x4_t vout0123c1 = vminq_f32(vacc0123c1, vmax);
        float32x4_t vout4567c1 = vminq_f32(vacc4567c1, vmax);

        vout0123c0 = vmaxq_f32(vout0123c0, vmin);
        vout4567c0 = vmaxq_f32(vout4567c0, vmin);
        vout0123c1 = vmaxq_f32(vout0123c1, vmin);
        vout4567c1 = vmaxq_f32(vout4567c1, vmin);

        vst1q_f32(c + 0 * m + 0, vout0123c0);
        vst1q_f32(c + 0 * m + 4, vout4567c0);
        vst1q_f32(c + 1 * m + 0, vout0123c1);
        vst1q_f32(c + 1 * m + 4, vout4567c1);
        c += 2 * m;
        j -= 2;
      }

      // clean up loop, fall back to nr=1
      if XNN_UNLIKELY(j != 0) {
        do {
          uint32_t nnz = *nnzmap++;
          float32x4_t vacc0123 = vld1q_dup_f32(w); w += 1;
          float32x4_t vacc4567 = vacc0123;
          if XNN_LIKELY(nnz != 0) {
            do {
              const intptr_t diff = *dmap++;
              const float32x4_t va0123 = vld1q_f32(a);
              const float32x4_t va4567 = vld1q_f32(a + 4);
              a = (const float*restrict) ((uintptr_t) a + (uintptr_t) diff);
              const float32x4_t vb = vld1q_dup_f32(w); w += 1;
              vacc0123 = vfmaq_f32(vacc0123, va0123, vb);
              vacc4567 = vfmaq_f32(vacc4567, va4567, vb);
            } while (--nnz != 0);
          }
          float32x4_t vout0123 = vminq_f32(vacc0123, vmax);
          float32x4_t vout4567 = vminq_f32(vacc4567, vmax);

          vout0123 = vmaxq_f32(vout0123, vmin);
          vout4567 = vmaxq_f32(vout4567, vmin);

          vst1q_f32(c + 0, vout0123);
          vst1q_f32(c + 4, vout4567);
          c += m;
          j -= 1;
        } while (j != 0);
      }
      c -= m * n;
      c += 8;
      a += 8;
    }
    if (i & 4) {
      const float*restrict w = weights;
      const int32_t* dmap = widx_dmap;
      const uint32_t* nnzmap = nidx_nnzmap;
      size_t j = n;
      while (j >= 2) {
        uint32_t nnz = *nnzmap++;
        float32x4_t vacc0123c0 = vld1q_dup_f32(w); w += 1;
        float32x4_t vacc0123c1 = vld1q_dup_f32(w); w += 1;
        if XNN_LIKELY(nnz != 0) {
          do {
            const intptr_t diff = *dmap++;
            const float32x4_t va0123 = vld1q_f32(a);
            a = (const float*restrict) ((uintptr_t) a + (uintptr_t) diff);
            const float32x2_t vb = vld1_f32(w); w += 2;

            vacc0123c0 = vfmaq_lane_f32(vacc0123c0, va0123, vb, 0);
            vacc0123c1 = vfmaq_lane_f32(vacc0123c1, va0123, vb, 1);
          } while (--nnz != 0);
        }
        float32x4_t vout0123c0 = vminq_f32(vacc0123c0, vmax);
        float32x4_t vout0123c1 = vminq_f32(vacc0123c1, vmax);

        vout0123c0 = vmaxq_f32(vout0123c0, vmin);
        vout0123c1 = vmaxq_f32(vout0123c1, vmin);

        vst1q_f32(c + 0 * m + 0, vout0123c0);
        vst1q_f32(c + 1 * m + 0, vout0123c1);
        c += 2 * m;
        j -= 2;
      }

      // clean up loop, fall back to nr=1
      if XNN_UNLIKELY(j != 0) {
        do {
          uint32_t nnz = *nnzmap++;
          float32x4_t vacc0123 = vld1q_dup_f32(w); w += 1;
          if XNN_LIKELY(nnz != 0) {
            do {
              const intptr_t diff = *dmap++;
              const float32x4_t va0123 = vld1q_f32(a);
              a = (const float*restrict) ((uintptr_t) a + (uintptr_t) diff);
              const float32x4_t vb = vld1q_dup_f32(w); w += 1;
              vacc0123 = vfmaq_f32(vacc0123, va0123, vb);
            } while (--nnz != 0);
          }
          float32x4_t vout0123 = vminq_f32(vacc0123, vmax);

          vout0123 = vmaxq_f32(vout0123, vmin);

          vst1q_f32(c + 0, vout0123);
          c += m;
          j -= 1;
        } while (j != 0);
      }
      c -= m * n;
      c += 4;
      a += 4;
    }
    if (i & 2) {
      const float*restrict w = weights;
      const int32_t* dmap = widx_dmap;
      const uint32_t* nnzmap = nidx_nnzmap;
      size_t j = n;
      while (j >= 2) {
        uint32_t nnz = *nnzmap++;
        float32x2_t vacc01c0 = vld1_dup_f32(w); w += 1;
        float32x2_t vacc01c1 = vld1_dup_f32(w); w += 1;
        if XNN_LIKELY(nnz != 0) {
          do {
            const intptr_t diff = *dmap++;
            const float32x2_t va01 = vld1_f32(a);
            a = (const float*restrict) ((uintptr_t) a + (uintptr_t) diff);
            const float32x2_t vb = vld1_f32(w); w += 2;

            vacc01c0 = vfma_lane_f32(vacc01c0, va01, vb, 0);
            vacc01c1 = vfma_lane_f32(vacc01c1, va01, vb, 1);
          } while (--nnz != 0);
        }
        float32x2_t vout01c0 = vmin_f32(vacc01c0, vget_low_f32(vmax));
        float32x2_t vout01c1 = vmin_f32(vacc01c1, vget_low_f32(vmax));

        vout01c0 = vmax_f32(vout01c0, vget_low_f32(vmin));
        vout01c1 = vmax_f32(vout01c1, vget_low_f32(vmin));

        vst1_f32(c + 0 * m + 0, vout01c0);
        vst1_f32(c + 1 * m + 0, vout01c1);
        c += 2 * m;
        j -= 2;
      }

      // clean up loop, fall back to nr=1
      if XNN_UNLIKELY(j != 0) {
        do {
          uint32_t nnz = *nnzmap++;
          float32x2_t vacc01 = vld1_dup_f32(w); w += 1;
          if XNN_LIKELY(nnz != 0) {
            do {
              const intptr_t diff = *dmap++;
              const float32x2_t va01 = vld1_f32(a);
              a = (const float*restrict) ((uintptr_t) a + (uintptr_t) diff);
              const float32x2_t vb = vld1_dup_f32(w); w += 1;
              vacc01 = vfma_f32(vacc01, va01, vb);
            } while (--nnz != 0);
          }
          float32x2_t vout01 = vmin_f32(vacc01, vget_low_f32(vmax));
          vout01 = vmax_f32(vout01, vget_low_f32(vmin));

          vst1_f32(c, vout01);
          c += m;
          j -= 1;
        } while (j != 0);
      }
      c -= m * n;
      c += 2;
      a += 2;
    }
    if (i & 1) {
      const float*restrict w = weights;
      const int32_t* dmap = widx_dmap;
      const uint32_t* nnzmap = nidx_nnzmap;
      size_t j = n;
      while (j >= 2) {
        uint32_t nnz = *nnzmap++;
        float32x2_t vacc0c0 = vld1_dup_f32(w); w += 1;
        float32x2_t vacc0c1 = vld1_dup_f32(w); w += 1;
        if XNN_LIKELY(nnz != 0) {
          do {
            const intptr_t diff = *dmap++;
            const float32x2_t va0 = vld1_dup_f32(a);
            a = (const float*restrict) ((uintptr_t) a + (uintptr_t) diff);
            const float32x2_t vb = vld1_f32(w); w += 2;

            vacc0c0 = vfma_lane_f32(vacc0c0, va0, vb, 0);
            vacc0c1 = vfma_lane_f32(vacc0c1, va0, vb, 1);
          } while (--nnz != 0);
        }
        float32x2_t vout0c0 = vmin_f32(vacc0c0, vget_low_f32(vmax));
        float32x2_t vout0c1 = vmin_f32(vacc0c1, vget_low_f32(vmax));

        vout0c0 = vmax_f32(vout0c0, vget_low_f32(vmin));
        vout0c1 = vmax_f32(vout0c1, vget_low_f32(vmin));

        vst1_lane_f32(c + 0 * m + 0, vout0c0, 0);
        vst1_lane_f32(c + 1 * m + 0, vout0c1, 0);
        c += 2 * m;
        j -= 2;
      }

      // clean up loop, fall back to nr=1
      if XNN_UNLIKELY(j != 0) {
        do {
          uint32_t nnz = *nnzmap++;
          float32x2_t vacc0 = vld1_dup_f32(w); w += 1;
          if XNN_LIKELY(nnz != 0) {
            do {
              const intptr_t diff = *dmap++;
              const float32x2_t va0 = vld1_dup_f32(a);
              a = (const float*restrict) ((uintptr_t) a + (uintptr_t) diff);
              const float32x2_t vb = vld1_dup_f32(w); w += 1;
              vacc0 = vfma_f32(vacc0, va0, vb);
            } while (--nnz != 0);
          }
          float32x2_t vout0 = vmin_f32(vacc0, vget_low_f32(vmax));
          vout0 = vmax_f32(vout0, vget_low_f32(vmin));

          vst1_lane_f32(c, vout0, 1);
          c += m;
          j -= 1;
        } while (j != 0);
      }
      c -= m * n;
      c += 1;
      a += 1;
    }
    }
}
