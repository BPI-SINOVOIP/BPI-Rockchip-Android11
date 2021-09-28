#define LOG_NDEBUG 0
#define LOG_TAG "DrmVopRender"

#include <fcntl.h>
#include <errno.h>
#include "DrmVopRender.h"
#include "log/log.h"
#include <unistd.h>

#include <sys/mman.h>
#include <cutils/properties.h>

#define HAS_ATOMIC 1

#define PROPERTY_TYPE "vendor"

namespace android {

#define ALIGN_DOWN( value, base)     (value & (~(base-1)) )

DrmVopRender::DrmVopRender()
    : mDrmFd(0),
      mInitialized(false)
{
    memset(&mOutputs, 0, sizeof(mOutputs));
}

DrmVopRender::~DrmVopRender()
{
   // WARN_IF_NOT_DEINIT();
}

bool DrmVopRender::initialize()
{
    ALOGE("initialize in");
    if (mInitialized) {
        ALOGE(">>Drm object has been initialized");
        return true;
    }

    const char *path = "/dev/dri/card0";

    mDrmFd = open(path, O_RDWR);
    if (mDrmFd < 0) {
        ALOGD("failed to open Drm, error: %s", strerror(errno));
        return false;
    }
    ALOGE("mDrmFd = %d", mDrmFd);

    memset(&mOutputs, 0, sizeof(mOutputs));
    mInitialized = true;
    int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                      (const hw_module_t **)&gralloc_);

    if (ret) {
        ALOGE("Failed to open gralloc module");
        return ret;
    }
    return true;
}

void DrmVopRender::deinitialize()
{
    for (int i = 0; i < OUTPUT_MAX; i++) {
        resetOutput(i);
    }

    if (mDrmFd) {
        close(mDrmFd);
        mDrmFd = 0;
    }

    for (const auto &fbidMap : mFbidMap) {
        int fbid = fbidMap.second;
        if (drmModeRmFB(mDrmFd, fbid))
            ALOGE("Failed to rm fb");
    }

    mInitialized = false;
}

bool DrmVopRender::detect() {

    detect(HWC_DISPLAY_PRIMARY);
    return true;
}

bool DrmVopRender::detect(int device)
{
    int outputIndex = getOutputIndex(device);
    if (outputIndex < 0 ) {
        return false;
    }

    resetOutput(outputIndex);
    drmModeConnectorPtr connector = NULL;
    DrmOutput *output = &mOutputs[outputIndex];
    bool ret = false;
    // get drm resources
    drmModeResPtr resources = drmModeGetResources(mDrmFd);
    if (!resources) {
        ALOGE("fail to get drm resources, error: %s", strerror(errno));
        return false;
    }

    ret = drmSetClientCap(mDrmFd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    if (ret) {
        ALOGE("Failed to set atomic cap %s", strerror(errno));
        return ret;
    }
    ret = drmSetClientCap(mDrmFd, DRM_CLIENT_CAP_ATOMIC, 1);
    if (ret) {
        ALOGE("Failed to set atomic cap %s", strerror(errno));
        return ret;
    }

    output->res = resources;
    ALOGD("resources->count_connectors=%d",resources->count_connectors);
    // find connector for the given device
    for (int i = 0; i < resources->count_connectors; i++) {
        if (!resources->connectors || !resources->connectors[i]) {
            ALOGE("fail to get drm resources connectors, error: %s", strerror(errno));
            continue;
        }

        connector = drmModeGetConnector(mDrmFd, resources->connectors[i]);
        if (!connector) {
            ALOGE("drmModeGetConnector failed");
            continue;
        }

        if (connector->connection != DRM_MODE_CONNECTED) {
            ALOGE("+++device %d is not connected", device);
            drmModeFreeConnector(connector);
            ret = true;
            break;
        }

        output->connector = connector;
        output->connected = true;
        ALOGD("connector %d connected",outputIndex);
        // get proper encoder for the given connector
        if (connector->encoder_id) {
            ALOGD("Drm connector has encoder attached on device %d", device);
            output->encoder = drmModeGetEncoder(mDrmFd, connector->encoder_id);
            if (!output->encoder) {
                ALOGD("failed to get encoder from a known encoder id");
                // fall through to get an encoder
            }
        }

        if (!output->encoder) {
            ALOGD("getting encoder for device %d", device);
            drmModeEncoderPtr encoder;
            for (int j = 0; j < resources->count_encoders; j++) {
                if (!resources->encoders || !resources->encoders[j]) {
                    ALOGE("fail to get drm resources encoders, error: %s", strerror(errno));
                    continue;
                }

                encoder = drmModeGetEncoder(mDrmFd, resources->encoders[i]);
                if (!encoder) {
                    ALOGE("drmModeGetEncoder failed");
                    continue;
                }
                ALOGD("++++encoder_type=%d,device=%d",encoder->encoder_type,getDrmEncoder(device));
                if (encoder->encoder_type == getDrmEncoder(device)) {
                    output->encoder = encoder;
                    break;
                }
                drmModeFreeEncoder(encoder);
                encoder = NULL;
            }
        }
        if (!output->encoder) {
            ALOGE("failed to get drm encoder");
            break;
        }

        // get an attached crtc or spare crtc
        if (output->encoder->crtc_id) {
            ALOGD("Drm encoder has crtc attached on device %d", device);
            output->crtc = drmModeGetCrtc(mDrmFd, output->encoder->crtc_id);
            if (!output->crtc) {
                ALOGE("failed to get crtc from a known crtc id");
                // fall through to get a spare crtc
            }
        }
        if (!output->crtc) {
            ALOGE("getting crtc for device %d", device);
            drmModeCrtcPtr crtc;
            for (int j = 0; j < resources->count_crtcs; j++) {
                if (!resources->crtcs || !resources->crtcs[j]) {
                    ALOGE("fail to get drm resources crtcs, error: %s", strerror(errno));
                    continue;
                }

                crtc = drmModeGetCrtc(mDrmFd, resources->crtcs[j]);
                if (!crtc) {
                    ALOGE("drmModeGetCrtc failed");
                    continue;
                }
                // check if legal crtc to the encoder
                if (output->encoder->possible_crtcs & (1<<j)) {
                    output->crtc = crtc;
                }
                drmModeObjectPropertiesPtr props;
                drmModePropertyPtr prop;
                props = drmModeObjectGetProperties(mDrmFd, crtc->crtc_id, DRM_MODE_OBJECT_CRTC);
                if (!props) {
                    ALOGD("Failed to found props crtc[%d] %s\n",
                        crtc->crtc_id, strerror(errno));
                    continue;
                }
                for (uint32_t i = 0; i < props->count_props; i++) {
                    prop = drmModeGetProperty(mDrmFd, props->props[i]);
                    if (!strcmp(prop->name, "ACTIVE")) {
                        ALOGD("Crtc id=%d is ACTIVE.", crtc->crtc_id);
                        if (props->prop_values[i]) {
                            output->crtc = crtc;
                            ALOGD("Crtc id=%d is active",crtc->crtc_id);
                            break;
                        }
                    }
                }
            }
        }
        if (!output->crtc) {
            ALOGE("failed to get drm crtc");
            break;
        }
        output->plane_res = drmModeGetPlaneResources(mDrmFd);
        break;
    }

    output->props = drmModeObjectGetProperties(mDrmFd, output->crtc->crtc_id, DRM_MODE_OBJECT_CRTC);
    if (!output->props) {
        ALOGE("Failed to found props crtc[%d] %s\n", output->crtc->crtc_id, strerror(errno));
    }
    drmModeFreeResources(resources);

    return ret;
}

uint32_t DrmVopRender::getDrmEncoder(int device)
{
    if (device == HWC_DISPLAY_PRIMARY)
        return 2;
    else if (device == HWC_DISPLAY_EXTERNAL)
        return DRM_MODE_ENCODER_TMDS;
    return DRM_MODE_ENCODER_NONE;
}

uint32_t DrmVopRender::ConvertHalFormatToDrm(uint32_t hal_format) {
  switch (hal_format) {
    case HAL_PIXEL_FORMAT_RGB_888:
      return DRM_FORMAT_BGR888;
    case HAL_PIXEL_FORMAT_BGRA_8888:
      return DRM_FORMAT_ARGB8888;
    case HAL_PIXEL_FORMAT_RGBX_8888:
      return DRM_FORMAT_XBGR8888;
    case HAL_PIXEL_FORMAT_RGBA_8888:
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

int DrmVopRender::FindSidebandPlane(int device) {
    drmModePlanePtr plane;
    drmModeObjectPropertiesPtr props;
    drmModePropertyPtr prop;
    int find_plan_id = 0;
    int outputIndex = getOutputIndex(device);
    if (outputIndex < 0 ) {
        ALOGE("invalid device");
        return false;
    }
    DrmOutput *output= &mOutputs[outputIndex];
    if (!output->connected) {
        ALOGE("device is not connected,outputIndex=%d",outputIndex);
        return false;
    }

    for(uint32_t i = 0; i < output->plane_res->count_planes; i++) {
        plane = drmModeGetPlane(mDrmFd, output->plane_res->planes[i]);
        props = drmModeObjectGetProperties(mDrmFd, plane->plane_id, DRM_MODE_OBJECT_PLANE);
        if (!props) {
            ALOGE("Failed to found props plane[%d] %s\n",plane->plane_id, strerror(errno));
           return -ENODEV;
        }
        for (uint32_t j = 0; j < props->count_props; j++) {
            prop = drmModeGetProperty(mDrmFd, props->props[j]);
            if (!strcmp(prop->name, "ASYNC_COMMIT")) {
                ALOGV("find ASYNC_COMMIT plane id=%d value=%lld", plane->plane_id, (long long)props->prop_values[j]);
                if (props->prop_values[j]) {
                    find_plan_id = plane->plane_id;
                    if (prop)
                        drmModeFreeProperty(prop);
                    break;
                }
            }
            if (prop)
                drmModeFreeProperty(prop);
        }
        if(props)
            drmModeFreeObjectProperties(props);
        if(plane)
            drmModeFreePlane(plane);
        ALOGV("FindSidebandPlane find_plan_id1=%d", find_plan_id);
        if (find_plan_id > 0) {
            break;
        }
    }
    if (find_plan_id == 0) {
       return -1;
    }
    return find_plan_id;
}

int DrmVopRender::GetFbid(buffer_handle_t handle) {

    hwc_drm_bo_t bo;
    int fd = 0;
    int ret = 0;
    int src_w = 0;
    int src_h = 0;
    int src_format = 0;
    int src_stride = 0;

    gralloc_->perform(gralloc_,
                      GRALLOC_MODULE_PERFORM_GET_HADNLE_PRIME_FD,
                      handle, &fd);
    std::map<int, int>::iterator it = mFbidMap.find(fd);
    int fbid = 0;
    if (it == mFbidMap.end()) {
        memset(&bo, 0, sizeof(hwc_drm_bo_t));
        uint32_t gem_handle;
        gralloc_->perform(gralloc_,
                        GRALLOC_MODULE_PERFORM_GET_HADNLE_PRIME_FD,
                        handle, &fd);
        ret = drmPrimeFDToHandle(mDrmFd, fd, &gem_handle);
        gralloc_->perform(gralloc_, GRALLOC_MODULE_PERFORM_GET_HADNLE_WIDTH, handle, &src_w);
        gralloc_->perform(gralloc_, GRALLOC_MODULE_PERFORM_GET_HADNLE_HEIGHT, handle, &src_h);
        gralloc_->perform(gralloc_, GRALLOC_MODULE_PERFORM_GET_HADNLE_FORMAT, handle, &src_format);
        gralloc_->perform(gralloc_, GRALLOC_MODULE_PERFORM_GET_HADNLE_BYTE_STRIDE, handle, &src_stride);
        bo.width = src_w;
        bo.height = src_h;
        bo.format = ConvertHalFormatToDrm(src_format);
        bo.pitches[0] = src_stride;
        bo.gem_handles[0] = gem_handle;
        bo.offsets[0] = 0;
        if(src_format == HAL_PIXEL_FORMAT_YCrCb_NV12 || src_format == HAL_PIXEL_FORMAT_YCrCb_NV12_10)
        {
            bo.pitches[1] = bo.pitches[0];
            bo.gem_handles[1] = gem_handle;
            bo.offsets[1] = bo.pitches[1] * bo.height;
        }
        if (src_format == HAL_PIXEL_FORMAT_YCrCb_NV12_10) {
            bo.width = src_w / 1.25;
            bo.width = ALIGN_DOWN(bo.width, 2);
        }
        ALOGV("width=%d,height=%d,format=%x,fd=%d,src_stride=%d",bo.width, bo.height, bo.format, fd, src_stride);
        ret = drmModeAddFB2(mDrmFd, bo.width, bo.height, bo.format, bo.gem_handles,\
                     bo.pitches, bo.offsets, &bo.fb_id, 0);
        fbid = bo.fb_id;
        mFbidMap.insert(std::make_pair(fd, fbid));
    } else {
        fbid = it->second;
    }
    if (fbid <= 0) {
        ALOGD("fbid is error.");
        return -1;
    }

    return fbid;
}

void DrmVopRender::resetOutput(int index)
{
    DrmOutput *output = &mOutputs[index];

    output->connected = false;
    memset(&output->mode, 0, sizeof(drmModeModeInfo));

    if (output->connector) {
        drmModeFreeConnector(output->connector);
        output->connector = 0;
    }
    if (output->encoder) {
        drmModeFreeEncoder(output->encoder);
        output->encoder = 0;
    }
    if (output->crtc) {
        drmModeFreeCrtc(output->crtc);
        output->crtc = 0;
    }
    if (output->fbId) {
        drmModeRmFB(mDrmFd, output->fbId);
        output->fbId = 0;
    }
    if (output->fbHandle) {
        output->fbHandle = 0;
    }
}

bool DrmVopRender::SetDrmPlane(int device, int32_t width, int32_t height, buffer_handle_t handle) {
    int ret = 0;
    int plane_id = FindSidebandPlane(device);
    int fb_id = GetFbid(handle);
    int flags = 0;
    int src_left = 0;
    int src_top = 0;
    int src_right = 0;
    int src_bottom = 0;
    int dst_left = 0;
    int dst_top = 0;
    int dst_right = 0;
    int dst_bottom = 0;
    int src_w = 0;
    int src_h = 0;
    int dst_w = 0;
    int dst_h = 0;
    int src_format = 0;
    char sideband_crop[PROPERTY_VALUE_MAX];
    memset(sideband_crop, 0, sizeof(sideband_crop));
    DrmOutput *output= &mOutputs[device];
    int length = property_get(PROPERTY_TYPE ".sideband.crop", sideband_crop, NULL);
    if (length > 0) {
       sscanf(sideband_crop, "%d-%d-%d-%d-%d-%d-%d-%d",\
              &src_left, &src_top, &src_right, &src_bottom,\
              &dst_left, &dst_top, &dst_right, &dst_bottom);
       dst_w = dst_right - dst_left;
       dst_h = dst_bottom - dst_top;
    } else {
       dst_w = output->crtc->width;
       dst_h = output->crtc->height;
    }
    src_w = width;
    src_h = height;
    gralloc_->perform(gralloc_, GRALLOC_MODULE_PERFORM_GET_HADNLE_FORMAT, handle, &src_format);
    ALOGV("dst_w %d dst_h %d src_w %d src_h %d in", dst_w, dst_h, src_w, src_h);
    if (plane_id > 0) {
        ret = drmModeSetPlane(mDrmFd, plane_id,
                          output->crtc->crtc_id, fb_id, flags,
                          dst_left, dst_top,
                          dst_w, dst_h,
                          0, 0,
                          src_w << 16, src_h << 16);

    }

    return true;
}

int DrmVopRender::getOutputIndex(int device)
{
    switch (device) {
    case HWC_DISPLAY_PRIMARY:
        return OUTPUT_PRIMARY;
    case HWC_DISPLAY_EXTERNAL:
        return OUTPUT_EXTERNAL;
    default:
        ALOGD("invalid display device");
        break;
    }

    return -1;
}

} // namespace android

