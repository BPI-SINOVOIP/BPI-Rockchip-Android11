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

#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

#include "drmdevice.h"
#include "platform.h"
#include "rockchip/drmgralloc.h"
#include "rockchip/drmbaseparameter.h"
#include "drmdisplaycompositor.h"
#include "drmhwctwo.h"

#include <string.h>
#include <set>
#include <map>

namespace android {
class DrmDisplayCompositor;
class DrmHwcTwo;

class ResourceManager {
 public:
  static ResourceManager* getInstance(){
    static ResourceManager drmResourceManager_;
    return &drmResourceManager_;
  }

  int Init(DrmHwcTwo *hwc2);
  DrmDevice *GetDrmDevice(int display);
  std::shared_ptr<Importer> GetImporter(int display);
  DrmConnector *AvailableWritebackConnector(int display);

  const std::vector<std::unique_ptr<DrmDevice>> &GetDrmDevices() const {
    return drms_;
  }

  const std::unique_ptr<HwcPlatform> &GetHwcPlatform() const {
    return hwcPlatform_;
  }

  DrmHwcTwo *GetHwc2() const {
    return hwc2_;
  }

  int getDisplayCount() const {
    return num_displays_;
  }

  std::map<int,int> getDisplays() const {
    return displays_;
  }

  void creatActiveDisplayCnt(int display) {
    if(active_display_.count(display) == 0)
      active_display_.insert(display);
  }
  void removeActiveDisplayCnt(int display) {
    if(active_display_.count(display) > 0)
      active_display_.erase(display);
  }
  uint32_t getActiveDisplayCnt() { return active_display_.size();}

  int assignPlaneGroup();

  int getFb0Fd() { return fb0_fd;}
  int getSocId() { return soc_id_;}
  std::shared_ptr<DrmDisplayCompositor> GetDrmDisplayCompositor(DrmCrtc* crtc);


 private:
  ResourceManager();
  ResourceManager(const ResourceManager &) = delete;
  ResourceManager &operator=(const ResourceManager &) = delete;
  int AddDrmDevice(std::string path);

  int num_displays_;
  std::set<int> active_display_;
  std::vector<std::unique_ptr<DrmDevice>> drms_;
  std::vector<std::shared_ptr<Importer>> importers_;
  std::unique_ptr<HwcPlatform> hwcPlatform_;
  std::map<int, std::shared_ptr<DrmDisplayCompositor>> mapDrmDisplayCompositor_;
  std::map<int,int> displays_;
  DrmGralloc *drmGralloc_;
  DrmHwcTwo *hwc2_;
  int fb0_fd;
  int soc_id_;
  int drmVersion_;
  bool dynamic_assigin_enable_;
};
}  // namespace android

#endif  // RESOURCEMANAGER_H
