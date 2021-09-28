/*
 * Copyright 2013 Advanced Micro Devices, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "si_pipe.h"
#include "sid.h"

/* Set this if you want the ME to wait until CP DMA is done.
 * It should be set on the last CP DMA packet. */
#define CP_DMA_SYNC (1 << 0)

/* Set this if the source data was used as a destination in a previous CP DMA
 * packet. It's for preventing a read-after-write (RAW) hazard between two
 * CP DMA packets. */
#define CP_DMA_RAW_WAIT    (1 << 1)
#define CP_DMA_DST_IS_GDS  (1 << 2)
#define CP_DMA_CLEAR       (1 << 3)
#define CP_DMA_PFP_SYNC_ME (1 << 4)
#define CP_DMA_SRC_IS_GDS  (1 << 5)

/* The max number of bytes that can be copied per packet. */
static inline unsigned cp_dma_max_byte_count(struct si_context *sctx)
{
   unsigned max =
      sctx->chip_class >= GFX9 ? S_414_BYTE_COUNT_GFX9(~0u) : S_414_BYTE_COUNT_GFX6(~0u);

   /* make it aligned for optimal performance */
   return max & ~(SI_CPDMA_ALIGNMENT - 1);
}

/* Emit a CP DMA packet to do a copy from one buffer to another, or to clear
 * a buffer. The size must fit in bits [20:0]. If CP_DMA_CLEAR is set, src_va is a 32-bit
 * clear value.
 */
static void si_emit_cp_dma(struct si_context *sctx, struct radeon_cmdbuf *cs, uint64_t dst_va,
                           uint64_t src_va, unsigned size, unsigned flags,
                           enum si_cache_policy cache_policy)
{
   uint32_t header = 0, command = 0;

   assert(size <= cp_dma_max_byte_count(sctx));
   assert(sctx->chip_class != GFX6 || cache_policy == L2_BYPASS);

   if (sctx->chip_class >= GFX9)
      command |= S_414_BYTE_COUNT_GFX9(size);
   else
      command |= S_414_BYTE_COUNT_GFX6(size);

   /* Sync flags. */
   if (flags & CP_DMA_SYNC)
      header |= S_411_CP_SYNC(1);
   else {
      if (sctx->chip_class >= GFX9)
         command |= S_414_DISABLE_WR_CONFIRM_GFX9(1);
      else
         command |= S_414_DISABLE_WR_CONFIRM_GFX6(1);
   }

   if (flags & CP_DMA_RAW_WAIT)
      command |= S_414_RAW_WAIT(1);

   /* Src and dst flags. */
   if (sctx->chip_class >= GFX9 && !(flags & CP_DMA_CLEAR) && src_va == dst_va) {
      header |= S_411_DST_SEL(V_411_NOWHERE); /* prefetch only */
   } else if (flags & CP_DMA_DST_IS_GDS) {
      header |= S_411_DST_SEL(V_411_GDS);
      /* GDS increments the address, not CP. */
      command |= S_414_DAS(V_414_REGISTER) | S_414_DAIC(V_414_NO_INCREMENT);
   } else if (sctx->chip_class >= GFX7 && cache_policy != L2_BYPASS) {
      header |=
         S_411_DST_SEL(V_411_DST_ADDR_TC_L2) | S_500_DST_CACHE_POLICY(cache_policy == L2_STREAM);
   }

   if (flags & CP_DMA_CLEAR) {
      header |= S_411_SRC_SEL(V_411_DATA);
   } else if (flags & CP_DMA_SRC_IS_GDS) {
      header |= S_411_SRC_SEL(V_411_GDS);
      /* Both of these are required for GDS. It does increment the address. */
      command |= S_414_SAS(V_414_REGISTER) | S_414_SAIC(V_414_NO_INCREMENT);
   } else if (sctx->chip_class >= GFX7 && cache_policy != L2_BYPASS) {
      header |=
         S_411_SRC_SEL(V_411_SRC_ADDR_TC_L2) | S_500_SRC_CACHE_POLICY(cache_policy == L2_STREAM);
   }

   if (sctx->chip_class >= GFX7) {
      radeon_emit(cs, PKT3(PKT3_DMA_DATA, 5, 0));
      radeon_emit(cs, header);
      radeon_emit(cs, src_va);       /* SRC_ADDR_LO [31:0] */
      radeon_emit(cs, src_va >> 32); /* SRC_ADDR_HI [31:0] */
      radeon_emit(cs, dst_va);       /* DST_ADDR_LO [31:0] */
      radeon_emit(cs, dst_va >> 32); /* DST_ADDR_HI [31:0] */
      radeon_emit(cs, command);
   } else {
      header |= S_411_SRC_ADDR_HI(src_va >> 32);

      radeon_emit(cs, PKT3(PKT3_CP_DMA, 4, 0));
      radeon_emit(cs, src_va);                  /* SRC_ADDR_LO [31:0] */
      radeon_emit(cs, header);                  /* SRC_ADDR_HI [15:0] + flags. */
      radeon_emit(cs, dst_va);                  /* DST_ADDR_LO [31:0] */
      radeon_emit(cs, (dst_va >> 32) & 0xffff); /* DST_ADDR_HI [15:0] */
      radeon_emit(cs, command);
   }

   /* CP DMA is executed in ME, but index buffers are read by PFP.
    * This ensures that ME (CP DMA) is idle before PFP starts fetching
    * indices. If we wanted to execute CP DMA in PFP, this packet
    * should precede it.
    */
   if (sctx->has_graphics && flags & CP_DMA_PFP_SYNC_ME) {
      radeon_emit(cs, PKT3(PKT3_PFP_SYNC_ME, 0, 0));
      radeon_emit(cs, 0);
   }
}

void si_cp_dma_wait_for_idle(struct si_context *sctx)
{
   /* Issue a dummy DMA that copies zero bytes.
    *
    * The DMA engine will see that there's no work to do and skip this
    * DMA request, however, the CP will see the sync flag and still wait
    * for all DMAs to complete.
    */
   si_emit_cp_dma(sctx, sctx->gfx_cs, 0, 0, 0, CP_DMA_SYNC, L2_BYPASS);
}

static void si_cp_dma_prepare(struct si_context *sctx, struct pipe_resource *dst,
                              struct pipe_resource *src, unsigned byte_count,
                              uint64_t remaining_size, unsigned user_flags, enum si_coherency coher,
                              bool *is_first, unsigned *packet_flags)
{
   /* Fast exit for a CPDMA prefetch. */
   if ((user_flags & SI_CPDMA_SKIP_ALL) == SI_CPDMA_SKIP_ALL) {
      *is_first = false;
      return;
   }

   if (!(user_flags & SI_CPDMA_SKIP_BO_LIST_UPDATE)) {
      /* Count memory usage in so that need_cs_space can take it into account. */
      if (dst)
         si_context_add_resource_size(sctx, dst);
      if (src)
         si_context_add_resource_size(sctx, src);
   }

   if (!(user_flags & SI_CPDMA_SKIP_CHECK_CS_SPACE))
      si_need_gfx_cs_space(sctx, 0);

   /* This must be done after need_cs_space. */
   if (!(user_flags & SI_CPDMA_SKIP_BO_LIST_UPDATE)) {
      if (dst)
         radeon_add_to_buffer_list(sctx, sctx->gfx_cs, si_resource(dst), RADEON_USAGE_WRITE,
                                   RADEON_PRIO_CP_DMA);
      if (src)
         radeon_add_to_buffer_list(sctx, sctx->gfx_cs, si_resource(src), RADEON_USAGE_READ,
                                   RADEON_PRIO_CP_DMA);
   }

   /* Flush the caches for the first copy only.
    * Also wait for the previous CP DMA operations.
    */
   if (!(user_flags & SI_CPDMA_SKIP_GFX_SYNC) && sctx->flags)
      sctx->emit_cache_flush(sctx);

   if (!(user_flags & SI_CPDMA_SKIP_SYNC_BEFORE) && *is_first && !(*packet_flags & CP_DMA_CLEAR))
      *packet_flags |= CP_DMA_RAW_WAIT;

   *is_first = false;

   /* Do the synchronization after the last dma, so that all data
    * is written to memory.
    */
   if (!(user_flags & SI_CPDMA_SKIP_SYNC_AFTER) && byte_count == remaining_size) {
      *packet_flags |= CP_DMA_SYNC;

      if (coher == SI_COHERENCY_SHADER)
         *packet_flags |= CP_DMA_PFP_SYNC_ME;
   }
}

void si_cp_dma_clear_buffer(struct si_context *sctx, struct radeon_cmdbuf *cs,
                            struct pipe_resource *dst, uint64_t offset, uint64_t size,
                            unsigned value, unsigned user_flags, enum si_coherency coher,
                            enum si_cache_policy cache_policy)
{
   struct si_resource *sdst = si_resource(dst);
   uint64_t va = (sdst ? sdst->gpu_address : 0) + offset;
   bool is_first = true;

   assert(size && size % 4 == 0);

   /* Mark the buffer range of destination as valid (initialized),
    * so that transfer_map knows it should wait for the GPU when mapping
    * that range. */
   if (sdst)
      util_range_add(dst, &sdst->valid_buffer_range, offset, offset + size);

   /* Flush the caches. */
   if (sdst && !(user_flags & SI_CPDMA_SKIP_GFX_SYNC)) {
      sctx->flags |= SI_CONTEXT_PS_PARTIAL_FLUSH | SI_CONTEXT_CS_PARTIAL_FLUSH |
                     si_get_flush_flags(sctx, coher, cache_policy);
   }

   while (size) {
      unsigned byte_count = MIN2(size, cp_dma_max_byte_count(sctx));
      unsigned dma_flags = CP_DMA_CLEAR | (sdst ? 0 : CP_DMA_DST_IS_GDS);

      si_cp_dma_prepare(sctx, dst, NULL, byte_count, size, user_flags, coher, &is_first,
                        &dma_flags);

      /* Emit the clear packet. */
      si_emit_cp_dma(sctx, cs, va, value, byte_count, dma_flags, cache_policy);

      size -= byte_count;
      va += byte_count;
   }

   if (sdst && cache_policy != L2_BYPASS)
      sdst->TC_L2_dirty = true;

   /* If it's not a framebuffer fast clear... */
   if (coher == SI_COHERENCY_SHADER) {
      sctx->num_cp_dma_calls++;
      si_prim_discard_signal_next_compute_ib_start(sctx);
   }
}

/**
 * Realign the CP DMA engine. This must be done after a copy with an unaligned
 * size.
 *
 * \param size  Remaining size to the CP DMA alignment.
 */
static void si_cp_dma_realign_engine(struct si_context *sctx, unsigned size, unsigned user_flags,
                                     enum si_coherency coher, enum si_cache_policy cache_policy,
                                     bool *is_first)
{
   uint64_t va;
   unsigned dma_flags = 0;
   unsigned scratch_size = SI_CPDMA_ALIGNMENT * 2;

   assert(size < SI_CPDMA_ALIGNMENT);

   /* Use the scratch buffer as the dummy buffer. The 3D engine should be
    * idle at this point.
    */
   if (!sctx->scratch_buffer || sctx->scratch_buffer->b.b.width0 < scratch_size) {
      si_resource_reference(&sctx->scratch_buffer, NULL);
      sctx->scratch_buffer = si_aligned_buffer_create(&sctx->screen->b,
                                                      SI_RESOURCE_FLAG_UNMAPPABLE | SI_RESOURCE_FLAG_DRIVER_INTERNAL,
                                                      PIPE_USAGE_DEFAULT, scratch_size, 256);
      if (!sctx->scratch_buffer)
         return;

      si_mark_atom_dirty(sctx, &sctx->atoms.s.scratch_state);
   }

   si_cp_dma_prepare(sctx, &sctx->scratch_buffer->b.b, &sctx->scratch_buffer->b.b, size, size,
                     user_flags, coher, is_first, &dma_flags);

   va = sctx->scratch_buffer->gpu_address;
   si_emit_cp_dma(sctx, sctx->gfx_cs, va, va + SI_CPDMA_ALIGNMENT, size, dma_flags, cache_policy);
}

/**
 * Do memcpy between buffers using CP DMA.
 * If src or dst is NULL, it means read or write GDS, respectively.
 *
 * \param user_flags    bitmask of SI_CPDMA_*
 */
void si_cp_dma_copy_buffer(struct si_context *sctx, struct pipe_resource *dst,
                           struct pipe_resource *src, uint64_t dst_offset, uint64_t src_offset,
                           unsigned size, unsigned user_flags, enum si_coherency coher,
                           enum si_cache_policy cache_policy)
{
   uint64_t main_dst_offset, main_src_offset;
   unsigned skipped_size = 0;
   unsigned realign_size = 0;
   unsigned gds_flags = (dst ? 0 : CP_DMA_DST_IS_GDS) | (src ? 0 : CP_DMA_SRC_IS_GDS);
   bool is_first = true;

   assert(size);

   if (dst) {
      /* Skip this for the L2 prefetch. */
      if (dst != src || dst_offset != src_offset) {
         /* Mark the buffer range of destination as valid (initialized),
          * so that transfer_map knows it should wait for the GPU when mapping
          * that range. */
         util_range_add(dst, &si_resource(dst)->valid_buffer_range, dst_offset, dst_offset + size);
      }

      dst_offset += si_resource(dst)->gpu_address;
   }
   if (src)
      src_offset += si_resource(src)->gpu_address;

   /* The workarounds aren't needed on Fiji and beyond. */
   if (sctx->family <= CHIP_CARRIZO || sctx->family == CHIP_STONEY) {
      /* If the size is not aligned, we must add a dummy copy at the end
       * just to align the internal counter. Otherwise, the DMA engine
       * would slow down by an order of magnitude for following copies.
       */
      if (size % SI_CPDMA_ALIGNMENT)
         realign_size = SI_CPDMA_ALIGNMENT - (size % SI_CPDMA_ALIGNMENT);

      /* If the copy begins unaligned, we must start copying from the next
       * aligned block and the skipped part should be copied after everything
       * else has been copied. Only the src alignment matters, not dst.
       *
       * GDS doesn't need the source address to be aligned.
       */
      if (src && src_offset % SI_CPDMA_ALIGNMENT) {
         skipped_size = SI_CPDMA_ALIGNMENT - (src_offset % SI_CPDMA_ALIGNMENT);
         /* The main part will be skipped if the size is too small. */
         skipped_size = MIN2(skipped_size, size);
         size -= skipped_size;
      }
   }

   /* TMZ handling */
   if (unlikely(radeon_uses_secure_bos(sctx->ws) &&
                !(user_flags & SI_CPDMA_SKIP_TMZ))) {
      bool secure = src && (si_resource(src)->flags & RADEON_FLAG_ENCRYPTED);
      assert(!secure || (!dst || (si_resource(dst)->flags & RADEON_FLAG_ENCRYPTED)));
      if (secure != sctx->ws->cs_is_secure(sctx->gfx_cs)) {
         si_flush_gfx_cs(sctx, RADEON_FLUSH_ASYNC_START_NEXT_GFX_IB_NOW |
                               RADEON_FLUSH_TOGGLE_SECURE_SUBMISSION, NULL);
      }
   }

   /* Flush the caches. */
   if ((dst || src) && !(user_flags & SI_CPDMA_SKIP_GFX_SYNC)) {
      sctx->flags |= SI_CONTEXT_PS_PARTIAL_FLUSH | SI_CONTEXT_CS_PARTIAL_FLUSH |
                     si_get_flush_flags(sctx, coher, cache_policy);
   }

   /* This is the main part doing the copying. Src is always aligned. */
   main_dst_offset = dst_offset + skipped_size;
   main_src_offset = src_offset + skipped_size;

   while (size) {
      unsigned byte_count = MIN2(size, cp_dma_max_byte_count(sctx));
      unsigned dma_flags = gds_flags;

      si_cp_dma_prepare(sctx, dst, src, byte_count, size + skipped_size + realign_size, user_flags,
                        coher, &is_first, &dma_flags);

      si_emit_cp_dma(sctx, sctx->gfx_cs, main_dst_offset, main_src_offset, byte_count, dma_flags,
                     cache_policy);

      size -= byte_count;
      main_src_offset += byte_count;
      main_dst_offset += byte_count;
   }

   /* Copy the part we skipped because src wasn't aligned. */
   if (skipped_size) {
      unsigned dma_flags = gds_flags;

      si_cp_dma_prepare(sctx, dst, src, skipped_size, skipped_size + realign_size, user_flags,
                        coher, &is_first, &dma_flags);

      si_emit_cp_dma(sctx, sctx->gfx_cs, dst_offset, src_offset, skipped_size, dma_flags,
                     cache_policy);
   }

   /* Finally, realign the engine if the size wasn't aligned. */
   if (realign_size) {
      si_cp_dma_realign_engine(sctx, realign_size, user_flags, coher, cache_policy, &is_first);
   }

   if (dst && cache_policy != L2_BYPASS)
      si_resource(dst)->TC_L2_dirty = true;

   /* If it's not a prefetch or GDS copy... */
   if (dst && src && (dst != src || dst_offset != src_offset)) {
      sctx->num_cp_dma_calls++;
      si_prim_discard_signal_next_compute_ib_start(sctx);
   }
}

void cik_prefetch_TC_L2_async(struct si_context *sctx, struct pipe_resource *buf, uint64_t offset,
                              unsigned size)
{
   assert(sctx->chip_class >= GFX7);

   si_cp_dma_copy_buffer(sctx, buf, buf, offset, offset, size, SI_CPDMA_SKIP_ALL,
                         SI_COHERENCY_SHADER, L2_LRU);
}

static void cik_prefetch_shader_async(struct si_context *sctx, struct si_pm4_state *state)
{
   struct pipe_resource *bo = &state->shader->bo->b.b;

   cik_prefetch_TC_L2_async(sctx, bo, 0, bo->width0);
}

static void cik_prefetch_VBO_descriptors(struct si_context *sctx)
{
   if (!sctx->vertex_elements || !sctx->vertex_elements->vb_desc_list_alloc_size)
      return;

   cik_prefetch_TC_L2_async(sctx, &sctx->vb_descriptors_buffer->b.b, sctx->vb_descriptors_offset,
                            sctx->vertex_elements->vb_desc_list_alloc_size);
}

/**
 * Prefetch shaders and VBO descriptors.
 *
 * \param vertex_stage_only  Whether only the the API VS and VBO descriptors
 *                           should be prefetched.
 */
void cik_emit_prefetch_L2(struct si_context *sctx, bool vertex_stage_only)
{
   unsigned mask = sctx->prefetch_L2_mask;
   assert(mask);

   /* Prefetch shaders and VBO descriptors to TC L2. */
   if (sctx->chip_class >= GFX9) {
      /* Choose the right spot for the VBO prefetch. */
      if (sctx->queued.named.hs) {
         if (mask & SI_PREFETCH_HS)
            cik_prefetch_shader_async(sctx, sctx->queued.named.hs);
         if (mask & SI_PREFETCH_VBO_DESCRIPTORS)
            cik_prefetch_VBO_descriptors(sctx);
         if (vertex_stage_only) {
            sctx->prefetch_L2_mask &= ~(SI_PREFETCH_HS | SI_PREFETCH_VBO_DESCRIPTORS);
            return;
         }

         if (mask & SI_PREFETCH_GS)
            cik_prefetch_shader_async(sctx, sctx->queued.named.gs);
         if (mask & SI_PREFETCH_VS)
            cik_prefetch_shader_async(sctx, sctx->queued.named.vs);
      } else if (sctx->queued.named.gs) {
         if (mask & SI_PREFETCH_GS)
            cik_prefetch_shader_async(sctx, sctx->queued.named.gs);
         if (mask & SI_PREFETCH_VBO_DESCRIPTORS)
            cik_prefetch_VBO_descriptors(sctx);
         if (vertex_stage_only) {
            sctx->prefetch_L2_mask &= ~(SI_PREFETCH_GS | SI_PREFETCH_VBO_DESCRIPTORS);
            return;
         }

         if (mask & SI_PREFETCH_VS)
            cik_prefetch_shader_async(sctx, sctx->queued.named.vs);
      } else {
         if (mask & SI_PREFETCH_VS)
            cik_prefetch_shader_async(sctx, sctx->queued.named.vs);
         if (mask & SI_PREFETCH_VBO_DESCRIPTORS)
            cik_prefetch_VBO_descriptors(sctx);
         if (vertex_stage_only) {
            sctx->prefetch_L2_mask &= ~(SI_PREFETCH_VS | SI_PREFETCH_VBO_DESCRIPTORS);
            return;
         }
      }
   } else {
      /* GFX6-GFX8 */
      /* Choose the right spot for the VBO prefetch. */
      if (sctx->tes_shader.cso) {
         if (mask & SI_PREFETCH_LS)
            cik_prefetch_shader_async(sctx, sctx->queued.named.ls);
         if (mask & SI_PREFETCH_VBO_DESCRIPTORS)
            cik_prefetch_VBO_descriptors(sctx);
         if (vertex_stage_only) {
            sctx->prefetch_L2_mask &= ~(SI_PREFETCH_LS | SI_PREFETCH_VBO_DESCRIPTORS);
            return;
         }

         if (mask & SI_PREFETCH_HS)
            cik_prefetch_shader_async(sctx, sctx->queued.named.hs);
         if (mask & SI_PREFETCH_ES)
            cik_prefetch_shader_async(sctx, sctx->queued.named.es);
         if (mask & SI_PREFETCH_GS)
            cik_prefetch_shader_async(sctx, sctx->queued.named.gs);
         if (mask & SI_PREFETCH_VS)
            cik_prefetch_shader_async(sctx, sctx->queued.named.vs);
      } else if (sctx->gs_shader.cso) {
         if (mask & SI_PREFETCH_ES)
            cik_prefetch_shader_async(sctx, sctx->queued.named.es);
         if (mask & SI_PREFETCH_VBO_DESCRIPTORS)
            cik_prefetch_VBO_descriptors(sctx);
         if (vertex_stage_only) {
            sctx->prefetch_L2_mask &= ~(SI_PREFETCH_ES | SI_PREFETCH_VBO_DESCRIPTORS);
            return;
         }

         if (mask & SI_PREFETCH_GS)
            cik_prefetch_shader_async(sctx, sctx->queued.named.gs);
         if (mask & SI_PREFETCH_VS)
            cik_prefetch_shader_async(sctx, sctx->queued.named.vs);
      } else {
         if (mask & SI_PREFETCH_VS)
            cik_prefetch_shader_async(sctx, sctx->queued.named.vs);
         if (mask & SI_PREFETCH_VBO_DESCRIPTORS)
            cik_prefetch_VBO_descriptors(sctx);
         if (vertex_stage_only) {
            sctx->prefetch_L2_mask &= ~(SI_PREFETCH_VS | SI_PREFETCH_VBO_DESCRIPTORS);
            return;
         }
      }
   }

   if (mask & SI_PREFETCH_PS)
      cik_prefetch_shader_async(sctx, sctx->queued.named.ps);

   sctx->prefetch_L2_mask = 0;
}

void si_test_gds(struct si_context *sctx)
{
   struct pipe_context *ctx = &sctx->b;
   struct pipe_resource *src, *dst;
   unsigned r[4] = {};
   unsigned offset = debug_get_num_option("OFFSET", 16);

   src = pipe_buffer_create(ctx->screen, 0, PIPE_USAGE_DEFAULT, 16);
   dst = pipe_buffer_create(ctx->screen, 0, PIPE_USAGE_DEFAULT, 16);
   si_cp_dma_clear_buffer(sctx, sctx->gfx_cs, src, 0, 4, 0xabcdef01, 0, SI_COHERENCY_SHADER,
                          L2_BYPASS);
   si_cp_dma_clear_buffer(sctx, sctx->gfx_cs, src, 4, 4, 0x23456789, 0, SI_COHERENCY_SHADER,
                          L2_BYPASS);
   si_cp_dma_clear_buffer(sctx, sctx->gfx_cs, src, 8, 4, 0x87654321, 0, SI_COHERENCY_SHADER,
                          L2_BYPASS);
   si_cp_dma_clear_buffer(sctx, sctx->gfx_cs, src, 12, 4, 0xfedcba98, 0, SI_COHERENCY_SHADER,
                          L2_BYPASS);
   si_cp_dma_clear_buffer(sctx, sctx->gfx_cs, dst, 0, 16, 0xdeadbeef, 0, SI_COHERENCY_SHADER,
                          L2_BYPASS);

   si_cp_dma_copy_buffer(sctx, NULL, src, offset, 0, 16, 0, SI_COHERENCY_NONE, L2_BYPASS);
   si_cp_dma_copy_buffer(sctx, dst, NULL, 0, offset, 16, 0, SI_COHERENCY_NONE, L2_BYPASS);

   pipe_buffer_read(ctx, dst, 0, sizeof(r), r);
   printf("GDS copy  = %08x %08x %08x %08x -> %s\n", r[0], r[1], r[2], r[3],
          r[0] == 0xabcdef01 && r[1] == 0x23456789 && r[2] == 0x87654321 && r[3] == 0xfedcba98
             ? "pass"
             : "fail");

   si_cp_dma_clear_buffer(sctx, sctx->gfx_cs, NULL, offset, 16, 0xc1ea4146, 0, SI_COHERENCY_NONE,
                          L2_BYPASS);
   si_cp_dma_copy_buffer(sctx, dst, NULL, 0, offset, 16, 0, SI_COHERENCY_NONE, L2_BYPASS);

   pipe_buffer_read(ctx, dst, 0, sizeof(r), r);
   printf("GDS clear = %08x %08x %08x %08x -> %s\n", r[0], r[1], r[2], r[3],
          r[0] == 0xc1ea4146 && r[1] == 0xc1ea4146 && r[2] == 0xc1ea4146 && r[3] == 0xc1ea4146
             ? "pass"
             : "fail");

   pipe_resource_reference(&src, NULL);
   pipe_resource_reference(&dst, NULL);
   exit(0);
}

void si_cp_write_data(struct si_context *sctx, struct si_resource *buf, unsigned offset,
                      unsigned size, unsigned dst_sel, unsigned engine, const void *data)
{
   struct radeon_cmdbuf *cs = sctx->gfx_cs;

   assert(offset % 4 == 0);
   assert(size % 4 == 0);

   if (sctx->chip_class == GFX6 && dst_sel == V_370_MEM)
      dst_sel = V_370_MEM_GRBM;

   radeon_add_to_buffer_list(sctx, cs, buf, RADEON_USAGE_WRITE, RADEON_PRIO_CP_DMA);
   uint64_t va = buf->gpu_address + offset;

   radeon_emit(cs, PKT3(PKT3_WRITE_DATA, 2 + size / 4, 0));
   radeon_emit(cs, S_370_DST_SEL(dst_sel) | S_370_WR_CONFIRM(1) | S_370_ENGINE_SEL(engine));
   radeon_emit(cs, va);
   radeon_emit(cs, va >> 32);
   radeon_emit_array(cs, (const uint32_t *)data, size / 4);
}

void si_cp_copy_data(struct si_context *sctx, struct radeon_cmdbuf *cs, unsigned dst_sel,
                     struct si_resource *dst, unsigned dst_offset, unsigned src_sel,
                     struct si_resource *src, unsigned src_offset)
{
   /* cs can point to the compute IB, which has the buffer list in gfx_cs. */
   if (dst) {
      radeon_add_to_buffer_list(sctx, sctx->gfx_cs, dst, RADEON_USAGE_WRITE, RADEON_PRIO_CP_DMA);
   }
   if (src) {
      radeon_add_to_buffer_list(sctx, sctx->gfx_cs, src, RADEON_USAGE_READ, RADEON_PRIO_CP_DMA);
   }

   uint64_t dst_va = (dst ? dst->gpu_address : 0ull) + dst_offset;
   uint64_t src_va = (src ? src->gpu_address : 0ull) + src_offset;

   radeon_emit(cs, PKT3(PKT3_COPY_DATA, 4, 0));
   radeon_emit(cs, COPY_DATA_SRC_SEL(src_sel) | COPY_DATA_DST_SEL(dst_sel) | COPY_DATA_WR_CONFIRM);
   radeon_emit(cs, src_va);
   radeon_emit(cs, src_va >> 32);
   radeon_emit(cs, dst_va);
   radeon_emit(cs, dst_va >> 32);
}
