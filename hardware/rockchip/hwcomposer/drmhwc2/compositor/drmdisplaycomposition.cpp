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

#define LOG_TAG "hwc-drm-display-composition"

#include "drmdisplaycomposition.h"
#include "drmcrtc.h"
#include "drmdevice.h"
#include "drmdisplaycompositor.h"
#include "drmplane.h"
#include "platform.h"

#include <stdlib.h>

#include <algorithm>
#include <unordered_set>

#include <log/log.h>
#include <libsync/sw_sync.h>
#include <xf86drmMode.h>
#include <utils/Trace.h>

namespace android {

DrmDisplayComposition::~DrmDisplayComposition() {
  if (timeline_fd_ >= 0) {
    SignalCompositionDone();
    close(timeline_fd_);
  }

}

int DrmDisplayComposition::Init(DrmDevice *drm, DrmCrtc *crtc,
                                Importer *importer, Planner *planner,
                                uint64_t frame_no) {
  drm_ = drm;
  crtc_ = crtc;  // Can be NULL if we haven't modeset yet
  importer_ = importer;
  planner_ = planner;
  frame_no_ = frame_no;


  int ret = sw_sync_timeline_create();
  if (ret < 0) {
    ALOGE("Failed to create sw sync timeline %d", ret);
    return ret;
  }
  timeline_fd_ = ret;

  return 0;
}

bool DrmDisplayComposition::validate_composition_type(DrmCompositionType des) {
  return type_ == DRM_COMPOSITION_TYPE_EMPTY || type_ == des;
}

int DrmDisplayComposition::SetLayers(DrmHwcLayer *layers, size_t num_layers,
                                     bool geometry_changed) {
  if (!validate_composition_type(DRM_COMPOSITION_TYPE_FRAME))
    return -EINVAL;

  layers_.clear();

  geometry_changed_ = geometry_changed;

  for (size_t layer_index = 0; layer_index < num_layers; layer_index++) {
    layers_.emplace_back(std::move(layers[layer_index]));
  }

  //sort
  if(layers_.size() > 1){
    for (auto i = layers_.begin(); i != layers_.end()-1; i++){
      for (auto j = i+1; j != layers_.end(); j++){
        if((*i).iDrmZpos_ > (*j).iDrmZpos_){
            std::swap(*i, *j);
        }
      }
    }
  }
  type_ = DRM_COMPOSITION_TYPE_FRAME;
  return 0;
}

int DrmDisplayComposition::SetDpmsMode(uint32_t dpms_mode) {
  if (!validate_composition_type(DRM_COMPOSITION_TYPE_DPMS))
    return -EINVAL;
  dpms_mode_ = dpms_mode;
  type_ = DRM_COMPOSITION_TYPE_DPMS;
  return 0;
}

int DrmDisplayComposition::SetDisplayMode(const DrmMode &display_mode) {
  if (!validate_composition_type(DRM_COMPOSITION_TYPE_MODESET))
    return -EINVAL;
  display_mode_ = display_mode;
  dpms_mode_ = DRM_MODE_DPMS_ON;
  type_ = DRM_COMPOSITION_TYPE_MODESET;
  return 0;
}

int DrmDisplayComposition::AddPlaneDisable(DrmPlane *plane) {
  composition_planes_.emplace_back(DrmCompositionPlane::Type::kDisable, plane,
                                   crtc_);
  return 0;
}

int DrmDisplayComposition::AddPlaneComposition(DrmCompositionPlane plane) {
  composition_planes_.emplace_back(std::move(plane));
  return 0;
}

int DrmDisplayComposition::DisableUnusedPlanes() {
  if (type_ != DRM_COMPOSITION_TYPE_FRAME)
    return 0;

  std::vector<PlaneGroup*> plane_groups = drm_->GetPlaneGroups();

  int soc_id = crtc()->get_soc_id();

  //loop plane groups.
  for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
     iter != plane_groups.end(); ++iter) {

    // Reserved DrmPlane feature.
    if((*iter)->bReserved)
      continue;

    bool release_plane = false;
    bool disable_plane = false;
    //loop plane
    uint32_t crtc_mask = 1 << crtc()->pipe();
    if((*iter)->is_release(crtc_mask) && (*iter)->release_necessary_cnt(crtc_mask)){
        release_plane = true;
    }else if((*iter)->acquire(crtc_mask)){
        disable_plane = true;
    }

    if(isRK3566(soc_id))
      disable_plane = true;

    if(disable_plane){
        for(std::vector<DrmPlane*> ::const_iterator iter_plane=(*iter)->planes.begin();
              !(*iter)->planes.empty() && iter_plane != (*iter)->planes.end(); ++iter_plane) {
              if (!(*iter_plane)->is_use()) {
                  ALOGD_IF(LogLevel(DBG_DEBUG),"DisableUnusedPlanes plane_groups plane id=%d (%s) %s",
                            (*iter_plane)->id(),(*iter_plane)->name(),
                            release_plane ? "release_necessary_cnt plane" : "");
                  AddPlaneDisable(*iter_plane);
                 // break;
              }
        }
    }

    if(release_plane){
        for(std::vector<DrmPlane*> ::const_iterator iter_plane=(*iter)->planes.begin();
              !(*iter)->planes.empty() && iter_plane != (*iter)->planes.end(); ++iter_plane) {
              ALOGD_IF(LogLevel(DBG_DEBUG),"DisableUnusedPlanes plane_groups plane id=%d (%s) %s",
                        (*iter_plane)->id(),(*iter_plane)->name(),
                        release_plane ? "release_necessary_cnt plane" : "");
              AddPlaneDisable(*iter_plane);
             // break;
        }
    }
  }
  return 0;
}
int DrmDisplayComposition::CreateNextTimelineFence(const char* fence_name) {
  ++timeline_;
  ALOGV("rk-debug CreateNextTimelineFence timeline_fd_ =%d ,timeline_ = %d",timeline_fd_,timeline_);
  return sw_sync_fence_create(timeline_fd_, fence_name,
                                timeline_);
}
int DrmDisplayComposition::IncreaseTimelineToPoint(int point) {
  int timeline_increase = point - timeline_current_;
  if (timeline_increase <= 0)
    return 0;
  ALOGV("rk-debug IncreaseTimelineToPoint timeline_fd_ =%d ,point = %d ,timeline_current_ = %d ,timeline_increase = %d",timeline_fd_,point,timeline_current_,timeline_increase);

  int ret = sw_sync_timeline_inc(timeline_fd_, timeline_increase);
  if (ret)
    ALOGE("Failed to increment sync timeline %d", ret);
  else
    timeline_current_ = point;

  return ret;
}

int DrmDisplayComposition::CreateAndAssignReleaseFences() {
  ATRACE_CALL();

  std::unordered_set<DrmHwcLayer *> comp_layers;

  for (const DrmCompositionPlane &plane : composition_planes_) {
    if (plane.type() == DrmCompositionPlane::Type::kLayer) {
      for (auto i : plane.source_layers()) {
        DrmHwcLayer *source_layer = &layers_[i];
        comp_layers.emplace(source_layer);
      }
    }
  }

  if(comp_layers.empty())
    return 0;

  char acBuf[50];
  for (DrmHwcLayer *layer : comp_layers) {
    if (!layer || !layer->release_fence){
      continue;
    }
    sprintf(acBuf,"frame-%" PRIu64 ,frame_no_);
    int ret = layer->release_fence.Set(CreateNextTimelineFence(acBuf));
    if (ret < 0){
        ALOGE("creat release fence failed ret=%d,%s",ret,strerror(errno));
      return ret;
    }
    //ALOGD("%s,line=%d layerId = %" PRIu32 " frame no = %" PRIu64 " releaseFd = %d" ,__FUNCTION__,__LINE__,
    //      layer->uId_,frame_no_,layer->release_fence.get());
  }
  return 0;
}

static const char *DrmCompositionTypeToString(DrmCompositionType type) {
  switch (type) {
    case DRM_COMPOSITION_TYPE_EMPTY:
      return "EMPTY";
    case DRM_COMPOSITION_TYPE_FRAME:
      return "FRAME";
    case DRM_COMPOSITION_TYPE_DPMS:
      return "DPMS";
    case DRM_COMPOSITION_TYPE_MODESET:
      return "MODESET";
    default:
      return "<invalid>";
  }
}

static const char *DPMSModeToString(int dpms_mode) {
  switch (dpms_mode) {
    case DRM_MODE_DPMS_ON:
      return "ON";
    case DRM_MODE_DPMS_OFF:
      return "OFF";
    default:
      return "<invalid>";
  }
}

static void DumpBuffer(const DrmHwcBuffer &buffer, std::ostringstream *out) {
  if (!buffer) {
    *out << "buffer=<invalid>";
    return;
  }

  *out << "buffer[w/h/format]=";
  *out << buffer->width << "/" << buffer->height << "/" << buffer->format;
}

static void DumpTransform(uint32_t transform, std::ostringstream *out) {
  *out << "[";

  if (transform == 0)
    *out << "IDENTITY";

  bool separator = false;
  if (transform & DrmHwcTransform::kFlipH) {
    *out << "FLIPH";
    separator = true;
  }
  if (transform & DrmHwcTransform::kFlipV) {
    if (separator)
      *out << "|";
    *out << "FLIPV";
    separator = true;
  }
  if (transform & DrmHwcTransform::kRotate90) {
    if (separator)
      *out << "|";
    *out << "ROTATE90";
    separator = true;
  }
  if (transform & DrmHwcTransform::kRotate180) {
    if (separator)
      *out << "|";
    *out << "ROTATE180";
    separator = true;
  }
  if (transform & DrmHwcTransform::kRotate270) {
    if (separator)
      *out << "|";
    *out << "ROTATE270";
    separator = true;
  }

  uint32_t valid_bits = DrmHwcTransform::kFlipH | DrmHwcTransform::kFlipH |
                        DrmHwcTransform::kRotate90 |
                        DrmHwcTransform::kRotate180 |
                        DrmHwcTransform::kRotate270;
  if (transform & ~valid_bits) {
    if (separator)
      *out << "|";
    *out << "INVALID";
  }
  *out << "]";
}

static const char *BlendingToString(DrmHwcBlending blending) {
  switch (blending) {
    case DrmHwcBlending::kNone:
      return "NONE";
    case DrmHwcBlending::kPreMult:
      return "PREMULT";
    case DrmHwcBlending::kCoverage:
      return "COVERAGE";
    default:
      return "<invalid>";
  }
}

void DrmDisplayComposition::Dump(std::ostringstream *out) const {
  *out << "----DrmDisplayComposition"
       << " crtc=" << (crtc_ ? crtc_->id() : -1)
       << " type=" << DrmCompositionTypeToString(type_);

  switch (type_) {
    case DRM_COMPOSITION_TYPE_DPMS:
      *out << " dpms_mode=" << DPMSModeToString(dpms_mode_);
      break;
    case DRM_COMPOSITION_TYPE_MODESET:
      *out << " display_mode=" << display_mode_.h_display() << "x"
           << display_mode_.v_display();
      break;
    default:
      break;
  }

  *out << "    Layers: count=" << layers_.size() << "\n";
  for (size_t i = 0; i < layers_.size(); i++) {
    const DrmHwcLayer &layer = layers_[i];
    *out << "      [" << i << "] ";

    DumpBuffer(layer.buffer, out);

    if (layer.protected_usage())
      *out << " protected";

    *out << " transform=";
    DumpTransform(layer.transform, out);
    *out << " blending[a=" << (int)layer.alpha
         << "]=" << BlendingToString(layer.blending) << "\n";
  }

  *out << "    Planes: count=" << composition_planes_.size() << "\n";
  for (size_t i = 0; i < composition_planes_.size(); i++) {
    const DrmCompositionPlane &comp_plane = composition_planes_[i];
    *out << "      [" << i << "]"
         << " plane=" << (comp_plane.plane() ? comp_plane.plane()->id() : -1)
         << " type=";
    switch (comp_plane.type()) {
      case DrmCompositionPlane::Type::kDisable:
        *out << "DISABLE";
        break;
      case DrmCompositionPlane::Type::kLayer:
        *out << "LAYER";
        break;
      default:
        *out << "<invalid>";
        break;
    }

    *out << " source_layer=";
    for (auto i : comp_plane.source_layers()) {
      *out << i << " ";
    }
    *out << "\n";
  }
}
}  // namespace android
