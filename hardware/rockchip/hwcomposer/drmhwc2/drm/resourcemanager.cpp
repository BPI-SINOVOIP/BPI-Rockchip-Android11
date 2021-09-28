/*
 * Copyright (C) 2018 The Android Open Source Project
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

#define LOG_TAG "hwc-resource-manager"

#include "resourcemanager.h"
#include "drmhwcomposer.h"

#include <cutils/properties.h>
#include <log/log.h>
#include <sstream>
#include <string>


#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

namespace android {

ResourceManager::ResourceManager() :
  num_displays_(0) {
  drmGralloc_ = DrmGralloc::getInstance();
}

int ResourceManager::Init() {
  char path_pattern[PROPERTY_VALUE_MAX];
  // Could be a valid path or it can have at the end of it the wildcard %
  // which means that it will try open all devices until an error is met.
  int path_len = property_get("vendor.hwc.drm.device", path_pattern, "/dev/dri/card0");
  int ret = 0;
  if (path_pattern[path_len - 1] != '%') {
    ret = AddDrmDevice(std::string(path_pattern));
  } else {
    path_pattern[path_len - 1] = '\0';
    for (int idx = 0; !ret; ++idx) {
      std::ostringstream path;
      path << path_pattern << idx;
      ret = AddDrmDevice(path.str());
    }
  }

  if (!num_displays_) {
    ALOGE("Failed to initialize any displays");
    return ret ? -EINVAL : ret;
  }

  fb0_fd = open("/dev/graphics/fb0", O_RDWR, 0);
  if(fb0_fd < 0){
    ALOGE("Open fb0 fail in %s",__FUNCTION__);
  }
  return 0;
}

int ResourceManager::AddDrmDevice(std::string path) {
  std::unique_ptr<DrmDevice> drm = std::make_unique<DrmDevice>();
  int displays_added, ret;
  std::tie(ret, displays_added) = drm->Init(path.c_str(), num_displays_);
  if (ret)
    return ret;

  //Get soc id
  soc_id_ = drm->getSocId();
  //DrmVersion
  drmVersion_ = drm->getDrmVersion();
  drmGralloc_->set_drm_version(drmVersion_);

  std::shared_ptr<Importer> importer;
  importer.reset(Importer::CreateInstance(drm.get()));
  if (!importer) {
    ALOGE("Failed to create importer instance");
    return -ENODEV;
  }
  importers_.push_back(importer);
  drms_.push_back(std::move(drm));
  num_displays_ += displays_added;
  return ret;
}

DrmConnector *ResourceManager::AvailableWritebackConnector(int display) {
  DrmDevice *drm_device = GetDrmDevice(display);
  DrmConnector *writeback_conn = NULL;
  if (drm_device) {
    writeback_conn = drm_device->AvailableWritebackConnector(display);
    if (writeback_conn)
      return writeback_conn;
  }
  for (auto &drm : drms_) {
    if (drm.get() == drm_device)
      continue;
    writeback_conn = drm->AvailableWritebackConnector(display);
    if (writeback_conn)
      return writeback_conn;
  }
  return writeback_conn;
}

DrmDevice *ResourceManager::GetDrmDevice(int display) {
  for (auto &drm : drms_) {
    if (drm->HandlesDisplay(display))
      return drm.get();
  }
  return NULL;
}

std::shared_ptr<Importer> ResourceManager::GetImporter(int display) {
  for (unsigned int i = 0; i < drms_.size(); i++) {
    if (drms_[i]->HandlesDisplay(display))
      return importers_[i];
  }
  return NULL;
}

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

struct planes_mask_name {
  uint64_t mask;
  const char *name;
};

struct planes_mask_name planes_mask_names[] = {
  { DRM_PLANE_TYPE_CLUSTER0_WIN0 | DRM_PLANE_TYPE_CLUSTER1_WIN0, "Cluster-win0" },
  { DRM_PLANE_TYPE_CLUSTER0_WIN1 | DRM_PLANE_TYPE_CLUSTER1_WIN1, "Cluster-win1" },
  { DRM_PLANE_TYPE_ESMART0_MASK  | DRM_PLANE_TYPE_ESMART1_MASK, "Esmart" },
  { DRM_PLANE_TYPE_SMART0_MASK   | DRM_PLANE_TYPE_SMART1_MASK, "Smart" },
};

int ResourceManager::assignPlaneByPlaneMask(DrmDevice* drm, int active_display_num){
  // all plane mask init
  uint64_t all_unused_plane_mask = 0;
  std::vector<PlaneGroup*> all_plane_group = drm->GetPlaneGroups();
  for(auto &plane_group : all_plane_group){
    if((all_unused_plane_mask & plane_group->win_type) == 0){
      all_unused_plane_mask |= plane_group->win_type;
    }
  }

  // First, assign active display plane_mask
  for(auto &display_id : active_display_){
    DrmCrtc *crtc = drm->GetCrtcForDisplay(display_id);
    if(!crtc){
        ALOGE("%s,line=%d crtc is NULL.",__FUNCTION__,__LINE__);
        return -1;
    }

    uint32_t crtc_mask = 1 << crtc->pipe();
    uint64_t plane_mask = crtc->get_plane_mask();
    ALOGI_IF(DBG_INFO,"%s,line=%d, crtc-id=%d mask=0x%x ,plane_mask=0x%" PRIx64 ,__FUNCTION__,__LINE__,
             crtc->id(),crtc_mask,plane_mask);
    for(auto &plane_group : all_plane_group){
      uint64_t plane_group_win_type = plane_group->win_type;
      if((plane_mask & plane_group_win_type) == plane_group_win_type){
        plane_group->set_current_possible_crtcs(crtc_mask);
        all_unused_plane_mask &= (~plane_group_win_type);
      }
    }
  }

  // Second, assign still unused DrmPlane
  if(active_display_num == 1){
    if(all_unused_plane_mask!=0){
      for(auto &display_id : active_display_){
        DrmCrtc *crtc = drm->GetCrtcForDisplay(display_id);
        if(!crtc){
            ALOGE("%s,line=%d crtc is NULL.",__FUNCTION__,__LINE__);
            return -1;
        }

        if(!dynamic_assigin_enable_ && crtc->get_plane_mask() > 0)
          continue;

        uint32_t crtc_mask = 1 << crtc->pipe();
        for(auto &plane_group : all_plane_group){
          uint64_t plane_group_win_type = plane_group->win_type;
          if((all_unused_plane_mask & plane_group_win_type) > 0){
            plane_group->set_current_possible_crtcs(crtc_mask);
            all_unused_plane_mask &= (~plane_group_win_type);
          }
        }
      }
    }
  }else if(active_display_num == 2){
    for(auto &display_id : active_display_){
      DrmCrtc *crtc = drm->GetCrtcForDisplay(display_id);
      if(!crtc){
          ALOGE("%s,line=%d crtc is NULL.",__FUNCTION__,__LINE__);
          return -1;
      }

      if(!dynamic_assigin_enable_ && crtc->get_plane_mask() > 0)
        continue;

      uint32_t crtc_mask = 1 << crtc->pipe();
      uint64_t plane_mask = crtc->get_plane_mask();
      uint64_t need_plane_mask = 0;
      for(int i = 0 ; i < ARRAY_SIZE(planes_mask_names); i++){
        if((planes_mask_names[i].mask & plane_mask)==0){
          need_plane_mask |= planes_mask_names[i].mask;
        }
      }

      for(auto &plane_group : all_plane_group){
        uint64_t plane_group_win_type = plane_group->win_type;
        if(((all_unused_plane_mask & need_plane_mask) & plane_group_win_type) == plane_group_win_type){
          plane_group->set_current_possible_crtcs(crtc_mask);
          all_unused_plane_mask &= (~plane_group_win_type);

          for(int i = 0 ; i < ARRAY_SIZE(planes_mask_names); i++){
            if((planes_mask_names[i].mask & plane_group_win_type) > 0){
              need_plane_mask &= (~planes_mask_names[i].mask);
            }
          }
        }
      }
    }
  }

  for(auto &plane_group : all_plane_group){
    ALOGI_IF(DBG_INFO,"%s,line=%d, name=%s cur_crtcs_mask=0x%x",__FUNCTION__,__LINE__,
             plane_group->planes[0]->name(),plane_group->current_possible_crtcs);
  }
  return 0;
}

struct assign_plane_group{
	int display_type;
  uint64_t drm_type_mask;
  bool have_assigin;
};

struct assign_plane_group assign_mask_default[] = {
  { HWC_DISPLAY_PRIMARY , DRM_PLANE_TYPE_CLUSTER0_WIN0 | DRM_PLANE_TYPE_CLUSTER0_WIN1 | DRM_PLANE_TYPE_ESMART0_WIN0 | DRM_PLANE_TYPE_SMART0_WIN0, false},
  { HWC_DISPLAY_EXTERNAL, DRM_PLANE_TYPE_CLUSTER1_WIN0 | DRM_PLANE_TYPE_CLUSTER1_WIN1 | DRM_PLANE_TYPE_SMART1_WIN0, false},
  { HWC_DISPLAY_VIRTUAL , DRM_PLANE_TYPE_ESMART1_WIN0, false},
};

struct assign_plane_group assign_mask_rk3566[] = {
  { HWC_DISPLAY_PRIMARY , DRM_PLANE_TYPE_CLUSTER0_WIN0 | DRM_PLANE_TYPE_CLUSTER0_WIN1 | DRM_PLANE_TYPE_ESMART0_WIN0 | DRM_PLANE_TYPE_SMART0_WIN0, false},
  { HWC_DISPLAY_EXTERNAL, DRM_PLANE_TYPE_CLUSTER1_WIN0 | DRM_PLANE_TYPE_CLUSTER1_WIN1 | DRM_PLANE_TYPE_ESMART1_WIN0 | DRM_PLANE_TYPE_SMART1_WIN0, false},
};


int ResourceManager::assignPlaneByRK3566(DrmDevice* drm, int active_display_num){
  // all plane mask init
  uint64_t all_unused_plane_mask = 0;
  std::vector<PlaneGroup*> all_plane_group = drm->GetPlaneGroups();
  for(auto &plane_group : all_plane_group){
    if((all_unused_plane_mask & plane_group->win_type) == 0){
      all_unused_plane_mask |= plane_group->win_type;
    }
  }
  // Reset have_assigin
  for(int i = 0; i < ARRAY_SIZE(assign_mask_rk3566);i++){
      assign_mask_rk3566[i].have_assigin = false;
  }

  // hwc_plane_mask: First, set display hwc_plane_mask
  for(auto &display_id : active_display_){
    DrmCrtc *crtc = drm->GetCrtcForDisplay(display_id);
    if(!crtc){
        ALOGE("%s,line=%d crtc is NULL.",__FUNCTION__,__LINE__);
        return -1;
    }
    uint64_t hwc_plane_mask=0;
    for(int i = 0; i < ARRAY_SIZE(assign_mask_rk3566);i++){
      if(display_id == assign_mask_rk3566[i].display_type){
        hwc_plane_mask = assign_mask_rk3566[i].drm_type_mask;
        assign_mask_rk3566[i].have_assigin = true;
        break;
      }
    }
    if(hwc_plane_mask > 0){
      crtc->set_hwc_plane_mask(hwc_plane_mask);
      ALOGI_IF(DBG_INFO,"%s,line=%d, crtc-id=%d ,hwc_plane_mask=0x%" PRIx64 ,__FUNCTION__,__LINE__,
               crtc->id(),hwc_plane_mask);
    }
  }

  // hwc_plane_mask: Second, set display hwc_plane_mask
  for(auto &display_id : active_display_){
    DrmCrtc *crtc = drm->GetCrtcForDisplay(display_id);
    if(!crtc){
        ALOGE("%s,line=%d crtc is NULL.",__FUNCTION__,__LINE__);
        return -1;
    }
    if(crtc->get_hwc_plane_mask())
      continue;

    uint64_t hwc_plane_mask=0;
    for(int i = 0; i < ARRAY_SIZE(assign_mask_rk3566);i++){
      if(assign_mask_rk3566[i].have_assigin == false){
        hwc_plane_mask = assign_mask_rk3566[i].drm_type_mask;
        assign_mask_rk3566[i].have_assigin = true;
      }
    }
    if(hwc_plane_mask > 0){
      crtc->set_hwc_plane_mask(hwc_plane_mask);
      ALOGI_IF(DBG_INFO,"%s,line=%d, crtc-id=%d ,hwc_plane_mask=0x%" PRIx64 ,__FUNCTION__,__LINE__,
               crtc->id(),hwc_plane_mask);
    }
  }

  // First, assign active display hwc_plane_mask
  for(auto &display_id : active_display_){
    DrmCrtc *crtc = drm->GetCrtcForDisplay(display_id);
    if(!crtc){
        ALOGE("%s,line=%d crtc is NULL.",__FUNCTION__,__LINE__);
        return -1;
    }

    uint32_t crtc_mask = 1 << crtc->pipe();
    uint64_t plane_mask = crtc->get_hwc_plane_mask();
    ALOGI_IF(DBG_INFO,"%s,line=%d, crtc-id=%d mask=0x%x ,plane_mask=0x%" PRIx64 ,__FUNCTION__,__LINE__,
             crtc->id(),crtc_mask,plane_mask);
    for(auto &plane_group : all_plane_group){
      uint64_t plane_group_win_type = plane_group->win_type;
      if((plane_mask & plane_group_win_type) == plane_group_win_type){
        plane_group->set_current_possible_crtcs(crtc_mask);
        all_unused_plane_mask &= (~plane_group_win_type);
      }
    }
  }

  for(auto &plane_group : all_plane_group){
    ALOGI_IF(DBG_INFO,"%s,line=%d, name=%s cur_crtcs_mask=0x%x",__FUNCTION__,__LINE__,
             plane_group->planes[0]->name(),plane_group->current_possible_crtcs);
  }

  return 0;
}


int ResourceManager::assignPlaneByHWC(DrmDevice* drm, int active_display_num){
  // all plane mask init
  uint64_t all_unused_plane_mask = 0;
  std::vector<PlaneGroup*> all_plane_group = drm->GetPlaneGroups();
  for(auto &plane_group : all_plane_group){
    if((all_unused_plane_mask & plane_group->win_type) == 0){
      all_unused_plane_mask |= plane_group->win_type;
    }
  }
  // Reset have_assigin
  for(int i = 0; i < ARRAY_SIZE(assign_mask_default);i++){
      assign_mask_default[i].have_assigin = false;
  }

  // hwc_plane_mask: First, set display hwc_plane_mask
  for(auto &display_id : active_display_){
    DrmCrtc *crtc = drm->GetCrtcForDisplay(display_id);
    if(!crtc){
        ALOGE("%s,line=%d crtc is NULL.",__FUNCTION__,__LINE__);
        return -1;
    }
    uint64_t hwc_plane_mask=0;
    for(int i = 0; i < ARRAY_SIZE(assign_mask_default);i++){
      if(display_id == assign_mask_default[i].display_type){
        hwc_plane_mask = assign_mask_default[i].drm_type_mask;
        assign_mask_default[i].have_assigin = true;
        break;
      }
    }
    if(hwc_plane_mask > 0){
      crtc->set_hwc_plane_mask(hwc_plane_mask);
      ALOGI_IF(DBG_INFO,"%s,line=%d, crtc-id=%d ,hwc_plane_mask=0x%" PRIx64 ,__FUNCTION__,__LINE__,
               crtc->id(),hwc_plane_mask);
    }
  }

  // hwc_plane_mask: Second, set display hwc_plane_mask
  for(auto &display_id : active_display_){
    DrmCrtc *crtc = drm->GetCrtcForDisplay(display_id);
    if(!crtc){
        ALOGE("%s,line=%d crtc is NULL.",__FUNCTION__,__LINE__);
        return -1;
    }
    if(crtc->get_hwc_plane_mask())
      continue;

    uint64_t hwc_plane_mask=0;
    for(int i = 0; i < ARRAY_SIZE(assign_mask_default);i++){
      if(assign_mask_default[i].have_assigin == false){
        hwc_plane_mask = assign_mask_default[i].drm_type_mask;
        assign_mask_default[i].have_assigin = true;
      }
    }
    if(hwc_plane_mask > 0){
      crtc->set_hwc_plane_mask(hwc_plane_mask);
      ALOGI_IF(DBG_INFO,"%s,line=%d, crtc-id=%d ,hwc_plane_mask=0x%" PRIx64 ,__FUNCTION__,__LINE__,
               crtc->id(),hwc_plane_mask);
    }
  }

  // First, assign active display hwc_plane_mask
  for(auto &display_id : active_display_){
    DrmCrtc *crtc = drm->GetCrtcForDisplay(display_id);
    if(!crtc){
        ALOGE("%s,line=%d crtc is NULL.",__FUNCTION__,__LINE__);
        return -1;
    }

    uint32_t crtc_mask = 1 << crtc->pipe();
    uint64_t plane_mask = crtc->get_hwc_plane_mask();
    ALOGI_IF(DBG_INFO,"%s,line=%d, crtc-id=%d mask=0x%x ,plane_mask=0x%" PRIx64 ,__FUNCTION__,__LINE__,
             crtc->id(),crtc_mask,plane_mask);
    for(auto &plane_group : all_plane_group){
      uint64_t plane_group_win_type = plane_group->win_type;
      if((plane_mask & plane_group_win_type) == plane_group_win_type){
        plane_group->set_current_possible_crtcs(crtc_mask);
        all_unused_plane_mask &= (~plane_group_win_type);
      }
    }
  }

  // Second, assign still unused DrmPlane
  if(active_display_num == 1){
    if(all_unused_plane_mask!=0){
      for(auto &display_id : active_display_){
        DrmCrtc *crtc = drm->GetCrtcForDisplay(display_id);
        if(!crtc){
            ALOGE("%s,line=%d crtc is NULL.",__FUNCTION__,__LINE__);
            return -1;
        }

        uint32_t crtc_mask = 1 << crtc->pipe();
        for(auto &plane_group : all_plane_group){
          uint64_t plane_group_win_type = plane_group->win_type;
          if((all_unused_plane_mask & plane_group_win_type) > 0){
            plane_group->set_current_possible_crtcs(crtc_mask);
            all_unused_plane_mask &= (~plane_group_win_type);
          }
        }
      }
    }
  }else if(active_display_num == 2){
    for(auto &display_id : active_display_){
      DrmCrtc *crtc = drm->GetCrtcForDisplay(display_id);
      if(!crtc){
          ALOGE("%s,line=%d crtc is NULL.",__FUNCTION__,__LINE__);
          return -1;
      }

      uint32_t crtc_mask = 1 << crtc->pipe();
      uint64_t plane_mask = crtc->get_hwc_plane_mask();
      uint64_t need_plane_mask = 0;
      for(int i = 0 ; i < ARRAY_SIZE(planes_mask_names); i++){
        if((planes_mask_names[i].mask & plane_mask)==0){
          need_plane_mask |= planes_mask_names[i].mask;
        }
      }

      for(auto &plane_group : all_plane_group){
        uint64_t plane_group_win_type = plane_group->win_type;
        if(((all_unused_plane_mask & need_plane_mask) & plane_group_win_type) == plane_group_win_type){
          plane_group->set_current_possible_crtcs(crtc_mask);
          all_unused_plane_mask &= (~plane_group_win_type);

          for(int i = 0 ; i < ARRAY_SIZE(planes_mask_names); i++){
            if((planes_mask_names[i].mask & plane_group_win_type) > 0){
              need_plane_mask &= (~planes_mask_names[i].mask);
            }
          }
        }
      }
    }
  }

  for(auto &plane_group : all_plane_group){
    ALOGI_IF(DBG_INFO,"%s,line=%d, name=%s cur_crtcs_mask=0x%x",__FUNCTION__,__LINE__,
             plane_group->planes[0]->name(),plane_group->current_possible_crtcs);
  }

  return 0;
}


int ResourceManager::assignPlaneGroup(){

  dynamic_assigin_enable_ = hwc_get_bool_property("vendor.hwc.dynamic_assigin_plane","false");

  uint32_t active_display_num = getActiveDisplayCnt();
  if(active_display_num==0){
    ALOGI_IF(DBG_INFO,"%s,line=%d, active_display_num = %u not to assignPlaneGroup",
                                 __FUNCTION__,__LINE__,active_display_num);
    return -1;
  }

  DrmDevice* drm = NULL;
  bool have_plane_mask = false;
  for(auto &display_id : active_display_){
    drm = GetDrmDevice(display_id);
    DrmCrtc *crtc = drm->GetCrtcForDisplay(display_id);
    if(!crtc){
      ALOGE("%s,line=%d crtc is NULL.",__FUNCTION__,__LINE__);
      continue;
    }

    ALOGI_IF(DBG_INFO,"%s,line=%d, active_display_num = %u, display=%d",
                                 __FUNCTION__,__LINE__,active_display_num,display_id);

    if(crtc->get_plane_mask() > 0){
      have_plane_mask = true;
    }
  }

  if(isRK3566(soc_id_)){
    if(have_plane_mask){
      assignPlaneByPlaneMask(drm, active_display_num);
    }else{
      assignPlaneByRK3566(drm, active_display_num);
    }
  }else{
    if(have_plane_mask){
      assignPlaneByPlaneMask(drm, active_display_num);
    }else{
      assignPlaneByHWC(drm, active_display_num);
    }
  }
  return 0;
}
}  // namespace android
