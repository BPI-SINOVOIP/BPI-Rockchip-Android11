/*
 * Copyright © 2016 Broadcom
 * Copyright © 2019 Collabora, Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <assert.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "drmtest.h"
#include "igt_aux.h"
#include "igt_core.h"
#include "igt_panfrost.h"
#include "ioctl_wrappers.h"
#include "intel_reg.h"
#include "intel_chipset.h"
#include "panfrost_drm.h"
#include "panfrost-job.h"

/**
 * SECTION:igt_panfrost
 * @short_description: PANFROST support library
 * @title: PANFROST
 * @include: igt.h
 *
 * This library provides various auxiliary helper functions for writing PANFROST
 * tests.
 */

struct panfrost_bo *
igt_panfrost_gem_new(int fd, size_t size)
{
        struct panfrost_bo *bo = calloc(1, sizeof(*bo));

        struct drm_panfrost_create_bo create_bo = {
                .size = size,
        };

        do_ioctl(fd, DRM_IOCTL_PANFROST_CREATE_BO, &create_bo);

        bo->handle = create_bo.handle;
        bo->offset = create_bo.offset;
        bo->size = size;
        return bo;
}

void
igt_panfrost_free_bo(int fd, struct panfrost_bo *bo)
{
        if (bo->map)
                munmap(bo->map, bo->size);
        gem_close(fd, bo->handle);
        free(bo);
}

uint32_t
igt_panfrost_get_bo_offset(int fd, uint32_t handle)
{
        struct drm_panfrost_get_bo_offset get = {
                .handle = handle,
        };

        do_ioctl(fd, DRM_IOCTL_PANFROST_GET_BO_OFFSET, &get);

        return get.offset;
}

uint32_t
igt_panfrost_get_param(int fd, int param)
{
        struct drm_panfrost_get_param get = {
                .param = param,
        };

        do_ioctl(fd, DRM_IOCTL_PANFROST_GET_PARAM, &get);

        return get.value;
}

void *
igt_panfrost_mmap_bo(int fd, uint32_t handle, uint32_t size, unsigned prot)
{
        struct drm_panfrost_mmap_bo mmap_bo = {
                .handle = handle,
        };
        void *ptr;

        mmap_bo.handle = handle;
        do_ioctl(fd, DRM_IOCTL_PANFROST_MMAP_BO, &mmap_bo);

        ptr = mmap(0, size, prot, MAP_SHARED, fd, mmap_bo.offset);
        if (ptr == MAP_FAILED)
                return NULL;
        else
                return ptr;
}

void igt_panfrost_bo_mmap(int fd, struct panfrost_bo *bo)
{
        bo->map = igt_panfrost_mmap_bo(fd, bo->handle, bo->size,
                                  PROT_READ | PROT_WRITE);
        igt_assert(bo->map);
}

struct panfrost_submit *igt_panfrost_trivial_job(int fd, bool do_crash, int width, int height, uint32_t color)
{
        struct panfrost_submit *submit;
        struct mali_job_descriptor_header header = {
                .job_type = JOB_TYPE_FRAGMENT,
                .job_index = 1,
                .job_descriptor_size = 1,
        };
        struct mali_payload_fragment payload = {
                .min_tile_coord = MALI_COORDINATE_TO_TILE_MIN(0, 0),
                .max_tile_coord = MALI_COORDINATE_TO_TILE_MAX(ALIGN(width, 16), height),
        };
        struct bifrost_framebuffer mfbd_framebuffer = {
            .unk0 = 0x0,
            .unknown1 = 0x0,
            .tiler_meta = 0xff00000000,
            .width1 = MALI_POSITIVE(ALIGN(width, 16)),
            .height1 = MALI_POSITIVE(height),
            .width2 = MALI_POSITIVE(ALIGN(width, 16)),
            .height2 = MALI_POSITIVE(height),
            .unk1 = 0x1080,
            .unk2 = 0x0,
            .rt_count_1 = MALI_POSITIVE(1),
            .rt_count_2 = 1,
            .unk3 = 0x100,
            .clear_stencil = 0x0,
            .clear_depth = 0.000000,
            .unknown2 = 0x1f,
        };
        struct mali_single_framebuffer sfbd_framebuffer = {
            .unknown2 = 0x1f,
            .width = MALI_POSITIVE(width),
            .height = MALI_POSITIVE(height),
            .stride = width * 4,
            .resolution_check = ((width + height) / 3) << 4,
            .tiler_flags = 0xfff,
            .clear_color_1 = color,
            .clear_color_2 = color,
            .clear_color_3 = color,
            .clear_color_4 = color,
            .clear_flags = 0x101100 | MALI_CLEAR_SLOW,
            .format = 0xb84e0281,
        };
        struct mali_rt_format fmt = {
                .unk1 = 0x4000000,
                .unk2 = 0x1,
                .nr_channels = MALI_POSITIVE(4),
                .flags = do_crash ? 0x444 | (1 << 8) : 0x444,
                .swizzle = MALI_CHANNEL_BLUE | (MALI_CHANNEL_GREEN << 3) | (MALI_CHANNEL_RED << 6) | (MALI_CHANNEL_ONE << 9),
                .unk4 = 0x8,
        };
        struct bifrost_render_target rts = {
                .format = fmt,
                .chunknown = {
                    .unk = 0x0,
                    .pointer = 0x0,
                },
                .framebuffer_stride = ALIGN(width, 16) * 4 / 16,
                .clear_color_1 = color,
                .clear_color_2 = color,
                .clear_color_3 = color,
                .clear_color_4 = color,
        };
        int gpu_prod_id = igt_panfrost_get_param(fd, DRM_PANFROST_PARAM_GPU_PROD_ID);
        uint32_t *known_unknown;
        uint32_t *bos;

        submit = malloc(sizeof(*submit));

        submit->fbo = igt_panfrost_gem_new(fd, ALIGN(width, 16) * height * 4);
        rts.framebuffer = submit->fbo->offset;
        sfbd_framebuffer.framebuffer = submit->fbo->offset;

        submit->tiler_heap_bo = igt_panfrost_gem_new(fd, 32768 * 128);
        mfbd_framebuffer.tiler_heap_start = submit->tiler_heap_bo->offset;
        mfbd_framebuffer.tiler_heap_end = submit->tiler_heap_bo->offset + 32768 * 128;
        sfbd_framebuffer.tiler_heap_free = mfbd_framebuffer.tiler_heap_start;
        sfbd_framebuffer.tiler_heap_end = mfbd_framebuffer.tiler_heap_end;

        submit->tiler_scratch_bo = igt_panfrost_gem_new(fd, 128 * 128 * 128);
        mfbd_framebuffer.tiler_scratch_start = submit->tiler_scratch_bo->offset;
        mfbd_framebuffer.tiler_scratch_middle = submit->tiler_scratch_bo->offset + 0xf0000;
        sfbd_framebuffer.unknown_address_0 = mfbd_framebuffer.tiler_scratch_start;

        submit->scratchpad_bo = igt_panfrost_gem_new(fd, 64 * 4096);
        igt_panfrost_bo_mmap(fd, submit->scratchpad_bo);
        mfbd_framebuffer.scratchpad = submit->scratchpad_bo->offset;
        sfbd_framebuffer.unknown_address_1 = submit->scratchpad_bo->offset;
        sfbd_framebuffer.unknown_address_2 = submit->scratchpad_bo->offset + 512;

        known_unknown = ((void*)submit->scratchpad_bo->map) + 512;
        *known_unknown = 0xa0000000;

        if (gpu_prod_id >= 0x0750) {
            submit->fb_bo = igt_panfrost_gem_new(fd, sizeof(mfbd_framebuffer) + sizeof(struct bifrost_render_target));
            igt_panfrost_bo_mmap(fd, submit->fb_bo);
            memcpy(submit->fb_bo->map, &mfbd_framebuffer, sizeof(mfbd_framebuffer));
            memcpy(submit->fb_bo->map + sizeof(mfbd_framebuffer), &rts, sizeof(struct bifrost_render_target));
            payload.framebuffer = submit->fb_bo->offset | MALI_MFBD;
        } else {
            // We don't know yet how to cause a hang on <=T720
            // Should probably use an infinite loop to hang the GPU
            igt_require(!do_crash);
            submit->fb_bo = igt_panfrost_gem_new(fd, sizeof(sfbd_framebuffer));
            igt_panfrost_bo_mmap(fd, submit->fb_bo);
            memcpy(submit->fb_bo->map, &sfbd_framebuffer, sizeof(sfbd_framebuffer));
            payload.framebuffer = submit->fb_bo->offset | MALI_SFBD;
        }

        submit->submit_bo = igt_panfrost_gem_new(fd, sizeof(header) + sizeof(payload) + 1024000);
        igt_panfrost_bo_mmap(fd, submit->submit_bo);

        memcpy(submit->submit_bo->map, &header, sizeof(header));
        memcpy(submit->submit_bo->map + sizeof(header), &payload, sizeof(payload));

        submit->args = malloc(sizeof(*submit->args));
        memset(submit->args, 0, sizeof(*submit->args));
        submit->args->jc = submit->submit_bo->offset;
        submit->args->requirements = PANFROST_JD_REQ_FS;

        bos = malloc(sizeof(*bos) * 6);
        bos[0] = submit->fbo->handle;
        bos[1] = submit->tiler_heap_bo->handle;
        bos[2] = submit->tiler_scratch_bo->handle;
        bos[3] = submit->scratchpad_bo->handle;
        bos[4] = submit->fb_bo->handle;
        bos[5] = submit->submit_bo->handle;

        submit->args->bo_handles = to_user_pointer(bos);
        submit->args->bo_handle_count = 6;

        igt_assert_eq(drmSyncobjCreate(fd, DRM_SYNCOBJ_CREATE_SIGNALED, &submit->args->out_sync), 0);

        return submit;
}

void igt_panfrost_free_job(int fd, struct panfrost_submit *submit)
{
        free(from_user_pointer(submit->args->bo_handles));
        igt_panfrost_free_bo(fd, submit->submit_bo);
        igt_panfrost_free_bo(fd, submit->fb_bo);
        igt_panfrost_free_bo(fd, submit->scratchpad_bo);
        igt_panfrost_free_bo(fd, submit->tiler_scratch_bo);
        igt_panfrost_free_bo(fd, submit->tiler_heap_bo);
        igt_panfrost_free_bo(fd, submit->fbo);
        free(submit->args);
        free(submit);
}
