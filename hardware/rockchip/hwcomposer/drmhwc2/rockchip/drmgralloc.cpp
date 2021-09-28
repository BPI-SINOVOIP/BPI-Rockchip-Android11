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

// #define ENABLE_DEBUG_LOG
#define LOG_TAG "drm_hwc2_gralloc"

#if USE_GRALLOC_4
#include "rockchip/drmgralloc4.h"
#endif
#include "rockchip/drmgralloc.h"


#include <log/log.h>
#include <inttypes.h>
#include <errno.h>
namespace android {

DrmGralloc::DrmGralloc(){
#if USE_GRALLOC_4
#else
  int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                         (const hw_module_t **)&gralloc_);
  if(ret)
    ALOGE("hw_get_module fail");
#endif
}

DrmGralloc::~DrmGralloc(){}

void DrmGralloc::set_drm_version(int version){

#if USE_GRALLOC_4
    gralloc4::set_drm_version(version);
    drmVersion_ = version;
#endif
    return;
}
int DrmGralloc::hwc_get_handle_width(buffer_handle_t hnd)
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

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &width);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return width;
#endif
}

int DrmGralloc::hwc_get_handle_height(buffer_handle_t hnd)
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

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &height);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return height;
#endif
}

int DrmGralloc::hwc_get_handle_stride(buffer_handle_t hnd)
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

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &stride);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return stride;
#endif
}

int DrmGralloc::hwc_get_handle_byte_stride_workround(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    int byte_stride;

    int err = gralloc4::get_byte_stride_workround(hnd, &byte_stride);
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

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &byte_stride);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return byte_stride;
#endif
}

int DrmGralloc::hwc_get_handle_byte_stride(buffer_handle_t hnd)
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

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &byte_stride);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return byte_stride;
#endif
}

int DrmGralloc::hwc_get_handle_format(buffer_handle_t hnd)
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

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &format);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return format;
#endif
}

int DrmGralloc::hwc_get_handle_usage(buffer_handle_t hnd)
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

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &usage);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return usage;
#endif
}

int DrmGralloc::hwc_get_handle_size(buffer_handle_t hnd)
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

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &size);
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
int DrmGralloc::hwc_get_handle_attributes(buffer_handle_t hnd, std::vector<int> *attrs)
{
    int ret = 0;
#if USE_GRALLOC_4
#else
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_ATTRIBUTES;

    if (!hnd)
        return -EINVAL;

    if(gralloc_ && gralloc_->perform)
    {
        ret = gralloc_->perform(gralloc_, op, hnd, attrs);
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

int DrmGralloc::hwc_get_handle_attibute(buffer_handle_t hnd, attribute_flag_t flag)
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
        case ATT_BYTE_STRIDE_WORKROUND:
            return hwc_get_handle_byte_stride_workround(hnd);
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
int DrmGralloc::hwc_get_handle_primefd(buffer_handle_t hnd)
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

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &fd);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return fd;
#endif
}

int DrmGralloc::hwc_get_handle_name(buffer_handle_t hnd, std::string &name){
#if USE_GRALLOC_4

    int err = gralloc4::get_name(hnd, name);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer share_fd, err : %d", err);
        return -1;
    }

    return (int)err;
#else   // USE_GRALLOC_4
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_PRIME_FD;
    int fd = -1;

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &fd);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return fd;
#endif

}

int DrmGralloc::hwc_get_handle_buffer_id(buffer_handle_t hnd, uint64_t *buffer_id){
#if USE_GRALLOC_4

    int err = gralloc4::get_buffer_id(hnd, buffer_id);
    if (err != android::OK)
    {
        ALOGE("Failed to get buffer share_fd, err : %d", err);
        return -1;
    }

    return (int)err;
#else   // USE_GRALLOC_4
    return -1;
#endif

}

uint32_t DrmGralloc::hwc_get_handle_phy_addr(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    return 0;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_PHY_ADDR;
    uint32_t phy_addr = 0;

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &phy_addr);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return phy_addr;
#endif
}

uint64_t DrmGralloc::hwc_get_handle_format_modifier(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    uint64_t format_modifier = 0;
    format_modifier = gralloc4::get_format_modifier(hnd);
    return format_modifier;
#else // #if USE_GRALLOC_4
    return 0;
#endif
}


uint32_t DrmGralloc::hwc_get_handle_fourcc_format(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    uint32_t fourcc_format = 0;
    fourcc_format = gralloc4::get_fourcc_format(hnd);
    return fourcc_format;
#else // #if USE_GRALLOC_4
    return 0;
#endif
}


uint64_t DrmGralloc::hwc_get_handle_internal_format(buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    uint64_t internal_format = 0;
    internal_format = gralloc4::get_internal_format(hnd);
    return internal_format;
#else // #if USE_GRALLOC_4
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_INTERNAL_FORMAT;
    uint64_t internal_format = 0;

    if(gralloc_ && gralloc_->perform)
        ret = gralloc_->perform(gralloc_, op, hnd, &internal_format);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return internal_format;
#endif
}

void* DrmGralloc::hwc_get_handle_lock(buffer_handle_t hnd, int width, int height){
  void* cpu_addr = NULL;
#if USE_GRALLOC_4
  int ret = gralloc4::lock(hnd,GRALLOC_USAGE_SW_READ_MASK,0,0,width,height,(void **)&cpu_addr);
  if(ret != 0)
  {
    ALOGE("%s: fail to lock buffer, ret : %d",  __FUNCTION__, ret);
  }
#else // #if USE_GRALLOC_4
  if(gralloc_)
    gralloc_->lock(gralloc_,
                        hnd,
                        GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                        0,
                        0,
                        width,
                        height,
                        (void **)&cpu_addr);
#endif
  return cpu_addr;
}

int DrmGralloc::hwc_get_handle_unlock(buffer_handle_t hnd){
  int ret = 0;
#if USE_GRALLOC_4
  gralloc4::unlock(hnd);
#else   // USE_GRALLOC_4
  ret = gralloc_->unlock(gralloc_, hnd);
#endif  // USE_GRALLOC_4
  return ret;
}




}
