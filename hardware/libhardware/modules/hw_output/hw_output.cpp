/*
 * Copyright 2014 The Android Open Source Project
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
#define LOG_TAG "hw_output"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdio.h>
#include <stdint.h>
#include <drm_fourcc.h>

#include <string>
#include <map>
#include <vector>
#include <iostream>

#include <cutils/native_handle.h>
#include <cutils/properties.h>
#include <log/log.h>

#include <hardware/hw_output.h>
#include "baseparameter_api.h"
#include "hw_types.h"

#include "rkdisplay/drmresources.h"
#include "rkdisplay/drmmode.h"
#include "rkdisplay/drmconnector.h"
#include "rkdisplay/drmgamma.h"
#include "rockchip/baseparameter.h"

using namespace android;

#define LOG_LEVEL_ERROR 0
#define LOG_LEVEL_WARN 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_DEBUG 3

static int mHwcVersion = 0;
std::map<int,DrmConnector*> mGlobalConns;
static int dbgLevel = 3;
#define LOG_INFO(format, ...) \
    do {\
        if (dbgLevel < LOG_LEVEL_INFO)\
            break;\
        ALOGD("%s[%d]" format, __FUNCTION__, __LINE__, ##_VA_ARGS__); \
    } while(0)

/*****************************************************************************/

typedef struct hw_output_private {
    hw_output_device_t device;

    // Callback related data
    void* callback_data;
    DrmResources *drm_;
    DrmConnector* primary;
    DrmConnector* extend;
    BaseParameter* mBaseParmeter;
    struct lut_info* mlut;
}hw_output_private_t;

static int hw_output_device_open(const struct hw_module_t* module,
        const char* name, struct hw_device_t** device);

static struct hw_module_methods_t hw_output_module_methods = {
    .open = hw_output_device_open
};

hw_output_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 0,
        .id = HW_OUTPUT_HARDWARE_MODULE_ID,
        .name = "Sample hw output module",
        .author = "The Android Open Source Project",
        .methods = &hw_output_module_methods,
        .dso = NULL,
        .reserved = {0},
    }
};

static bool builtInHdmi(int type){
    return type == DRM_MODE_CONNECTOR_HDMIA || type == DRM_MODE_CONNECTOR_HDMIB;
}

static void checkBcshInfo(uint32_t* mBcsh)
{
    if (mBcsh[0] < 0)
        mBcsh[0] = 0;
    else if (mBcsh[0] > 100)
        mBcsh[0] = 100;

    if (mBcsh[1] < 0)
        mBcsh[1] = 0;
    else if (mBcsh[1] > 100)
        mBcsh[1] = 100;

    if (mBcsh[2] < 0)
        mBcsh[2] = 0;
    else if (mBcsh[2] > 100)
        mBcsh[2] = 100;

    if (mBcsh[3] < 0)
        mBcsh[3] = 0;
    else if (mBcsh[3] > 100)
        mBcsh[3] = 100;
}

static void updateTimeline()
{
    std::string strTimeline;
    char property[100];
    int timeline = property_get_int32("vendor.display.timeline", 1);

    timeline++;
    strTimeline = std::to_string(timeline);
    property_set("vendor.display.timeline", strTimeline.c_str());

    property_get("vendor.hw_output.debug", property, "3");
    dbgLevel = atoi(property);
}

DrmConnector* getValidDrmConnector(hw_output_private_t *priv, int dpy)
{
    std::map<int, DrmConnector*> mConns = mGlobalConns;
    std::map<int, DrmConnector*>::iterator iter;
    DrmConnector* mConnector = nullptr;
    (void)priv;

    iter = mConns.find(dpy);
    if (iter != mConns.end()) {
        mConnector = iter->second;
    }

    return mConnector;
}

static std::string getPropertySuffix(hw_output_private_t *priv, std::string header, int dpy)
{
    DrmConnector* conn = getValidDrmConnector(priv, dpy);
    std::string suffix;

    suffix = header;
    if (mHwcVersion == 2) {
        if (conn != nullptr) {
            const char* connTypeStr = priv->drm_->connector_type_str(conn->get_type());
            int id = conn->connector_id();
            suffix += connTypeStr;
            suffix += '-';
            ALOGD("id=%d", id);
            suffix += std::to_string(id);
        }
    } else {
        if (dpy == HWC_DISPLAY_PRIMARY)
            suffix += "main";
        else
            suffix += "aux";
    }
    ALOGD("suffix=%s", suffix.c_str());
    return suffix;
}

static int findSuitableInfoSlot(struct disp_info* info, int type, int id)
{
    int found=0;
    for (int i=0;i<4;i++) {
        if (info->screen_info[i].type !=0 && info->screen_info[i].type == type &&
            info->screen_info[i].id == id) {
            found = i;
            break;
        } else if (info->screen_info[i].type !=0 && found == false){
            found++;
        }
    }
    if (found == -1) {
        found = 0;
        ALOGD("noting saved, used the first slot");
    }
    ALOGD("findSuitableInfoSlot: %d type=%d", found, type);
    return found;
}

static bool getResolutionInfo(hw_output_private_t *priv, int dpy, char* resolution)
{
    drmModePropertyBlobPtr blob;
    drmModeObjectPropertiesPtr props;
    DrmConnector* mCurConnector = NULL;
    DrmCrtc *crtc = NULL;
    struct drm_mode_modeinfo *drm_mode;
    struct disp_info info;
    BaseParameter* mBaseParmeter = priv->mBaseParmeter;
    int value;
    bool found = false;

    mCurConnector = getValidDrmConnector(priv, dpy);
    if (mCurConnector == nullptr) {
        sprintf(resolution, "%s", "Auto");
        return false;
    }

    if (mBaseParmeter && mBaseParmeter->have_baseparameter()) {
        if (mCurConnector)
            mBaseParmeter->get_disp_info(mCurConnector->get_type(), mCurConnector->connector_id(), &info);
        int slot = findSuitableInfoSlot(&info, mCurConnector->get_type(), mCurConnector->connector_id());
        if (!info.screen_info[slot].resolution.hdisplay ||
            !info.screen_info[slot].resolution.clock ||
            !info.screen_info[slot].resolution.vdisplay) {
            sprintf(resolution, "%s", "Auto");
            return false;
        }
    }

    if (mCurConnector != NULL) {
        crtc = priv->drm_->GetCrtcFromConnector(mCurConnector);
        if (crtc == NULL) {
            return false;
        }
        props = drmModeObjectGetProperties(priv->drm_->fd(), crtc->id(), DRM_MODE_OBJECT_CRTC);
        for (int i = 0; !found && (size_t)i < props->count_props; ++i) {
            drmModePropertyPtr p = drmModeGetProperty(priv->drm_->fd(), props->props[i]);
            if (!strcmp(p->name, "MODE_ID")) {
                found = true;
                if (!drm_property_type_is(p, DRM_MODE_PROP_BLOB)) {
                    ALOGE("%s:line=%d,is not blob",__FUNCTION__,__LINE__);
                    drmModeFreeProperty(p);
                    drmModeFreeObjectProperties(props);
                    return false;
                }
                if (!p->count_blobs)
                    value = props->prop_values[i];
                else
                    value = p->blob_ids[0];
                blob = drmModeGetPropertyBlob(priv->drm_->fd(), value);
                if (!blob) {
                    ALOGE("%s:line=%d, blob is null",__FUNCTION__,__LINE__);
                    drmModeFreeProperty(p);
                    drmModeFreeObjectProperties(props);
                    return false;
                }

                float vfresh;
                drm_mode = (struct drm_mode_modeinfo *)blob->data;
                if (drm_mode->flags & DRM_MODE_FLAG_INTERLACE)
                    vfresh = drm_mode->clock *2/ (float)(drm_mode->vtotal * drm_mode->htotal) * 1000.0f;
                else
                    vfresh = drm_mode->clock / (float)(drm_mode->vtotal * drm_mode->htotal) * 1000.0f;
                ALOGD("nativeGetCurMode: crtc_id=%d clock=%d w=%d %d %d %d %d %d flag=0x%x vfresh %.2f drm.vrefresh=%.2f", 
                        crtc->id(), drm_mode->clock, drm_mode->htotal, drm_mode->hsync_start,
                        drm_mode->hsync_end, drm_mode->vtotal, drm_mode->vsync_start, drm_mode->vsync_end, drm_mode->flags,
                        vfresh, (float)drm_mode->vrefresh);
                sprintf(resolution, "%dx%d@%.2f-%d-%d-%d-%d-%d-%d-%x-%d", drm_mode->hdisplay, drm_mode->vdisplay, vfresh,
                        drm_mode->hsync_start, drm_mode->hsync_end, drm_mode->htotal,
                        drm_mode->vsync_start, drm_mode->vsync_end, drm_mode->vtotal,
                        (drm_mode->flags&0xFFFF), drm_mode->clock);
                drmModeFreePropertyBlob(blob);
            }
            drmModeFreeProperty(p);
        }
        drmModeFreeObjectProperties(props);
    } else {
        return false;
    }

    return true;
}

static void updateConnectors(hw_output_private_t *priv){
    if (priv->drm_->connectors().size() == 2) {
        bool foundHdmi=false;
        int cnt=0,crtcId1=0,crtcId2=0;
        for (auto &conn : priv->drm_->connectors()) {
            if (cnt == 0 && priv->drm_->GetCrtcFromConnector(conn.get())) {
                ALOGD("encoderId1: %d", conn->encoder()->id());
                crtcId1 = priv->drm_->GetCrtcFromConnector(conn.get())->id();
            } else if (priv->drm_->GetCrtcFromConnector(conn.get())){
                ALOGD("encoderId2: %d", conn->encoder()->id());
                crtcId2 = priv->drm_->GetCrtcFromConnector(conn.get())->id();
            }

            if (builtInHdmi(conn->get_type()))
                foundHdmi=true;
            cnt++;
        }
        ALOGD("crtc: %d %d foundHdmi %d 2222", crtcId1, crtcId2, foundHdmi);
        char property[PROPERTY_VALUE_MAX];
        property_get("vendor.hwc.device.primary", property, "null");
        if (crtcId1 == crtcId2 && foundHdmi && strstr(property, "HDMI-A") == NULL) {
            for (auto &conn : priv->drm_->connectors()) {
                if (builtInHdmi(conn->get_type()) && conn->state() == DRM_MODE_CONNECTED) {
                    priv->extend = conn.get();
                    conn->set_display(1);
                } else if(!builtInHdmi(conn->get_type()) && conn->state() == DRM_MODE_CONNECTED) {
                    priv->primary = conn.get();
                    conn->set_display(0);
                }
            }
        }
    }
}

/*****************************************************************************/
static void hw_output_save_config(struct hw_output_device* dev){
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    if (priv->mBaseParmeter)
        priv->mBaseParmeter->saveConfig();
}

static void hw_output_hotplug_update(struct hw_output_device* dev){
    hw_output_private_t* priv = (hw_output_private_t*)dev;

    DrmConnector *mextend = NULL;
    DrmConnector *mprimary = NULL;
    int dpy = 0;

    for (auto &conn : priv->drm_->connectors()) {
        drmModeConnection old_state = conn->state();

        conn->UpdateModes();

        drmModeConnection cur_state = conn->state();
        ALOGD("old_state %d cur_state %d conn->get_type() %d", old_state, cur_state, conn->get_type());

        if (cur_state == old_state)
            continue;
        ALOGI("%s event  for connector %u\n",
                cur_state == DRM_MODE_CONNECTED ? "Plug" : "Unplug", conn->id());

        if (cur_state == DRM_MODE_CONNECTED) {
            if (conn->possible_displays() & HWC_DISPLAY_EXTERNAL_BIT) {
                mextend = conn.get();
            } else if (conn->possible_displays() & HWC_DISPLAY_PRIMARY_BIT) {
                mprimary = conn.get();
            }
        }
    }

    /*
     * status changed?
     */
    priv->drm_->DisplayChanged();
    dpy = mGlobalConns.size();

    DrmConnector *old_primary = priv->drm_->GetConnectorFromType(HWC_DISPLAY_PRIMARY);
    mprimary = mprimary ? mprimary : old_primary;
    if (!mprimary || mprimary->state() != DRM_MODE_CONNECTED) {
        mprimary = NULL;
        for (auto &conn : priv->drm_->connectors()) {
            if (!(conn->possible_displays() & HWC_DISPLAY_PRIMARY_BIT))
                continue;
            if (conn->state() == DRM_MODE_CONNECTED) {
                mprimary = conn.get();
                //mGlobalConns[HWC_DISPLAY_PRIMARY] = conn.get();
                break;
            }
        }
    }

    if (!mprimary) {
        ALOGE("%s %d Failed to find primary display\n", __FUNCTION__, __LINE__);
        //return;
    }
    if (mprimary && mprimary != old_primary) {
        priv->drm_->SetPrimaryDisplay(mprimary);
    }

    DrmConnector *old_extend = priv->drm_->GetConnectorFromType(HWC_DISPLAY_EXTERNAL);
    mextend = mextend ? mextend : old_extend;
    dpy = 1;
    if (!mextend || mextend->state() != DRM_MODE_CONNECTED) {
        mextend = NULL;
        for (auto &conn : priv->drm_->connectors()) {
            if (!(conn->possible_displays() & HWC_DISPLAY_EXTERNAL_BIT))
                continue;
            if (mprimary && conn->id() == mprimary->id())
                continue;
            if (conn->state() == DRM_MODE_CONNECTED) {
                mextend = conn.get();
                //mGlobalConns[dpy] = conn.get();
                break;
            }
        }
    }
    priv->drm_->SetExtendDisplay(mextend);
    priv->drm_->DisplayChanged();
    priv->drm_->UpdateDisplayRoute();
    priv->drm_->ClearDisplay();

    updateConnectors(priv);
}

static int hw_output_init_baseparameter(BaseParameter** mBaseParmeter)
{
    char property[100];
    property_get("vendor.ghwc.version", property, NULL);
    if (strstr(property, "HWC2") != NULL) {
        *mBaseParmeter = new BaseParameterV2();
        mHwcVersion = 2;
    } else {
        *mBaseParmeter = new BaseParameterV1();
        mHwcVersion = 1;
    }
    return 0;
}

static int hw_output_initialize(struct hw_output_device* dev, void* data)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;

    priv->drm_ = NULL;
    priv->primary = NULL;
    priv->extend = NULL;
    priv->mlut = NULL;
    priv->callback_data = data;
    hw_output_init_baseparameter(&priv->mBaseParmeter);

    if (priv->drm_ == NULL) {
        priv->drm_ = new DrmResources();
        priv->drm_->Init();
        ALOGD("nativeInit: ");
        if (mHwcVersion >= 2) {
            int id=0;
            for (auto &conn : priv->drm_->connectors())
                mGlobalConns.insert(std::make_pair(id++, conn.get()));
        } else {
            int id=1;
            for (auto &conn : priv->drm_->connectors()) {
                if (conn->possible_displays() & HWC_DISPLAY_PRIMARY_BIT)
                    mGlobalConns.insert(std::make_pair(HWC_DISPLAY_PRIMARY, conn.get()));
                else
                    mGlobalConns.insert(std::make_pair(id++, conn.get()));
            }
        }
        priv->mBaseParmeter->set_drm_connectors(mGlobalConns);
        hw_output_hotplug_update(dev);
        if (priv->primary == NULL) {
            for (auto &conn : priv->drm_->connectors()) {
                if ((conn->possible_displays() & HWC_DISPLAY_PRIMARY_BIT)) {
                    //mGlobalConns[HWC_DISPLAY_PRIMARY] = conn.get();
                }
                if ((conn->possible_displays() & HWC_DISPLAY_EXTERNAL_BIT) && conn->state() == DRM_MODE_CONNECTED) {
                    priv->drm_->SetExtendDisplay(conn.get());
                    priv->extend = conn.get();
                }
            }
        }
        ALOGD("primary: %p extend: %p ", priv->primary, priv->extend);
    }

    return 0;
}

/*****************************************************************************/

static int hw_output_set_mode(struct hw_output_device* dev, int dpy, const char* mode)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    DrmConnector* conn = getValidDrmConnector(priv, dpy);
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    char property[PROPERTY_VALUE_MAX];
    std::string propertyStr;

    propertyStr = getPropertySuffix(priv, "persist.vendor.resolution.", dpy);

    ALOGD("nativeSetMode %s display %d", mode, dpy);

    if (strcmp(mode, property) !=0) {
        property_set(propertyStr.c_str(), mode);
        updateTimeline();
        struct disp_info info;
        float vfresh=0.0f;
        int slot = 0;

        mBaseParameter->get_disp_info(conn->get_type(), conn->connector_id(), &info);
        slot = findSuitableInfoSlot(&info, conn->get_type(), conn->connector_id());
        info.screen_info[slot].type = conn->get_type();
        info.screen_info[slot].id = conn->connector_id();
        if (strncmp(mode, "Auto", 4) != 0 && strncmp(mode, "0x0p0-0", 7) !=0) {
            sscanf(mode,"%dx%d@%f-%d-%d-%d-%d-%d-%d-%x-%d",
                    &info.screen_info[slot].resolution.hdisplay, &info.screen_info[slot].resolution.vdisplay,
                    &vfresh, &info.screen_info[slot].resolution.hsync_start,&info.screen_info[slot].resolution.hsync_end,
                    &info.screen_info[slot].resolution.htotal,&info.screen_info[slot].resolution.vsync_start,
                    &info.screen_info[slot].resolution.vsync_end, &info.screen_info[slot].resolution.vtotal,
                    &info.screen_info[slot].resolution.flags, &info.screen_info[slot].resolution.clock);
           info.screen_info[slot].resolution.vrefresh = (int)vfresh;
        } else {
            info.screen_info[slot].feature|= RESOLUTION_AUTO;
            memset(&info.screen_info[slot].resolution, 0, sizeof(info.screen_info[slot].resolution));
        }
        mBaseParameter->set_disp_info(conn->get_type(), conn->connector_id(), &info);
    }
    return 0;
}

static int hw_output_set_3d_mode(struct hw_output_device*, const char* mode)
{
    char property[PROPERTY_VALUE_MAX];

    property_get("vendor.3d_resolution.main", property, "null");
    if (strcmp(mode, property) !=0) {
        property_set("vendor.3d_resolution.main", mode);
        updateTimeline();
    }
    return 0;
}

static int hw_output_set_gamma(struct hw_output_device* dev, int dpy, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    DrmConnector* mConnector = getValidDrmConnector(priv, dpy);
    int ret = -1;
    int crtc_id = 0;

    if (mConnector)
        crtc_id = priv->drm_->GetCrtcFromConnector(mConnector)->id();

    ret = DrmGamma::set_3x1d_gamma(priv->drm_->fd(), crtc_id, size, r, g, b);
    if (ret < 0)
        ALOGE("fail to SetGamma %d(%s)", ret, strerror(errno));
    if(ret == 0){
        struct gamma_lut_data data;
        data.size = size;
        for(int i = 0; i< size; i++){
            data.lred[i] = r[i];
            data.lgreen[i] = g[i];
            data.lblue[i] = b[i];
        }
        mBaseParameter->set_gamma_lut_data(mConnector->get_type(), mConnector->connector_id(), &data);
    }
    return ret;
}

static int hw_output_set_3d_lut(struct hw_output_device* dev, int dpy, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    DrmConnector* mConnector = getValidDrmConnector(priv, dpy);
    int ret = -1;
    int crtc_id = 0;

    if (mConnector)
        crtc_id = priv->drm_->GetCrtcFromConnector(mConnector)->id();
    ret = DrmGamma::set_cubic_lut(priv->drm_->fd(), crtc_id, size, r, g, b);
    if (ret < 0)
        ALOGE("fail to set 3d lut %d(%s)", ret, strerror(errno));
    if(ret == 0){
        struct cubic_lut_data data;
        for(int i = 0; i< size; i++){
            data.lred[i] = r[i];
            data.lgreen[i] = g[i];
            data.lblue[i] = b[i];
        }
        mBaseParameter->set_cubic_lut_data(mConnector->get_type(), mConnector->connector_id(), &data);
    }
    return ret;
}

static int hw_output_set_brightness(struct hw_output_device* dev, int dpy, int brightness)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    DrmConnector* conn = getValidDrmConnector(priv, dpy);
    char property[PROPERTY_VALUE_MAX];
    char tmp[128];
    std::string propertyStr;

    propertyStr = getPropertySuffix(priv, "persist.vendor.brightness.", dpy);
    sprintf(tmp, "%d", brightness);
    property_get(propertyStr.c_str(), property, "50");

    if (atoi(property) != brightness) {
        property_set(propertyStr.c_str(), tmp);
        updateTimeline();
        mBaseParameter->set_brightness(conn->get_type(), conn->connector_id(), brightness);
    }
    return 0;
}

static int hw_output_set_contrast(struct hw_output_device* dev, int dpy, int contrast)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    DrmConnector* conn = getValidDrmConnector(priv, dpy);
    char property[PROPERTY_VALUE_MAX];
    char tmp[128];
    std::string propertyStr;

    sprintf(tmp, "%d", contrast);
    propertyStr = getPropertySuffix(priv, "persist.vendor.contrast.", dpy);
    property_get(propertyStr.c_str(), property, "50");

    if (atoi(property) != contrast) {
        property_set(propertyStr.c_str(), tmp);
        updateTimeline();
        mBaseParameter->set_contrast(conn->get_type(), conn->connector_id(), contrast);
    }
    return 0;
}

static int hw_output_set_sat(struct hw_output_device* dev, int dpy, int sat)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    DrmConnector* conn = getValidDrmConnector(priv, dpy);
    char property[PROPERTY_VALUE_MAX];
    char tmp[128];
    std::string propertyStr;

    sprintf(tmp, "%d", sat);
    propertyStr = getPropertySuffix(priv, "persist.vendor.saturation.", dpy);
    property_get(propertyStr.c_str(), property, "50");

    if (atoi(property) != sat) {
        property_set(propertyStr.c_str(), tmp);
        updateTimeline();
        mBaseParameter->set_saturation(conn->get_type(), conn->connector_id(), sat);
    }
    return 0;
}

static int hw_output_set_hue(struct hw_output_device* dev, int dpy, int hue)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    DrmConnector* conn = getValidDrmConnector(priv, dpy);
    char property[PROPERTY_VALUE_MAX];
    char tmp[128];
    std::string propertyStr;

    sprintf(tmp, "%d", hue);
    propertyStr = getPropertySuffix(priv, "persist.vendor.hue.", dpy);
    property_get(propertyStr.c_str(), property, "50");

    if (atoi(property) != hue) {
        property_set(propertyStr.c_str(), tmp);
        updateTimeline();
        mBaseParameter->set_hue(conn->get_type(), conn->connector_id(), hue);
    }
    return 0;
}

static int hw_output_set_screen_scale(struct hw_output_device* dev, int dpy, int direction, int value)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    DrmConnector* conn = getValidDrmConnector(priv, dpy);
    std::string propertyStr;
    char property[PROPERTY_VALUE_MAX];
    char overscan[128];
    int left,top,right,bottom;

    propertyStr = getPropertySuffix(priv, "persist.vendor.overscan.", dpy);
    property_get(propertyStr.c_str(), property, "overscan 100,100,100,100");
    sscanf(property, "overscan %d,%d,%d,%d", &left, &top, &right, &bottom);

    if (direction == OVERSCAN_LEFT)
        left = value;
    else if (direction == OVERSCAN_TOP)
        top = value;
    else if (direction == OVERSCAN_RIGHT)
        right = value;
    else if (direction == OVERSCAN_BOTTOM)
        bottom = value;

    sprintf(overscan, "overscan %d,%d,%d,%d", left, top, right, bottom);

    if (strcmp(property, overscan) != 0) {
        property_set(propertyStr.c_str(), overscan);
        updateTimeline();
        struct overscan_info overscan;
        overscan.maxvalue = 100;
        overscan.leftscale = left;
        overscan.topscale = top;
        overscan.rightscale = right;
        overscan.bottomscale = bottom;
        mBaseParameter->set_overscan_info(conn->get_type(), conn->id(), &overscan);
    }

    return 0;
}

static int hw_output_set_hdr_mode(struct hw_output_device* dev, int dpy, int hdr_mode)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    std::string propertyStr, hdrStr;
    char property[PROPERTY_VALUE_MAX];
    char tmp[128];

    sprintf(tmp, "%d", hdr_mode);
    propertyStr = getPropertySuffix(priv, "persist.vendor.hdr_mode.", dpy);
    property_get(propertyStr.c_str(), property, "50");

    if (atoi(property) != hdr_mode) {
        property_set(propertyStr.c_str(), tmp);
        updateTimeline();
    }
    return 0;
}

static int hw_output_set_color_mode(struct hw_output_device* dev, int dpy, const char* color_mode)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    DrmConnector* conn = getValidDrmConnector(priv, dpy);
    std::string propertyStr;
    struct disp_info info;
    char property[PROPERTY_VALUE_MAX];

    propertyStr = getPropertySuffix(priv, "persist.vendor.color.", dpy);
    property_get(propertyStr.c_str(), property, NULL);
    ALOGD("hw_output_set_color_mode %s display %d property=%s", color_mode, dpy, property);

    if (strcmp(color_mode, property) !=0) {
        property_set(propertyStr.c_str(), color_mode);
        property_get(propertyStr.c_str(), property, NULL);
        updateTimeline();
    }
    if (conn) {
        mBaseParameter->get_disp_info(conn->get_type(), conn->connector_id(), &info);
        int slot = findSuitableInfoSlot(&info, conn->get_type(), conn->connector_id());
        if (strncmp(property, "Auto", 4) != 0){
            if (strstr(property, "RGB") != 0)
                info.screen_info[slot].format = output_rgb;
            else if (strstr(property, "YCBCR444") != 0)
                info.screen_info[slot].format = output_ycbcr444;
            else if (strstr(property, "YCBCR422") != 0)
                info.screen_info[slot].format = output_ycbcr422;
            else if (strstr(property, "YCBCR420") != 0)
                info.screen_info[slot].format = output_ycbcr420;
            else {
                info.screen_info[slot].feature |= COLOR_AUTO;
                info.screen_info[slot].format = output_ycbcr_high_subsampling;
            }

            if (strstr(property, "8bit") != NULL)
                info.screen_info[slot].depthc = depth_24bit;
            else if (strstr(property, "10bit") != NULL)
                info.screen_info[slot].depthc = depth_30bit;
            else
                info.screen_info[slot].depthc = Automatic;
        } else {
            info.screen_info[slot].depthc = Automatic;
            info.screen_info[slot].format = output_ycbcr_high_subsampling;
            info.screen_info[slot].feature |= COLOR_AUTO;
        }
        ALOGD("saveConfig: color=%d-%d", info.screen_info[slot].format, info.screen_info[slot].depthc);
        mBaseParameter->set_disp_info(conn->get_type(), conn->connector_id(), &info);
    }
    return 0;
}

static int hw_output_get_cur_mode(struct hw_output_device* dev, int dpy, char* curMode)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    bool found=false;

    if (curMode != NULL)
        found = getResolutionInfo(priv, dpy, curMode);
    else
        return -1;

    if (!found) {
        sprintf(curMode, "%s", "Auto");
    }

    return 0;
}

static int hw_output_get_cur_color_mode(struct hw_output_device* dev, int dpy, char* curColorMode)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    DrmConnector* mCurConnector = getValidDrmConnector(priv, dpy);
    BaseParameter* mBaseParmeter = priv->mBaseParmeter;
    struct disp_info dispInfo;
    std::string propertyStr;
    char colorMode[PROPERTY_VALUE_MAX];

    int len=0;

    propertyStr = getPropertySuffix(priv, "persist.vendor.color.", dpy);
    len = property_get(propertyStr.c_str(), colorMode, NULL);

    ALOGD("nativeGetCurCorlorMode: property=%s", colorMode);
    if (!len && mBaseParmeter && mBaseParmeter->have_baseparameter()) {
        mBaseParmeter->get_disp_info(mCurConnector->get_type(), mCurConnector->connector_id(), &dispInfo);
        int slot = findSuitableInfoSlot(&dispInfo, mCurConnector->get_type(), mCurConnector->connector_id());
        if (dispInfo.screen_info[slot].depthc == Automatic &&
                dispInfo.screen_info[slot].format == output_ycbcr_high_subsampling)
            sprintf(colorMode, "%s", "Auto");
        }

    sprintf(curColorMode, "%s", colorMode);
    ALOGD("nativeGetCurCorlorMode: colorMode=%s", colorMode);
    return 0;
}

static int hw_output_get_num_connectors(struct hw_output_device* dev, int, int* numConnectors)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    (void)priv;

    *numConnectors = mGlobalConns.size();//priv->drm_->connectors().size();
    return 0;
}

static int hw_output_get_connector_state(struct hw_output_device* dev, int dpy, int* state)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    int ret = 0;
    DrmConnector* mConn = getValidDrmConnector(priv, dpy);

    if (mConn != nullptr) {
        *state = mConn->state();
    } else {
        ret = -1;
    }
    return ret;
}

static int hw_output_get_color_configs(struct hw_output_device* dev, int dpy, int* configs)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    DrmConnector* mCurConnector = getValidDrmConnector(priv, dpy);;
    uint64_t color_capacity=0;
    uint64_t depth_capacity=0;

    if (mCurConnector != NULL) {
        if (mCurConnector->hdmi_output_mode_capacity_property().id())
            mCurConnector->hdmi_output_mode_capacity_property().value( &color_capacity);

        if (mCurConnector->hdmi_output_depth_capacity_property().id())
            mCurConnector->hdmi_output_depth_capacity_property().value(&depth_capacity);

        configs[0] = (int)color_capacity;
        configs[1] = (int)depth_capacity;
        ALOGD("nativeGetCorlorModeConfigs: corlor=%d depth=%d configs:%d %d",(int)color_capacity,(int)depth_capacity, configs[0], configs[1]);
    }
    return 0;
}

static int hw_output_get_overscan(struct hw_output_device* dev, int dpy, uint32_t* overscans)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    char property[PROPERTY_VALUE_MAX];
    std::string propertyStr;
    int left,top,right,bottom;

    propertyStr = getPropertySuffix(priv, "persist.vendor.overscan.", dpy);
    property_get(propertyStr.c_str(), property, "overscan 100,100,100,100");

    sscanf(property, "overscan %d,%d,%d,%d", &left, &top, &right, &bottom);
    overscans[0] = left;
    overscans[1] = top;
    overscans[2] = right;
    overscans[3] = bottom;
    return 0;
}

static int hw_output_get_bcsh(struct hw_output_device* dev, int dpy, uint32_t* bcshs)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParmeter = priv->mBaseParmeter;
    DrmConnector* conn = getValidDrmConnector(priv, dpy);
    char mBcshProperty[PROPERTY_VALUE_MAX];
    std::string propertyStr;

    propertyStr = getPropertySuffix(priv, "persist.vendor.brightness.", dpy);
    if (property_get(propertyStr.c_str(), mBcshProperty, NULL) > 0) {
        bcshs[0] = atoi(mBcshProperty);
    } else if (mBaseParmeter&&mBaseParmeter->have_baseparameter()) {
        bcshs[0] = mBaseParmeter->get_brightness(conn->get_type(), conn->connector_id());
    } else {
         bcshs[0] = DEFAULT_BRIGHTNESS;
    }

    memset(mBcshProperty, 0, sizeof(mBcshProperty));
    propertyStr = getPropertySuffix(priv, "persist.vendor.contrast.", dpy);
    if (property_get(propertyStr.c_str(), mBcshProperty, NULL) > 0) {
        bcshs[1] = atoi(mBcshProperty);
    } else if (mBaseParmeter&&mBaseParmeter->have_baseparameter()) {
        bcshs[1] = mBaseParmeter->get_contrast(conn->get_type(), conn->connector_id());
    } else {
         bcshs[1] = DEFAULT_CONTRAST;
    }

    memset(mBcshProperty, 0, sizeof(mBcshProperty));
    propertyStr = getPropertySuffix(priv, "persist.vendor.saturation.", dpy);
    if (property_get(propertyStr.c_str(), mBcshProperty, NULL) > 0) {
        bcshs[2] = atoi(mBcshProperty);
    } else if (mBaseParmeter&&mBaseParmeter->have_baseparameter()) {
        bcshs[2] = mBaseParmeter->get_contrast(conn->get_type(), conn->connector_id());
    } else {
         bcshs[2] = DEFAULT_SATURATION;
    }

    memset(mBcshProperty, 0, sizeof(mBcshProperty));
    propertyStr = getPropertySuffix(priv, "persist.vendor.hue.", dpy);
    if (property_get(propertyStr.c_str(), mBcshProperty, NULL) > 0) {
        bcshs[3] = atoi(mBcshProperty);
    } else if (mBaseParmeter&&mBaseParmeter->have_baseparameter()) {
        bcshs[3] = mBaseParmeter->get_hue(conn->get_type(), conn->connector_id());
    } else {
         bcshs[3] = DEFAULT_SATURATION;
    }

    checkBcshInfo(bcshs);
    ALOGD("Bcsh: %d %d %d %d ", bcshs[0], bcshs[1], bcshs[2], bcshs[3]);
    return 0;
}

static int hw_output_get_builtin(struct hw_output_device* dev, int dpy, int* builtin)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    DrmConnector* mConnector = getValidDrmConnector(priv, dpy);
    if (mConnector) {
        *builtin = mConnector->get_type();
    } else {
        *builtin = 0;
    }
    return 0;
}

static drm_mode_t* hw_output_get_display_modes(struct hw_output_device* dev, int dpy, uint32_t* size)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    std::vector<DrmMode> mModes;
    DrmConnector* mCurConnector;
    drm_mode_t* drm_modes = NULL;
    int idx=0;

    *size = 0;
    mCurConnector = getValidDrmConnector(priv, dpy);
    if (mCurConnector) {
        mModes = mCurConnector->modes();
    } else {
        return NULL;
    }

    if (mModes.size() == 0)
        return NULL;

    drm_modes = (drm_mode_t*)malloc(sizeof(drm_mode_t) * mModes.size());

    for (size_t c = 0; c < mModes.size(); ++c) {
        const DrmMode& info = mModes[c];
        float vfresh;

        if (info.flags() & DRM_MODE_FLAG_INTERLACE)
            vfresh = info.clock()*2 / (float)(info.v_total()* info.h_total()) * 1000.0f;
        else
            vfresh = info.clock()/ (float)(info.v_total()* info.h_total()) * 1000.0f;
        drm_modes[c].width = info.h_display();
        drm_modes[c].height = info.v_display();
        drm_modes[c].refreshRate = vfresh;
        drm_modes[c].clock = info.clock();
        drm_modes[c].flags = info.flags();
        drm_modes[c].interlaceFlag = info.flags()&(1<<4);
        drm_modes[c].yuvFlag = (info.flags()&(1<<24) || info.flags()&(1<<23));
        drm_modes[c].connectorId = mCurConnector->id();
        drm_modes[c].mode_type = info.type();
        drm_modes[c].idx = idx;
        drm_modes[c].hsync_start = info.h_sync_start();
        drm_modes[c].hsync_end = info.h_sync_end();
        drm_modes[c].htotal = info.h_total();
        drm_modes[c].hskew = info.h_skew();
        drm_modes[c].vsync_start = info.v_sync_start();
        drm_modes[c].vsync_end = info.v_sync_end();
        drm_modes[c].vtotal = info.v_total();
        drm_modes[c].vscan = info.v_scan();
        idx++;
        ALOGV("display%d mode[%d]  %dx%d fps %f clk %d  h_start %d h_enc %d htotal %d hskew %d",
                dpy,(int)c, info.h_display(), info.v_display(), info.v_refresh(),
                info.clock(),  info.h_sync_start(),info.h_sync_end(),
                info.h_total(), info.h_skew());
        ALOGV("vsync_start %d vsync_end %d vtotal %d vscan %d flags 0x%x",
                info.v_sync_start(), info.v_sync_end(), info.v_total(), info.v_scan(),
                info.flags());
    }
    *size = idx;
    return drm_modes;
}

/*****************************************************************************/
static int hw_output_device_close(struct hw_device_t *dev)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;

    if (priv->mBaseParmeter) {
        delete priv->mBaseParmeter;
    }
    if (priv) {
        free(priv);
    }
    return 0;
}

static connector_info_t* hw_output_get_connector_info(struct hw_output_device* dev, uint32_t* size)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    *size = 0;
    connector_info_t* connector_info = NULL;
    connector_info = (connector_info_t*)malloc(sizeof(connector_info_t) * priv->drm_->connectors().size());
    int i = 0;
    for (auto &conn : mGlobalConns) {
        DrmConnector* mConn = conn.second;
        connector_info[i].type = mConn->get_type();
        connector_info[i].id = (uint32_t)mConn->connector_id();
        connector_info[i].state = (uint32_t)mConn->state();
        i++;
    }
    *size = i;
    ALOGE("%s:%d i=%d", __FUNCTION__, __LINE__, i);
    return connector_info;
}

static int hw_output_update_disp_header(struct hw_output_device *dev)
{
    bool found = false;
    int ret = 0, firstEmptyHeader = -1;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParmeter = priv->mBaseParmeter;
    struct disp_header * headers = (disp_header *)malloc(sizeof(disp_header) * 8);
    for (auto &conn : priv->drm_->connectors()) {
        if(conn->state() == DRM_MODE_CONNECTED){
            found = false;
            firstEmptyHeader = -1;
            mBaseParmeter->get_all_disp_header(headers);
            for(int i = 0; i < 8; i++){
                if(headers[i].connector_type == conn->get_type() && headers[i].connector_id == conn->connector_id()){
                    found = true;
                }
                if(firstEmptyHeader == -1 && headers[i].connector_type == 0 && headers[i].connector_id == 0){
                    firstEmptyHeader = i;
                }
            }
            if(!found){
                ret = mBaseParmeter->set_disp_header(firstEmptyHeader, conn->get_type(), conn->connector_id());
            }
        }
    }
    free(headers);
    return ret;
}

static int hw_output_device_open(const struct hw_module_t* module,
        const char* name, struct hw_device_t** device)
{
    int status = -EINVAL;
    if (!strcmp(name, HW_OUTPUT_DEFAULT_DEVICE)) {
        hw_output_private_t* dev = (hw_output_private_t*)malloc(sizeof(*dev));

        /* initialize our state here */
        //memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = HW_OUTPUT_DEVICE_API_VERSION_0_1;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = hw_output_device_close;

        dev->device.initialize = hw_output_initialize;
        dev->device.setMode = hw_output_set_mode;
        dev->device.set3DMode = hw_output_set_3d_mode;
        dev->device.setBrightness = hw_output_set_brightness;
        dev->device.setContrast = hw_output_set_contrast;
        dev->device.setSat = hw_output_set_sat;
        dev->device.setHue = hw_output_set_hue;
        dev->device.setColorMode = hw_output_set_color_mode;
        dev->device.setHdrMode = hw_output_set_hdr_mode;
        dev->device.setGamma = hw_output_set_gamma;
        dev->device.setScreenScale = hw_output_set_screen_scale;

        dev->device.getCurColorMode = hw_output_get_cur_color_mode;
        dev->device.getBcsh = hw_output_get_bcsh;
        dev->device.getBuiltIn = hw_output_get_builtin;
        dev->device.getColorConfigs = hw_output_get_color_configs;
        dev->device.getConnectorState = hw_output_get_connector_state;
        dev->device.getCurMode = hw_output_get_cur_mode;
        dev->device.getDisplayModes = hw_output_get_display_modes;
        dev->device.getNumConnectors = hw_output_get_num_connectors;
        dev->device.getOverscan = hw_output_get_overscan;

        dev->device.hotplug = hw_output_hotplug_update;
        dev->device.saveConfig = hw_output_save_config;
        dev->device.set3DLut = hw_output_set_3d_lut;
        dev->device.getConnectorInfo = hw_output_get_connector_info;
        dev->device.updateDispHeader = hw_output_update_disp_header;
        *device = &dev->device.common;
        status = 0;
    }
    return status;
}
