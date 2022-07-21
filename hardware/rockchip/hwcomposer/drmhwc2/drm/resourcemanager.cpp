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
#include "drmlayer.h"

#include <cutils/properties.h>
#include <log/log.h>
#include <sstream>
#include <string>

//XML prase
#include <tinyxml2.h>

namespace android {

ResourceManager::ResourceManager() :
  num_displays_(0) {
  drmGralloc_ = DrmGralloc::getInstance();
}

int ResourceManager::Init(DrmHwcTwo *hwc2) {
  hwc2_ = hwc2;
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

  DrmDevice *drm = drms_.front().get();
  for(auto &crtc : drm->crtcs()){
    mapDrmDisplayCompositor_.insert(
      std::pair<int, std::shared_ptr<DrmDisplayCompositor>>(crtc->id(),std::make_shared<DrmDisplayCompositor>()));
    HWC2_ALOGI("Create DrmDisplayCompositor crtc=%d",crtc->id());
  }

  displays_ = drm->GetDisplays();
  if(displays_.size() == 0){
    ALOGE("Failed to initialize any displays");
    return ret ? -EINVAL : ret;
  }

  hwcPlatform_ = HwcPlatform::CreateInstance(drm);
  if (!hwcPlatform_) {
    ALOGE("Failed to create HwcPlatform instance");
    return -1;
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
  drmGralloc_->set_drm_version(dup(drm->fd()),drmVersion_);

  std::shared_ptr<Importer> importer;
  importer.reset(Importer::CreateInstance(drm.get()));
  if (!importer) {
    ALOGE("Failed to create importer instance");
    return -ENODEV;
  }
  importers_.push_back(std::move(importer));
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
    if (drm->HandlesDisplay(display & ~DRM_CONNECTOR_SPILT_MODE_MASK))
      return drm.get();
  }
  return NULL;
}

std::shared_ptr<Importer> ResourceManager::GetImporter(int display) {
  for (unsigned int i = 0; i < drms_.size(); i++) {
    if (drms_[i]->HandlesDisplay(display & ~DRM_CONNECTOR_SPILT_MODE_MASK))
      return importers_[i];
  }
  return NULL;
}

std::shared_ptr<DrmDisplayCompositor> ResourceManager::GetDrmDisplayCompositor(DrmCrtc* crtc){
  if(!crtc){
    HWC2_ALOGE("crtc is null");
    return NULL;
  }

  if(mapDrmDisplayCompositor_.size() == 0){
    HWC2_ALOGE("mapDrmDisplayCompositor_.size()=0");
    return NULL;
  }

  auto pairDrmDisplayCompositor = mapDrmDisplayCompositor_.find(crtc->id());
  return pairDrmDisplayCompositor->second;
}

int ResourceManager::assignPlaneGroup(){
  uint32_t active_display_num = getActiveDisplayCnt();
  if(active_display_num==0){
    ALOGI_IF(DBG_INFO,"%s,line=%d, active_display_num = %u not to assignPlaneGroup",
                                 __FUNCTION__,__LINE__,active_display_num);
    return -1;
  }

  int ret = hwcPlatform_->TryAssignPlane(drms_.front().get(), active_display_);
  if(ret){
    HWC2_ALOGI("TryAssignPlane fail, ret = %d",ret);
    return ret;
  }
  return 0;
}
}  // namespace android
