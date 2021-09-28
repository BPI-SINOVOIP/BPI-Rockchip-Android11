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

#include <cutils/ashmem.h>
#include <log/log.h>
#include <sys/mman.h>
#include <sys/user.h>
#include <unistd.h>
#include "gralloc_drm_handle.h"
#include "gralloc_buffer_priv.h"

#if 0
/*
 * Allocate shared memory for attribute storage. Only to be
 * used by gralloc internally.
 *
 * Return 0 on success.
 */
int gralloc_buffer_attr_allocate( struct gralloc_drm_handle_t *hnd )
{
	int rval = -1;

	if( !hnd )
		goto out;

	if( hnd->share_attr_fd >= 0 )
	{
		ALOGW("Warning share attribute fd already exists during create. Closing.");
		close( hnd->share_attr_fd );
	}

	hnd->share_attr_fd = ashmem_create_region( "gralloc_shared_attr",
                                              PAGE_SIZE); // 直接分配一个 page, page 是 mmap 操作的最小单位.
        // ashmem_create_region 定义在 system/core/libcutils/include/cutils/ashmem.h
	if(hnd->share_attr_fd < 0)
	{
		ALOGE("Failed to allocate page for shared attribute region");
		goto err_ashmem;
	}

	/*
	 * Default protection on the shm region is PROT_EXEC | PROT_READ | PROT_WRITE.
	 *
	 * Personality flag READ_IMPLIES_EXEC which is used by some processes, namely gdbserver,
	 * causes a mmap with PROT_READ to be translated to PROT_READ | PROT_EXEC.
	 *
	 * If we were to drop PROT_EXEC here with a call to ashmem_set_prot_region()
	 * this can potentially cause clients to fail importing this gralloc attribute buffer
	 * with EPERM error since PROT_EXEC is not allowed.
	 *
	 * Because of this we keep the PROT_EXEC flag.
	 */

	hnd->attr_base = mmap( NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, hnd->share_attr_fd, 0 );
	if(hnd->attr_base != MAP_FAILED)
	{
		/* The attribute region contains signed integers only.
		 * The reason for this is because we can set a value less than 0 for
		 * not-initialized values.
		 */
		attr_region *region = (attr_region *) hnd->attr_base;

		memset(hnd->attr_base, 0xff, PAGE_SIZE);
        /* 完成 对 buffer 的必要初始化之后, unmap. */
		munmap( hnd->attr_base, PAGE_SIZE );
		hnd->attr_base = MAP_FAILED;
	}
	else
	{
		ALOGE("Failed to mmap shared attribute region");
		goto err_ashmem;
	}

	rval = 0;
	goto out;

err_ashmem:
	if( hnd->share_attr_fd >= 0 )
	{
		close( hnd->share_attr_fd );
		hnd->share_attr_fd = -1;
	}

out:
	return rval;
}

/*
 * Frees the shared memory allocated for attribute storage.
 * Only to be used by gralloc internally.

 * Return 0 on success.
 */
int gralloc_buffer_attr_free( struct gralloc_drm_handle_t *hnd )
{
	int rval = -1;

	if( !hnd )
		goto out;

	if( hnd->share_attr_fd < 0 )
	{
		ALOGE("Shared attribute region not avail to free");
		goto out;
	}
	if( hnd->attr_base != MAP_FAILED )
	{
		ALOGW("Warning shared attribute region mapped at free. Unmapping");
		munmap( hnd->attr_base, PAGE_SIZE );
		hnd->attr_base = MAP_FAILED;
	}

	close( hnd->share_attr_fd );
	hnd->share_attr_fd = -1;
	rval = 0;

out:
	return rval;
}
#endif

#ifdef USE_HWC2
/*
 * Allocate shared memory for attribute storage. Only to be
 * used by gralloc internally.
 *
 * Return 0 on success.
 */
int gralloc_rk_ashmem_allocate(struct gralloc_drm_handle_t *hnd )
{
	int rval = -1;

	if( !hnd )
	{
		ALOGE("%s: handle is null",__FUNCTION__);
		goto out;
	}

	if( hnd->ashmem_fd >= 0 )
	{
		ALOGW("Warning share attribute fd already exists during create. Closing.");
		close( hnd->ashmem_fd );
	}

	hnd->ashmem_fd = ashmem_create_region( "gralloc_rk_handle_ashmem", PAGE_SIZE );
	if(hnd->ashmem_fd < 0)
	{
		ALOGE("Failed to allocate page for shared attribute region");
		goto err_ashmem;
	}

	/*
	 * Default protection on the shm region is PROT_EXEC | PROT_READ | PROT_WRITE.
	 *
	 * Personality flag READ_IMPLIES_EXEC which is used by some processes, namely gdbserver,
	 * causes a mmap with PROT_READ to be translated to PROT_READ | PROT_EXEC.
	 *
	 * If we were to drop PROT_EXEC here with a call to ashmem_set_prot_region()
	 * this can potentially cause clients to fail importing this gralloc attribute buffer
	 * with EPERM error since PROT_EXEC is not allowed.
	 *
	 * Because of this we keep the PROT_EXEC flag.
	 */

	hnd->ashmem_base = mmap( NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, hnd->ashmem_fd, 0 );
	if(hnd->ashmem_base != MAP_FAILED)
	{
		/* The attribute region contains signed integers only.
		 * The reason for this is because we can set a value less than 0 for
		 * not-initialized values.
		 */
		struct rk_ashmem_t *rk_ashmem = (struct rk_ashmem_t *) hnd->ashmem_base;

		memset(hnd->ashmem_base, 0x0, PAGE_SIZE);

		rk_ashmem->alreadyStereo = 0;
		rk_ashmem->displayStereo = 0;
		strcpy(rk_ashmem->LayerName, "");

		munmap( hnd->ashmem_base, PAGE_SIZE );
		hnd->ashmem_base = MAP_FAILED;
	}
	else
	{
		ALOGE("Failed to mmap shared attribute region");
		goto err_ashmem;
	}

	rval = 0;
	goto out;

err_ashmem:
	if( hnd->ashmem_fd >= 0 )
	{
		close( hnd->ashmem_fd );
		hnd->ashmem_fd = -1;
	}

out:
	return rval;
}



/*
 * Frees the shared memory allocated for attribute storage.
 * Only to be used by gralloc internally.

 * Return 0 on success.
 */
int gralloc_rk_ashmem_free(struct gralloc_drm_handle_t *hnd )
{
	int rval = -1;

	if( !hnd )
	{
		ALOGE("%s: handle is null",__FUNCTION__);
		goto out;
	}

	if( hnd->ashmem_fd < 0 )
	{
		ALOGE("Shared attribute region not avail to free");
		goto out;
	}
	if( hnd->ashmem_base != MAP_FAILED )
	{
		ALOGW("Warning shared attribute region mapped at free. Unmapping");
		munmap( hnd->ashmem_base, PAGE_SIZE );
		hnd->ashmem_base = MAP_FAILED;
	}

	close( hnd->ashmem_fd );
	hnd->ashmem_fd = -1;
	rval = 0;

out:
	return rval;
}
#endif
