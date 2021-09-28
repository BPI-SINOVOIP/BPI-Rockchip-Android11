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

#include "radeonsi/si_pipe.h"
#include "util/u_memory.h"
#include "util/u_transfer.h"
#include "util/u_upload_mgr.h"

#include <inttypes.h>
#include <stdio.h>

bool si_rings_is_buffer_referenced(struct si_context *sctx, struct pb_buffer *buf,
                                   enum radeon_bo_usage usage)
{
   if (sctx->ws->cs_is_buffer_referenced(sctx->gfx_cs, buf, usage)) {
      return true;
   }
   if (radeon_emitted(sctx->sdma_cs, 0) &&
       sctx->ws->cs_is_buffer_referenced(sctx->sdma_cs, buf, usage)) {
      return true;
   }
   return false;
}

void *si_buffer_map_sync_with_rings(struct si_context *sctx, struct si_resource *resource,
                                    unsigned usage)
{
   enum radeon_bo_usage rusage = RADEON_USAGE_READWRITE;
   bool busy = false;

   assert(!(resource->flags & RADEON_FLAG_SPARSE));

   if (usage & PIPE_MAP_UNSYNCHRONIZED) {
      return sctx->ws->buffer_map(resource->buf, NULL, usage);
   }

   if (!(usage & PIPE_MAP_WRITE)) {
      /* have to wait for the last write */
      rusage = RADEON_USAGE_WRITE;
   }

   if (radeon_emitted(sctx->gfx_cs, sctx->initial_gfx_cs_size) &&
       sctx->ws->cs_is_buffer_referenced(sctx->gfx_cs, resource->buf, rusage)) {
      if (usage & PIPE_MAP_DONTBLOCK) {
         si_flush_gfx_cs(sctx, RADEON_FLUSH_ASYNC_START_NEXT_GFX_IB_NOW, NULL);
         return NULL;
      } else {
         si_flush_gfx_cs(sctx, RADEON_FLUSH_ASYNC_START_NEXT_GFX_IB_NOW, NULL);
         busy = true;
      }
   }
   if (radeon_emitted(sctx->sdma_cs, 0) &&
       sctx->ws->cs_is_buffer_referenced(sctx->sdma_cs, resource->buf, rusage)) {
      if (usage & PIPE_MAP_DONTBLOCK) {
         si_flush_dma_cs(sctx, PIPE_FLUSH_ASYNC, NULL);
         return NULL;
      } else {
         si_flush_dma_cs(sctx, 0, NULL);
         busy = true;
      }
   }

   if (busy || !sctx->ws->buffer_wait(resource->buf, 0, rusage)) {
      if (usage & PIPE_MAP_DONTBLOCK) {
         return NULL;
      } else {
         /* We will be wait for the GPU. Wait for any offloaded
          * CS flush to complete to avoid busy-waiting in the winsys. */
         sctx->ws->cs_sync_flush(sctx->gfx_cs);
         if (sctx->sdma_cs)
            sctx->ws->cs_sync_flush(sctx->sdma_cs);
      }
   }

   /* Setting the CS to NULL will prevent doing checks we have done already. */
   return sctx->ws->buffer_map(resource->buf, NULL, usage);
}

void si_init_resource_fields(struct si_screen *sscreen, struct si_resource *res, uint64_t size,
                             unsigned alignment)
{
   struct si_texture *tex = (struct si_texture *)res;

   res->bo_size = size;
   res->bo_alignment = alignment;
   res->flags = 0;
   res->texture_handle_allocated = false;
   res->image_handle_allocated = false;

   switch (res->b.b.usage) {
   case PIPE_USAGE_STREAM:
      res->flags = RADEON_FLAG_GTT_WC;
      /* fall through */
   case PIPE_USAGE_STAGING:
      /* Transfers are likely to occur more often with these
       * resources. */
      res->domains = RADEON_DOMAIN_GTT;
      break;
   case PIPE_USAGE_DYNAMIC:
      /* Older kernels didn't always flush the HDP cache before
       * CS execution
       */
      if (!sscreen->info.kernel_flushes_hdp_before_ib) {
         res->domains = RADEON_DOMAIN_GTT;
         res->flags |= RADEON_FLAG_GTT_WC;
         break;
      }
      /* fall through */
   case PIPE_USAGE_DEFAULT:
   case PIPE_USAGE_IMMUTABLE:
   default:
      /* Not listing GTT here improves performance in some
       * apps. */
      res->domains = RADEON_DOMAIN_VRAM;
      res->flags |= RADEON_FLAG_GTT_WC;
      break;
   }

   if (res->b.b.target == PIPE_BUFFER && res->b.b.flags & PIPE_RESOURCE_FLAG_MAP_PERSISTENT) {
      /* Use GTT for all persistent mappings with older
       * kernels, because they didn't always flush the HDP
       * cache before CS execution.
       *
       * Write-combined CPU mappings are fine, the kernel
       * ensures all CPU writes finish before the GPU
       * executes a command stream.
       *
       * radeon doesn't have good BO move throttling, so put all
       * persistent buffers into GTT to prevent VRAM CPU page faults.
       */
      if (!sscreen->info.kernel_flushes_hdp_before_ib || !sscreen->info.is_amdgpu)
         res->domains = RADEON_DOMAIN_GTT;
   }

   /* Tiled textures are unmappable. Always put them in VRAM. */
   if ((res->b.b.target != PIPE_BUFFER && !tex->surface.is_linear) ||
       res->b.b.flags & SI_RESOURCE_FLAG_UNMAPPABLE) {
      res->domains = RADEON_DOMAIN_VRAM;
      res->flags |= RADEON_FLAG_NO_CPU_ACCESS | RADEON_FLAG_GTT_WC;
   }

   /* Displayable and shareable surfaces are not suballocated. */
   if (res->b.b.bind & (PIPE_BIND_SHARED | PIPE_BIND_SCANOUT))
      res->flags |= RADEON_FLAG_NO_SUBALLOC; /* shareable */
   else
      res->flags |= RADEON_FLAG_NO_INTERPROCESS_SHARING;

   if (res->b.b.bind & PIPE_BIND_PROTECTED ||
       /* Force scanout/depth/stencil buffer allocation to be encrypted */
       (sscreen->debug_flags & DBG(TMZ) &&
        res->b.b.bind & (PIPE_BIND_SCANOUT | PIPE_BIND_DEPTH_STENCIL)))
      res->flags |= RADEON_FLAG_ENCRYPTED;

   if (res->b.b.flags & PIPE_RESOURCE_FLAG_ENCRYPTED)
      res->flags |= RADEON_FLAG_ENCRYPTED;

   if (sscreen->debug_flags & DBG(NO_WC))
      res->flags &= ~RADEON_FLAG_GTT_WC;

   if (res->b.b.flags & SI_RESOURCE_FLAG_READ_ONLY)
      res->flags |= RADEON_FLAG_READ_ONLY;

   if (res->b.b.flags & SI_RESOURCE_FLAG_32BIT)
      res->flags |= RADEON_FLAG_32BIT;

   if (res->b.b.flags & SI_RESOURCE_FLAG_DRIVER_INTERNAL)
      res->flags |= RADEON_FLAG_DRIVER_INTERNAL;

   /* For higher throughput and lower latency over PCIe assuming sequential access.
    * Only CP DMA, SDMA, and optimized compute benefit from this.
    * GFX8 and older don't support RADEON_FLAG_UNCACHED.
    */
   if (sscreen->info.chip_class >= GFX9 &&
       res->b.b.flags & SI_RESOURCE_FLAG_UNCACHED)
      res->flags |= RADEON_FLAG_UNCACHED;

   /* Set expected VRAM and GART usage for the buffer. */
   res->vram_usage = 0;
   res->gart_usage = 0;
   res->max_forced_staging_uploads = 0;
   res->b.max_forced_staging_uploads = 0;

   if (res->domains & RADEON_DOMAIN_VRAM) {
      res->vram_usage = size;

      res->max_forced_staging_uploads = res->b.max_forced_staging_uploads =
         sscreen->info.has_dedicated_vram && size >= sscreen->info.vram_vis_size / 4 ? 1 : 0;
   } else if (res->domains & RADEON_DOMAIN_GTT) {
      res->gart_usage = size;
   }
}

bool si_alloc_resource(struct si_screen *sscreen, struct si_resource *res)
{
   struct pb_buffer *old_buf, *new_buf;

   /* Allocate a new resource. */
   new_buf = sscreen->ws->buffer_create(sscreen->ws, res->bo_size, res->bo_alignment, res->domains,
                                        res->flags);
   if (!new_buf) {
      return false;
   }

   /* Replace the pointer such that if res->buf wasn't NULL, it won't be
    * NULL. This should prevent crashes with multiple contexts using
    * the same buffer where one of the contexts invalidates it while
    * the others are using it. */
   old_buf = res->buf;
   res->buf = new_buf; /* should be atomic */
   res->gpu_address = sscreen->ws->buffer_get_virtual_address(res->buf);

   if (res->flags & RADEON_FLAG_32BIT) {
      uint64_t start = res->gpu_address;
      uint64_t last = start + res->bo_size - 1;
      (void)start;
      (void)last;

      assert((start >> 32) == sscreen->info.address32_hi);
      assert((last >> 32) == sscreen->info.address32_hi);
   }

   pb_reference(&old_buf, NULL);

   util_range_set_empty(&res->valid_buffer_range);
   res->TC_L2_dirty = false;

   /* Print debug information. */
   if (sscreen->debug_flags & DBG(VM) && res->b.b.target == PIPE_BUFFER) {
      fprintf(stderr, "VM start=0x%" PRIX64 "  end=0x%" PRIX64 " | Buffer %" PRIu64 " bytes\n",
              res->gpu_address, res->gpu_address + res->buf->size, res->buf->size);
   }

   if (res->b.b.flags & SI_RESOURCE_FLAG_CLEAR)
      si_screen_clear_buffer(sscreen, &res->b.b, 0, res->bo_size, 0);

   return true;
}

static void si_buffer_destroy(struct pipe_screen *screen, struct pipe_resource *buf)
{
   struct si_resource *buffer = si_resource(buf);

   threaded_resource_deinit(buf);
   util_range_destroy(&buffer->valid_buffer_range);
   pb_reference(&buffer->buf, NULL);
   FREE(buffer);
}

/* Reallocate the buffer a update all resource bindings where the buffer is
 * bound.
 *
 * This is used to avoid CPU-GPU synchronizations, because it makes the buffer
 * idle by discarding its contents.
 */
static bool si_invalidate_buffer(struct si_context *sctx, struct si_resource *buf)
{
   /* Shared buffers can't be reallocated. */
   if (buf->b.is_shared)
      return false;

   /* Sparse buffers can't be reallocated. */
   if (buf->flags & RADEON_FLAG_SPARSE)
      return false;

   /* In AMD_pinned_memory, the user pointer association only gets
    * broken when the buffer is explicitly re-allocated.
    */
   if (buf->b.is_user_ptr)
      return false;

   /* Check if mapping this buffer would cause waiting for the GPU. */
   if (si_rings_is_buffer_referenced(sctx, buf->buf, RADEON_USAGE_READWRITE) ||
       !sctx->ws->buffer_wait(buf->buf, 0, RADEON_USAGE_READWRITE)) {
      /* Reallocate the buffer in the same pipe_resource. */
      si_alloc_resource(sctx->screen, buf);
      si_rebind_buffer(sctx, &buf->b.b);
   } else {
      util_range_set_empty(&buf->valid_buffer_range);
   }

   return true;
}

/* Replace the storage of dst with src. */
void si_replace_buffer_storage(struct pipe_context *ctx, struct pipe_resource *dst,
                               struct pipe_resource *src)
{
   struct si_context *sctx = (struct si_context *)ctx;
   struct si_resource *sdst = si_resource(dst);
   struct si_resource *ssrc = si_resource(src);

   pb_reference(&sdst->buf, ssrc->buf);
   sdst->gpu_address = ssrc->gpu_address;
   sdst->b.b.bind = ssrc->b.b.bind;
   sdst->b.max_forced_staging_uploads = ssrc->b.max_forced_staging_uploads;
   sdst->max_forced_staging_uploads = ssrc->max_forced_staging_uploads;
   sdst->flags = ssrc->flags;

   assert(sdst->vram_usage == ssrc->vram_usage);
   assert(sdst->gart_usage == ssrc->gart_usage);
   assert(sdst->bo_size == ssrc->bo_size);
   assert(sdst->bo_alignment == ssrc->bo_alignment);
   assert(sdst->domains == ssrc->domains);

   si_rebind_buffer(sctx, dst);
}

static void si_invalidate_resource(struct pipe_context *ctx, struct pipe_resource *resource)
{
   struct si_context *sctx = (struct si_context *)ctx;
   struct si_resource *buf = si_resource(resource);

   /* We currently only do anyting here for buffers */
   if (resource->target == PIPE_BUFFER)
      (void)si_invalidate_buffer(sctx, buf);
}

static void *si_buffer_get_transfer(struct pipe_context *ctx, struct pipe_resource *resource,
                                    unsigned usage, const struct pipe_box *box,
                                    struct pipe_transfer **ptransfer, void *data,
                                    struct si_resource *staging, unsigned offset)
{
   struct si_context *sctx = (struct si_context *)ctx;
   struct si_transfer *transfer;

   if (usage & PIPE_MAP_THREAD_SAFE)
      transfer = malloc(sizeof(*transfer));
   else if (usage & TC_TRANSFER_MAP_THREADED_UNSYNC)
      transfer = slab_alloc(&sctx->pool_transfers_unsync);
   else
      transfer = slab_alloc(&sctx->pool_transfers);

   transfer->b.b.resource = NULL;
   pipe_resource_reference(&transfer->b.b.resource, resource);
   transfer->b.b.level = 0;
   transfer->b.b.usage = usage;
   transfer->b.b.box = *box;
   transfer->b.b.stride = 0;
   transfer->b.b.layer_stride = 0;
   transfer->b.staging = NULL;
   transfer->offset = offset;
   transfer->staging = staging;
   *ptransfer = &transfer->b.b;
   return data;
}

static void *si_buffer_transfer_map(struct pipe_context *ctx, struct pipe_resource *resource,
                                    unsigned level, unsigned usage, const struct pipe_box *box,
                                    struct pipe_transfer **ptransfer)
{
   struct si_context *sctx = (struct si_context *)ctx;
   struct si_resource *buf = si_resource(resource);
   uint8_t *data;

   assert(box->x + box->width <= resource->width0);

   /* From GL_AMD_pinned_memory issues:
    *
    *     4) Is glMapBuffer on a shared buffer guaranteed to return the
    *        same system address which was specified at creation time?
    *
    *        RESOLVED: NO. The GL implementation might return a different
    *        virtual mapping of that memory, although the same physical
    *        page will be used.
    *
    * So don't ever use staging buffers.
    */
   if (buf->b.is_user_ptr)
      usage |= PIPE_MAP_PERSISTENT;

   /* See if the buffer range being mapped has never been initialized,
    * in which case it can be mapped unsynchronized. */
   if (!(usage & (PIPE_MAP_UNSYNCHRONIZED | TC_TRANSFER_MAP_NO_INFER_UNSYNCHRONIZED)) &&
       usage & PIPE_MAP_WRITE && !buf->b.is_shared &&
       !util_ranges_intersect(&buf->valid_buffer_range, box->x, box->x + box->width)) {
      usage |= PIPE_MAP_UNSYNCHRONIZED;
   }

   /* If discarding the entire range, discard the whole resource instead. */
   if (usage & PIPE_MAP_DISCARD_RANGE && box->x == 0 && box->width == resource->width0) {
      usage |= PIPE_MAP_DISCARD_WHOLE_RESOURCE;
   }

   /* If a buffer in VRAM is too large and the range is discarded, don't
    * map it directly. This makes sure that the buffer stays in VRAM.
    */
   bool force_discard_range = false;
   if (usage & (PIPE_MAP_DISCARD_WHOLE_RESOURCE | PIPE_MAP_DISCARD_RANGE) &&
       !(usage & PIPE_MAP_PERSISTENT) &&
       /* Try not to decrement the counter if it's not positive. Still racy,
        * but it makes it harder to wrap the counter from INT_MIN to INT_MAX. */
       buf->max_forced_staging_uploads > 0 &&
       p_atomic_dec_return(&buf->max_forced_staging_uploads) >= 0) {
      usage &= ~(PIPE_MAP_DISCARD_WHOLE_RESOURCE | PIPE_MAP_UNSYNCHRONIZED);
      usage |= PIPE_MAP_DISCARD_RANGE;
      force_discard_range = true;
   }

   if (usage & PIPE_MAP_DISCARD_WHOLE_RESOURCE &&
       !(usage & (PIPE_MAP_UNSYNCHRONIZED | TC_TRANSFER_MAP_NO_INVALIDATE))) {
      assert(usage & PIPE_MAP_WRITE);

      if (si_invalidate_buffer(sctx, buf)) {
         /* At this point, the buffer is always idle. */
         usage |= PIPE_MAP_UNSYNCHRONIZED;
      } else {
         /* Fall back to a temporary buffer. */
         usage |= PIPE_MAP_DISCARD_RANGE;
      }
   }

   if (usage & PIPE_MAP_FLUSH_EXPLICIT &&
       buf->b.b.flags & SI_RESOURCE_FLAG_UPLOAD_FLUSH_EXPLICIT_VIA_SDMA) {
      usage &= ~(PIPE_MAP_UNSYNCHRONIZED | PIPE_MAP_PERSISTENT);
      usage |= PIPE_MAP_DISCARD_RANGE;
      force_discard_range = true;
   }

   if (usage & PIPE_MAP_DISCARD_RANGE &&
       ((!(usage & (PIPE_MAP_UNSYNCHRONIZED | PIPE_MAP_PERSISTENT))) ||
        (buf->flags & RADEON_FLAG_SPARSE))) {
      assert(usage & PIPE_MAP_WRITE);

      /* Check if mapping this buffer would cause waiting for the GPU.
       */
      if (buf->flags & RADEON_FLAG_SPARSE || force_discard_range ||
          si_rings_is_buffer_referenced(sctx, buf->buf, RADEON_USAGE_READWRITE) ||
          !sctx->ws->buffer_wait(buf->buf, 0, RADEON_USAGE_READWRITE)) {
         /* Do a wait-free write-only transfer using a temporary buffer. */
         struct u_upload_mgr *uploader;
         struct si_resource *staging = NULL;
         unsigned offset;

         /* If we are not called from the driver thread, we have
          * to use the uploader from u_threaded_context, which is
          * local to the calling thread.
          */
         if (usage & TC_TRANSFER_MAP_THREADED_UNSYNC)
            uploader = sctx->tc->base.stream_uploader;
         else
            uploader = sctx->b.stream_uploader;

         u_upload_alloc(uploader, 0, box->width + (box->x % SI_MAP_BUFFER_ALIGNMENT),
                        sctx->screen->info.tcc_cache_line_size, &offset,
                        (struct pipe_resource **)&staging, (void **)&data);

         if (staging) {
            data += box->x % SI_MAP_BUFFER_ALIGNMENT;
            return si_buffer_get_transfer(ctx, resource, usage, box, ptransfer, data, staging,
                                          offset);
         } else if (buf->flags & RADEON_FLAG_SPARSE) {
            return NULL;
         }
      } else {
         /* At this point, the buffer is always idle (we checked it above). */
         usage |= PIPE_MAP_UNSYNCHRONIZED;
      }
   }
   /* Use a staging buffer in cached GTT for reads. */
   else if (((usage & PIPE_MAP_READ) && !(usage & PIPE_MAP_PERSISTENT) &&
             (buf->domains & RADEON_DOMAIN_VRAM || buf->flags & RADEON_FLAG_GTT_WC)) ||
            (buf->flags & RADEON_FLAG_SPARSE)) {
      struct si_resource *staging;

      assert(!(usage & (TC_TRANSFER_MAP_THREADED_UNSYNC | PIPE_MAP_THREAD_SAFE)));
      staging = si_aligned_buffer_create(ctx->screen,
                                         SI_RESOURCE_FLAG_UNCACHED | SI_RESOURCE_FLAG_DRIVER_INTERNAL,
                                         PIPE_USAGE_STAGING,
                                         box->width + (box->x % SI_MAP_BUFFER_ALIGNMENT), 256);
      if (staging) {
         /* Copy the VRAM buffer to the staging buffer. */
         si_sdma_copy_buffer(sctx, &staging->b.b, resource, box->x % SI_MAP_BUFFER_ALIGNMENT,
                             box->x, box->width);

         data = si_buffer_map_sync_with_rings(sctx, staging, usage & ~PIPE_MAP_UNSYNCHRONIZED);
         if (!data) {
            si_resource_reference(&staging, NULL);
            return NULL;
         }
         data += box->x % SI_MAP_BUFFER_ALIGNMENT;

         return si_buffer_get_transfer(ctx, resource, usage, box, ptransfer, data, staging, 0);
      } else if (buf->flags & RADEON_FLAG_SPARSE) {
         return NULL;
      }
   }

   data = si_buffer_map_sync_with_rings(sctx, buf, usage);
   if (!data) {
      return NULL;
   }
   data += box->x;

   return si_buffer_get_transfer(ctx, resource, usage, box, ptransfer, data, NULL, 0);
}

static void si_buffer_do_flush_region(struct pipe_context *ctx, struct pipe_transfer *transfer,
                                      const struct pipe_box *box)
{
   struct si_context *sctx = (struct si_context *)ctx;
   struct si_transfer *stransfer = (struct si_transfer *)transfer;
   struct si_resource *buf = si_resource(transfer->resource);

   if (stransfer->staging) {
      unsigned src_offset =
         stransfer->offset + transfer->box.x % SI_MAP_BUFFER_ALIGNMENT + (box->x - transfer->box.x);

      if (buf->b.b.flags & SI_RESOURCE_FLAG_UPLOAD_FLUSH_EXPLICIT_VIA_SDMA) {
         /* This should be true for all uploaders. */
         assert(transfer->box.x == 0);

         /* Find a previous upload and extend its range. The last
          * upload is likely to be at the end of the list.
          */
         for (int i = sctx->num_sdma_uploads - 1; i >= 0; i--) {
            struct si_sdma_upload *up = &sctx->sdma_uploads[i];

            if (up->dst != buf)
               continue;

            assert(up->src == stransfer->staging);
            assert(box->x > up->dst_offset);
            up->size = box->x + box->width - up->dst_offset;
            return;
         }

         /* Enlarge the array if it's full. */
         if (sctx->num_sdma_uploads == sctx->max_sdma_uploads) {
            unsigned size;

            sctx->max_sdma_uploads += 4;
            size = sctx->max_sdma_uploads * sizeof(sctx->sdma_uploads[0]);
            sctx->sdma_uploads = realloc(sctx->sdma_uploads, size);
         }

         /* Add a new upload. */
         struct si_sdma_upload *up = &sctx->sdma_uploads[sctx->num_sdma_uploads++];
         up->dst = up->src = NULL;
         si_resource_reference(&up->dst, buf);
         si_resource_reference(&up->src, stransfer->staging);
         up->dst_offset = box->x;
         up->src_offset = src_offset;
         up->size = box->width;
         return;
      }

      /* Copy the staging buffer into the original one. */
      si_copy_buffer(sctx, transfer->resource, &stransfer->staging->b.b, box->x, src_offset,
                     box->width);
   }

   util_range_add(&buf->b.b, &buf->valid_buffer_range, box->x, box->x + box->width);
}

static void si_buffer_flush_region(struct pipe_context *ctx, struct pipe_transfer *transfer,
                                   const struct pipe_box *rel_box)
{
   unsigned required_usage = PIPE_MAP_WRITE | PIPE_MAP_FLUSH_EXPLICIT;

   if ((transfer->usage & required_usage) == required_usage) {
      struct pipe_box box;

      u_box_1d(transfer->box.x + rel_box->x, rel_box->width, &box);
      si_buffer_do_flush_region(ctx, transfer, &box);
   }
}

static void si_buffer_transfer_unmap(struct pipe_context *ctx, struct pipe_transfer *transfer)
{
   struct si_context *sctx = (struct si_context *)ctx;
   struct si_transfer *stransfer = (struct si_transfer *)transfer;

   if (transfer->usage & PIPE_MAP_WRITE && !(transfer->usage & PIPE_MAP_FLUSH_EXPLICIT))
      si_buffer_do_flush_region(ctx, transfer, &transfer->box);

   si_resource_reference(&stransfer->staging, NULL);
   assert(stransfer->b.staging == NULL); /* for threaded context only */
   pipe_resource_reference(&transfer->resource, NULL);

   if (transfer->usage & PIPE_MAP_THREAD_SAFE) {
      free(transfer);
   } else {
      /* Don't use pool_transfers_unsync. We are always in the driver
       * thread. Freeing an object into a different pool is allowed.
       */
      slab_free(&sctx->pool_transfers, transfer);
   }
}

static void si_buffer_subdata(struct pipe_context *ctx, struct pipe_resource *buffer,
                              unsigned usage, unsigned offset, unsigned size, const void *data)
{
   struct pipe_transfer *transfer = NULL;
   struct pipe_box box;
   uint8_t *map = NULL;

   usage |= PIPE_MAP_WRITE;

   if (!(usage & PIPE_MAP_DIRECTLY))
      usage |= PIPE_MAP_DISCARD_RANGE;

   u_box_1d(offset, size, &box);
   map = si_buffer_transfer_map(ctx, buffer, 0, usage, &box, &transfer);
   if (!map)
      return;

   memcpy(map, data, size);
   si_buffer_transfer_unmap(ctx, transfer);
}

static const struct u_resource_vtbl si_buffer_vtbl = {
   NULL,                     /* get_handle */
   si_buffer_destroy,        /* resource_destroy */
   si_buffer_transfer_map,   /* transfer_map */
   si_buffer_flush_region,   /* transfer_flush_region */
   si_buffer_transfer_unmap, /* transfer_unmap */
};

static struct si_resource *si_alloc_buffer_struct(struct pipe_screen *screen,
                                                  const struct pipe_resource *templ)
{
   struct si_resource *buf;

   buf = MALLOC_STRUCT(si_resource);

   buf->b.b = *templ;
   buf->b.b.next = NULL;
   pipe_reference_init(&buf->b.b.reference, 1);
   buf->b.b.screen = screen;

   buf->b.vtbl = &si_buffer_vtbl;
   threaded_resource_init(&buf->b.b);

   buf->buf = NULL;
   buf->bind_history = 0;
   buf->TC_L2_dirty = false;
   util_range_init(&buf->valid_buffer_range);
   return buf;
}

static struct pipe_resource *si_buffer_create(struct pipe_screen *screen,
                                              const struct pipe_resource *templ, unsigned alignment)
{
   struct si_screen *sscreen = (struct si_screen *)screen;
   struct si_resource *buf = si_alloc_buffer_struct(screen, templ);

   if (templ->flags & PIPE_RESOURCE_FLAG_SPARSE)
      buf->b.b.flags |= SI_RESOURCE_FLAG_UNMAPPABLE;

   si_init_resource_fields(sscreen, buf, templ->width0, alignment);

   if (templ->flags & PIPE_RESOURCE_FLAG_SPARSE)
      buf->flags |= RADEON_FLAG_SPARSE;

   if (!si_alloc_resource(sscreen, buf)) {
      FREE(buf);
      return NULL;
   }
   return &buf->b.b;
}

struct pipe_resource *pipe_aligned_buffer_create(struct pipe_screen *screen, unsigned flags,
                                                 unsigned usage, unsigned size, unsigned alignment)
{
   struct pipe_resource buffer;

   memset(&buffer, 0, sizeof buffer);
   buffer.target = PIPE_BUFFER;
   buffer.format = PIPE_FORMAT_R8_UNORM;
   buffer.bind = 0;
   buffer.usage = usage;
   buffer.flags = flags;
   buffer.width0 = size;
   buffer.height0 = 1;
   buffer.depth0 = 1;
   buffer.array_size = 1;
   return si_buffer_create(screen, &buffer, alignment);
}

struct si_resource *si_aligned_buffer_create(struct pipe_screen *screen, unsigned flags,
                                             unsigned usage, unsigned size, unsigned alignment)
{
   return si_resource(pipe_aligned_buffer_create(screen, flags, usage, size, alignment));
}

static struct pipe_resource *si_buffer_from_user_memory(struct pipe_screen *screen,
                                                        const struct pipe_resource *templ,
                                                        void *user_memory)
{
   struct si_screen *sscreen = (struct si_screen *)screen;
   struct radeon_winsys *ws = sscreen->ws;
   struct si_resource *buf = si_alloc_buffer_struct(screen, templ);

   buf->domains = RADEON_DOMAIN_GTT;
   buf->flags = 0;
   buf->b.is_user_ptr = true;
   util_range_add(&buf->b.b, &buf->valid_buffer_range, 0, templ->width0);
   util_range_add(&buf->b.b, &buf->b.valid_buffer_range, 0, templ->width0);

   /* Convert a user pointer to a buffer. */
   buf->buf = ws->buffer_from_ptr(ws, user_memory, templ->width0);
   if (!buf->buf) {
      FREE(buf);
      return NULL;
   }

   buf->gpu_address = ws->buffer_get_virtual_address(buf->buf);
   buf->vram_usage = 0;
   buf->gart_usage = templ->width0;

   return &buf->b.b;
}

struct pipe_resource *si_buffer_from_winsys_buffer(struct pipe_screen *screen,
                                                   const struct pipe_resource *templ,
                                                   struct pb_buffer *imported_buf,
                                                   bool dedicated)
{
   struct si_screen *sscreen = (struct si_screen *)screen;
   struct si_resource *res = si_alloc_buffer_struct(screen, templ);

   if (!res)
      return 0;

   res->buf = imported_buf;
   res->gpu_address = sscreen->ws->buffer_get_virtual_address(res->buf);
   res->bo_size = imported_buf->size;
   res->bo_alignment = imported_buf->alignment;
   res->domains = sscreen->ws->buffer_get_initial_domain(res->buf);

   if (res->domains & RADEON_DOMAIN_VRAM)
      res->vram_usage = res->bo_size;
   else if (res->domains & RADEON_DOMAIN_GTT)
      res->gart_usage = res->bo_size;

   if (sscreen->ws->buffer_get_flags)
      res->flags = sscreen->ws->buffer_get_flags(res->buf);

   if (templ->flags & PIPE_RESOURCE_FLAG_SPARSE) {
      res->b.b.flags |= SI_RESOURCE_FLAG_UNMAPPABLE;
      res->flags |= RADEON_FLAG_SPARSE;
   }

   return &res->b.b;
}

static struct pipe_resource *si_resource_create(struct pipe_screen *screen,
                                                const struct pipe_resource *templ)
{
   if (templ->target == PIPE_BUFFER) {
      return si_buffer_create(screen, templ, 256);
   } else {
      return si_texture_create(screen, templ);
   }
}

static bool si_resource_commit(struct pipe_context *pctx, struct pipe_resource *resource,
                               unsigned level, struct pipe_box *box, bool commit)
{
   struct si_context *ctx = (struct si_context *)pctx;
   struct si_resource *res = si_resource(resource);

   /*
    * Since buffer commitment changes cannot be pipelined, we need to
    * (a) flush any pending commands that refer to the buffer we're about
    *     to change, and
    * (b) wait for threaded submit to finish, including those that were
    *     triggered by some other, earlier operation.
    */
   if (radeon_emitted(ctx->gfx_cs, ctx->initial_gfx_cs_size) &&
       ctx->ws->cs_is_buffer_referenced(ctx->gfx_cs, res->buf, RADEON_USAGE_READWRITE)) {
      si_flush_gfx_cs(ctx, RADEON_FLUSH_ASYNC_START_NEXT_GFX_IB_NOW, NULL);
   }
   if (radeon_emitted(ctx->sdma_cs, 0) &&
       ctx->ws->cs_is_buffer_referenced(ctx->sdma_cs, res->buf, RADEON_USAGE_READWRITE)) {
      si_flush_dma_cs(ctx, PIPE_FLUSH_ASYNC, NULL);
   }

   if (ctx->sdma_cs)
      ctx->ws->cs_sync_flush(ctx->sdma_cs);
   ctx->ws->cs_sync_flush(ctx->gfx_cs);

   assert(resource->target == PIPE_BUFFER);

   return ctx->ws->buffer_commit(res->buf, box->x, box->width, commit);
}

void si_init_screen_buffer_functions(struct si_screen *sscreen)
{
   sscreen->b.resource_create = si_resource_create;
   sscreen->b.resource_destroy = u_resource_destroy_vtbl;
   sscreen->b.resource_from_user_memory = si_buffer_from_user_memory;
}

void si_init_buffer_functions(struct si_context *sctx)
{
   sctx->b.invalidate_resource = si_invalidate_resource;
   sctx->b.transfer_map = u_transfer_map_vtbl;
   sctx->b.transfer_flush_region = u_transfer_flush_region_vtbl;
   sctx->b.transfer_unmap = u_transfer_unmap_vtbl;
   sctx->b.texture_subdata = u_default_texture_subdata;
   sctx->b.buffer_subdata = si_buffer_subdata;
   sctx->b.resource_commit = si_resource_commit;
}
