/*
 * Copyright (C) 2016 The Android Open Source Project
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
#define LOG_TAG "hwc-drm-two"

#include "platform.h"
#include "rockchip/platform/drmhwc356x.h"
#include "drmdevice.h"

#include <log/log.h>

namespace android {

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

void Hwc356x::Init(){
}

bool Hwc356x::SupportPlatform(uint32_t soc_id){
  switch(soc_id){
    case 0x3566:
    case 0x3568:
    // after ECO
    case 0x3566a:
    case 0x3568a:
      return true;
    default:
      break;
  }
  return false;
}

struct assign_plane_group_356x{
	int display_type;
  uint64_t drm_type_mask;
  bool have_assigin;
};
struct assign_plane_group_356x assign_mask_default_356x[] = {
  { -1 , DRM_PLANE_TYPE_CLUSTER0_WIN0 | DRM_PLANE_TYPE_CLUSTER0_WIN1 | DRM_PLANE_TYPE_ESMART0_WIN0 | DRM_PLANE_TYPE_SMART0_WIN0, false},
  { -1 , DRM_PLANE_TYPE_CLUSTER1_WIN0 | DRM_PLANE_TYPE_CLUSTER1_WIN1 | DRM_PLANE_TYPE_SMART1_WIN0, false},
  { -1 , DRM_PLANE_TYPE_ESMART1_WIN0, false},
};

int Hwc356x::assignPlaneByHWC(DrmDevice* drm, const std::set<int> &active_display){
  HWC2_ALOGW("Crtc PlaneMask not set, have to use HwcPlaneMask, please check Crtc::PlaneMask info.");
  std::vector<PlaneGroup*> all_plane_group = drm->GetPlaneGroups();
  for(auto &display_id : active_display){
    DrmCrtc *crtc = drm->GetCrtcForDisplay(display_id);
    if(!crtc){
        ALOGE("%s,line=%d crtc is NULL.",__FUNCTION__,__LINE__);
        return -1;
    }

    uint64_t plane_mask=0;
    for(int i = 0; i < ARRAY_SIZE(assign_mask_default_356x);i++){
      if(display_id == assign_mask_default_356x[i].display_type){
        plane_mask = assign_mask_default_356x[i].drm_type_mask;
        break;
      }
    }

    if(plane_mask == 0){
      for(int i = 0; i < ARRAY_SIZE(assign_mask_default_356x);i++){
        if(assign_mask_default_356x[i].have_assigin == false){
          assign_mask_default_356x[i].display_type = display_id;
          plane_mask = assign_mask_default_356x[i].drm_type_mask;
          assign_mask_default_356x[i].have_assigin = true;
          break;
        }
      }
    }

    uint32_t crtc_mask = 1 << crtc->pipe();
    ALOGI_IF(DBG_INFO,"%s,line=%d, crtc-id=%d mask=0x%x ,plane_mask=0x%" PRIx64 ,__FUNCTION__,__LINE__,
             crtc->id(),crtc_mask,plane_mask);
    for(auto &plane_group : all_plane_group){
      uint64_t plane_group_win_type = plane_group->win_type;
      if((plane_mask & plane_group_win_type) == plane_group_win_type){
        plane_group->set_current_crtc(crtc_mask);
      }
    }
  }

  for(auto &plane_group : all_plane_group){
    ALOGI_IF(DBG_INFO,"%s,line=%d, name=%s cur_crtcs_mask=0x%x",__FUNCTION__,__LINE__,
             plane_group->planes[0]->name(),plane_group->current_crtc_);
  }
  return 0;
}

int Hwc356x::assignPlaneByPlaneMask(DrmDevice* drm, const std::set<int> &active_display){
  std::vector<PlaneGroup*> all_plane_group = drm->GetPlaneGroups();
  // First, assign active display plane_mask
  for(auto &display_id : active_display){
    DrmCrtc *crtc = drm->GetCrtcForDisplay(display_id);
    if(!crtc){
        ALOGE("%s,line=%d crtc is NULL.",__FUNCTION__,__LINE__);
        return -1;
    }

    DrmConnector *conn = drm->GetConnectorForDisplay(display_id);
    if(!conn){
        ALOGE("%s,line=%d connector is NULL.",__FUNCTION__,__LINE__);
        return -1;
    }

    // Connector SplitMode
    if(conn->isHorizontalSpilt()){
      uint32_t crtc_mask = 1 << crtc->pipe();
      uint64_t plane_mask = crtc->get_plane_mask();
      HWC2_ALOGI("SpiltDisplay id=%d crtc-id=%d mask=0x%x ,plane_mask=0x%" PRIx64,
              display_id, crtc->id(), crtc_mask, plane_mask);
      for(auto &plane_group : all_plane_group){
        uint64_t plane_group_win_type = plane_group->win_type;
        if(((plane_mask & plane_group_win_type) == plane_group_win_type)){
          if(display_id < DRM_CONNECTOR_SPILT_MODE_MASK &&
              (plane_group_win_type & DRM_PLANE_TYPE_ALL_CLUSTER_MASK) > 0)
            plane_group->set_current_crtc(crtc_mask, display_id);
          else if(display_id >= DRM_CONNECTOR_SPILT_MODE_MASK &&
                   (plane_group_win_type & DRM_PLANE_TYPE_ALL_ESMART_MASK) > 0)
            plane_group->set_current_crtc(crtc_mask, display_id);
        }
      }
    }else{ // Normal Mode
      uint32_t crtc_mask = 1 << crtc->pipe();
      uint64_t plane_mask = crtc->get_plane_mask();
      HWC2_ALOGI("display-id=%d crtc-id=%d mask=0x%x ,plane_mask=0x%" PRIx64,
              display_id, crtc->id(), crtc_mask, plane_mask);
      for(auto &plane_group : all_plane_group){
        uint64_t plane_group_win_type = plane_group->win_type;
        if(((plane_mask & plane_group_win_type) == plane_group_win_type)){
          plane_group->set_current_crtc(crtc_mask, display_id & 0xf);
        }
      }
    }
  }

  for(auto &plane_group : all_plane_group){
    HWC2_ALOGI("name=%s cur_crtcs_mask=0x%x possible-display=%" PRIi64 ,
            plane_group->planes[0]->name(),plane_group->current_crtc_,plane_group->possible_display_);

  }
  return 0;
}

int Hwc356x::TryAssignPlane(DrmDevice* drm, const std::set<int> &active_display){
  int ret = -1;
  bool exist_plane_mask = false;
  for(auto &display_id : active_display){
    DrmCrtc *crtc = drm->GetCrtcForDisplay(display_id);
    if(!crtc){
      ALOGE("%s,line=%d crtc is NULL.",__FUNCTION__,__LINE__);
      continue;
    }

    ALOGI_IF(DBG_INFO,"%s,line=%d, active_display_num = %zu, display=%d",
                                 __FUNCTION__,__LINE__, active_display.size(),display_id);

    if(crtc->get_plane_mask() > 0){
      exist_plane_mask = true;
    }
  }

  if(exist_plane_mask){
    assignPlaneByPlaneMask(drm, active_display);
  }else{
    assignPlaneByHWC(drm, active_display);
  }

  return ret;
}
}