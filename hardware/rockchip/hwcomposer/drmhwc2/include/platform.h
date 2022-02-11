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

#ifndef ANDROID_DRM_PLATFORM_H_
#define ANDROID_DRM_PLATFORM_H_

#include "drmdisplaycomposition.h"
#include "drmlayer.h"

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

#include <map>
#include <vector>
#include <stack>

#define UNUSED(x) (void)(x)

namespace android {

class DrmDevice;
class DrmPlanes;

typedef struct tagPlaneGroup{
  bool     bReserved;
  bool     bUse;
  uint32_t zpos;
  uint32_t possible_crtcs;
  uint64_t share_id;
  uint64_t win_type;
	std::vector<DrmPlane*> planes;

  // RK356x support dynamic switching
  uint32_t enable_possible_crtc=0;
  uint32_t disable_possible_crtc=0;
  uint32_t disable_necessary_cnt=0;
  uint32_t current_crtc=0;
  uint32_t current_possible_crtcs=0;

  bool is_release(uint32_t   crtc_mask){
    if(bReserved)
      return false;
    if((crtc_mask & current_crtc) > 0 && !(crtc_mask & current_possible_crtcs)){
      return true;
    }
    return false;
  }
  bool release(uint32_t crtc_mask){
    if(bReserved)
      return false;
    if(!(possible_crtcs & crtc_mask))
      return false;

    if(!(current_crtc & crtc_mask))
      return false;

    enable_possible_crtc = 0;
    disable_possible_crtc = 0;

    return true;
  }

  bool release_necessary_cnt(uint32_t crtc_mask){
    if(bReserved)
      return false;
    if(!(possible_crtcs & crtc_mask))
      return false;

    if(!(current_crtc & crtc_mask))
      return false;

    if(disable_necessary_cnt < 3){
      disable_necessary_cnt++;
      disable_possible_crtc=0;
    }else{
      enable_possible_crtc=0;
      disable_necessary_cnt=0;
    }

    return true;
  }

  bool acquire(uint32_t crtc_mask){
    if(bReserved)
      return false;
    if(possible_crtcs == crtc_mask){
      set_current_possible_crtcs(crtc_mask);
      enable_possible_crtc = crtc_mask;
      disable_possible_crtc = crtc_mask;
      return true;
    }

    if(!(possible_crtcs & crtc_mask))
      return false;

    if(!(current_possible_crtcs & crtc_mask))
      return false;

    if(!disable_possible_crtc && !enable_possible_crtc){
      current_crtc = crtc_mask;
    }

    if(!(current_crtc & crtc_mask))
      return false;

    enable_possible_crtc = crtc_mask;
    disable_possible_crtc = crtc_mask;
    return true;
  }

  bool set_current_possible_crtcs(uint32_t crtc_mask){
    current_possible_crtcs = crtc_mask;
    return true;
  }
}PlaneGroup;

class Importer {
 public:
  virtual ~Importer() {
  }

  // Creates a platform-specific importer instance
  static Importer *CreateInstance(DrmDevice *drm);

  // Imports the buffer referred to by handle into bo.
  //
  // Note: This can be called from a different thread than ReleaseBuffer. The
  //       implementation is responsible for ensuring thread safety.
  virtual int ImportBuffer(buffer_handle_t handle, hwc_drm_bo_t *bo) = 0;

  // Releases the buffer object (ie: does the inverse of ImportBuffer)
  //
  // Note: This can be called from a different thread than ImportBuffer. The
  //       implementation is responsible for ensuring thread safety.
  virtual int ReleaseBuffer(hwc_drm_bo_t *bo) = 0;

  // Checks if importer can import the buffer.
  virtual bool CanImportBuffer(buffer_handle_t handle) = 0;
};

class Planner {
 public:
  class PlanStage {
   public:
    virtual ~PlanStage() {
    }

    virtual bool SupportPlatform(uint32_t soc_id) = 0;

    virtual int TryHwcPolicy(std::vector<DrmCompositionPlane> *composition,
                                std::vector<DrmHwcLayer*> &layers,
                                DrmCrtc *crtc, bool gles_policy) = 0;
    virtual int MatchPlanes(std::vector<DrmCompositionPlane> *composition,
                                std::vector<DrmHwcLayer*> &layers,
                                DrmCrtc *crtc,
                                std::vector<PlaneGroup *> &plane_groups) = 0;
   protected:

    // Inserts the given layer:plane in the composition at the back
    virtual int MatchPlane(std::vector<DrmCompositionPlane> *composition_planes,
                     std::vector<PlaneGroup *> &plane_groups,
                     DrmCompositionPlane::Type type, DrmCrtc *crtc,
                     std::pair<int, std::vector<DrmHwcLayer*>> layers, int zpos, bool match_best) = 0;
  };

  // Creates a planner instance with platform-specific planning stages
  static std::unique_ptr<Planner> CreateInstance(DrmDevice *drm);

  // Takes a stack of layers and provisions hardware planes for them. If the
  // entire stack can't fit in hardware, FIXME
  //
  // @layers: a map of index:layer of layers to composite
  // @primary_planes: a vector of primary planes available for this frame
  // @overlay_planes: a vector of overlay planes available for this frame
  //
  // Returns: A tuple with the status of the operation (0 for success) and
  //          a vector of the resulting plan (ie: layer->plane mapping).
  std::tuple<int, std::vector<DrmCompositionPlane>> TryHwcPolicy(
      std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc, bool gles_policy);

  template <typename T, typename... A>
  void AddStage(A &&... args) {
    stages_.emplace_back(
        std::unique_ptr<PlanStage>(new T(std::forward(args)...)));
  }

 private:

  std::vector<std::unique_ptr<PlanStage>> stages_;
};
}  // namespace android
#endif
