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

#define LOG_TAG "hwc-platform-drm-generic"

#include "drmresources.h"
#include "platform.h"
#include "platformdrmgeneric.h"

#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#ifdef ANDROID_P
#include <log/log.h>
#else
#include <cutils/log.h>
#endif

#if USE_GRALLOC_4
#include "drmgralloc4.h"
#endif

#if RK_DRM_GRALLOC
#include <gralloc_drm_handle.h>
#endif
#include <hardware/gralloc.h>
#include "hwc_util.h"
#include "hwc_rockchip.h"

namespace android {

#ifdef USE_DRM_GENERIC_IMPORTER
// static
Importer *Importer::CreateInstance(DrmResources *drm) {
  DrmGenericImporter *importer = new DrmGenericImporter(drm);
  if (!importer)
    return NULL;

  int ret = importer->Init();
  if (ret) {
    ALOGE("Failed to initialize the nv importer %d", ret);
    delete importer;
    return NULL;
  }
  return importer;
}
#endif

DrmGenericImporter::DrmGenericImporter(DrmResources *drm) : drm_(drm) {
}

DrmGenericImporter::~DrmGenericImporter() {
}

int DrmGenericImporter::Init() {
#if USE_GRALLOC_4
    gralloc_ = NULL;
#else   // USE_GRALLOC_4

  int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                          (const hw_module_t **)&gralloc_);
  if (ret) {
    ALOGE("Failed to open gralloc module");
    return ret;
  }
#endif  // USE_GRALLOC_4
  flag_ = NO_FLAG;
  return 0;
}
void DrmGenericImporter::SetFlag(DrmGenericImporterFlag_t flag){
  flag_ = flag;
}

uint32_t DrmGenericImporter::ConvertHalFormatToDrm(uint32_t hal_format) {
  /*
   * RK3326 VOP not support alpha scale, need to convert layer format
   *   DRM_FORMAT_ARGB8888 -> DRM_FORMAT_XRGB8888
   *   DRM_FORMAT_ABGR8888 -> DRM_FORMAT_XBGR8888
   */
  switch (hal_format) {
    case HAL_PIXEL_FORMAT_RGB_888:
      return DRM_FORMAT_BGR888;
    case HAL_PIXEL_FORMAT_BGRA_8888:
        if(flag_ == VOP_NOT_SUPPORT_ALPHA_SCALE)
          return DRM_FORMAT_XRGB8888;
      return DRM_FORMAT_ARGB8888;
    case HAL_PIXEL_FORMAT_RGBX_8888:
      return DRM_FORMAT_XBGR8888;
    case HAL_PIXEL_FORMAT_RGBA_8888:
        if(flag_ == VOP_NOT_SUPPORT_ALPHA_SCALE)
          return DRM_FORMAT_XBGR8888;
      return DRM_FORMAT_ABGR8888;
    //Fix color error in NenaMark2.
    case HAL_PIXEL_FORMAT_RGB_565:
      return DRM_FORMAT_RGB565;
    case HAL_PIXEL_FORMAT_YV12:
      return DRM_FORMAT_YVU420;
    case HAL_PIXEL_FORMAT_YCrCb_NV12:
      return DRM_FORMAT_NV12;
    case HAL_PIXEL_FORMAT_YCrCb_NV12_10:
      return DRM_FORMAT_NV12_10;
    default:
      ALOGE("Cannot convert hal format to drm format %u", hal_format);
      return -EINVAL;
  }
}

#ifndef u64
#define u64 uint64_t
#endif
int DrmGenericImporter::ImportBuffer(buffer_handle_t handle, hwc_drm_bo_t *bo
#if RK_VIDEO_SKIP_LINE
, uint32_t SkipLine
#endif
) {
  int fd,width,height,byte_stride,format;

  fd = hwc_get_handle_primefd(gralloc_, handle);
#if (!RK_PER_MODE && RK_DRM_GRALLOC)
  width = hwc_get_handle_attibute(gralloc_,handle,ATT_WIDTH);
  height = hwc_get_handle_attibute(gralloc_,handle,ATT_HEIGHT);
  byte_stride = hwc_get_handle_attibute(gralloc_,handle,ATT_BYTE_STRIDE);
  format = hwc_get_handle_attibute(gralloc_,handle,ATT_FORMAT);
#else
  width = hwc_get_handle_width(gralloc_,handle);
  height = hwc_get_handle_height(gralloc_,handle);
  byte_stride = hwc_get_handle_byte_stride(gralloc_,handle);
  format = hwc_get_handle_format(gralloc_,handle);
#endif
  uint32_t gem_handle;
  int ret = drmPrimeFDToHandle(drm_->fd(), fd, &gem_handle);
  if (ret) {
    ALOGE("failed to import prime fd %d ret=%d", fd, ret);
    return ret;
  }

  memset(bo, 0, sizeof(hwc_drm_bo_t));
  if(format == HAL_PIXEL_FORMAT_YCrCb_NV12_10)
  {
      bo->width = width/1.25;
      bo->width = ALIGN_DOWN(bo->width,2);
  }
  else
  {
      bo->width = width;
  }

#if RK_VIDEO_SKIP_LINE
  if(SkipLine)
  {
    bo->pitches[0] = byte_stride * SkipLine;
    bo->height = (height / SkipLine) + ((height / SkipLine) % 2);
  }
  else
#endif
  {
      bo->pitches[0] = byte_stride;
      bo->height = height;
  }

  bo->format = ConvertHalFormatToDrm(format);
  bo->gem_handles[0] = gem_handle;
  bo->offsets[0] = 0;

  if(format == HAL_PIXEL_FORMAT_YCrCb_NV12 || format == HAL_PIXEL_FORMAT_YCrCb_NV12_10)
  {
    bo->pitches[1] = bo->pitches[0];
    bo->gem_handles[1] = gem_handle;
    bo->offsets[1] = bo->pitches[1] * bo->height;
  }
#if USE_AFBC_LAYER
  __u64 modifier[4];
  uint64_t internal_format;
  memset(modifier, 0, sizeof(modifier));
#if USE_GRALLOC_4
    internal_format = gralloc4::get_internal_format(handle);
#else // #if USE_GRALLOC_4

#if RK_PER_MODE
  struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)handle;
  internal_format = drm_hnd->internal_format;
#else
  gralloc_->perform(gralloc_, GRALLOC_MODULE_PERFORM_GET_INTERNAL_FORMAT,
                         handle, &internal_format);
#endif
#endif // #if USE_GRALLOC_4


#if USE_GRALLOC_4
  if ( gralloc4::does_use_afbc_format(handle) )
#else
  if (isAfbcInternalFormat(internal_format))
#endif
  {
    ALOGD_IF(log_level(DBG_DEBUG),"KP : to set DRM_FORMAT_MOD_ARM_AFBC.");
#ifdef ANDROID_R
    modifier[0] = DRM_FORMAT_MOD_ARM_AFBC(1);
#else
    modifier[0] = DRM_FORMAT_MOD_ARM_AFBC;
#endif
  }

  ret = drmModeAddFB2_ext(drm_->fd(), bo->width, bo->height, bo->format,
                      bo->gem_handles, bo->pitches, bo->offsets, modifier,
		      &bo->fb_id, DRM_MODE_FB_MODIFIERS);
#else
  ret = drmModeAddFB2(drm_->fd(), bo->width, bo->height, bo->format,
                      bo->gem_handles, bo->pitches, bo->offsets, &bo->fb_id, 0);
#endif

    ALOGD_IF(log_level(DBG_DEBUG), "ImportBuffer fd=%d,w=%d,h=%d,format=0x%x,bo->format=0x%x,gem_handle=%d,bo->pitches[0]=%d,fb_id=%d",
        drm_->fd(), bo->width, bo->height, format,bo->format,
        gem_handle, bo->pitches[0], bo->fb_id);

  if (ret) {
    ALOGE("could not create drm fb %d", ret);
    ALOGE("ImportBuffer fd=%d,w=%d,h=%d,format=0x%x,bo->format=0x%x,gem_handle=%d,bo->pitches[0]=%d,fb_id=%d",
        drm_->fd(), bo->width, bo->height, format,bo->format,
        gem_handle, bo->pitches[0], bo->fb_id);
#if RK_VIDEO_SKIP_LINE
    ALOGE("SkipLine=%d",SkipLine);
#endif
    return ret;
  }

  //Fix "Failed to close gem handle" bug which lead by no reference counting.
#if 1
    struct drm_gem_close gem_close;
    int num_gem_handles;
    memset(&gem_close, 0, sizeof(gem_close));
    num_gem_handles = 1;

    for (int i = 0; i < num_gem_handles; i++) {
        if (!bo->gem_handles[i])
            continue;

        gem_close.handle = bo->gem_handles[i];
        int ret = drmIoctl(drm_->fd(), DRM_IOCTL_GEM_CLOSE, &gem_close);
        if (ret)
            ALOGE("Failed to close gem handle %d %d", i, ret);
        else
            bo->gem_handles[i] = 0;
    }
#endif

  return ret;
}

int DrmGenericImporter::ReleaseBuffer(hwc_drm_bo_t *bo) {
  if (bo->fb_id)
    if (drmModeRmFB(drm_->fd(), bo->fb_id))
      ALOGE("Failed to rm fb");

#if 0
  struct drm_gem_close gem_close;
  memset(&gem_close, 0, sizeof(gem_close));
  int num_gem_handles = sizeof(bo->gem_handles) / sizeof(bo->gem_handles[0]);
  for (int i = 0; i < num_gem_handles; i++) {
    if (!bo->gem_handles[i])
      continue;

    gem_close.handle = bo->gem_handles[i];
    int ret = drmIoctl(drm_->fd(), DRM_IOCTL_GEM_CLOSE, &gem_close);
    if (ret)
      ALOGE("Failed to close gem handle %d %d", i, ret);
    else
      bo->gem_handles[i] = 0;
  }
#endif
  return 0;
}

#ifdef USE_DRM_GENERIC_IMPORTER
std::unique_ptr<Planner> Planner::CreateInstance(DrmResources *) {
  std::unique_ptr<Planner> planner(new Planner);
  planner->AddStage<PlanStageGreedy>();
  return planner;
}
#endif
}
