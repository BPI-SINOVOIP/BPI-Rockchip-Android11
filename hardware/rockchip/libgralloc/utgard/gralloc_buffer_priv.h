/*
 * Copyright (C) 2014 ARM Limited. All rights reserved.
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

#ifndef GRALLOC_BUFFER_PRIV_H_
#define GRALLOC_BUFFER_PRIV_H_

//#include "gralloc_priv.h"
#include <errno.h>
#include <sys/user.h>
#include <string.h>
#include <sys/mman.h>
#include "gralloc_drm_handle.h"
#ifdef __cplusplus
extern "C" {
#endif
#if 0
// private gralloc buffer manipulation API

/**
 * .DP : gralloc_buffer_attr_buffer_t
 */
struct attr_region
{
	/* Rectangle to be cropped from the full frame (Origin in top-left corner!) */
	int crop_top;
	int crop_left;
	int crop_height;
	int crop_width;
	int use_yuv_transform;
	int use_sparse_alloc;
} __attribute__ ((packed));

typedef struct attr_region attr_region;

enum
{
	 /* CROP_RECT and YUV_TRANS are intended to be
	 * written by producers and read by consumers.
	 * A producer should write these parameters before
	 * it queues a buffer to the consumer.
	 */

	/* CROP RECT, defined as an int array of top, left, height, width. Origin in top-left corner */
	GRALLOC_ARM_BUFFER_ATTR_CROP_RECT                  = 1,

	/* Set if the AFBC format used a YUV transform before compressing */
	GRALLOC_ARM_BUFFER_ATTR_AFBC_YUV_TRANS             = 2,

	/* Set if the AFBC format uses sparse allocation */
	GRALLOC_ARM_BUFFER_ATTR_AFBC_SPARSE_ALLOC          = 3,
	GRALLOC_ARM_BUFFER_ATTR_LAST
};

typedef uint32_t buf_attr;

/*
 * Allocate shared memory for attribute storage. Only to be
 * used by gralloc internally.
 *
 * Return 0 on success.
 */
int gralloc_buffer_attr_allocate( struct gralloc_drm_handle_t *hnd );

/*
 * Frees the shared memory allocated for attribute storage.
 * Only to be used by gralloc internally.

 * Return 0 on success.
 */
int gralloc_buffer_attr_free( struct gralloc_drm_handle_t *hnd );
#endif

#ifdef USE_HWC2
/*
 * Allocate shared memory for rk ashmem. Only to be
 * used by gralloc internally.
 *
 * Return 0 on success.
 */
int gralloc_rk_ashmem_allocate( struct gralloc_drm_handle_t *hnd );

/*
 * Frees the shared memory allocated for rk ashmem.
 * Only to be used by gralloc internally.

 * Return 0 on success.
 */
int gralloc_rk_ashmem_free( struct gralloc_drm_handle_t *hnd );

/*
 * Map the rk_ashmem area before attempting to
 * read/write from it.
 *
 * Return 0 on success.
 */
static inline int gralloc_rk_ashmem_map( struct gralloc_drm_handle_t *hnd, int readwrite)
{
	int rval = -1;
	int prot_flags = PROT_READ;

	if( !hnd )
		goto out;

	if( hnd->ashmem_fd < 0 )
	{
		ALOGE("Shared attribute region not available to be mapped");
		goto out;
	}

	if( readwrite )
	{
		prot_flags |=  PROT_WRITE;
	}

	hnd->ashmem_base = mmap( NULL, PAGE_SIZE, prot_flags, MAP_SHARED, hnd->ashmem_fd, 0 );
	if(hnd->ashmem_base == MAP_FAILED)
	{
		ALOGE("Failed to mmap shared attribute region err=%s",strerror(errno));
		goto out;
	}

	rval = 0;

out:
	return rval;
}

/*
 * Unmap the rk_ashmem area when done with it.
 *
 * Return 0 on success.
 */
static inline int gralloc_rk_ashmem_unmap( struct gralloc_drm_handle_t *hnd )
{
	int rval = -1;

	if( !hnd )
	{
		ALOGE("%s:handle is null",__FUNCTION__);
		goto out;
	}

	if( hnd->ashmem_base != MAP_FAILED )
	{
		if ( munmap( hnd->ashmem_base, PAGE_SIZE ) == 0 )
		{
			hnd->ashmem_base = MAP_FAILED;
			rval = 0;
		}
	}

out:
	return rval;
}

/*
#define LayerNameLength		100
typedef struct rk_ashmem_t
{
    int32_t alreadyStereo;
    int32_t displayStereo;
    char LayerName[LayerNameLength + 1];
} rk_ashmem_t;
*/
/*
 * Read or write rk_ashmem from/to the storage area.
 *
 * Return 0 on success.
 */
static inline int gralloc_rk_ashmem_write( struct gralloc_drm_handle_t *hnd, struct rk_ashmem_t *val )
{
	int rval = -1;

	if( !hnd || !val)
	{
		ALOGE("%s:parameters is null",__FUNCTION__);
		goto out;
	}

	if( hnd->ashmem_base != MAP_FAILED )
	{
		memcpy(hnd->ashmem_base, val, sizeof(struct rk_ashmem_t));
//		ALOGD("gralloc_rk_ashmem_write LayerName=%s,alreadyStereo=%d,displayStereo=%d",val->LayerName,val->alreadyStereo,val->displayStereo);
		rval = 0;
	}

out:
	return rval;
}

static inline int gralloc_rk_ashmem_read( struct gralloc_drm_handle_t *hnd, struct rk_ashmem_t *val )
{
	int rval = -1;

	if( !hnd || !val )
	{
		ALOGE("%s:parameters is null",__FUNCTION__);
		goto out;
	}

	if( hnd->ashmem_base != MAP_FAILED )
	{
		memcpy(val, hnd->ashmem_base, sizeof(struct rk_ashmem_t));
		//ALOGD("gralloc_rk_ashmem_read LayerName=%s,alreadyStereo=%d,displayStereo=%d",val->LayerName,val->alreadyStereo,val->displayStereo);
		rval = 0;
	}

out:
	return rval;
}
#endif

#if 0
/*
 * Map the attribute storage area before attempting to
 * read/write from it.
 *
 * Return 0 on success.
 */
static inline int gralloc_buffer_attr_map( struct gralloc_drm_handle_t *hnd, int readwrite)
{
	int rval = -1;
	int prot_flags = PROT_READ;

	if( !hnd )
		goto out;

	if( hnd->share_attr_fd < 0 )
	{
		ALOGE("Shared attribute region not available to be mapped");
		goto out;
	}

	if( readwrite )
	{
		prot_flags |=  PROT_WRITE;
	}

	hnd->attr_base = mmap( NULL, PAGE_SIZE, prot_flags, MAP_SHARED, hnd->share_attr_fd, 0 );
	if(hnd->attr_base == MAP_FAILED)
	{
		ALOGE("Failed to mmap shared attribute region err=%s",strerror(errno));
		goto out;
	}

	rval = 0;

out:
	return rval;
}

/*
 * Unmap the attribute storage area when done with it.
 *
 * Return 0 on success.
 */
static inline int gralloc_buffer_attr_unmap( struct gralloc_drm_handle_t *hnd )
{
	int rval = -1;

	if( !hnd )
		goto out;

	if( hnd->attr_base != MAP_FAILED )
	{
		if ( munmap( hnd->attr_base, PAGE_SIZE ) == 0 )
		{
			hnd->attr_base = MAP_FAILED;
			rval = 0;
		}
	}

out:
	return rval;
}

/*
 * Read or write an attribute from/to the storage area.
 *
 * Return 0 on success.
 */
static inline int gralloc_buffer_attr_write( struct gralloc_drm_handle_t *hnd, buf_attr attr, int *val )
{
	int rval = -1;

	if( !hnd || !val || attr >= GRALLOC_ARM_BUFFER_ATTR_LAST )
		goto out;

	if( hnd->attr_base != MAP_FAILED )
	{
		attr_region *region = (attr_region *) hnd->attr_base;

		switch( attr )
		{
			case GRALLOC_ARM_BUFFER_ATTR_CROP_RECT:
				memcpy( &region->crop_top, val, sizeof(int)*4 );
				rval = 0;
				break;

			case GRALLOC_ARM_BUFFER_ATTR_AFBC_YUV_TRANS:
				region->use_yuv_transform = *val;
				rval = 0;
				break;

			case GRALLOC_ARM_BUFFER_ATTR_AFBC_SPARSE_ALLOC:
				region->use_sparse_alloc = *val;
				rval = 0;
				break;
		}
	}

out:
	return rval;
}

static inline int gralloc_buffer_attr_read( struct gralloc_drm_handle_t *hnd, buf_attr attr, int *val )
{
	int rval = -1;

	if( !hnd || !val || attr >= GRALLOC_ARM_BUFFER_ATTR_LAST )
		goto out;

	if( hnd->attr_base != MAP_FAILED )
	{
		attr_region *region = (attr_region *) hnd->attr_base;

		switch( attr )
		{
			case GRALLOC_ARM_BUFFER_ATTR_CROP_RECT:
				memcpy( val, &region->crop_top, sizeof(int)*4 );
				rval = 0;
				break;

			case GRALLOC_ARM_BUFFER_ATTR_AFBC_YUV_TRANS:
				*val = region->use_yuv_transform;
				rval = 0;
				break;

			case GRALLOC_ARM_BUFFER_ATTR_AFBC_SPARSE_ALLOC:
				*val = region->use_sparse_alloc;
				rval = 0;
				break;
		}
	}

out:
	return rval;
}
#endif
#ifdef __cplusplus
}
#endif
#endif /* GRALLOC_BUFFER_PRIV_H_ */
