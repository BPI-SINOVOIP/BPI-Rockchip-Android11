/*
 *
 * Copyright 2015 Rockchip Electronics Co. LTD
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

/*
 * @file        gralloc_pirv_omx.cpp
 * @brief       get gpu private handle for omx use
 * @author      csy(csy@rock-chips.com)
 * @version     1.0.0
 * @history
 *   2015.1.16 : Create
 */

#include <cutils/log.h>
#include <hardware/gralloc.h>
#include "gralloc_priv_omx.h"
#if defined (GPU_G6110)
#include <hardware/img_gralloc_public.h>
#define private_handle_t tagIMG_native_handle_t
#elif defined (USE_DRM)
#include <gralloc_drm_handle.h>
#define private_handle_t gralloc_drm_handle_t
#elif defined (USE_GRALLOC_4)
#include "platform_gralloc4.h"
#include "src/mali_gralloc_formats.h"
#include "custom_log.h"
#define private_handle_t buffer_handle_t
#else
#include <gralloc_priv.h>
#endif

int32_t Rockchip_get_gralloc_private(uint32_t *handle,gralloc_private_handle_t *private_hnd){
    if(private_hnd == NULL){
        return -1;
    }
#ifdef USE_GRALLOC_4
    private_handle_t priv_hnd = (private_handle_t)handle;
    int format_requested, share_fd, pixel_stride,err;
    uint64_t allocation_size;
    err = gralloc4::get_format_requested(priv_hnd, &format_requested);
    if (err != android::OK )
    {
        E("get_format_requested err : %d", err);
        //return err;
    }else{
        private_hnd->format = format_requested;
    }
    err = gralloc4::get_share_fd(priv_hnd, &share_fd);
    if (err != android::OK )
    {
        E("get_share_fd err : %d", err);
        //return err;
    }else{
        private_hnd->share_fd = share_fd;
    }
    err = gralloc4::get_pixel_stride(priv_hnd, &pixel_stride);
    if (err != android::OK )
    {
        E("get_pixel_stride err : %d", err);
        //return err;
    }else{
        private_hnd->stride = pixel_stride;
    }
    err = gralloc4::get_allocation_size(priv_hnd, &allocation_size);
    if (err != android::OK )
    {
        E("get_allocation_size err : %d", err);
        //return err;
    }else{
        private_hnd->size = (int)allocation_size;
    }

#else
    private_handle_t *priv_hnd = (private_handle_t *)handle;
    private_hnd->format = priv_hnd->format;

#ifdef USE_DRM
    private_hnd->share_fd = priv_hnd->prime_fd;
    private_hnd->stride = priv_hnd->pixel_stride;
#else
    #ifdef GPU_G6110
        private_hnd->share_fd = priv_hnd->fd[0];
    #else
        private_hnd->share_fd = priv_hnd->share_fd;
    #endif 
    private_hnd->type = priv_hnd->type;
    private_hnd->stride = priv_hnd->stride;
#endif
    private_hnd->size = priv_hnd->size;
#endif
    return 0;
}

