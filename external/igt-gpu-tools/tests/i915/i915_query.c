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

#include <limits.h>

IGT_TEST_DESCRIPTION("Testing the i915 query uAPI.");

/*
 * We should at least get 3 bytes for data for each slices, subslices & EUs
 * masks.
 */
#define MIN_TOPOLOGY_ITEM_SIZE (sizeof(struct drm_i915_query_topology_info) + 3)

static int
__i915_query(int fd, struct drm_i915_query *q)
{
	if (igt_ioctl(fd, DRM_IOCTL_I915_QUERY, q))
		return -errno;
	return 0;
}

static int
__i915_query_items(int fd, struct drm_i915_query_item *items, uint32_t n_items)
{
	struct drm_i915_query q = {
		.num_items = n_items,
		.items_ptr = to_user_pointer(items),
	};
	return __i915_query(fd, &q);
}

#define i915_query_items(fd, items, n_items) do { \
		igt_assert_eq(__i915_query_items(fd, items, n_items), 0); \
		errno = 0; \
	} while (0)
#define i915_query_items_err(fd, items, n_items, err) do { \
		igt_assert_eq(__i915_query_items(fd, items, n_items), -err); \
	} while (0)

static bool has_query_supports(int fd)
{
	struct drm_i915_query query = {};

	return __i915_query(fd, &query) == 0;
}

static void test_query_garbage(int fd)
{
	struct drm_i915_query query;
	struct drm_i915_query_item item;

	/* Verify that invalid query pointers are rejected. */
	igt_assert_eq(__i915_query(fd, NULL), -EFAULT);
	igt_assert_eq(__i915_query(fd, (void *) -1), -EFAULT);

	/*
	 * Query flags field is currently valid only if equals to 0. This might
	 * change in the future.
	 */
	memset(&query, 0, sizeof(query));
	query.flags = 42;
	igt_assert_eq(__i915_query(fd, &query), -EINVAL);

	/* Test a couple of invalid pointers. */
	i915_query_items_err(fd, (void *) ULONG_MAX, 1, EFAULT);
	i915_query_items_err(fd, (void *) 0, 1, EFAULT);

	/* Test the invalid query id = 0. */
	memset(&item, 0, sizeof(item));
	i915_query_items_err(fd, &item, 1, EINVAL);
}

static void test_query_garbage_items(int fd)
{
	struct drm_i915_query_item items[2];
	struct drm_i915_query_item *items_ptr;
	int i, n_items;

	/*
	 * Query item flags field is currently valid only if equals to 0.
	 * Subject to change in the future.
	 */
	memset(items, 0, sizeof(items));
	items[0].query_id = DRM_I915_QUERY_TOPOLOGY_INFO;
	items[0].flags = 42;
	i915_query_items(fd, items, 1);
	igt_assert_eq(items[0].length, -EINVAL);

	/*
	 * Test an invalid query id in the second item and verify that the first
	 * one is properly processed.
	 */
	memset(items, 0, sizeof(items));
	items[0].query_id = DRM_I915_QUERY_TOPOLOGY_INFO;
	items[1].query_id = ULONG_MAX;
	i915_query_items(fd, items, 2);
	igt_assert_lte(MIN_TOPOLOGY_ITEM_SIZE, items[0].length);
	igt_assert_eq(items[1].length, -EINVAL);

	/*
	 * Test a invalid query id in the first item and verify that the second
	 * one is properly processed (the driver is expected to go through them
	 * all and place error codes in the failed items).
	 */
	memset(items, 0, sizeof(items));
	items[0].query_id = ULONG_MAX;
	items[1].query_id = DRM_I915_QUERY_TOPOLOGY_INFO;
	i915_query_items(fd, items, 2);
	igt_assert_eq(items[0].length, -EINVAL);
	igt_assert_lte(MIN_TOPOLOGY_ITEM_SIZE, items[1].length);

	/* Test a couple of invalid data pointer in query item. */
	memset(items, 0, sizeof(items));
	items[0].query_id = DRM_I915_QUERY_TOPOLOGY_INFO;
	i915_query_items(fd, items, 1);
	igt_assert_lte(MIN_TOPOLOGY_ITEM_SIZE, items[0].length);

	items[0].data_ptr = 0;
	i915_query_items(fd, items, 1);
	igt_assert_eq(items[0].length, -EFAULT);

	items[0].data_ptr = ULONG_MAX;
	i915_query_items(fd, items, 1);
	igt_assert_eq(items[0].length, -EFAULT);


	/* Test an invalid query item length. */
	memset(items, 0, sizeof(items));
	items[0].query_id = DRM_I915_QUERY_TOPOLOGY_INFO;
	items[1].query_id = DRM_I915_QUERY_TOPOLOGY_INFO;
	items[1].length = sizeof(struct drm_i915_query_topology_info) - 1;
	i915_query_items(fd, items, 2);
	igt_assert_lte(MIN_TOPOLOGY_ITEM_SIZE, items[0].length);
	igt_assert_eq(items[1].length, -EINVAL);

	/*
	 * Map memory for a query item in which the kernel is going to write the
	 * length of the item in the first ioctl(). Then unmap that memory and
	 * verify that the kernel correctly returns EFAULT as memory of the item
	 * has been removed from our address space.
	 */
	items_ptr = mmap(0, 4096, PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	items_ptr[0].query_id = DRM_I915_QUERY_TOPOLOGY_INFO;
	i915_query_items(fd, items_ptr, 1);
	igt_assert_lte(MIN_TOPOLOGY_ITEM_SIZE, items_ptr[0].length);
	munmap(items_ptr, 4096);
	i915_query_items_err(fd, items_ptr, 1, EFAULT);

	/*
	 * Map memory for a query item, then make it read only and verify that
	 * the kernel errors out with EFAULT.
	 */
	items_ptr = mmap(0, 4096, PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	items_ptr[0].query_id = DRM_I915_QUERY_TOPOLOGY_INFO;
	igt_assert_eq(0, mprotect(items_ptr, 4096, PROT_READ));
	i915_query_items_err(fd, items_ptr, 1, EFAULT);
	munmap(items_ptr, 4096);

	/*
	 * Allocate 2 pages, prepare those 2 pages with valid query items, then
	 * switch the second page to read only and expect an EFAULT error.
	 */
	items_ptr = mmap(0, 8192, PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	memset(items_ptr, 0, 8192);
	n_items = 8192 / sizeof(struct drm_i915_query_item);
	for (i = 0; i < n_items; i++)
		items_ptr[i].query_id = DRM_I915_QUERY_TOPOLOGY_INFO;
	mprotect(((uint8_t *)items_ptr) + 4096, 4096, PROT_READ);
	i915_query_items_err(fd, items_ptr, n_items, EFAULT);
	munmap(items_ptr, 8192);
}

/*
 * Allocate more on both sides of where the kernel is going to write and verify
 * that it writes only where it's supposed to.
 */
static void test_query_topology_kernel_writes(int fd)
{
	struct drm_i915_query_item item;
	struct drm_i915_query_topology_info *topo_info;
	uint8_t *_topo_info;
	int b, total_size;

	memset(&item, 0, sizeof(item));
	item.query_id = DRM_I915_QUERY_TOPOLOGY_INFO;
	i915_query_items(fd, &item, 1);
	igt_assert_lte(MIN_TOPOLOGY_ITEM_SIZE, item.length);

	total_size = item.length + 2 * sizeof(*_topo_info);
	_topo_info = malloc(total_size);
	memset(_topo_info, 0xff, total_size);
	topo_info = (struct drm_i915_query_topology_info *) (_topo_info + sizeof(*_topo_info));
	memset(topo_info, 0, item.length);

	item.data_ptr = to_user_pointer(topo_info);
	i915_query_items(fd, &item, 1);

	for (b = 0; b < sizeof(*_topo_info); b++) {
		igt_assert_eq(_topo_info[b], 0xff);
		igt_assert_eq(_topo_info[sizeof(*_topo_info) + item.length + b], 0xff);
	}
}

static bool query_topology_supported(int fd)
{
	struct drm_i915_query_item item = {
		.query_id = DRM_I915_QUERY_TOPOLOGY_INFO,
	};

	return __i915_query_items(fd, &item, 1) == 0 && item.length > 0;
}

static void test_query_topology_unsupported(int fd)
{
	struct drm_i915_query_item item = {
		.query_id = DRM_I915_QUERY_TOPOLOGY_INFO,
	};

	i915_query_items(fd, &item, 1);
	igt_assert_eq(item.length, -ENODEV);
}

static bool
slice_available(const struct drm_i915_query_topology_info *topo_info,
		int s)
{
	return (topo_info->data[s / 8] >> (s % 8)) & 1;
}

static bool
subslice_available(const struct drm_i915_query_topology_info *topo_info,
		   int s, int ss)
{
	return (topo_info->data[topo_info->subslice_offset +
				s * topo_info->subslice_stride +
				ss / 8] >> (ss % 8)) & 1;
}

static bool
eu_available(const struct drm_i915_query_topology_info *topo_info,
	     int s, int ss, int eu)
{
	return (topo_info->data[topo_info->eu_offset +
				(s * topo_info->max_subslices + ss) * topo_info->eu_stride +
				eu / 8] >> (eu % 8)) & 1;
}

/*
 * Verify that we get coherent values between the legacy getparam slice/subslice
 * masks and the new topology query.
 */
static void
test_query_topology_coherent_slice_mask(int fd)
{
	struct drm_i915_query_item item;
	struct drm_i915_query_topology_info *topo_info;
	drm_i915_getparam_t gp;
	int slice_mask, subslice_mask;
	int s, topology_slices, topology_subslices_slice0;
	int32_t first_query_length;

	gp.param = I915_PARAM_SLICE_MASK;
	gp.value = &slice_mask;
	igt_skip_on(igt_ioctl(fd, DRM_IOCTL_I915_GETPARAM, &gp) != 0);

	gp.param = I915_PARAM_SUBSLICE_MASK;
	gp.value = &subslice_mask;
	igt_skip_on(igt_ioctl(fd, DRM_IOCTL_I915_GETPARAM, &gp) != 0);

	/* Slices */
	memset(&item, 0, sizeof(item));
	item.query_id = DRM_I915_QUERY_TOPOLOGY_INFO;
	i915_query_items(fd, &item, 1);
	/* We expect at least one byte for each slices, subslices & EUs masks. */
	igt_assert_lte(MIN_TOPOLOGY_ITEM_SIZE, item.length);
	first_query_length = item.length;

	topo_info = calloc(1, item.length);

	item.data_ptr = to_user_pointer(topo_info);
	i915_query_items(fd, &item, 1);
	/* We should get the same size once the data has been written. */
	igt_assert_eq(first_query_length, item.length);
	/* We expect at least one byte for each slices, subslices & EUs masks. */
	igt_assert_lte(MIN_TOPOLOGY_ITEM_SIZE, item.length);

	topology_slices = 0;
	for (s = 0; s < topo_info->max_slices; s++) {
		if (slice_available(topo_info, s))
			topology_slices |= 1UL << s;
	}

	igt_debug("slice mask getparam=0x%x / query=0x%x\n",
		  slice_mask, topology_slices);

	/* These 2 should always match. */
	igt_assert_eq(slice_mask, topology_slices);

	topology_subslices_slice0 = 0;
	for (s = 0; s < topo_info->max_subslices; s++) {
		if (subslice_available(topo_info, 0, s))
			topology_subslices_slice0 |= 1UL << s;
	}

	igt_debug("subslice mask getparam=0x%x / query=0x%x\n",
		  subslice_mask, topology_subslices_slice0);

	/*
	 * I915_PARAM_SUBSLICE_MASK returns the value for slice0, we should
	 * match the values for the first slice of the topology.
	 */
	igt_assert_eq(subslice_mask, topology_subslices_slice0);

	free(topo_info);
}

/*
 * Verify that we get same total number of EUs from getparam and topology query.
 */
static void
test_query_topology_matches_eu_total(int fd)
{
	struct drm_i915_query_item item;
	struct drm_i915_query_topology_info *topo_info;
	drm_i915_getparam_t gp;
	int n_eus, n_eus_topology, s;

	gp.param = I915_PARAM_EU_TOTAL;
	gp.value = &n_eus;
	do_ioctl(fd, DRM_IOCTL_I915_GETPARAM, &gp);
	igt_debug("n_eus=%i\n", n_eus);

	memset(&item, 0, sizeof(item));
	item.query_id = DRM_I915_QUERY_TOPOLOGY_INFO;
	i915_query_items(fd, &item, 1);

	topo_info = calloc(1, item.length);

	item.data_ptr = to_user_pointer(topo_info);
	i915_query_items(fd, &item, 1);

	igt_debug("max_slices=%hu max_subslices=%hu max_eus_per_subslice=%hu\n",
		  topo_info->max_slices, topo_info->max_subslices,
		  topo_info->max_eus_per_subslice);
	igt_debug(" subslice_offset=%hu subslice_stride=%hu\n",
		  topo_info->subslice_offset, topo_info->subslice_stride);
	igt_debug(" eu_offset=%hu eu_stride=%hu\n",
		  topo_info->eu_offset, topo_info->eu_stride);

	n_eus_topology = 0;
	for (s = 0; s < topo_info->max_slices; s++) {
		int ss;

		igt_debug("slice%i: (%s)\n", s,
			  slice_available(topo_info, s) ? "available" : "fused");

		if (!slice_available(topo_info, s))
			continue;

		for (ss = 0; ss < topo_info->max_subslices; ss++) {
			int eu, n_subslice_eus = 0;

			igt_debug("\tsubslice%i: (%s)\n", ss,
				  subslice_available(topo_info, s, ss) ? "available" : "fused");

			if (!subslice_available(topo_info, s, ss))
				continue;

			igt_debug("\t\teu_mask: 0b");
			for (eu = 0; eu < topo_info->max_eus_per_subslice; eu++) {
				uint8_t val = eu_available(topo_info, s, ss,
							   topo_info->max_eus_per_subslice - 1 - eu);
				igt_debug("%hhi", val);
				n_subslice_eus += __builtin_popcount(val);
				n_eus_topology += __builtin_popcount(val);
			}

			igt_debug(" (%i)\n", n_subslice_eus);

			/* Sanity checks. */
			if (n_subslice_eus > 0) {
				igt_assert(slice_available(topo_info, s));
				igt_assert(subslice_available(topo_info, s, ss));
			}
			if (subslice_available(topo_info, s, ss)) {
				igt_assert(slice_available(topo_info, s));
			}
		}
	}

	free(topo_info);

	igt_assert(n_eus_topology == n_eus);
}

/*
 * Verify some numbers on Gens that we know for sure the characteristics from
 * the PCI ids.
 */
static void
test_query_topology_known_pci_ids(int fd, int devid)
{
	const struct intel_device_info *dev_info = intel_get_device_info(devid);
	struct drm_i915_query_item item;
	struct drm_i915_query_topology_info *topo_info;
	int n_slices = 0, n_subslices = 0;
	int s, ss;

	/* The GT size on some Broadwell skus is not defined, skip those. */
	igt_skip_on(dev_info->gt == 0);

	memset(&item, 0, sizeof(item));
	item.query_id = DRM_I915_QUERY_TOPOLOGY_INFO;
	i915_query_items(fd, &item, 1);

	topo_info = (struct drm_i915_query_topology_info *) calloc(1, item.length);

	item.data_ptr = to_user_pointer(topo_info);
	i915_query_items(fd, &item, 1);

	for (s = 0; s < topo_info->max_slices; s++) {
		if (slice_available(topo_info, s))
			n_slices++;

		for (ss = 0; ss < topo_info->max_subslices; ss++) {
			if (subslice_available(topo_info, s, ss))
				n_subslices++;
		}
	}

	igt_debug("Platform=%s GT=%u slices=%u subslices=%u\n",
		  dev_info->codename, dev_info->gt, n_slices, n_subslices);

	switch (dev_info->gt) {
	case 1:
		igt_assert_eq(n_slices, 1);
		igt_assert(n_subslices == 1 || n_subslices == 2 || n_subslices == 3);
		break;
	case 2:
		igt_assert_eq(n_slices, 1);
		if (dev_info->is_haswell)
			igt_assert_eq(n_subslices, 2);
		else
			igt_assert_eq(n_subslices, 3);
		break;
	case 3:
		igt_assert_eq(n_slices, 2);
		if (dev_info->is_haswell)
			igt_assert_eq(n_subslices, 2 * 2);
		else
			igt_assert_eq(n_subslices, 2 * 3);
		break;
	case 4:
		igt_assert_eq(n_slices, 3);
		igt_assert_eq(n_subslices, 3 * 3);
		break;
	default:
		igt_assert(false);
	}

	free(topo_info);
}

static bool query_engine_info_supported(int fd)
{
	struct drm_i915_query_item item = {
		.query_id = DRM_I915_QUERY_ENGINE_INFO,
	};

	return __i915_query_items(fd, &item, 1) == 0 && item.length > 0;
}

static void engines_invalid(int fd)
{
	struct drm_i915_query_engine_info *engines;
	struct drm_i915_query_item item;
	unsigned int len;

	/* Flags is MBZ. */
	memset(&item, 0, sizeof(item));
	item.query_id = DRM_I915_QUERY_ENGINE_INFO;
	item.flags = 1;
	i915_query_items(fd, &item, 1);
	igt_assert_eq(item.length, -EINVAL);

	/* Length not zero and not greater or equal required size. */
	memset(&item, 0, sizeof(item));
	item.query_id = DRM_I915_QUERY_ENGINE_INFO;
	item.length = 1;
	i915_query_items(fd, &item, 1);
	igt_assert_eq(item.length, -EINVAL);

	/* Query correct length. */
	memset(&item, 0, sizeof(item));
	item.query_id = DRM_I915_QUERY_ENGINE_INFO;
	i915_query_items(fd, &item, 1);
	igt_assert(item.length >= 0);
	len = item.length;

	engines = malloc(len);
	igt_assert(engines);

	/* Ivalid pointer. */
	memset(&item, 0, sizeof(item));
	item.query_id = DRM_I915_QUERY_ENGINE_INFO;
	item.length = len;
	i915_query_items(fd, &item, 1);
	igt_assert_eq(item.length, -EFAULT);

	/* All fields in engines query are MBZ and only filled by the kernel. */

	memset(engines, 0, len);
	engines->num_engines = 1;
	memset(&item, 0, sizeof(item));
	item.query_id = DRM_I915_QUERY_ENGINE_INFO;
	item.length = len;
	item.data_ptr = to_user_pointer(engines);
	i915_query_items(fd, &item, 1);
	igt_assert_eq(item.length, -EINVAL);

	memset(engines, 0, len);
	engines->rsvd[0] = 1;
	memset(&item, 0, sizeof(item));
	item.query_id = DRM_I915_QUERY_ENGINE_INFO;
	item.length = len;
	item.data_ptr = to_user_pointer(engines);
	i915_query_items(fd, &item, 1);
	igt_assert_eq(item.length, -EINVAL);

	memset(engines, 0, len);
	engines->rsvd[1] = 1;
	memset(&item, 0, sizeof(item));
	item.query_id = DRM_I915_QUERY_ENGINE_INFO;
	item.length = len;
	item.data_ptr = to_user_pointer(engines);
	i915_query_items(fd, &item, 1);
	igt_assert_eq(item.length, -EINVAL);

	memset(engines, 0, len);
	engines->rsvd[2] = 1;
	memset(&item, 0, sizeof(item));
	item.query_id = DRM_I915_QUERY_ENGINE_INFO;
	item.length = len;
	item.data_ptr = to_user_pointer(engines);
	i915_query_items(fd, &item, 1);
	igt_assert_eq(item.length, -EINVAL);

	free(engines);

	igt_assert(len <= 4096);
	engines = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON,
		       -1, 0);
	igt_assert(engines != MAP_FAILED);

	/* PROT_NONE is similar to unmapped area. */
	memset(engines, 0, len);
	igt_assert_eq(mprotect(engines, len, PROT_NONE), 0);
	memset(&item, 0, sizeof(item));
	item.query_id = DRM_I915_QUERY_ENGINE_INFO;
	item.length = len;
	item.data_ptr = to_user_pointer(engines);
	i915_query_items(fd, &item, 1);
	igt_assert_eq(item.length, -EFAULT);
	igt_assert_eq(mprotect(engines, len, PROT_WRITE), 0);

	/* Read-only so kernel cannot fill the data back. */
	memset(engines, 0, len);
	igt_assert_eq(mprotect(engines, len, PROT_READ), 0);
	memset(&item, 0, sizeof(item));
	item.query_id = DRM_I915_QUERY_ENGINE_INFO;
	item.length = len;
	item.data_ptr = to_user_pointer(engines);
	i915_query_items(fd, &item, 1);
	igt_assert_eq(item.length, -EFAULT);

	munmap(engines, 4096);
}

static bool
has_engine(struct drm_i915_query_engine_info *engines,
	   unsigned class, unsigned instance)
{
	unsigned int i;

	for (i = 0; i < engines->num_engines; i++) {
		struct drm_i915_engine_info *engine =
			(struct drm_i915_engine_info *)&engines->engines[i];

		if (engine->engine.engine_class == class &&
		    engine->engine.engine_instance == instance)
			return true;
	}

	return false;
}

static void engines(int fd)
{
	struct drm_i915_query_engine_info *engines;
	struct drm_i915_query_item item;
	unsigned int len, i;

	engines = malloc(4096);
	igt_assert(engines);

	/* Query required buffer length. */
	memset(engines, 0, 4096);
	memset(&item, 0, sizeof(item));
	item.query_id = DRM_I915_QUERY_ENGINE_INFO;
	item.data_ptr = to_user_pointer(engines);
	i915_query_items(fd, &item, 1);
	igt_assert(item.length >= 0);
	igt_assert(item.length <= 4096);
	len = item.length;

	/* Check length larger than required works and reports same length. */
	memset(engines, 0, 4096);
	memset(&item, 0, sizeof(item));
	item.query_id = DRM_I915_QUERY_ENGINE_INFO;
	item.length = 4096;
	item.data_ptr = to_user_pointer(engines);
	i915_query_items(fd, &item, 1);
	igt_assert_eq(item.length, len);

	/* Actual query. */
	memset(engines, 0, 4096);
	memset(&item, 0, sizeof(item));
	item.query_id = DRM_I915_QUERY_ENGINE_INFO;
	item.length = len;
	item.data_ptr = to_user_pointer(engines);
	i915_query_items(fd, &item, 1);
	igt_assert_eq(item.length, len);

	/* Every GPU has at least one engine. */
	igt_assert(engines->num_engines > 0);

	/* MBZ fields. */
	igt_assert_eq(engines->rsvd[0], 0);
	igt_assert_eq(engines->rsvd[1], 0);
	igt_assert_eq(engines->rsvd[2], 0);

	/* Check results match the legacy GET_PARAM (where we can). */
	for (i = 0; i < engines->num_engines; i++) {
		struct drm_i915_engine_info *engine =
			(struct drm_i915_engine_info *)&engines->engines[i];

		igt_debug("%u: class=%u instance=%u flags=%llx capabilities=%llx\n",
			  i,
			  engine->engine.engine_class,
			  engine->engine.engine_instance,
			  engine->flags,
			  engine->capabilities);

		/* MBZ fields. */
		igt_assert_eq(engine->rsvd0, 0);
		igt_assert_eq(engine->rsvd1[0], 0);
		igt_assert_eq(engine->rsvd1[1], 0);

		switch (engine->engine.engine_class) {
		case I915_ENGINE_CLASS_RENDER:
			/* Will be tested later. */
			break;
		case I915_ENGINE_CLASS_COPY:
			igt_assert(gem_has_blt(fd));
			break;
		case I915_ENGINE_CLASS_VIDEO:
			switch (engine->engine.engine_instance) {
			case 0:
				igt_assert(gem_has_bsd(fd));
				break;
			case 1:
				igt_assert(gem_has_bsd2(fd));
				break;
			}
			break;
		case I915_ENGINE_CLASS_VIDEO_ENHANCE:
			igt_assert(gem_has_vebox(fd));
			break;
		default:
			igt_assert(0);
		}
	}

	/* Reverse check to the above - all GET_PARAM engines are present. */
	igt_assert(has_engine(engines, I915_ENGINE_CLASS_RENDER, 0));
	if (gem_has_blt(fd))
		igt_assert(has_engine(engines, I915_ENGINE_CLASS_COPY, 0));
	if (gem_has_bsd(fd))
		igt_assert(has_engine(engines, I915_ENGINE_CLASS_VIDEO, 0));
	if (gem_has_bsd2(fd))
		igt_assert(has_engine(engines, I915_ENGINE_CLASS_VIDEO, 1));
	if (gem_has_vebox(fd))
		igt_assert(has_engine(engines, I915_ENGINE_CLASS_VIDEO_ENHANCE,
				       0));

	free(engines);
}

igt_main
{
	int fd = -1;
	int devid;

	igt_fixture {
		fd = drm_open_driver(DRIVER_INTEL);
		igt_require(has_query_supports(fd));
		devid = intel_get_drm_devid(fd);
	}

	igt_subtest("query-garbage")
		test_query_garbage(fd);

	igt_subtest("query-garbage-items") {
		igt_require(query_topology_supported(fd));
		test_query_garbage_items(fd);
	}

	igt_subtest("query-topology-kernel-writes") {
		igt_require(query_topology_supported(fd));
		test_query_topology_kernel_writes(fd);
	}

	igt_subtest("query-topology-unsupported") {
		igt_require(!query_topology_supported(fd));
		test_query_topology_unsupported(fd);
	}

	igt_subtest("query-topology-coherent-slice-mask") {
		igt_require(query_topology_supported(fd));
		test_query_topology_coherent_slice_mask(fd);
	}

	igt_subtest("query-topology-matches-eu-total") {
		igt_require(query_topology_supported(fd));
		test_query_topology_matches_eu_total(fd);
	}

	igt_subtest("query-topology-known-pci-ids") {
		igt_require(query_topology_supported(fd));
		igt_require(IS_HASWELL(devid) || IS_BROADWELL(devid) ||
			    IS_SKYLAKE(devid) || IS_KABYLAKE(devid) ||
			    IS_COFFEELAKE(devid));
		test_query_topology_known_pci_ids(fd, devid);
	}

	igt_subtest_group {
		igt_fixture {
			igt_require(query_engine_info_supported(fd));
		}

		igt_subtest("engine-info-invalid")
			engines_invalid(fd);

		igt_subtest("engine-info")
			engines(fd);
	}

	igt_fixture {
		close(fd);
	}
}
