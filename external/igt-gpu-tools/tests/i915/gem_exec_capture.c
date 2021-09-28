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

#include <zlib.h>

#include "igt.h"
#include "igt_device.h"
#include "igt_rand.h"
#include "igt_sysfs.h"

#define LOCAL_OBJECT_CAPTURE (1 << 7)
#define LOCAL_PARAM_HAS_EXEC_CAPTURE 45

IGT_TEST_DESCRIPTION("Check that we capture the user specified objects on a hang");

static void check_error_state(int dir, struct drm_i915_gem_exec_object2 *obj)
{
	char *error, *str;
	bool found = false;

	error = igt_sysfs_get(dir, "error");
	igt_sysfs_set(dir, "error", "Begone!");

	igt_assert(error);
	igt_debug("%s\n", error);

	/* render ring --- user = 0x00000000 ffffd000 */
	for (str = error; (str = strstr(str, "--- user = ")); str++) {
		uint64_t addr;
		uint32_t hi, lo;

		igt_assert(sscanf(str, "--- user = 0x%x %x", &hi, &lo) == 2);
		addr = hi;
		addr <<= 32;
		addr |= lo;
		igt_assert_eq_u64(addr, obj->offset);
		found = true;
	}

	free(error);
	igt_assert(found);
}

static void __capture1(int fd, int dir, unsigned ring, uint32_t target)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	struct drm_i915_gem_exec_object2 obj[4];
#define SCRATCH 0
#define CAPTURE 1
#define NOCAPTURE 2
#define BATCH 3
	struct drm_i915_gem_relocation_entry reloc[2];
	struct drm_i915_gem_execbuffer2 execbuf;
	uint32_t *batch, *seqno;
	int i;

	memset(obj, 0, sizeof(obj));
	obj[SCRATCH].handle = gem_create(fd, 4096);
	obj[CAPTURE].handle = target;
	obj[CAPTURE].flags = LOCAL_OBJECT_CAPTURE;
	obj[NOCAPTURE].handle = gem_create(fd, 4096);

	obj[BATCH].handle = gem_create(fd, 4096);
	obj[BATCH].relocs_ptr = (uintptr_t)reloc;
	obj[BATCH].relocation_count = ARRAY_SIZE(reloc);

	memset(reloc, 0, sizeof(reloc));
	reloc[0].target_handle = obj[BATCH].handle; /* recurse */
	reloc[0].presumed_offset = 0;
	reloc[0].offset = 5*sizeof(uint32_t);
	reloc[0].delta = 0;
	reloc[0].read_domains = I915_GEM_DOMAIN_COMMAND;
	reloc[0].write_domain = 0;

	reloc[1].target_handle = obj[SCRATCH].handle; /* breadcrumb */
	reloc[1].presumed_offset = 0;
	reloc[1].offset = sizeof(uint32_t);
	reloc[1].delta = 0;
	reloc[1].read_domains = I915_GEM_DOMAIN_RENDER;
	reloc[1].write_domain = I915_GEM_DOMAIN_RENDER;

	seqno = gem_mmap__wc(fd, obj[SCRATCH].handle, 0, 4096, PROT_READ);
	gem_set_domain(fd, obj[SCRATCH].handle,
			I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);

	batch = gem_mmap__cpu(fd, obj[BATCH].handle, 0, 4096, PROT_WRITE);
	gem_set_domain(fd, obj[BATCH].handle,
			I915_GEM_DOMAIN_CPU, I915_GEM_DOMAIN_CPU);

	i = 0;
	batch[i] = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
	if (gen >= 8) {
		batch[++i] = 0;
		batch[++i] = 0;
	} else if (gen >= 4) {
		batch[++i] = 0;
		batch[++i] = 0;
		reloc[1].offset += sizeof(uint32_t);
	} else {
		batch[i]--;
		batch[++i] = 0;
	}
	batch[++i] = 0xc0ffee;
	if (gen < 4)
		batch[++i] = MI_NOOP;

	batch[++i] = MI_BATCH_BUFFER_START; /* not crashed? try again! */
	if (gen >= 8) {
		batch[i] |= 1 << 8 | 1;
		batch[++i] = 0;
		batch[++i] = 0;
	} else if (gen >= 6) {
		batch[i] |= 1 << 8;
		batch[++i] = 0;
	} else {
		batch[i] |= 2 << 6;
		batch[++i] = 0;
		if (gen < 4) {
			batch[i] |= 1;
			reloc[0].delta = 1;
		}
	}
	munmap(batch, 4096);

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = (uintptr_t)obj;
	execbuf.buffer_count = ARRAY_SIZE(obj);
	execbuf.flags = ring;
	if (gen > 3 && gen < 6)
		execbuf.flags |= I915_EXEC_SECURE;

	igt_assert(!READ_ONCE(*seqno));
	gem_execbuf(fd, &execbuf);

	/* Wait for the request to start */
	while (READ_ONCE(*seqno) != 0xc0ffee)
		igt_assert(gem_bo_busy(fd, obj[SCRATCH].handle));
	munmap(seqno, 4096);

	/* Check that only the buffer we marked is reported in the error */
	igt_force_gpu_reset(fd);
	check_error_state(dir, &obj[CAPTURE]);

	gem_sync(fd, obj[BATCH].handle);

	gem_close(fd, obj[BATCH].handle);
	gem_close(fd, obj[NOCAPTURE].handle);
	gem_close(fd, obj[SCRATCH].handle);
}

static void capture(int fd, int dir, unsigned ring)
{
	uint32_t handle;

	handle = gem_create(fd, 4096);
	__capture1(fd, dir, ring, handle);
	gem_close(fd, handle);
}

static int cmp(const void *A, const void *B)
{
	const uint64_t *a = A, *b = B;

	if (*a < *b)
		return -1;

	if (*a > *b)
		return 1;

	return 0;
}

static struct offset {
	uint64_t addr;
	unsigned long idx;
} *__captureN(int fd, int dir, unsigned ring,
	      unsigned int size, int count,
	      unsigned int flags)
#define INCREMENTAL 0x1
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	struct drm_i915_gem_exec_object2 *obj;
	struct drm_i915_gem_relocation_entry reloc[2];
	struct drm_i915_gem_execbuffer2 execbuf;
	uint32_t *batch, *seqno;
	struct offset *offsets;
	int i;

	offsets = calloc(count , sizeof(*offsets));
	igt_assert(offsets);

	obj = calloc(count + 2, sizeof(*obj));
	igt_assert(obj);

	obj[0].handle = gem_create(fd, 4096);
	for (i = 0; i < count; i++) {
		obj[i + 1].handle = gem_create(fd, size);
		obj[i + 1].flags =
			LOCAL_OBJECT_CAPTURE | EXEC_OBJECT_SUPPORTS_48B_ADDRESS;
		if (flags & INCREMENTAL) {
			uint32_t *ptr;

			ptr = gem_mmap__cpu(fd, obj[i + 1].handle,
					    0, size, PROT_WRITE);
			for (unsigned int n = 0; n < size / sizeof(*ptr); n++)
				ptr[n] = i * size + n;
			munmap(ptr, size);
		}
	}

	obj[count + 1].handle = gem_create(fd, 4096);
	obj[count + 1].relocs_ptr = (uintptr_t)reloc;
	obj[count + 1].relocation_count = ARRAY_SIZE(reloc);

	memset(reloc, 0, sizeof(reloc));
	reloc[0].target_handle = obj[count + 1].handle; /* recurse */
	reloc[0].presumed_offset = 0;
	reloc[0].offset = 5*sizeof(uint32_t);
	reloc[0].delta = 0;
	reloc[0].read_domains = I915_GEM_DOMAIN_COMMAND;
	reloc[0].write_domain = 0;

	reloc[1].target_handle = obj[0].handle; /* breadcrumb */
	reloc[1].presumed_offset = 0;
	reloc[1].offset = sizeof(uint32_t);
	reloc[1].delta = 0;
	reloc[1].read_domains = I915_GEM_DOMAIN_RENDER;
	reloc[1].write_domain = I915_GEM_DOMAIN_RENDER;

	seqno = gem_mmap__wc(fd, obj[0].handle, 0, 4096, PROT_READ);
	gem_set_domain(fd, obj[0].handle,
			I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);

	batch = gem_mmap__cpu(fd, obj[count + 1].handle, 0, 4096, PROT_WRITE);
	gem_set_domain(fd, obj[count + 1].handle,
			I915_GEM_DOMAIN_CPU, I915_GEM_DOMAIN_CPU);

	i = 0;
	batch[i] = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
	if (gen >= 8) {
		batch[++i] = 0;
		batch[++i] = 0;
	} else if (gen >= 4) {
		batch[++i] = 0;
		batch[++i] = 0;
		reloc[1].offset += sizeof(uint32_t);
	} else {
		batch[i]--;
		batch[++i] = 0;
	}
	batch[++i] = 0xc0ffee;
	if (gen < 4)
		batch[++i] = MI_NOOP;

	batch[++i] = MI_BATCH_BUFFER_START; /* not crashed? try again! */
	if (gen >= 8) {
		batch[i] |= 1 << 8 | 1;
		batch[++i] = 0;
		batch[++i] = 0;
	} else if (gen >= 6) {
		batch[i] |= 1 << 8;
		batch[++i] = 0;
	} else {
		batch[i] |= 2 << 6;
		batch[++i] = 0;
		if (gen < 4) {
			batch[i] |= 1;
			reloc[0].delta = 1;
		}
	}
	munmap(batch, 4096);

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = (uintptr_t)obj;
	execbuf.buffer_count = count + 2;
	execbuf.flags = ring;
	if (gen > 3 && gen < 6)
		execbuf.flags |= I915_EXEC_SECURE;

	igt_assert(!READ_ONCE(*seqno));
	gem_execbuf(fd, &execbuf);

	/* Wait for the request to start */
	while (READ_ONCE(*seqno) != 0xc0ffee)
		igt_assert(gem_bo_busy(fd, obj[0].handle));
	munmap(seqno, 4096);

	igt_force_gpu_reset(fd);

	gem_sync(fd, obj[count + 1].handle);
	gem_close(fd, obj[count + 1].handle);
	for (i = 0; i < count; i++) {
		offsets[i].addr = obj[i + 1].offset;
		offsets[i].idx = i;
		gem_close(fd, obj[i + 1].handle);
	}
	gem_close(fd, obj[0].handle);

	qsort(offsets, count, sizeof(*offsets), cmp);
	igt_assert(offsets[0].addr <= offsets[count-1].addr);
	return offsets;
}

static unsigned long zlib_inflate(uint32_t **ptr, unsigned long len)
{
	struct z_stream_s zstream;
	void *out;

	memset(&zstream, 0, sizeof(zstream));

	zstream.next_in = (unsigned char *)*ptr;
	zstream.avail_in = 4*len;

	if (inflateInit(&zstream) != Z_OK)
		return 0;

	out = malloc(128*4096); /* approximate obj size */
	zstream.next_out = out;
	zstream.avail_out = 128*4096;

	do {
		switch (inflate(&zstream, Z_SYNC_FLUSH)) {
		case Z_STREAM_END:
			goto end;
		case Z_OK:
			break;
		default:
			inflateEnd(&zstream);
			return 0;
		}

		if (zstream.avail_out)
			break;

		out = realloc(out, 2*zstream.total_out);
		if (out == NULL) {
			inflateEnd(&zstream);
			return 0;
		}

		zstream.next_out = (unsigned char *)out + zstream.total_out;
		zstream.avail_out = zstream.total_out;
	} while (1);
end:
	inflateEnd(&zstream);
	free(*ptr);
	*ptr = out;
	return zstream.total_out / 4;
}

static unsigned long
ascii85_decode(char *in, uint32_t **out, bool inflate, char **end)
{
	unsigned long len = 0, size = 1024;

	*out = realloc(*out, sizeof(uint32_t)*size);
	if (*out == NULL)
		return 0;

	while (*in >= '!' && *in <= 'z') {
		uint32_t v = 0;

		if (len == size) {
			size *= 2;
			*out = realloc(*out, sizeof(uint32_t)*size);
			if (*out == NULL)
				return 0;
		}

		if (*in == 'z') {
			in++;
		} else {
			v += in[0] - 33; v *= 85;
			v += in[1] - 33; v *= 85;
			v += in[2] - 33; v *= 85;
			v += in[3] - 33; v *= 85;
			v += in[4] - 33;
			in += 5;
		}
		(*out)[len++] = v;
	}
	*end = in;

	if (!inflate)
		return len;

	return zlib_inflate(out, len);
}

static void many(int fd, int dir, uint64_t size, unsigned int flags)
{
	uint64_t ram, gtt;
	unsigned long count, blobs;
	struct offset *offsets;
	char *error, *str;

	gtt = gem_aperture_size(fd) / size;
	ram = (intel_get_avail_ram_mb() << 20) / size;
	igt_debug("Available objects in GTT:%"PRIu64", RAM:%"PRIu64"\n",
		  gtt, ram);

	count = min(gtt, ram) / 4;
	igt_require(count > 1);

	intel_require_memory(count, size, CHECK_RAM);

	offsets = __captureN(fd, dir, 0, size, count, flags);

	error = igt_sysfs_get(dir, "error");
	igt_sysfs_set(dir, "error", "Begone!");
	igt_assert(error);

	blobs = 0;
	/* render ring --- user = 0x00000000 ffffd000 */
	str = strstr(error, "--- user = ");
	while (str) {
		uint32_t *data = NULL;
		unsigned long i, sz;
		uint64_t addr;

		if (strncmp(str, "--- user = 0x", 13))
			break;

		str += 13;
		addr = strtoul(str, &str, 16);
		addr <<= 32;
		addr |= strtoul(str + 1, &str, 16);
		igt_assert(*str++ == '\n');

		if (!(*str == ':' || *str == '~'))
			continue;

		igt_debug("blob:%.64s\n", str);
		sz = ascii85_decode(str + 1, &data, *str == ':', &str);
		igt_assert_eq(4 * sz, size);
		igt_assert(*str++ == '\n');
		str = strchr(str, '-');

		if (flags & INCREMENTAL) {
			unsigned long start = 0;
			unsigned long end = count;
			uint32_t expect;

			while (end > start) {
				i = (end - start) / 2 + start;
				if (offsets[i].addr < addr)
					start = i + 1;
				else if (offsets[i].addr > addr)
					end = i;
				else
					break;
			}
			igt_assert(offsets[i].addr == addr);
			igt_debug("offset:%"PRIx64", index:%ld\n",
				  addr, offsets[i].idx);

			expect = offsets[i].idx * size;
			for (i = 0; i < sz; i++)
				igt_assert_eq(data[i], expect++);
		} else {
			for (i = 0; i < sz; i++)
				igt_assert_eq(data[i], 0);
		}

		blobs++;
		free(data);
	}
	igt_info("Captured %lu %"PRId64"-blobs out of a total of %lu\n",
		 blobs, size >> 12, count);
	igt_assert(count);

	free(error);
	free(offsets);
}

static void userptr(int fd, int dir)
{
	uint32_t handle;
	void *ptr;

	igt_assert(posix_memalign(&ptr, 4096, 4096) == 0);
	igt_require(__gem_userptr(fd, ptr, 4096, 0, 0, &handle) == 0);

	__capture1(fd, dir, 0, handle);

	gem_close(fd, handle);
	free(ptr);
}

static bool has_capture(int fd)
{
	drm_i915_getparam_t gp;
	int async = -1;

	gp.param = LOCAL_PARAM_HAS_EXEC_CAPTURE;
	gp.value = &async;
	drmIoctl(fd, DRM_IOCTL_I915_GETPARAM, &gp);

	return async > 0;
}

static size_t safer_strlen(const char *s)
{
	return s ? strlen(s) : 0;
}

igt_main
{
	const struct intel_execution_engine *e;
	igt_hang_t hang;
	int fd = -1;
	int dir = -1;

	igt_skip_on_simulation();

	igt_fixture {
		int gen;

		fd = drm_open_driver(DRIVER_INTEL);

		gen = intel_gen(intel_get_drm_devid(fd));
		if (gen > 3 && gen < 6) /* ctg and ilk need secure batches */
			igt_device_set_master(fd);

		igt_require_gem(fd);
		gem_require_mmap_wc(fd);
		igt_require(has_capture(fd));
		igt_allow_hang(fd, 0, HANG_ALLOW_CAPTURE);

		dir = igt_sysfs_open(fd);
		igt_require(igt_sysfs_set(dir, "error", "Begone!"));
		igt_require(safer_strlen(igt_sysfs_get(dir, "error")) > 0);
	}

	for (e = intel_execution_engines; e->name; e++) {
		/* default exec-id is purely symbolic */
		if (e->exec_id == 0)
			continue;

		igt_subtest_f("capture-%s", e->name) {
			igt_require(gem_ring_has_physical_engine(fd, e->exec_id | e->flags));
			igt_require(gem_can_store_dword(fd, e->exec_id | e->flags));
			capture(fd, dir, e->exec_id | e->flags);
		}
	}

	igt_subtest_f("many-4K-zero") {
		igt_require(gem_can_store_dword(fd, 0));
		many(fd, dir, 1<<12, 0);
	}

	igt_subtest_f("many-4K-incremental") {
		igt_require(gem_can_store_dword(fd, 0));
		many(fd, dir, 1<<12, INCREMENTAL);
	}

	igt_subtest_f("many-2M-zero") {
		igt_require(gem_can_store_dword(fd, 0));
		many(fd, dir, 2<<20, 0);
	}

	igt_subtest_f("many-2M-incremental") {
		igt_require(gem_can_store_dword(fd, 0));
		many(fd, dir, 2<<20, INCREMENTAL);
	}

	igt_subtest_f("many-256M-incremental") {
		igt_require(gem_can_store_dword(fd, 0));
		many(fd, dir, 256<<20, INCREMENTAL);
	}

	/* And check we can read from different types of objects */

	igt_subtest_f("userptr") {
		igt_require(gem_can_store_dword(fd, 0));
		userptr(fd, dir);
	}

	igt_fixture {
		close(dir);
		igt_disallow_hang(fd, hang);
		close(fd);
	}
}
