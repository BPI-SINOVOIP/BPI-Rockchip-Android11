/*
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co.Ltd.
 *
 * Modification based on code covered by the Apache License, Version 2.0 (the "License").
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS TO YOU ON AN "AS IS" BASIS
 * AND ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
 * NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.
 *
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (C) 2015 The Android Open Source Project
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

#define LOG_TAG "hwc_rk"

#include <inttypes.h>
#ifdef TARGET_BOARD_PLATFORM_RK3368
#include <hardware/img_gralloc_public.h>
#endif
#include "hwc_rockchip.h"
#include "hwc_util.h"

#if USE_GRALLOC_4
#include "drmgralloc4.h"
#endif


namespace android {

gralloc_module_t *gralloc;

int hwc_init_version()
{
    char acVersion[50];
    char acCommit[50];
    memset(acVersion,0,sizeof(acVersion));

    strcpy(acVersion,GHWC_VERSION);

#ifdef TARGET_BOARD_PLATFORM_RK3288
    strcat(acVersion,"-rk3288");
#endif
#ifdef TARGET_BOARD_PLATFORM_RK3368
    strcat(acVersion,"-rk3368");
#endif
#ifdef TARGET_BOARD_PLATFORM_RK3366
    strcat(acVersion,"-rk3366");
#endif
#ifdef TARGET_BOARD_PLATFORM_RK3399
    strcat(acVersion,"-rk3399");
#endif
#ifdef TARGET_BOARD_PLATFORM_RK3326
    strcat(acVersion,"-rk3326");
#endif


#ifdef TARGET_BOARD_PLATFORM_RK3126C
    strcat(acVersion,"-rk3126c");
#endif

#ifdef TARGET_BOARD_PLATFORM_RK3328
    strcat(acVersion,"-rk3328");
#endif

#ifdef RK_MID
    strcat(acVersion,"-MID");
#endif
#ifdef RK_BOX
    strcat(acVersion,"-BOX");
#endif
#ifdef RK_PHONE
    strcat(acVersion,"-PHONE");
#endif
#ifdef RK_VIR
    strcat(acVersion,"-VR");
#endif
    int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                      (const hw_module_t **)&gralloc);
    if (ret) {
        ALOGE("Failed to open gralloc module");
        return ret;
    }

    /* RK_GRAPHICS_VER=commit-id:067e5d0: only keep string after '=' */
//    sscanf(RK_GRAPHICS_VER, "%*[^=]=%s", acCommit);

//    property_set("sys.ghwc.version", acVersion);
//    property_set("sys.ghwc.commit", acCommit);
//    ALOGD(RK_GRAPHICS_VER);
    return 0;
}
int hwc_lock(buffer_handle_t hnd,int usage, int x, int y, int w, int h, void **cpu_addr){
#if USE_GRALLOC_4
  return gralloc4::lock(hnd,usage,x,y,w,h,(void **)cpu_addr);
#else   // USE_GRALLOC_4
  return gralloc->lock(gralloc, hnd, usage, x, y, w, h, (void **)cpu_addr);
#endif

}

int hwc_unlock(buffer_handle_t hnd){
#if USE_GRALLOC_4
  gralloc4::unlock(hnd);
  return 0;
#else   // USE_GRALLOC_4
  return gralloc->unlock(gralloc, hnd);
#endif
}

int hwc_get_handle_width(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    uint64_t width;

    int err = gralloc4::get_width(hnd, &width);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer width, err : %d", err);
        return -1;
    }

    return (int)width;
#else   // USE_GRALLOC_4

    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_WIDTH;
    int width = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &width);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return width;
#endif
}

int hwc_get_handle_height(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    uint64_t height;
    int err = gralloc4::get_height(hnd, &height);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer height, err : %d", err);
        return -1;
    }

    return (int)height;
#else   // USE_GRALLOC_4

    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_HEIGHT;
    int height = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &height);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return height;
#endif
}

int hwc_get_handle_stride(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    int pixel_stride;
    int err = gralloc4::get_pixel_stride(hnd, &pixel_stride);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer pixel_stride, err : %d", err);
        return -1;
    }

    return pixel_stride;
#else   // USE_GRALLOC_4

    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_STRIDE;
    int stride = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &stride);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return stride;
#endif
}

int hwc_get_handle_byte_stride(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    int byte_stride;
    int err = gralloc4::get_byte_stride(hnd, &byte_stride);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer byte_stride, err : %d", err);
        return -1;
    }

    return byte_stride;
#else   // USE_GRALLOC_4

    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_BYTE_STRIDE;
    int byte_stride = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &byte_stride);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return byte_stride;
#endif
}

int hwc_get_handle_format(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    int format_requested;

    int err = gralloc4::get_format_requested(hnd, &format_requested);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer format_requested, err : %d", err);
        return -1;
    }

    return format_requested;
#else   // USE_GRALLOC_4

    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_FORMAT;
    int format = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &format);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return format;
#endif
}

int hwc_get_handle_usage(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    uint64_t usage;
    int err = gralloc4::get_usage(hnd, &usage);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer usage, err : %d", err);
        return -1;
    }

    return (int)usage;
#else   // USE_GRALLOC_4

    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_USAGE;
    int usage = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &usage);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return usage;
#endif
}

int hwc_get_handle_size(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    uint64_t allocation_size;
    int err = gralloc4::get_allocation_size(hnd, &allocation_size);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer allocation_size, err : %d", err);
        return -1;
    }

    return (int)allocation_size;
#else   // USE_GRALLOC_4

    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_SIZE;
    int size = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &size);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return size;
#endif
}

/*
@func hwc_get_handle_attributes:get attributes from handle.Before call this api,As far as now,
    we need register the buffer first.May be the register is good for processer I think

@param hnd:
@param attrs: if size of attrs is small than 5,it will return EINVAL else
    width  = attrs[0]
    height = attrs[1]
    stride = attrs[2]
    format = attrs[3]
    size   = attrs[4]
*/
int hwc_get_handle_attributes(buffer_handle_t hnd, std::vector<int> *attrs)
{
    int ret = 0;
#if USE_GRALLOC_4
#else

    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_ATTRIBUTES;

    if (!hnd)
        return -EINVAL;

    if(gralloc && gralloc->perform)
    {
        ret = gralloc->perform(gralloc, op, hnd, attrs);
    }
    else
    {
        ret = -EINVAL;
    }


    if(ret) {
       ALOGE("hwc_get_handle_attributes fail %d for:%s hnd=%p",ret,strerror(ret),hnd);
    }
#endif
    return ret;
}

int hwc_get_handle_attibute(buffer_handle_t hnd, attribute_flag_t flag)
{

#if USE_GRALLOC_4
        switch ( flag )
        {
            case ATT_WIDTH:
                return hwc_get_handle_width(hnd);
            case ATT_HEIGHT:
                return hwc_get_handle_height(hnd);
            case ATT_STRIDE:
                return hwc_get_handle_stride(hnd);
            case ATT_FORMAT:
                return hwc_get_handle_format(hnd);
            case ATT_SIZE:
                return hwc_get_handle_size(hnd);
            case ATT_BYTE_STRIDE:
                return hwc_get_handle_byte_stride(hnd);
            default:
                LOG_ALWAYS_FATAL("unexpected flag : %d", flag);
                return -1;
        }
#else   // USE_GRALLOC_4

    std::vector<int> attrs;
    int ret=0;

    if(!hnd)
    {
        ALOGE("%s handle is null",__FUNCTION__);
        return -1;
    }

    ret = hwc_get_handle_attributes(hnd, &attrs);
    if(ret < 0)
    {
        ALOGE("getHandleAttributes fail %d for:%s",ret,strerror(ret));
        return ret;
    }
    else
    {
        return attrs.at(flag);
    }
#endif

}

/*
@func getHandlePrimeFd:get prime_fd  from handle.Before call this api,As far as now, we
    need register the buffer first.May be the register is good for processer I think

@param hnd:
@return fd: prime_fd. and driver can call the dma_buf_get to get the buffer

*/
int hwc_get_handle_primefd(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    int share_fd;
    int err = gralloc4::get_share_fd(hnd, &share_fd);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer share_fd, err : %d", err);
        return -1;
    }

    return (int)share_fd;
#else   // USE_GRALLOC_4

    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_PRIME_FD;
    int fd = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &fd);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return fd;
#endif
}

#if RK_DRM_GRALLOC
uint32_t hwc_get_handle_phy_addr(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    uint64_t internal_format = 0;
    internal_format = gralloc4::get_internal_format(hnd);
    return internal_format;
#else // #if USE_GRALLOC_4

    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_PHY_ADDR;
    uint32_t phy_addr = 0;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &phy_addr);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return phy_addr;
#endif
}
#endif

}

