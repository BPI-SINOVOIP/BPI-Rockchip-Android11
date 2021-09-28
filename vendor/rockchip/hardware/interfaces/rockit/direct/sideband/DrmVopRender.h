/*
 * Copyright 2019 Rockchip Electronics Co. LTD
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
 *
 */

#ifndef __DRM_VOP_RENDER_H__
#define __DRM_VOP_RENDER_H__

#include <cutils/native_handle.h>
#include <hardware/hwcomposer_defs.h>
#include <map>

extern "C" {
#include "xf86drm.h"
#include "xf86drmMode.h"
#include "drm_fourcc.h"
}

namespace android {

enum {
    PANEL_ORIENTATION_0 = 0,
    PANEL_ORIENTATION_180
};

#ifdef INTEL_SUPPORT_HDMI_PRIMARY
enum {
    DEFAULT_DRM_FB_WIDTH = 1920,
    DEFAULT_DRM_FB_HEIGHT = 1080,
};
#endif
struct plane_prop {
  int crtc_id;
  int fb_id;

  int src_x;
  int src_y;
  int src_w;
  int src_h;

  int crtc_x;
  int crtc_y;
  int crtc_w;
  int crtc_h;

  int zpos;
};

typedef struct hwc_drm_bo {
  uint32_t width;
  uint32_t height;
  uint32_t format; /* DRM_FORMAT_* from drm_fourcc.h */
  uint32_t pitches[4];
  uint32_t offsets[4];
  uint32_t gem_handles[4];
  uint32_t fb_id;
  int acquire_fence_fd;
  void *priv;
} hwc_drm_bo_t;


class DrmVopRender {
public:
    DrmVopRender();
    virtual ~DrmVopRender();
public:
    bool initialize();
    void deinitialize();
    bool detect();
    bool detect(int device);

    uint32_t ConvertHalFormatToDrm(uint32_t hal_format);

    bool SetDrmPlane(int device, int32_t width, int32_t height, buffer_handle_t handle) ;
private:
    void resetOutput(int index);
    int FindSidebandPlane(int device);
    int GetFbid(buffer_handle_t handle);
    uint32_t getDrmEncoder(int device);

    // map device type to output index, return -1 if not mapped
    inline int getOutputIndex(int device);
    std::map<int, int> mFbidMap;
private:
    // DRM object index
    enum {
        OUTPUT_PRIMARY = 0,
        OUTPUT_EXTERNAL,
        OUTPUT_MAX,
    };

    struct DrmOutput {
        drmModeConnectorPtr connector;
        drmModeEncoderPtr encoder;
        drmModeCrtcPtr crtc;
        drmModeModeInfo mode;

        drmModePlaneResPtr plane_res;
        drmModePlanePtr plane;
        drmModeResPtr res;
        drmModeAtomicReq *req;
        drmModeObjectPropertiesPtr props;
        drmModePropertyPtr prop;

        uint32_t fbHandle;
        uint32_t fbId;
        int connected;
        int panelOrientation;
    } mOutputs[OUTPUT_MAX];

    int mDrmFd;
   // Mutex mLock;
    bool mInitialized;
    const gralloc_module_t *gralloc_;
};

} // namespace android



#endif /* __DRM_H__ */
