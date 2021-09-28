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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#define LOG_TAG "hwc-drm-baseparameter"

#include "rockchip/drmbaseparameter.h"
#include "rockchip/utils/drmdebug.h"
#include "autolock.h"

#include <log/log.h>
#include <utils/String8.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

namespace android {


#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

struct type_name {
  int type;
  const char *name;
};

#define type_name_fn(res) \
  const char * res##_str(int type) {      \
    unsigned int i;         \
    for (i = 0; i < ARRAY_SIZE(res##_names); i++) { \
      if (res##_names[i].type == type)  \
        return res##_names[i].name; \
    }           \
    return "(invalid)";       \
  }

struct type_name base_connector_type_names[] = {
  { DRM_MODE_CONNECTOR_Unknown, "unknown" },
  { DRM_MODE_CONNECTOR_VGA, "VGA" },
  { DRM_MODE_CONNECTOR_DVII, "DVI-I" },
  { DRM_MODE_CONNECTOR_DVID, "DVI-D" },
  { DRM_MODE_CONNECTOR_DVIA, "DVI-A" },
  { DRM_MODE_CONNECTOR_Composite, "composite" },
  { DRM_MODE_CONNECTOR_SVIDEO, "s-video" },
  { DRM_MODE_CONNECTOR_LVDS, "LVDS" },
  { DRM_MODE_CONNECTOR_Component, "component" },
  { DRM_MODE_CONNECTOR_9PinDIN, "9-pin DIN" },
  { DRM_MODE_CONNECTOR_DisplayPort, "DP" },
  { DRM_MODE_CONNECTOR_HDMIA, "HDMI-A" },
  { DRM_MODE_CONNECTOR_HDMIB, "HDMI-B" },
  { DRM_MODE_CONNECTOR_TV, "TV" },
  { DRM_MODE_CONNECTOR_eDP, "eDP" },
  { DRM_MODE_CONNECTOR_VIRTUAL, "Virtual" },
  { DRM_MODE_CONNECTOR_DSI, "DSI" },
  { DRM_MODE_CONNECTOR_DPI, "DPI" },
};

type_name_fn(base_connector_type)


DrmBaseparameter::DrmBaseparameter()
  :api_(baseparameter_api()) {}

DrmBaseparameter::~DrmBaseparameter(){
  pthread_mutex_destroy(&lock_);
}

int DrmBaseparameter::Init(){
  int ret = pthread_mutex_init(&lock_, NULL);
  if (ret) {
    ALOGE("Failed to initialize drm compositor lock %d\n", ret);
    return ret;
  }

  if(api_.validate()){
    ALOGI("DrmBaseparameter validate success!");
  }else{
    ALOGI("DrmBaseparameter validate fail!");
    return -1;
  }

  init_success_ = true;
  return 0;
}

int DrmBaseparameter::UpdateConnectorBaseInfo(unsigned int connector_type,
    unsigned int connector_id, struct disp_info *info){

  AutoLock lock(&lock_, __func__);
  if (lock.Lock())
    return -1;

  // DrmBaseparameter not init success.
  if(!init_success_)
    return -1;

  int ret = api_.get_disp_info(connector_type,connector_id,info);
  if(ret){
      ALOGE("%s,line=%d, get_disp_info fail , ret=%d",__FUNCTION__,__LINE__,ret);
      return ret;
  }

  ALOGI("%s,line=%d, Update success! type=0x%x, id=%d",__FUNCTION__,__LINE__,connector_type,connector_id);

  String8 output;
  // Dump Screen Info
  output.appendFormat("DrmBaseparameter: Connector-Type=%s, Connector-Id=%d\n",base_connector_type_str(connector_type),connector_id);
  output.appendFormat(" ScreenInfo:\n");
  output.appendFormat("     Type=0x%x Id=%d resolution=%dx%d%c%d-%d-%d-%d-%d-%d-%d-%x-%d(clk)\n",
                      info->screen_info[0].type,info->screen_info[0].id,
                      info->screen_info[0].resolution.hdisplay,info->screen_info[0].resolution.vdisplay,
                      (info->screen_info[0].resolution.flags & DRM_MODE_FLAG_INTERLACE) > 0 ? 'c' : 'p',
                      info->screen_info[0].resolution.vrefresh,info->screen_info[0].resolution.hsync_start,
                      info->screen_info[0].resolution.hsync_end,info->screen_info[0].resolution.htotal,
                      info->screen_info[0].resolution.vsync_start,info->screen_info[0].resolution.vsync_end,
                      info->screen_info[0].resolution.vtotal,info->screen_info[0].resolution.flags,
                      info->screen_info[0].resolution.clock);
  output.appendFormat("     Framebuffer Size=%dx%d@%d\n",
                      info->framebuffer_info.framebuffer_width,info->framebuffer_info.framebuffer_height,info->framebuffer_info.fps);
  output.appendFormat("     Output-format=%d output-depth=%d feature=%d\n",
                      info->screen_info[0].format,info->screen_info[0].depthc,info->screen_info[0].feature);
  output.appendFormat("     BCSH: Brightness=%d Contrast=%d Saturation=%d Hue=%d\n",
                      info->bcsh_info.brightness,info->bcsh_info.contrast,
                      info->bcsh_info.saturation,info->bcsh_info.hue);
  output.appendFormat("     GAMMA: Size=%d\n",info->gamma_lut_data.size);
  output.appendFormat("     3DLUT: Size=%d\n",info->cubic_lut_data.size);

  ALOGI("%s",output.string());
  return 0;
}

int DrmBaseparameter::DumpConnectorBaseInfo(unsigned int connector_type,
    unsigned int connector_id, struct disp_info *info){

  AutoLock lock(&lock_, __func__);
  if (lock.Lock())
    return -1;

  // DrmBaseparameter not init success.
  if(!init_success_)
    return -1;

  int ret = api_.get_disp_info(connector_type,connector_id,info);
  if(ret){
      ALOGE("%s,line=%d, get_disp_info fail , ret=%d",__FUNCTION__,__LINE__,ret);
      return ret;
  }

  String8 output;
  // Dump Screen Info
  output.appendFormat("DrmBaseparameter: Connector-Type=%s, Connector-Id=%d\n",base_connector_type_str(connector_type),connector_id);
  output.appendFormat(" ScreenInfo:\n");
  output.appendFormat("     Type=0x%x Id=%d resolution=%dx%d%c%d-%d-%d-%d-%d-%d-%d-%x-%d(clk)\n",
                      info->screen_info[0].type,info->screen_info[0].id,
                      info->screen_info[0].resolution.hdisplay,info->screen_info[0].resolution.vdisplay,
                      (info->screen_info[0].resolution.flags & DRM_MODE_FLAG_INTERLACE) > 0 ? 'c' : 'p',
                      info->screen_info[0].resolution.vrefresh,info->screen_info[0].resolution.hsync_start,
                      info->screen_info[0].resolution.hsync_end,info->screen_info[0].resolution.htotal,
                      info->screen_info[0].resolution.vsync_start,info->screen_info[0].resolution.vsync_end,
                      info->screen_info[0].resolution.vtotal,info->screen_info[0].resolution.flags,
                      info->screen_info[0].resolution.clock);
  output.appendFormat("     Framebuffer Size=%dx%d@%d\n",
                      info->framebuffer_info.framebuffer_width,info->framebuffer_info.framebuffer_height,info->framebuffer_info.fps);
  output.appendFormat("     Output-format=%d output-depth=%d feature=%d\n",
                      info->screen_info[0].format,info->screen_info[0].depthc,info->screen_info[0].feature);
  output.appendFormat("     BCSH: Brightness=%d Contrast=%d Saturation=%d Hue=%d\n",
                      info->bcsh_info.brightness,info->bcsh_info.contrast,
                      info->bcsh_info.saturation,info->bcsh_info.hue);
  output.appendFormat("     GAMMA: Size=%d\n",info->gamma_lut_data.size);
  output.appendFormat("     3DLUT: Size=%d\n",info->cubic_lut_data.size);

  ALOGI("%s",output.string());
  return 0;
}

int DrmBaseparameter::SetScreenInfo(unsigned int connector_type,
                                        unsigned int connector_id,
                                        int index,
                                        struct screen_info *info){
  AutoLock lock(&lock_, __func__);
  if (lock.Lock())
    return -1;

  // DrmBaseparameter not init success.
  if(!init_success_)
    return -1;

  int ret = api_.set_screen_info(connector_type,connector_id,0,info);
  if(ret){
      ALOGE("%s,line=%d, get_disp_info fail , ret=%d",__FUNCTION__,__LINE__,ret);
      return ret;
  }
  String8 output;
  // Dump Screen Info
  output.appendFormat("DrmBaseparameter: Connector-Type=%s, Connector-Id=%d\n",base_connector_type_str(connector_type),connector_id);
  output.appendFormat(" ScreenInfo:\n");
  output.appendFormat("     Type=0x%x Id=%d resolution=%dx%d%c%d-%d-%d-%d-%d-%d-%d-%x-%d(clk)\n",
                      info[index].type,info[index].id,
                      info[index].resolution.hdisplay,info[index].resolution.vdisplay,
                      (info[index].resolution.flags & DRM_MODE_FLAG_INTERLACE) > 0 ? 'c' : 'p',
                      info[index].resolution.vrefresh,info[index].resolution.hsync_start,
                      info[index].resolution.hsync_end,info[index].resolution.htotal,
                      info[index].resolution.vsync_start,info[index].resolution.vsync_end,
                      info[index].resolution.vtotal,info[index].resolution.flags,
                      info[index].resolution.clock);
  ALOGI("%s",output.string());
  return ret;
}
}

