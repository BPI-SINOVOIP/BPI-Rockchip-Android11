/*
 * Copyright © 2016 Intel Corporation
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
#include "igt_vgem.h"

#include <amdgpu.h>
#include <amdgpu_drm.h>

#include <sys/poll.h>

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

struct cork {
	int device;
	uint32_t fence;
	union {
		uint32_t handle;
		amdgpu_bo_handle amd_handle;
	};
};

static void plug(int fd, struct cork *c)
{
	struct vgem_bo bo;
	int dmabuf;

	c->device = drm_open_driver(DRIVER_VGEM);

	bo.width = bo.height = 1;
	bo.bpp = 4;
	vgem_create(c->device, &bo);
	c->fence = vgem_fence_attach(c->device, &bo, VGEM_FENCE_WRITE);

	dmabuf = prime_handle_to_fd(c->device, bo.handle);
	c->handle = prime_fd_to_handle(fd, dmabuf);
	close(dmabuf);
}

static void amd_plug(amdgpu_device_handle device, struct cork *c)
{
	struct amdgpu_bo_import_result import;
	struct vgem_bo bo;
	int dmabuf;

	c->device = drm_open_driver(DRIVER_VGEM);

	bo.width = bo.height = 1;
	bo.bpp = 4;
	vgem_create(c->device, &bo);
	c->fence = vgem_fence_attach(c->device, &bo, VGEM_FENCE_WRITE);

	dmabuf = prime_handle_to_fd(c->device, bo.handle);
	amdgpu_bo_import(device, amdgpu_bo_handle_type_dma_buf_fd,
			 dmabuf, &import);
	close(dmabuf);

	c->amd_handle = import.buf_handle;
}

static void unplug(struct cork *c)
{
	vgem_fence_signal(c->device, c->fence);
	close(c->device);
}

static void i915_to_amd(int i915, int amd, amdgpu_device_handle device)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 obj[2];
	struct drm_i915_gem_execbuffer2 execbuf;
	unsigned int engines[16], engine;
	unsigned int nengine;
	unsigned long count;
	struct cork c;

	nengine = 0;
	for_each_physical_engine(i915, engine)
		engines[nengine++] = engine;
	igt_require(nengine);

	memset(obj, 0, sizeof(obj));
	obj[1].handle = gem_create(i915, 4096);
	gem_write(i915, obj[1].handle, 0, &bbe, sizeof(bbe));

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = 2;

	plug(i915, &c);
	obj[0].handle = c.handle;

	count = 0;
	igt_until_timeout(5) {
		execbuf.rsvd1 = gem_context_create(i915);

		for (unsigned n = 0; n < nengine; n++) {
			execbuf.flags = engines[n];
			gem_execbuf(i915, &execbuf);
		}

		gem_context_destroy(i915, execbuf.rsvd1);
		count++;

		if (!gem_uses_full_ppgtt(i915))
			break;
	}

	igt_info("Reservation width = %ldx%d\n", count, nengine);

	{
		const int ring = 0;
		const unsigned int ip_type = AMDGPU_HW_IP_GFX;
		struct amdgpu_bo_import_result import;
		amdgpu_bo_handle ib_result_handle;
		void *ib_result_cpu;
		uint64_t ib_result_mc_address;
		struct amdgpu_cs_request ibs_request;
		struct amdgpu_cs_ib_info ib_info;
		uint32_t *ptr;
		int i, r, dmabuf;
		amdgpu_bo_list_handle bo_list;
		amdgpu_va_handle va_handle;
		amdgpu_context_handle context;

		r = amdgpu_cs_ctx_create(device, &context);
		igt_assert_eq(r, 0);

		dmabuf = prime_handle_to_fd(i915, obj[1].handle);
		r = amdgpu_bo_import(device, amdgpu_bo_handle_type_dma_buf_fd,
				     dmabuf, &import);
		close(dmabuf);

		r = amdgpu_bo_alloc_and_map(device, 4096, 4096,
					    AMDGPU_GEM_DOMAIN_GTT, 0,
					    &ib_result_handle, &ib_result_cpu,
					    &ib_result_mc_address, &va_handle);
		igt_assert_eq(r, 0);

		ptr = ib_result_cpu;
		for (i = 0; i < 16; ++i)
			ptr[i] = GFX_COMPUTE_NOP;

		r = amdgpu_bo_list_create(device, 2,
					  (amdgpu_bo_handle[]) {
					  import.buf_handle,
					  ib_result_handle
					  },
					  NULL, &bo_list);
		igt_assert_eq(r, 0);

		memset(&ib_info, 0, sizeof(struct amdgpu_cs_ib_info));
		ib_info.ib_mc_address = ib_result_mc_address;
		ib_info.size = 16;

		memset(&ibs_request, 0, sizeof(struct amdgpu_cs_request));
		ibs_request.ip_type = ip_type;
		ibs_request.ring = ring;
		ibs_request.number_of_ibs = 1;
		ibs_request.ibs = &ib_info;
		ibs_request.resources = bo_list;

		r = amdgpu_cs_submit(context, 0, &ibs_request, 1);
		igt_assert_eq(r, 0);

		unplug(&c);

		amdgpu_cs_sync(context, ip_type, ring,
			       ibs_request.seq_no);


		r = amdgpu_bo_list_destroy(bo_list);
		igt_assert_eq(r, 0);

		amdgpu_bo_unmap_and_free(ib_result_handle, va_handle,
					 ib_result_mc_address, 4096);

		amdgpu_cs_ctx_free(context);
	}

	gem_sync(i915, obj[1].handle);
	gem_close(i915, obj[1].handle);
}

static void amd_to_i915(int i915, int amd, amdgpu_device_handle device)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 obj[2];
	struct drm_i915_gem_execbuffer2 execbuf;
	const int ring = 0;
	const unsigned int ip_type = AMDGPU_HW_IP_GFX;
	amdgpu_bo_handle ib_result_handle;
	void *ib_result_cpu;
	uint64_t ib_result_mc_address;
	struct amdgpu_cs_request ibs_request;
	struct amdgpu_cs_ib_info ib_info;
	uint32_t *ptr;
	amdgpu_context_handle *contexts;
	int i, r, dmabuf;
	amdgpu_bo_list_handle bo_list;
	amdgpu_va_handle va_handle;
	unsigned long count, size;
	struct cork c;

	memset(obj, 0, sizeof(obj));
	obj[1].handle = gem_create(i915, 4096);
	gem_write(i915, obj[1].handle, 0, &bbe, sizeof(bbe));

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = 2;

	r = amdgpu_bo_alloc_and_map(device, 4096, 4096,
				    AMDGPU_GEM_DOMAIN_GTT, 0,
				    &ib_result_handle, &ib_result_cpu,
				    &ib_result_mc_address, &va_handle);
	igt_assert_eq(r, 0);

	ptr = ib_result_cpu;
	for (i = 0; i < 16; ++i)
		ptr[i] = GFX_COMPUTE_NOP;

	amd_plug(device, &c);

	r = amdgpu_bo_list_create(device, 2,
				  (amdgpu_bo_handle[]) {
				  c.amd_handle,
				  ib_result_handle
				  },
				  NULL, &bo_list);
	igt_assert_eq(r, 0);

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
	size = 64 << 10;
	contexts = malloc(size * sizeof(*contexts));
	igt_until_timeout(2) { /* must all complete within vgem timeout (10s) */
		if (count == size) {
			size *= 2;
			contexts = realloc(contexts, size * sizeof(*contexts));
		}

		if (amdgpu_cs_ctx_create(device, &contexts[count]))
			break;

		r = amdgpu_cs_submit(contexts[count], 0, &ibs_request, 1);
		igt_assert_eq(r, 0);

		count++;
	}

	igt_info("Reservation width = %ld\n", count);
	igt_require(count);

	amdgpu_bo_export(ib_result_handle,
			 amdgpu_bo_handle_type_dma_buf_fd,
			 (uint32_t *)&dmabuf);
	igt_assert_eq(poll(&(struct pollfd){dmabuf, POLLOUT}, 1, 0), 0);
	obj[0].handle = prime_fd_to_handle(i915, dmabuf);
	obj[0].flags = EXEC_OBJECT_WRITE;
	close(dmabuf);

	gem_execbuf(i915, &execbuf);
	igt_assert(gem_bo_busy(i915, obj[1].handle));

	unplug(&c);

	gem_sync(i915, obj[1].handle);
	gem_close(i915, obj[1].handle);

	while (count--)
		amdgpu_cs_ctx_free(contexts[count]);
	free(contexts);

	r = amdgpu_bo_list_destroy(bo_list);
	igt_assert_eq(r, 0);

	amdgpu_bo_unmap_and_free(ib_result_handle, va_handle,
				 ib_result_mc_address, 4096);
}

static void shrink(int i915, int amd, amdgpu_device_handle device)
{
	struct amdgpu_bo_alloc_request request = {
		.alloc_size = 1024 * 1024 * 4,
		.phys_alignment = 4096,
		.preferred_heap = AMDGPU_GEM_DOMAIN_GTT,
	};
	amdgpu_bo_handle bo;
	uint32_t handle;
	int dmabuf;

	igt_assert_eq(amdgpu_bo_alloc(device, &request, &bo), 0);
	amdgpu_bo_export(bo,
			 amdgpu_bo_handle_type_dma_buf_fd,
			 (uint32_t *)&dmabuf);
	amdgpu_bo_free(bo);

	handle = prime_fd_to_handle(i915, dmabuf);
	close(dmabuf);

	/* Populate the i915_bo->pages. */
	gem_set_domain(i915, handle, I915_GEM_DOMAIN_GTT, 0);

	/* Now evict them, establishing the link from i915:shrinker to amd. */
	igt_drop_caches_set(i915, DROP_SHRINK_ALL);

	gem_close(i915, handle);
}

igt_main
{
	amdgpu_device_handle device;
	int i915 = -1, amd = -1;

	igt_skip_on_simulation();

	igt_fixture {
		uint32_t major, minor;
		int err;

		i915 = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(i915);
		igt_require(gem_has_exec_fence(i915));

		amd = drm_open_driver(DRIVER_AMDGPU);
		err = amdgpu_device_initialize(amd, &major, &minor, &device);
		igt_require(err == 0);
	}

	igt_subtest("i915-to-amd") {
		gem_require_contexts(i915);
		i915_to_amd(i915, amd, device);
	}

	igt_subtest("amd-to-i915")
		amd_to_i915(i915, amd, device);

	igt_subtest("shrink")
		shrink(i915, amd, device);

	igt_fixture {
		amdgpu_device_deinitialize(device);
		close(amd);
		close(i915);
	}
}
