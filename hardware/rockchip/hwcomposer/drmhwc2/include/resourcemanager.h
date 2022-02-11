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

#include <string.h>
#include <set>

namespace android {

class ResourceManager {
 public:
  static ResourceManager* getInstance(){
    static ResourceManager drmResourceManager_;
    return &drmResourceManager_;
  }

  int Init();
  DrmDevice *GetDrmDevice(int display);
  std::shared_ptr<Importer> GetImporter(int display);
  DrmConnector *AvailableWritebackConnector(int display);

  const std::vector<std::unique_ptr<DrmDevice>> &getDrmDevices() const {
    return drms_;
  }

  int getDisplayCount() const {
    return num_displays_;
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
  int assignPlaneByPlaneMask(DrmDevice* drm, int active_display_num);
  int assignPlaneByRK3566(DrmDevice* drm, int active_display_num);
  int assignPlaneByHWC(DrmDevice* drm, int active_display_num);

  int getFb0Fd() { return fb0_fd;}
  int getSocId() { return soc_id_;}

 private:
  ResourceManager();
  ResourceManager(const ResourceManager &) = delete;
  ResourceManager &operator=(const ResourceManager &) = delete;
  int AddDrmDevice(std::string path);

  int num_displays_;
  std::set<int> active_display_;
  std::vector<std::unique_ptr<DrmDevice>> drms_;
  std::vector<std::shared_ptr<Importer>> importers_;
  DrmGralloc *drmGralloc_;
  int fb0_fd;
  int soc_id_;
  int drmVersion_;
  bool dynamic_assigin_enable_;
};
}  // namespace android

#endif  // RESOURCEMANAGER_H
