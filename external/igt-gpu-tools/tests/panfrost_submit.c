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

#include "igt.h"
#include "igt_panfrost.h"
#include "igt_syncobj.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "panfrost-job.h"
#include "panfrost_drm.h"

#define WIDTH          1920
#define HEIGHT         1080
#define CLEAR_COLOR    0xff7f7f7f

/* One tenth of a second */
#define SHORT_TIME_NSEC 100000000ull

/* Add the time that the bad job takes to timeout (sched->timeout) and the time that a reset can take */
#define BAD_JOB_TIME_NSEC (SHORT_TIME_NSEC + 500000000ull + 100000000ull)

#define NSECS_PER_SEC 1000000000ull

static uint64_t
abs_timeout(uint64_t duration)
{
        struct timespec current;
        clock_gettime(CLOCK_MONOTONIC, &current);
        return (uint64_t)current.tv_sec * NSECS_PER_SEC + current.tv_nsec + duration;
}

static void check_error(int fd, struct panfrost_submit *submit)
{
	struct mali_job_descriptor_header *header;

        header = submit->submit_bo->map;
        igt_assert_eq_u64(header->fault_pointer, 0);
}

static void check_fb(int fd, struct panfrost_bo *bo)
{
        int gpu_prod_id = igt_panfrost_get_param(fd, DRM_PANFROST_PARAM_GPU_PROD_ID);
        __uint32_t *fbo;
        int i;

        fbo = bo->map;

        if (gpu_prod_id >= 0x0750) {
                for (i = 0; i < ALIGN(WIDTH, 16) * HEIGHT; i++)
                        igt_assert_eq_u32(fbo[i], CLEAR_COLOR);
        } else {
                // Mask the alpha away because on <=T720 we don't know how to have it
                for (i = 0; i < ALIGN(WIDTH, 16) * HEIGHT; i++)
                        igt_assert_eq_u32(fbo[i], CLEAR_COLOR & 0x00ffffff);
        }
}

igt_main
{
        int fd;

        igt_fixture {
                fd = drm_open_driver(DRIVER_PANFROST);
        }

        igt_subtest("pan-submit") {
                struct panfrost_submit *submit;

                submit = igt_panfrost_trivial_job(fd, false, WIDTH, HEIGHT,
                                                  CLEAR_COLOR);

                igt_panfrost_bo_mmap(fd, submit->fbo);
                do_ioctl(fd, DRM_IOCTL_PANFROST_SUBMIT, submit->args);
                igt_assert(syncobj_wait(fd, &submit->args->out_sync, 1,
                                        abs_timeout(SHORT_TIME_NSEC), 0, NULL));
                check_error(fd, submit);
                check_fb(fd, submit->fbo);
                igt_panfrost_free_job(fd, submit);
        }

        igt_subtest("pan-submit-error-no-jc") {
                struct drm_panfrost_submit submit = {.jc = 0,};
                do_ioctl_err(fd, DRM_IOCTL_PANFROST_SUBMIT, &submit, EINVAL);
        }

        igt_subtest("pan-submit-error-bad-in-syncs") {
                struct panfrost_submit *submit;

                submit = igt_panfrost_trivial_job(fd, false, WIDTH, HEIGHT,
                                                  CLEAR_COLOR);
                submit->args->in_syncs = 0ULL;
                submit->args->in_sync_count = 1;

                do_ioctl_err(fd, DRM_IOCTL_PANFROST_SUBMIT, submit->args, EFAULT);
        }

        igt_subtest("pan-submit-error-bad-bo-handles") {
                struct panfrost_submit *submit;

                submit = igt_panfrost_trivial_job(fd, false, WIDTH, HEIGHT,
                                                  CLEAR_COLOR);
                submit->args->bo_handles = 0ULL;
                submit->args->bo_handle_count = 1;

                do_ioctl_err(fd, DRM_IOCTL_PANFROST_SUBMIT, submit->args, EFAULT);
        }

        igt_subtest("pan-submit-error-bad-requirements") {
                struct panfrost_submit *submit;

                submit = igt_panfrost_trivial_job(fd, false, WIDTH, HEIGHT,
                                                  CLEAR_COLOR);
                submit->args->requirements = 2;

                do_ioctl_err(fd, DRM_IOCTL_PANFROST_SUBMIT, submit->args, EINVAL);
        }

        igt_subtest("pan-submit-error-bad-out-sync") {
                struct panfrost_submit *submit;

                submit = igt_panfrost_trivial_job(fd, false, WIDTH, HEIGHT,
                                                  CLEAR_COLOR);
                submit->args->out_sync = -1;

                do_ioctl_err(fd, DRM_IOCTL_PANFROST_SUBMIT, submit->args, ENODEV);
        }

        igt_subtest("pan-reset") {
                struct panfrost_submit *submit;

                submit = igt_panfrost_trivial_job(fd, true, WIDTH, HEIGHT,
                                                  CLEAR_COLOR);
                do_ioctl(fd, DRM_IOCTL_PANFROST_SUBMIT, submit->args);
                /* Expect for this job to timeout */
                igt_assert(!syncobj_wait(fd, &submit->args->out_sync, 1,
                                         abs_timeout(SHORT_TIME_NSEC), 0, NULL));
                igt_panfrost_free_job(fd, submit);

                submit = igt_panfrost_trivial_job(fd, false, WIDTH, HEIGHT,
                                                  CLEAR_COLOR);
                igt_panfrost_bo_mmap(fd, submit->fbo);
                do_ioctl(fd, DRM_IOCTL_PANFROST_SUBMIT, submit->args);
                /* This one should work */
                igt_assert(syncobj_wait(fd, &submit->args->out_sync, 1,
                                        abs_timeout(BAD_JOB_TIME_NSEC), 0, NULL));
                check_fb(fd, submit->fbo);
                igt_panfrost_free_job(fd, submit);
        }

        igt_fixture {
                close(fd);
        }
}
