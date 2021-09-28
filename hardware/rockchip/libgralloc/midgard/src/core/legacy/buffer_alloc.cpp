/*
 * Copyright (C) 2018-2020 ARM Limited. All rights reserved.
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

#include <hardware/hardware.h>
#include <inttypes.h>
#include <atomic>

#include <hardware/gralloc1.h>

#include "mali_gralloc_bufferallocation.h"
#include "allocator/mali_gralloc_ion.h"
#include "mali_gralloc_private_interface_types.h"
#include "mali_gralloc_buffer.h"
#include "gralloc_buffer_priv.h"
#include "mali_gralloc_bufferdescriptor.h"
#include "mali_gralloc_debug.h"
#include "legacy/buffer_alloc.h"

namespace legacy
{

#define AFBC_PIXELS_PER_BLOCK 16
#define AFBC_HEADER_BUFFER_BYTES_PER_BLOCKENTRY 16

#define AFBC_NORMAL_WIDTH_ALIGN 16
#define AFBC_NORMAL_HEIGHT_ALIGN 16
#define AFBC_WIDEBLK_WIDTH_ALIGN 32
#define AFBC_WIDEBLK_HEIGHT_ALIGN 16

/* When using tiled headers the alignment is 8 times the super-block size in each dimension. */
#define AFBC_TILED_HEADERS_BASIC_WIDTH_ALIGN 128
#define AFBC_TILED_HEADERS_BASIC_HEIGHT_ALIGN 128
#define AFBC_TILED_HEADERS_WIDEBLK_WIDTH_ALIGN 256
#define AFBC_TILED_HEADERS_WIDEBLK_HEIGHT_ALIGN 64
/* Tiled headers are always enabled with extra-wide block. */
#define AFBC_TILED_EXTRAWIDEBLK_WIDTH_ALIGN 512
#define AFBC_TILED_EXTRAWIDEBLK_HEIGHT_ALIGN 32

// This value is platform specific and should be set according to hardware YUV planes restrictions.
// Please note that EGL winsys platform config file needs to use the same value when importing buffers.
#if 0
#define YUV_MALI_PLANE_ALIGN 128
#else
#define YUV_MALI_PLANE_ALIGN 16
#endif

// Default YUV stride aligment in Android
#define YUV_ANDROID_PLANE_ALIGN 16

static int mali_gralloc_buffer_free_internal(buffer_handle_t *pHandle, uint32_t num_hnds);



static void afbc_buffer_align(const alloc_type_t type, int *size)
{
	const uint16_t AFBC_BODY_BUFFER_BYTE_ALIGNMENT = 1024;

	int buffer_byte_alignment = AFBC_BODY_BUFFER_BYTE_ALIGNMENT;

	if (type.is_tiled)
	{
		buffer_byte_alignment = 4 * AFBC_BODY_BUFFER_BYTE_ALIGNMENT;
	}

	*size = GRALLOC_ALIGN(*size, buffer_byte_alignment);
}


/*
 * Alignment of width/height (in pixels) calculated for worst case (buffer size).
 */
void get_afbc_alignment(const int width, const int height, const alloc_type_t type,
                        int * const w_aligned, int * const h_aligned)
{
	*h_aligned = GRALLOC_ALIGN(height, AFBC_NORMAL_HEIGHT_ALIGN);

	if (type.primary_type == UNCOMPRESSED)
	{
		*w_aligned = width;
		*h_aligned = height;
		return;
	}
	else if (type.is_tiled && type.primary_type == AFBC)
	{
		*w_aligned = GRALLOC_ALIGN(width, AFBC_TILED_HEADERS_BASIC_WIDTH_ALIGN);
		*h_aligned = GRALLOC_ALIGN(height, AFBC_TILED_HEADERS_BASIC_HEIGHT_ALIGN);
	}
	else if (type.is_tiled && type.primary_type == AFBC_WIDEBLK)
	{
		*w_aligned = GRALLOC_ALIGN(width, AFBC_TILED_HEADERS_WIDEBLK_WIDTH_ALIGN);
		*h_aligned = GRALLOC_ALIGN(height, AFBC_TILED_HEADERS_WIDEBLK_HEIGHT_ALIGN);
	}
	else if (type.primary_type == AFBC_PADDED)
	{
		*w_aligned = GRALLOC_ALIGN(width, 64);
	}
	else if (type.primary_type == AFBC_WIDEBLK)
	{
		*w_aligned = GRALLOC_ALIGN(width, AFBC_WIDEBLK_WIDTH_ALIGN);
		*h_aligned = GRALLOC_ALIGN(height, AFBC_WIDEBLK_HEIGHT_ALIGN);
	}
	else if (type.primary_type == AFBC_EXTRAWIDEBLK)
	{
		*w_aligned = GRALLOC_ALIGN(width, AFBC_TILED_EXTRAWIDEBLK_WIDTH_ALIGN);
		*h_aligned = GRALLOC_ALIGN(height, AFBC_TILED_EXTRAWIDEBLK_HEIGHT_ALIGN);
	}
	else
	{
		*w_aligned = GRALLOC_ALIGN(width, AFBC_NORMAL_WIDTH_ALIGN);
	}
}



/*
 * Computes the strides and size for an RGB buffer
 *
 * width               width of the buffer in pixels
 * height              height of the buffer in pixels
 * pixel_size          size of one pixel in bytes
 *
 * pixel_stride (out)  stride of the buffer in pixels
 * byte_stride  (out)  stride of the buffer in bytes
 * size         (out)  size of the buffer in bytes
 * type         (in)   if buffer should be allocated for afbc
 */
static void get_rgb_stride_and_size(int width, int height, int pixel_size, bool cpu_usage,
                                    int *pixel_stride, int *byte_stride, size_t *size, alloc_type_t type)
{
	int stride;

	if (type.primary_type != UNCOMPRESSED)
	{
		int nblocks;
		int body_alignment;

		stride = width * pixel_size;
		stride = GRALLOC_ALIGN(stride, 64);

		if (byte_stride != NULL)
		{
			*byte_stride = stride;
		}

		if (pixel_stride != NULL)
		{
			*pixel_stride = stride / pixel_size;
		}

		nblocks = width / AFBC_PIXELS_PER_BLOCK * height / AFBC_PIXELS_PER_BLOCK;

		if (size != NULL)
		{
			int header_size = nblocks * AFBC_HEADER_BUFFER_BYTES_PER_BLOCKENTRY;
			afbc_buffer_align(type, &header_size);
			*size = stride * height + header_size;
		}
	}
	else
	{
		stride = width * pixel_size;

		/* Align the lines to 64 bytes.
		 * It's more efficient to write to 64-byte aligned addresses because it's the burst size on the bus */
		const int stride_align = (cpu_usage) ? lcm(64, pixel_size) : 64;
		stride = GRALLOC_ALIGN(stride, stride_align);

		if (size != NULL)
		{
			*size = stride *height;
		}

		if (byte_stride != NULL)
		{
			*byte_stride = stride;
		}

		if (pixel_stride != NULL)
		{
			*pixel_stride = stride / pixel_size;
		}
	}
}

/*
 * Computes the strides and size for an AFBC 8BIT YUV 4:2:0 buffer
 *
 * width                Public known width of the buffer in pixels
 * height               Public known height of the buffer in pixels
 *
 * pixel_stride   (out) stride of the buffer in pixels
 * byte_stride    (out) stride of the buffer in bytes
 * size           (out) size of the buffer in bytes
 * type                 if buffer should be allocated for a certain afbc type
 */
static bool get_afbc_yuv420_8bit_stride_and_size(int width, int height, int *pixel_stride, int *byte_stride,
                                                 size_t *size, alloc_type_t type)
{
	int yuv420_afbc_luma_stride, yuv420_afbc_chroma_stride;
	int nblocks;

	if (type.primary_type == UNCOMPRESSED)
	{
		MALI_GRALLOC_LOGE(" Buffer must be allocated with AFBC mode for internal pixel format YUV420_8BIT_AFBC!");
		return false;
	}
	else if (type.primary_type == AFBC_PADDED)
	{
		MALI_GRALLOC_LOGE("MALI_GRALLOC_USAGE_AFBC_PADDING (64byte header row alignment for AFBC) is not supported for YUV");
		return false;
	}

	yuv420_afbc_luma_stride = width;
	yuv420_afbc_chroma_stride = GRALLOC_ALIGN(yuv420_afbc_luma_stride / 2, 16); /* Horizontal downsampling*/

	if (size != NULL)
	{
		int nblocks = width / AFBC_PIXELS_PER_BLOCK * height / AFBC_PIXELS_PER_BLOCK;
		int header_size = nblocks * AFBC_HEADER_BUFFER_BYTES_PER_BLOCKENTRY;
		afbc_buffer_align(type, &header_size);
		/* Simplification of (height * luma-stride + 2 * (height /2 * chroma_stride) */
		*size = (yuv420_afbc_luma_stride + yuv420_afbc_chroma_stride) * height + header_size;
	}

	if (byte_stride != NULL)
	{
		*byte_stride = yuv420_afbc_luma_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = yuv420_afbc_luma_stride;
	}

	return true;
}

/*
 * Computes the strides and size for an YV12 buffer
 *
 * width                  Public known width of the buffer in pixels
 * height                 Public known height of the buffer in pixels
 *
 * pixel_stride     (out) stride of the buffer in pixels
 * byte_stride      (out) stride of the buffer in bytes
 * size             (out) size of the buffer in bytes
 * type             (in)  if buffer should be allocated for a certain afbc type
 * stride_alignment (in)  stride aligment value in bytes.
 */
static bool get_yv12_stride_and_size(int width, int height, int *pixel_stride, int *byte_stride, size_t *size,
                                     alloc_type_t type, int stride_alignment)
{
	int luma_stride;

	if (type.primary_type != UNCOMPRESSED)
	{
		return get_afbc_yuv420_8bit_stride_and_size(width, height, pixel_stride, byte_stride, size, type);
	}

	/* 4:2:0 formats must have buffers with even height and width as the clump size is 2x2 pixels.
	 * Width will be even stride aligned anyway so just adjust height here for size calculation. */
	height = GRALLOC_ALIGN(height, 2);

	luma_stride = GRALLOC_ALIGN(width, stride_alignment);

	if (size != NULL)
	{
		int chroma_stride = GRALLOC_ALIGN(luma_stride / 2, stride_alignment);
		/* Simplification of ((height * luma_stride ) + 2 * ((height / 2) * chroma_stride)). */
		*size = height *(luma_stride + chroma_stride);
	}

	if (byte_stride != NULL)
	{
		*byte_stride = luma_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = luma_stride;
	}

	return true;
}

/*
 * Computes the strides and size for an 8 bit YUYV 422 buffer
 *
 * width                  Public known width of the buffer in pixels
 * height                 Public known height of the buffer in pixels
 *
 * pixel_stride     (out) stride of the buffer in pixels
 * byte_stride      (out) stride of the buffer in bytes
 * size             (out) size of the buffer in bytes
 */
static bool get_yuv422_8bit_stride_and_size(int width, int height, int *pixel_stride, int *byte_stride, size_t *size)
{
	int local_byte_stride, local_pixel_stride;

	/* 4:2:2 formats must have buffers with even width as the clump size is 2x1 pixels.
	 * This is taken care of by the even stride alignment. */

	local_pixel_stride = GRALLOC_ALIGN(width, YUV_MALI_PLANE_ALIGN);
	local_byte_stride = GRALLOC_ALIGN(width * 2, YUV_MALI_PLANE_ALIGN); /* 4 bytes per 2 pixels */

	if (size != NULL)
	{
		*size = local_byte_stride *height;
	}

	if (byte_stride != NULL)
	{
		*byte_stride = local_byte_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = local_pixel_stride;
	}

	return true;
}

/*
 * Computes the strides and size for an AFBC 8BIT YUV 4:2:2 buffer
 *
 * width               width of the buffer in pixels
 * height              height of the buffer in pixels
 *
 * pixel_stride (out)  stride of the buffer in pixels
 * byte_stride  (out)  stride of the buffer in bytes
 * size         (out)  size of the buffer in bytes
 * type                if buffer should be allocated for a certain afbc type
 */
static bool get_afbc_yuv422_8bit_stride_and_size(int width, int height, int *pixel_stride, int *byte_stride,
                                                 size_t *size, alloc_type_t type)
{
	int yuv422_afbc_luma_stride;
	int nblocks;

	if (type.primary_type == UNCOMPRESSED)
	{
		MALI_GRALLOC_LOGE(" Buffer must be allocated with AFBC mode for internal pixel format YUV422_8BIT_AFBC!");
		return false;
	}
	else if (type.primary_type == AFBC_PADDED)
	{
		MALI_GRALLOC_LOGE("MALI_GRALLOC_USAGE_AFBC_PADDING (64byte header row alignment for AFBC) is not supported for YUV");
		return false;
	}

	yuv422_afbc_luma_stride = width;

	if (size != NULL)
	{
		int nblocks = width / AFBC_PIXELS_PER_BLOCK * height / AFBC_PIXELS_PER_BLOCK;
		int header_size = nblocks * AFBC_HEADER_BUFFER_BYTES_PER_BLOCKENTRY;
		afbc_buffer_align(type, &header_size);
		/* YUV 4:2:2 luma size equals chroma size */
		*size = yuv422_afbc_luma_stride * height * 2 + header_size;
	}

	if (byte_stride != NULL)
	{
		*byte_stride = yuv422_afbc_luma_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = yuv422_afbc_luma_stride;
	}

	return true;
}

/*
 * Calculate strides and sizes for a P010 (Y-UV 4:2:0) or P210 (Y-UV 4:2:2) buffer.
 *
 * @param width         [in]    Buffer width.
 * @param height        [in]    Buffer height.
 * @param vss           [in]    Vertical sub-sampling factor (2 for P010, 1 for
 *                              P210. Anything else is invalid).
 * @param pixel_stride  [out]   Pixel stride; number of pixels between
 *                              consecutive rows.
 * @param byte_stride   [out]   Byte stride; number of bytes between
 *                              consecutive rows.
 * @param size          [out]   Size of the buffer in bytes. Cumulative sum of
 *                              sizes of all planes.
 *
 * @return true if the calculation was successful; false otherwise (invalid
 * parameter)
 */
static bool get_yuv_pX10_stride_and_size(int width, int height, int vss, int *pixel_stride, int *byte_stride,
                                         size_t *size)
{
	int luma_pixel_stride, luma_byte_stride;

	if (vss < 1 || vss > 2)
	{
		MALI_GRALLOC_LOGE("Invalid vertical sub-sampling factor: %d, should be 1 or 2", vss);
		return false;
	}

	/* 4:2:2 must have even width as the clump size is 2x1 pixels. This will be taken care of by the
	 * even stride alignment */
	if (vss == 2)
	{
		/* 4:2:0 must also have even height as the clump size is 2x2 */
		height = GRALLOC_ALIGN(height, 2);
	}

	luma_pixel_stride = GRALLOC_ALIGN(width, YUV_MALI_PLANE_ALIGN);
	luma_byte_stride = GRALLOC_ALIGN(width * 2, YUV_MALI_PLANE_ALIGN);

	if (size != NULL)
	{
		int chroma_size = GRALLOC_ALIGN(width * 2, YUV_MALI_PLANE_ALIGN) * (height / vss);
		*size = luma_byte_stride *height + chroma_size;
	}

	if (byte_stride != NULL)
	{
		*byte_stride = luma_byte_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = luma_pixel_stride;
	}

	return true;
}

/*
 *  Calculate strides and strides for Y210 (10 bit YUYV packed, 4:2:2) format buffer.
 *
 * @param width         [in]    Buffer width.
 * @param height        [in]    Buffer height.
 * @param pixel_stride  [out]   Pixel stride; number of pixels between
 *                              consecutive rows.
 * @param byte_stride   [out]   Byte stride; number of bytes between
 *                              consecutive rows.
 * @param size          [out]   Size of the buffer in bytes. Cumulative sum of
 *                              sizes of all planes.
 *
 * @return true if the calculation was successful; false otherwise (invalid
 * parameter)
 */
static bool get_yuv_y210_stride_and_size(int width, int height, int *pixel_stride, int *byte_stride, size_t *size)
{
	int y210_byte_stride, y210_pixel_stride;

	/* 4:2:2 formats must have buffers with even width as the clump size is 2x1 pixels.
	 * This is taken care of by the even stride alignment */

	y210_pixel_stride = GRALLOC_ALIGN(width, YUV_MALI_PLANE_ALIGN);
	/* 4x16 bits per 2 pixels */
	y210_byte_stride = GRALLOC_ALIGN(width * 4, YUV_MALI_PLANE_ALIGN);

	if (size != NULL)
	{
		*size = y210_byte_stride *height;
	}

	if (byte_stride != NULL)
	{
		*byte_stride = y210_byte_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = y210_pixel_stride;
	}

	return true;
}

/*
 *  Calculate strides and strides for Y0L2 (YUYAAYVYAA, 4:2:0) format buffer.
 *
 * @param width         [in]    Buffer width.
 * @param height        [in]    Buffer height.
 * @param pixel_stride  [out]   Pixel stride; number of pixels between
 *                              consecutive rows.
 * @param byte_stride   [out]   Byte stride; number of bytes between
 *                              consecutive rows.
 * @param size          [out]   Size of the buffer in bytes. Cumulative sum of
 *                              sizes of all planes.
 *
 * @return true if the calculation was successful; false otherwise (invalid
 * parameter)
 *
 * @note Each YUYAAYVYAA clump encodes a 2x2 area of pixels. YU&V are 10 bits. A is 1 bit. total 8 bytes
 *
 */
static bool get_yuv_y0l2_stride_and_size(int width, int height, int *pixel_stride, int *byte_stride, size_t *size)
{
	int y0l2_byte_stride, y0l2_pixel_stride;

	/* 4:2:0 formats must have buffers with even height and width
	 * as the clump size is 2x2 pixels.
	 * Width is take care of by the even stride alignment so just
	 * adjust height here for size calculation. */
	height = GRALLOC_ALIGN(height, 2);

	y0l2_pixel_stride = GRALLOC_ALIGN(width, YUV_MALI_PLANE_ALIGN);
	y0l2_byte_stride = GRALLOC_ALIGN(width * 4, YUV_MALI_PLANE_ALIGN); /* 2 horiz pixels per 8 byte clump */

	if (size != NULL)
	{
		*size = y0l2_byte_stride * height / 2; /* byte stride covers 2 vert pixels */
	}

	if (byte_stride != NULL)
	{
		*byte_stride = y0l2_byte_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = y0l2_pixel_stride;
	}

	return true;
}

/*
 *  Calculate strides and strides for Y410 (AVYU packed, 4:4:4) format buffer.
 *
 * @param width         [in]    Buffer width.
 * @param height        [in]    Buffer height.
 * @param pixel_stride  [out]   Pixel stride; number of pixels between
 *                              consecutive rows.
 * @param byte_stride   [out]   Byte stride; number of bytes between
 *                              consecutive rows.
 * @param size          [out]   Size of the buffer in bytes. Cumulative sum of
 *                              sizes of all planes.
 *
 * @return true if the calculation was successful; false otherwise (invalid
 * parameter)
 */
static bool get_yuv_y410_stride_and_size(int width, int height, int *pixel_stride, int *byte_stride, size_t *size)
{
	int y410_byte_stride, y410_pixel_stride;

	y410_pixel_stride = GRALLOC_ALIGN(width, YUV_MALI_PLANE_ALIGN);
	y410_byte_stride = GRALLOC_ALIGN(width * 4, YUV_MALI_PLANE_ALIGN);

	if (size != NULL)
	{
		/* 4x8bits per pixel */
		*size = y410_byte_stride *height;
	}

	if (byte_stride != NULL)
	{
		*byte_stride = y410_byte_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = y410_pixel_stride;
	}

	return true;
}

/*
 *  Calculate strides and strides for YUV420_10BIT_AFBC (Compressed, 4:2:0) format buffer.
 *
 * @param width         [in]    Buffer width.
 * @param height        [in]    Buffer height.
 * @param pixel_stride  [out]   Pixel stride; number of pixels between
 *                              consecutive rows.
 * @param byte_stride   [out]   Byte stride; number of bytes between
 *                              consecutive rows.
 * @param size          [out]   Size of the buffer in bytes. Cumulative sum of
 *                              sizes of all planes.
 * @param type          [in]    afbc mode that buffer should be allocated with.
 *
 * @return true if the calculation was successful; false otherwise (invalid
 * parameter)
 */
static bool get_yuv420_10bit_afbc_stride_and_size(int width, int height, int *pixel_stride, int *byte_stride,
                                                  size_t *size, alloc_type_t type)
{
	int yuv420_afbc_byte_stride, yuv420_afbc_pixel_stride;
	int nblocks;

	if (width & 3)
	{
		return false;
	}

	if (type.primary_type == UNCOMPRESSED)
	{
		MALI_GRALLOC_LOGE(" Buffer must be allocated with AFBC mode for internal pixel format YUV420_10BIT_AFBC!");
		return false;
	}
	else if (type.primary_type == AFBC_PADDED)
	{
		MALI_GRALLOC_LOGE("MALI_GRALLOC_USAGE_AFBC_PADDING (64byte header row alignment for AFBC) is not supported for YUV");
		return false;
	}

	yuv420_afbc_pixel_stride = GRALLOC_ALIGN(width, 16);
	yuv420_afbc_byte_stride = GRALLOC_ALIGN(width * 4, 16); /* 64-bit packed and horizontally downsampled */

	if (size != NULL)
	{
		int nblocks = width / AFBC_PIXELS_PER_BLOCK * height / AFBC_PIXELS_PER_BLOCK;
		int header_size = nblocks * AFBC_HEADER_BUFFER_BYTES_PER_BLOCKENTRY;
		afbc_buffer_align(type, &header_size);

		int w_aligned, h_aligned;
		get_afbc_alignment(width, height/2, type, &w_aligned, &h_aligned);

		*size = yuv420_afbc_byte_stride * h_aligned + header_size;
	}

	if (byte_stride != NULL)
	{
		*byte_stride = yuv420_afbc_byte_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = yuv420_afbc_pixel_stride;
	}

	return true;
}

/*
 *  Calculate strides and strides for YUV422_10BIT_AFBC (Compressed, 4:2:2) format buffer.
 *
 * @param width         [in]    Buffer width.
 * @param height        [in]    Buffer height.
 * @param pixel_stride  [out]   Pixel stride; number of pixels between
 *                              consecutive rows.
 * @param byte_stride   [out]   Byte stride; number of bytes between
 *                              consecutive rows.
 * @param size          [out]   Size of the buffer in bytes. Cumulative sum of
 *                              sizes of all planes.
 * @param type          [in]    afbc mode that buffer should be allocated with.
 *
 * @return true if the calculation was successful; false otherwise (invalid
 * parameter)
 */
static bool get_yuv422_10bit_afbc_stride_and_size(int width, int height, int *pixel_stride, int *byte_stride,
                                                  size_t *size, alloc_type_t type)
{
	int yuv422_afbc_byte_stride, yuv422_afbc_pixel_stride;
	int nblocks;

	if (width & 3)
	{
		return false;
	}

	if (type.primary_type == UNCOMPRESSED)
	{
		MALI_GRALLOC_LOGE(" Buffer must be allocated with AFBC mode for internal pixel format YUV422_10BIT_AFBC!");
		return false;
	}
	else if (type.primary_type == AFBC_PADDED)
	{
		MALI_GRALLOC_LOGE("MALI_GRALLOC_USAGE_AFBC_PADDING (64byte header row alignment for AFBC) is not supported for YUV");
		return false;
	}

	yuv422_afbc_pixel_stride = GRALLOC_ALIGN(width, 16);
	yuv422_afbc_byte_stride = GRALLOC_ALIGN(width * 2, 16);

	if (size != NULL)
	{
		int nblocks = width / AFBC_PIXELS_PER_BLOCK * height / AFBC_PIXELS_PER_BLOCK;
		int header_size = nblocks * AFBC_HEADER_BUFFER_BYTES_PER_BLOCKENTRY;
		afbc_buffer_align(type, &header_size);
		/* YUV 4:2:2 chroma size equals to luma size */
		*size = yuv422_afbc_byte_stride * height * 2 + header_size;
	}

	if (byte_stride != NULL)
	{
		*byte_stride = yuv422_afbc_byte_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = yuv422_afbc_pixel_stride;
	}

	return true;
}

/*
 *  Calculate strides and strides for Camera RAW and Blob formats
 *
 * @param w             [in]    Buffer width.
 * @param h             [in]    Buffer height.
 * @param format        [in]    Requested HAL format
 * @param out_stride    [out]   Pixel stride; number of pixels/bytes between
 *                              consecutive rows. Format description calls for
 *                              either bytes or pixels.
 * @param size          [out]   Size of the buffer in bytes. Cumulative sum of
 *                              sizes of all planes.
 *
 * @return true if the calculation was successful; false otherwise (invalid
 * parameter)
 */
static bool get_camera_formats_stride_and_size(int w, int h, uint64_t format, int *out_stride, size_t *out_size)
{
	int stride, size;

	switch (format)
	{
	case HAL_PIXEL_FORMAT_RAW16:
		stride = w; /* Format assumes stride in pixels */
		stride = GRALLOC_ALIGN(stride, 16); /* Alignment mandated by Android */
		size = stride * h * 2; /* 2 bytes per pixel */
		break;

	case HAL_PIXEL_FORMAT_RAW12:
		if (w % 4 != 0)
		{
			MALI_GRALLOC_LOGE("ERROR: Width for HAL_PIXEL_FORMAT_RAW12 buffers has to be multiple of 4.");
			return false;
		}

		stride = (w / 2) * 3; /* Stride in bytes; 2 pixels in 3 bytes */
		size = stride * h;
		break;

	case HAL_PIXEL_FORMAT_RAW10:
		if (w % 4 != 0)
		{
			MALI_GRALLOC_LOGE("ERROR: Width for HAL_PIXEL_FORMAT_RAW10 buffers has to be multiple of 4.");
			return false;
		}

		stride = (w / 4) * 5; /* Stride in bytes; 4 pixels in 5 bytes */
		size = stride * h;
		break;

	case HAL_PIXEL_FORMAT_BLOB:
		if (h != 1)
		{
			MALI_GRALLOC_LOGE("ERROR: Height for HAL_PIXEL_FORMAT_BLOB must be 1.");
			return false;
		}

		stride = 0; /* No 'rows', it's effectively a long one dimensional array */
		size = w;
		break;

	default:
		return false;
	}

	if (out_size != NULL)
	{
		*out_size = size;
	}

	if (out_stride != NULL)
	{
		*out_stride = stride;
	}

	return true;
}

int get_alloc_size(uint64_t internal_format,
                   uint64_t usage,
                   alloc_type_t alloc_type,
                   int old_alloc_width,
                   int old_alloc_height,
                   int * old_byte_stride,
                   int * pixel_stride,
                   size_t * size)
{
	uint64_t base_format = internal_format & MALI_GRALLOC_INTFMT_FMT_MASK;

	switch (base_format)
	{
	case HAL_PIXEL_FORMAT_RGBA_8888:
	case HAL_PIXEL_FORMAT_RGBX_8888:
	case HAL_PIXEL_FORMAT_BGRA_8888:
	case HAL_PIXEL_FORMAT_RGBA_1010102:
		get_rgb_stride_and_size(old_alloc_width, old_alloc_height, 4,
		                        usage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK),
		                        pixel_stride, old_byte_stride, size, alloc_type);
		break;

	case HAL_PIXEL_FORMAT_RGB_888:
		get_rgb_stride_and_size(old_alloc_width, old_alloc_height, 3,
		                        usage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK),
		                        pixel_stride, old_byte_stride, size, alloc_type);
		break;

	case HAL_PIXEL_FORMAT_RGB_565:
		get_rgb_stride_and_size(old_alloc_width, old_alloc_height, 2,
		                        usage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK),
		                        pixel_stride, old_byte_stride, size, alloc_type);
		break;
	case HAL_PIXEL_FORMAT_RGBA_FP16:
		get_rgb_stride_and_size(old_alloc_width, old_alloc_height, 8,
		                        usage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK),
		                        pixel_stride, old_byte_stride, size, alloc_type);
		break;

	case MALI_GRALLOC_FORMAT_INTERNAL_YV12:
	case MALI_GRALLOC_FORMAT_INTERNAL_NV12:
	case MALI_GRALLOC_FORMAT_INTERNAL_NV21:
	case HAL_PIXEL_FORMAT_YCrCb_420_SP:
	{
		/* Mali subsystem prefers higher stride alignment values (128 bytes) for YUV, but software components assume
		 * default of 16. We only need to care about YV12 as it's the only, implicit, HAL YUV format in Android.
		 */
		int yv12_align = YUV_MALI_PLANE_ALIGN;

		if (usage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK))
		{
			yv12_align = YUV_ANDROID_PLANE_ALIGN;
		}

		if (!get_yv12_stride_and_size(old_alloc_width, old_alloc_height, pixel_stride,
		                              old_byte_stride, size, alloc_type, yv12_align))
		{
			return -EINVAL;
		}

		break;
	}

	case MALI_GRALLOC_FORMAT_INTERNAL_YUV422_8BIT:
	{
		/* YUYV 4:2:2 */
		if (alloc_type.primary_type == UNCOMPRESSED)
		{
			if (!get_yuv422_8bit_stride_and_size(old_alloc_width, old_alloc_height,
			                                     pixel_stride, old_byte_stride, size))
			{
				return -EINVAL;
			}
		}
		else
		{
			/* We only support compressed for this format right now.
			 * Below will fail in case format is uncompressed.
			 */
			if (!get_afbc_yuv422_8bit_stride_and_size(old_alloc_width, old_alloc_height,
			                                          pixel_stride, old_byte_stride,
			                                          size, alloc_type))
			{
				return -EINVAL;
			}
		}
		break;
	}

	case HAL_PIXEL_FORMAT_RAW16:
	case HAL_PIXEL_FORMAT_RAW12:
	case HAL_PIXEL_FORMAT_RAW10:
	case HAL_PIXEL_FORMAT_BLOB:
		if (alloc_type.primary_type != UNCOMPRESSED)
		{
			return -EINVAL;
		}

		get_camera_formats_stride_and_size(old_alloc_width, old_alloc_height, base_format,
		                                   pixel_stride, size);
		/* For Raw/Blob formats stride is defined to be either in bytes or pixels per format */
		*old_byte_stride = *pixel_stride;
		break;

	case MALI_GRALLOC_FORMAT_INTERNAL_Y0L2:

		/* YUYAAYUVAA 4:2:0 with and without AFBC */
		if (alloc_type.primary_type != UNCOMPRESSED)
		{
			if (!get_yuv420_10bit_afbc_stride_and_size(
			        old_alloc_width, old_alloc_height, pixel_stride,
			        old_byte_stride, size, alloc_type))
			{
				return -EINVAL;
			}
		}
		else
		{
			if (!get_yuv_y0l2_stride_and_size(old_alloc_width, old_alloc_height,
			                                  pixel_stride, old_byte_stride, size))
			{
				return -EINVAL;
			}
		}

		break;

	case MALI_GRALLOC_FORMAT_INTERNAL_P010:

		/* Y-UV 4:2:0 */
		if (alloc_type.primary_type != UNCOMPRESSED ||
		    !get_yuv_pX10_stride_and_size(old_alloc_width, old_alloc_height, 2,
		                                  pixel_stride, old_byte_stride, size))
		{
			return -EINVAL;
		}

		break;

	case MALI_GRALLOC_FORMAT_INTERNAL_P210:

		/* Y-UV 4:2:2 */
		if (alloc_type.primary_type != UNCOMPRESSED ||
		    !get_yuv_pX10_stride_and_size(old_alloc_width, old_alloc_height, 1,
		                                  pixel_stride, old_byte_stride, size))
		{
			return -EINVAL;
		}

		break;

	case MALI_GRALLOC_FORMAT_INTERNAL_Y210:

		/* YUYV 4:2:2 with and without AFBC */
		if (alloc_type.primary_type != UNCOMPRESSED)
		{
			if (!get_yuv422_10bit_afbc_stride_and_size(old_alloc_width, old_alloc_height,
			                                           pixel_stride, old_byte_stride,
			                                           size, alloc_type))
			{
				return -EINVAL;
			}
		}
		else
		{
			if (!get_yuv_y210_stride_and_size(old_alloc_width, old_alloc_height,
			                                  pixel_stride, old_byte_stride, size))
			{
				return -EINVAL;
			}
		}

		break;

	case MALI_GRALLOC_FORMAT_INTERNAL_Y410:

		/* AVYU 2-10-10-10 */
		if (alloc_type.primary_type != UNCOMPRESSED ||
		    !get_yuv_y410_stride_and_size(old_alloc_width, old_alloc_height, pixel_stride,
		                                  old_byte_stride, size))
		{
			return -EINVAL;
		}

		break;

	/*
	 * Additional custom formats can be added here
	 * and must fill the variables pixel_stride, byte_stride and size.
	 */
	default:
		return -EINVAL;
	}


	return 0;
}


}


