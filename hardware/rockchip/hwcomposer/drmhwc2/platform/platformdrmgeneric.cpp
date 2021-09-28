/*
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

#include "platformdrmgeneric.h"
#include "drmdevice.h"
#include "platform.h"
#if USE_GRALLOC_4
#else
#include "gralloc_drm_handle.h"
#endif
#include "rockchip/drmgralloc.h"
#include "rockchip/platform/drmvop.h"
#include "rockchip/platform/drmvop2.h"

#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cutils/properties.h>
#include <hardware/gralloc.h>
#include <log/log.h>


#define ALIGN_DOWN( value, base)	(value & (~(base-1)) )

namespace android {

#ifdef USE_DRM_GENERIC_IMPORTER
// static
Importer *Importer::CreateInstance(DrmDevice *drm) {
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

DrmGenericImporter::DrmGenericImporter(DrmDevice *drm)
    : drm_(drm),
    exclude_non_hwfb_(false){
    drmGralloc_ = DrmGralloc::getInstance();
}

DrmGenericImporter::~DrmGenericImporter() {
}

int DrmGenericImporter::Init() {

  char exclude_non_hwfb_prop[PROPERTY_VALUE_MAX];
  property_get("hwc.drm.exclude_non_hwfb_imports", exclude_non_hwfb_prop, "0");
  exclude_non_hwfb_ = static_cast<bool>(strncmp(exclude_non_hwfb_prop, "0", 1));

  return 0;
}

uint32_t DrmGenericImporter::ConvertHalFormatToDrm(uint32_t hal_format) {
  switch (hal_format) {
    case HAL_PIXEL_FORMAT_RGB_888:
      return DRM_FORMAT_BGR888;
    case HAL_PIXEL_FORMAT_BGRA_8888:
      return DRM_FORMAT_ARGB8888;
    case HAL_PIXEL_FORMAT_RGBX_8888:
      return DRM_FORMAT_XBGR8888;
    case HAL_PIXEL_FORMAT_RGBA_8888:
      return DRM_FORMAT_ABGR8888;
    case HAL_PIXEL_FORMAT_RGBA_1010102:
      return DRM_FORMAT_ABGR2101010;
    //Fix color error in NenaMark2 and Taiji
    case HAL_PIXEL_FORMAT_RGB_565:
      return DRM_FORMAT_BGR565;
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

uint32_t DrmGenericImporter::DrmFormatToBitsPerPixel(uint32_t drm_format) {
  switch (drm_format) {
    case DRM_FORMAT_C8:
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_NV21:
    case DRM_FORMAT_NV16:
    case DRM_FORMAT_NV61:
    case DRM_FORMAT_YUV420:
    case DRM_FORMAT_YVU420:
      return 8;
    case DRM_FORMAT_ARGB4444:
    case DRM_FORMAT_XRGB4444:
    case DRM_FORMAT_ABGR4444:
    case DRM_FORMAT_XBGR4444:
    case DRM_FORMAT_RGBA4444:
    case DRM_FORMAT_RGBX4444:
    case DRM_FORMAT_BGRA4444:
    case DRM_FORMAT_BGRX4444:
    case DRM_FORMAT_ARGB1555:
    case DRM_FORMAT_XRGB1555:
    case DRM_FORMAT_ABGR1555:
    case DRM_FORMAT_XBGR1555:
    case DRM_FORMAT_RGBA5551:
    case DRM_FORMAT_RGBX5551:
    case DRM_FORMAT_BGRA5551:
    case DRM_FORMAT_BGRX5551:
    case DRM_FORMAT_RGB565:
    case DRM_FORMAT_BGR565:
    case DRM_FORMAT_UYVY:
    case DRM_FORMAT_VYUY:
    case DRM_FORMAT_YUYV:
    case DRM_FORMAT_YVYU:
      return 16;

    case DRM_FORMAT_BGR888:
    case DRM_FORMAT_RGB888:
      return 24;

    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_RGBA8888:
    case DRM_FORMAT_RGBX8888:
    case DRM_FORMAT_BGRA8888:
    case DRM_FORMAT_BGRX8888:
    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_XRGB2101010:
    case DRM_FORMAT_ABGR2101010:
    case DRM_FORMAT_XBGR2101010:
    case DRM_FORMAT_RGBA1010102:
    case DRM_FORMAT_RGBX1010102:
    case DRM_FORMAT_BGRA1010102:
    case DRM_FORMAT_BGRX1010102:
      return 32;

    case DRM_FORMAT_XRGB16161616F:
    case DRM_FORMAT_XBGR16161616F:
    case DRM_FORMAT_ARGB16161616F:
    case DRM_FORMAT_ABGR16161616F:
      return 64;
    default:
      ALOGE("Cannot convert hal format %u to bpp (returning 32)", drm_format);
      return 32;
  }
}

uint32_t DrmGenericImporter::DrmFormatToPlaneNum(uint32_t drm_format) {
  switch (drm_format) {
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_NV21:
    case DRM_FORMAT_NV16:
    case DRM_FORMAT_NV61:
    case DRM_FORMAT_NV12_10:
      return 2;
    default:
      return 1;
  }
}


int DrmGenericImporter::ImportBuffer(buffer_handle_t handle, hwc_drm_bo_t *bo) {

  uint32_t gem_handle;
  int ret = drmPrimeFDToHandle(drm_->fd(), bo->fd, &gem_handle);
  if (ret) {
    ALOGE("failed to import prime fd %d ret=%d", bo->fd, ret);
    return ret;
  }

  bo->pitches[0] = bo->byte_stride;
  bo->gem_handles[0] = gem_handle;
  bo->offsets[0] = 0;

  if(DrmFormatToPlaneNum(bo->format) == 2){
    bo->pitches[1] = bo->pitches[0];
    bo->gem_handles[1] = gem_handle;
    bo->offsets[1] = bo->pitches[1] * bo->height;
  }

  uint64_t modifier[4];
  memset(modifier, 0, sizeof(modifier));

  modifier[0] = bo->modifier;
  if(DrmFormatToPlaneNum(bo->format) == 2)
    modifier[1] = bo->modifier;

  ret = drmModeAddFB2WithModifiers(drm_->fd(), bo->width, bo->height, bo->format,
                      bo->gem_handles, bo->pitches, bo->offsets, modifier,
		                  &bo->fb_id, DRM_MODE_FB_MODIFIERS);

  ALOGD_IF(LogLevel(DBG_DEBUG),"ImportBuffer fd=%d,w=%d,h=%d,bo->format=%c%c%c%c,gem_handle=%d,bo->pitches[0]=%d,fb_id=%d,modifier = %" PRIx64 ,
      drm_->fd(), bo->width, bo->height, bo->format, bo->format >> 8, bo->format >> 16, bo->format >> 24,
      gem_handle, bo->pitches[0], bo->fb_id,bo->modifier);

  if (ret) {
    ALOGE("could not create drm fb %d", ret);
    ALOGE("ImportBuffer fail fd=%d,w=%d,h=%d,bo->format=%c%c%c%c,gem_handle=%d,bo->pitches[0]=%d,fb_id=%d,modifier=%" PRIx64 ,
    drm_->fd(), bo->width, bo->height, bo->format, bo->format >> 8, bo->format >> 16, bo->format >> 24,
    gem_handle, bo->pitches[0], bo->fb_id,bo->modifier);
    return ret;
  }


  // CopyBufferHandle need layer_cnt.
  unsigned int layer_count;
  for (layer_count = 0; layer_count < HWC_DRM_BO_MAX_PLANES; ++layer_count)
    if (bo->gem_handles[layer_count] == 0)
      break;
  bo->layer_cnt = layer_count;

  // Fix "Failed to close gem handle" bug which lead by no reference counting.
#if 1
  struct drm_gem_close gem_close;
  memset(&gem_close, 0, sizeof(gem_close));

  for (int i = 0; i < HWC_DRM_BO_MAX_PLANES; i++) {
    if (!bo->gem_handles[i])
      continue;

    gem_close.handle = bo->gem_handles[i];
    int ret = drmIoctl(drm_->fd(), DRM_IOCTL_GEM_CLOSE, &gem_close);
    if (ret) {
      ALOGE("Failed to close gem handle %d %d", i, ret);
    } else {
      for (int j = i + 1; j < HWC_DRM_BO_MAX_PLANES; j++)
        if (bo->gem_handles[j] == bo->gem_handles[i])
          bo->gem_handles[j] = 0;
      bo->gem_handles[i] = 0;
    }
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

  for (int i = 0; i < HWC_DRM_BO_MAX_PLANES; i++) {
    if (!bo->gem_handles[i])
      continue;

    gem_close.handle = bo->gem_handles[i];
    int ret = drmIoctl(drm_->fd(), DRM_IOCTL_GEM_CLOSE, &gem_close);
    if (ret) {
      ALOGE("Failed to close gem handle %d %d", i, ret);
    } else {
      for (int j = i + 1; j < HWC_DRM_BO_MAX_PLANES; j++)
        if (bo->gem_handles[j] == bo->gem_handles[i])
          bo->gem_handles[j] = 0;
      bo->gem_handles[i] = 0;
    }
  }
#endif
  return 0;
}

bool DrmGenericImporter::CanImportBuffer(buffer_handle_t handle) {
  if (handle == NULL)
    return false;

//  if (exclude_non_hwfb_) {
//    gralloc_drm_handle_t *hnd = gralloc_drm_handle(handle);
//    return hnd->usage & GRALLOC_USAGE_HW_FB;
//  }

  return true;
}

#ifdef USE_DRM_GENERIC_IMPORTER
std::unique_ptr<Planner> Planner::CreateInstance(DrmDevice *) {
  std::unique_ptr<Planner> planner(new Planner);
  planner->AddStage<PlanStageVop2>();
  planner->AddStage<PlanStageVop>();
  return planner;
}
#endif
}
