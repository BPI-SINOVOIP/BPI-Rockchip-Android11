/*
 * Copyright © 2017 Intel Corporation
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
#include "drmtest.h"

#include <amdgpu.h>
#include <amdgpu_drm.h>

#define GFX_COMPUTE_NOP  0xffff1000
#define SDMA_NOP  0x0

static int
amdgpu_bo_alloc_and_map(amdgpu_device_handle dev, unsigned size,
			unsigned alignment, unsigned heap, uint64_t flags,
			amdgpu_bo_handle *bo, void **cpu, uint64_t *mc_address,
			amdgpu_va_handle *va_handle)
{
	struct amdgpu_bo_alloc_request request = {
		.alloc_size = size,
		.phys_alignment = alignment,
		.preferred_heap = heap,
		.flags = flags,
	};
	amdgpu_bo_handle buf_handle;
	amdgpu_va_handle handle;
	uint64_t vmc_addr;
	int r;

	r = amdgpu_bo_alloc(dev, &request, &buf_handle);
	if (r)
		return r;

	r = amdgpu_va_range_alloc(dev,
				  amdgpu_gpu_va_range_general,
				  size, alignment, 0, &vmc_addr,
				  &handle, 0);
	if (r)
		goto error_va_alloc;

	r = amdgpu_bo_va_op(buf_handle, 0, size, vmc_addr, 0, AMDGPU_VA_OP_MAP);
	if (r)
		goto error_va_map;

	r = amdgpu_bo_cpu_map(buf_handle, cpu);
	if (r)
		goto error_cpu_map;

	*bo = buf_handle;
	*mc_address = vmc_addr;
	*va_handle = handle;

	return 0;

error_cpu_map:
	amdgpu_bo_cpu_unmap(buf_handle);

error_va_map:
	amdgpu_bo_va_op(buf_handle, 0, size, vmc_addr, 0, AMDGPU_VA_OP_UNMAP);

error_va_alloc:
	amdgpu_bo_free(buf_handle);
	return r;
}

static void
amdgpu_bo_unmap_and_free(amdgpu_bo_handle bo, amdgpu_va_handle va_handle,
			 uint64_t mc_addr, uint64_t size)
{
	amdgpu_bo_cpu_unmap(bo);
	amdgpu_bo_va_op(bo, 0, size, mc_addr, 0, AMDGPU_VA_OP_UNMAP);
	amdgpu_va_range_free(va_handle);
	amdgpu_bo_free(bo);
}

static void amdgpu_cs_sync(amdgpu_context_handle context,
			   unsigned int ip_type,
			   int ring,
			   unsigned int seqno)
{
	struct amdgpu_cs_fence fence = {
		.context = context,
		.ip_type = ip_type,
		.ring = ring,
		.fence = seqno,
	};
	uint32_t expired;
	int err;

	err = amdgpu_cs_query_fence_status(&fence,
					   AMDGPU_TIMEOUT_INFINITE,
					   0, &expired);
	igt_assert_eq(err, 0);
}

#define SYNC 0x1
#define FORK 0x2
static void nop_cs(amdgpu_device_handle device,
		   amdgpu_context_handle context,
		   const char *name,
		   unsigned int ip_type,
		   unsigned int ring,
		   unsigned int timeout,
		   unsigned int flags)
{
	const int ncpus = flags & FORK ? sysconf(_SC_NPROCESSORS_ONLN) : 1;
	amdgpu_bo_handle ib_result_handle;
	void *ib_result_cpu;
	uint64_t ib_result_mc_address;
	uint32_t *ptr;
	int i, r;
	amdgpu_bo_list_handle bo_list;
	amdgpu_va_handle va_handle;

	r = amdgpu_bo_alloc_and_map(device, 4096, 4096,
				    AMDGPU_GEM_DOMAIN_GTT, 0,
				    &ib_result_handle, &ib_result_cpu,
				    &ib_result_mc_address, &va_handle);
	igt_assert_eq(r, 0);

	ptr = ib_result_cpu;
	for (i = 0; i < 16; ++i)
		ptr[i] = GFX_COMPUTE_NOP;

	r = amdgpu_bo_list_create(device, 1, &ib_result_handle, NULL, &bo_list);
	igt_assert_eq(r, 0);

	igt_fork(child, ncpus) {
		struct amdgpu_cs_request ibs_request;
		struct amdgpu_cs_ib_info ib_info;
		struct timespec tv = {};
		uint64_t submit_ns, sync_ns;
		unsigned long count;

		memset(&ib_info, 0, sizeof(struct amdgpu_cs_ib_info));
		ib_info.ib_mc_address = ib_result_mc_address;
		ib_info.size = 16;

		memset(&ibs_request, 0, sizeof(struct amdgpu_cs_request));
		ibs_request.ip_type = ip_type;
		ibs_request.ring = ring;
		ibs_request.number_of_ibs = 1;
		ibs_request.ibs = &ib_info;
		ibs_request.resources = bo_list;

		count = 0;
		igt_nsec_elapsed(&tv);
		igt_until_timeout(timeout) {
			r = amdgpu_cs_submit(context, 0, &ibs_request, 1);
			igt_assert_eq(r, 0);
			if (flags & SYNC)
				amdgpu_cs_sync(context, ip_type, ring,
					       ibs_request.seq_no);
			count++;
		}
		submit_ns = igt_nsec_elapsed(&tv);

		amdgpu_cs_sync(context, ip_type, ring, ibs_request.seq_no);
		sync_ns = igt_nsec_elapsed(&tv);

		igt_info("%s.%d: %'lu cycles, submit %.2fus, sync %.2fus\n",
			 name, child, count,
			 1e-3 * submit_ns / count, 1e-3 * sync_ns / count);
	}
	igt_waitchildren();

	r = amdgpu_bo_list_destroy(bo_list);
	igt_assert_eq(r, 0);

	amdgpu_bo_unmap_and_free(ib_result_handle, va_handle,
				 ib_result_mc_address, 4096);
}

igt_main
{
	amdgpu_device_handle device;
	amdgpu_context_handle context;
	const struct phase {
		const char *name;
		unsigned int flags;
	} phase[] = {
		{ "nop", 0 },
		{ "sync", SYNC },
		{ "fork", FORK },
		{ "sync-fork", SYNC | FORK },
		{ },
	}, *p;
	const struct engine {
		const char *name;
		unsigned int ip_type;
	} engines[] = {
		{ "compute", AMDGPU_HW_IP_COMPUTE },
		{ "gfx", AMDGPU_HW_IP_GFX },
		{ },
	}, *e;
	int fd = -1;

	igt_fixture {
		uint32_t major, minor;
		int err;

		fd = drm_open_driver(DRIVER_AMDGPU);

		err = amdgpu_device_initialize(fd, &major, &minor, &device);
		igt_require(err == 0);

		err = amdgpu_cs_ctx_create(device, &context);
		igt_assert_eq(err, 0);
	}

	for (p = phase; p->name; p++) {
		for (e = engines; e->name; e++) {
			igt_subtest_f("%s-%s0", p->name, e->name)
				nop_cs(device, context, e->name, e->ip_type,
				       0, 20, p->flags);
		}
	}

	igt_fixture {
		amdgpu_cs_ctx_free(context);
		amdgpu_device_deinitialize(device);
		close(fd);
	}
}
