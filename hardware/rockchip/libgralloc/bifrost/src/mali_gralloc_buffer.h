/*
 * Copyright (C) 2017-2020 ARM Limited. All rights reserved.
 *
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef MALI_GRALLOC_BUFFER_H_
#define MALI_GRALLOC_BUFFER_H_

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <cutils/native_handle.h>
#include <string.h>

#ifdef __cplusplus
#include <new>
#endif

#include "mali_gralloc_log.h"
#include "mali_gralloc_private_interface_types.h"

/* the max string size of GRALLOC_HARDWARE_GPU0 & GRALLOC_HARDWARE_FB0
 * 8 is big enough for "gpu0" & "fb0" currently
 */
#define MALI_GRALLOC_HARDWARE_MAX_STR_LEN 8

/* Define number of shared file descriptors. Not guaranteed to be constant for a private_handle_t object
 * as fds that do not get initialized may instead be treated as integers.
 */
#define GRALLOC_ARM_NUM_FDS 2

#define NUM_INTS_IN_PRIVATE_HANDLE ((sizeof(struct private_handle_t) - sizeof(native_handle)) / sizeof(int) - GRALLOC_ARM_NUM_FDS)

#define SZ_4K 0x00001000
#define SZ_2M 0x00200000

/*
 * Maximum number of pixel format planes.
 * Plane [0]: Single plane formats (inc. RGB, YUV) and Y
 * Plane [1]: U/V, UV
 * Plane [2]: V/U
 */
#define MAX_PLANES 3

#ifdef __cplusplus
#define DEFAULT_INITIALIZER(x) = x
#else
#define DEFAULT_INITIALIZER(x)
#endif

typedef struct plane_info {

	/*
	 * Offset to plane (in bytes),
	 * from the start of the allocation.
	 */
	uint32_t offset;

	/*
	 * Byte Stride: number of bytes between two vertically adjacent
	 * pixels in given plane. This can be mathematically described by:
	 *
	 * byte_stride = ALIGN((alloc_width * bpp)/8, alignment)
	 *
	 * where,
	 *
	 * alloc_width: width of plane in pixels (c.f. pixel_stride)
	 * bpp: average bits per pixel
	 * alignment (in bytes): dependent upon pixel format and usage
	 *
	 * For uncompressed allocations, byte_stride might contain additional
	 * padding beyond the alloc_width. For AFBC, alignment is zero.
	 */
	uint32_t byte_stride;

	/*
	 * Dimensions of plane (in pixels).
	 *
	 * For single plane formats, pixels equates to luma samples.
	 * For multi-plane formats, pixels equates to the number of sample sites
	 * for the corresponding plane, even if subsampled.
	 *
	 * AFBC compressed formats: requested width/height are rounded-up
	 * to a whole AFBC superblock/tile (next superblock at minimum).
	 * Uncompressed formats: dimensions typically match width and height
	 * but might require pixel stride alignment.
	 *
	 * See 'byte_stride' for relationship between byte_stride and alloc_width.
	 *
	 * Any crop rectangle defined by GRALLOC_ARM_BUFFER_ATTR_CROP_RECT must
	 * be wholly within the allocation dimensions. The crop region top-left
	 * will be relative to the start of allocation.
	 */
	uint32_t alloc_width;
	uint32_t alloc_height;
} plane_info_t;

struct private_handle_t;

#ifndef __cplusplus
/* C99 with pedantic don't allow anonymous unions which is used in below struct
 * Disable pedantic for C for this struct only.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#ifdef __cplusplus
struct private_handle_t : public native_handle
{
#else
struct private_handle_t
{
	struct native_handle nativeHandle;
#endif

#ifdef __cplusplus
	/* Never intended to be used from C code */
	enum
	{
		PRIV_FLAGS_FRAMEBUFFER = 0x00000001,
		PRIV_FLAGS_USES_ION_COMPOUND_HEAP = 0x00000002,
		PRIV_FLAGS_USES_ION = 0x00000004,
		PRIV_FLAGS_USES_ION_DMA_HEAP = 0x00000008
	};

	enum
	{
		LOCK_STATE_WRITE = 1 << 31,
		LOCK_STATE_MAPPED = 1 << 30,
		LOCK_STATE_READ_MASK = 0x3FFFFFFF
	};
#endif

	/*
	 * Shared file descriptor for dma_buf sharing. This must be the first element in the
	 * structure so that binder knows where it is and can properly share it between
	 * processes.
	 * DO NOT MOVE THIS ELEMENT!
	 */
	int share_fd DEFAULT_INITIALIZER(-1);
	int share_attr_fd DEFAULT_INITIALIZER(-1);

	// ints
	int magic DEFAULT_INITIALIZER(sMagic);
	int flags DEFAULT_INITIALIZER(0);

	/*
	 * Input properties.
	 *
	 * req_format: Pixel format, base + private modifiers.
	 * width/height: Buffer dimensions.
	 * producer/consumer_usage: Buffer usage (indicates IP)
	 */
	int width DEFAULT_INITIALIZER(0);
	int height DEFAULT_INITIALIZER(0);
	int req_format DEFAULT_INITIALIZER(0);
	uint64_t producer_usage DEFAULT_INITIALIZER(0);
	uint64_t consumer_usage DEFAULT_INITIALIZER(0);

	/*
	 * DEPRECATED members.
	 * Equivalent information can be obtained from other fields:
	 *
	 * - 'internal_format' --> alloc_format
	 * - 'stride' (pixel stride) ~= plane_info[0].alloc_width
	 * - 'byte_stride' ~= plane_info[0].byte_stride
	 * - 'internalWidth' ~= plane_info[0].alloc_width
	 * - 'internalHeight' ~= plane_info[0].alloc_height
	 *
	 * '~=' (approximately equal) is used because the fields were either previously
	 * incorrectly populated by gralloc or the meaning has slightly changed.
	 *
	 * NOTE: 'stride' values sometimes vary significantly from plane_info[0].alloc_width.
	 */
	uint64_t internal_format DEFAULT_INITIALIZER(0);
	int stride DEFAULT_INITIALIZER(0);
	int byte_stride DEFAULT_INITIALIZER(0);
	int internalWidth DEFAULT_INITIALIZER(0);
	int internalHeight DEFAULT_INITIALIZER(0);

	/*
	 * Allocation properties.
	 *
	 * alloc_format: Pixel format (base + modifiers). NOTE: base might differ from requested
	 *               format (req_format) where fallback to single-plane format was required.
	 * plane_info:   Per plane allocation information.
	 * size:         Total bytes allocated for buffer (inc. all planes, layers. etc.).
	 * layer_count:  Number of layers allocated to buffer.
	 *               All layers are the same size (in bytes).
	 *               Multi-layers supported in v1.0, where GRALLOC1_CAPABILITY_LAYERED_BUFFERS is enabled.
	 *               Layer size: 'size' / 'layer_count'.
	 *               Layer (n) offset: n * ('size' / 'layer_count'), n=0 for the first layer.
	 *
	 */
	uint64_t alloc_format DEFAULT_INITIALIZER(0);
	plane_info_t plane_info[MAX_PLANES] DEFAULT_INITIALIZER({});
	int size DEFAULT_INITIALIZER(0);
	uint32_t layer_count DEFAULT_INITIALIZER(0);


	union
	{
		void *base DEFAULT_INITIALIZER(NULL);
		uint64_t padding;
	};
	uint64_t backing_store_id DEFAULT_INITIALIZER(0x0);
	int backing_store_size DEFAULT_INITIALIZER(0);
	int cpu_read DEFAULT_INITIALIZER(0);               /**< Buffer is locked for CPU read when non-zero. */
	int cpu_write DEFAULT_INITIALIZER(0);              /**< Buffer is locked for CPU write when non-zero. */
	int allocating_pid DEFAULT_INITIALIZER(0);
	int remote_pid DEFAULT_INITIALIZER(-1);
	int ref_count DEFAULT_INITIALIZER(0);
	// locally mapped shared attribute area
	union
	{
		void *attr_base DEFAULT_INITIALIZER(MAP_FAILED);
		uint64_t padding3;
	};

	/*
	 * Deprecated.
	 * Use GRALLOC_ARM_BUFFER_ATTR_DATASPACE
	 * instead.
	 */
	mali_gralloc_yuv_info yuv_info DEFAULT_INITIALIZER(MALI_YUV_NO_INFO);

	// For framebuffer only
	int fd DEFAULT_INITIALIZER(-1);
	union
	{
		off_t offset DEFAULT_INITIALIZER(0);
		uint64_t padding4;
	};

	/* Size of the attribute shared region in bytes. */
	uint64_t attr_size DEFAULT_INITIALIZER(0);

	uint64_t reserved_region_size DEFAULT_INITIALIZER(0);

	uint64_t imapper_version DEFAULT_INITIALIZER(0);

#ifdef __cplusplus
	/*
	 * We track the number of integers in the structure. There are 16 unconditional
	 * integers (magic - pid, yuv_info, fd and offset). Note that the fd element is
	 * considered an int not an fd because it is not intended to be used outside the
	 * surface flinger process. The GRALLOC_ARM_NUM_INTS variable is used to track the
	 * number of integers that are conditionally included. Similar considerations apply
	 * to the number of fds.
	 */
	static const int sNumFds = GRALLOC_ARM_NUM_FDS;
	static const int sMagic = 0x3141592;

	private_handle_t(int _flags, int _size, void *_base, uint64_t _consumer_usage, uint64_t _producer_usage,
	                 int fb_file, off_t fb_offset, int _byte_stride, int _width, int _height, uint64_t _alloc_format)
	    : flags(_flags)
	    , producer_usage(_producer_usage)
	    , consumer_usage(_consumer_usage)
	    , alloc_format(_alloc_format)
	    , size(_size)
	    , base(_base)
	    , allocating_pid(getpid())
	    , ref_count(1)
	    , fd(fb_file)
	    , offset(fb_offset)
	{
		version = sizeof(native_handle);
		numFds = sNumFds;
		numInts = NUM_INTS_IN_PRIVATE_HANDLE;

		plane_info[0].offset = fb_offset;
		plane_info[0].byte_stride = _byte_stride;
		plane_info[0].alloc_width = _width;
		plane_info[0].alloc_height = _height;
	}

	private_handle_t(int _flags, int _size, uint64_t _consumer_usage, uint64_t _producer_usage,
	                 int _shared_fd, int _req_format, uint64_t _internal_format, uint64_t _alloc_format, int _width,
	                 int _height, int _stride, int _internal_width, int _internal_height, int _byte_stride,
	                 int _backing_store_size, uint64_t _layer_count, plane_info_t _plane_info[MAX_PLANES])
	    : share_fd(_shared_fd)
	    , flags(_flags)
	    , width(_width)
	    , height(_height)
	    , req_format(_req_format)
	    , producer_usage(_producer_usage)
	    , consumer_usage(_consumer_usage)
	    , internal_format(_internal_format)
	    , stride(_stride)
	    , byte_stride(_byte_stride)
	    , internalWidth(_internal_width)
	    , internalHeight(_internal_height)
	    , alloc_format(_alloc_format)
	    , size(_size)
	    , layer_count(_layer_count)
	    , backing_store_size(_backing_store_size)
	    , allocating_pid(getpid())
	    , ref_count(1)
	{
		version = sizeof(native_handle);
		numFds = sNumFds;
		numInts = NUM_INTS_IN_PRIVATE_HANDLE;
		memcpy(plane_info, _plane_info, sizeof(plane_info_t) * MAX_PLANES);
	}

	~private_handle_t()
	{
		magic = 0;
	}

	bool usesPhysicallyContiguousMemory()
	{
		return (flags & PRIV_FLAGS_FRAMEBUFFER) ? true : false;
	}

	static int validate(const native_handle *h)
	{
		const private_handle_t *hnd = (const private_handle_t *)h;
		if (!h || h->version != sizeof(native_handle) || hnd->magic != sMagic ||
		    h->numFds + h->numInts != NUM_INTS_IN_PRIVATE_HANDLE + GRALLOC_ARM_NUM_FDS)
		{
			return -EINVAL;
		}
		return 0;
	}

	bool is_multi_plane() const
	{
		/* For multi-plane, the byte stride for the second plane will always be non-zero. */
		return (plane_info[1].byte_stride != 0);
	}

	static private_handle_t *dynamicCast(const native_handle *in)
	{
		if (validate(in) == 0)
		{
			return (private_handle_t *)in;
		}

		return NULL;
	}
#endif
};
#ifndef __cplusplus
/* Restore previous diagnostic for pedantic */
#pragma GCC diagnostic pop
#endif

#ifdef __cplusplus
static inline private_handle_t *make_private_handle(
    int flags, int size, uint64_t consumer_usage, uint64_t producer_usage,
    int shared_fd, int required_format, uint64_t internal_format, uint64_t allocated_format,
    int width, int height, int stride, int internal_width, int internal_height, int byte_stride,
    int backing_store_size, uint64_t layer_count, plane_info_t *plane_info)
{
	void *mem = native_handle_create(GRALLOC_ARM_NUM_FDS, NUM_INTS_IN_PRIVATE_HANDLE);
	if (mem == nullptr)
	{
		MALI_GRALLOC_LOGE("private_handle_t allocation failed");
		return nullptr;
	}

	return new(mem) private_handle_t(flags, size, consumer_usage, producer_usage,
	                                 shared_fd, required_format, internal_format, allocated_format,
	                                 width, height, stride, internal_width, internal_height, byte_stride,
	                                 backing_store_size, layer_count, plane_info);
}
#endif

#endif /* MALI_GRALLOC_BUFFER_H_ */
