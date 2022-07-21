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
#include "utils/drmfence.h"
#include "utils/autolock.h"

#include <stdlib.h>

#include <algorithm>
#include <unordered_set>

#include <log/log.h>
#include <libsync/sw_sync.h>
#include <xf86drmMode.h>
#include <utils/Trace.h>

namespace android {

DrmDisplayComposition::~DrmDisplayComposition() {
    SignalCompositionDone();
    pthread_mutex_destroy(&lock_);
}

int DrmDisplayComposition::Init(DrmDevice *drm, DrmCrtc *crtc,
                                Importer *importer, Planner *planner,
                                uint64_t frame_no,uint64_t display_id) {
  drm_ = drm;
  crtc_ = crtc;  // Can be NULL if we haven't modeset yet
  importer_ = importer;
  planner_ = planner;
  frame_no_ = frame_no;
  display_id_ = display_id;

  int ret = pthread_mutex_init(&lock_, NULL);
  if (ret) {
    ALOGE("Failed to initialize drm compositor lock %d\n", ret);
    return ret;
  }

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

int DrmDisplayComposition::AddPlaneComposition(DrmCompositionPlane &&plane) {
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

    bool disable_plane = false;
    //loop plane
    uint32_t crtc_mask = 1 << crtc()->pipe();
    if((*iter)->acquire(crtc_mask,display_id_)){
      disable_plane = true;
    }

    if(isRK3566(soc_id))
      disable_plane = true;

    if(disable_plane){
      for(std::vector<DrmPlane*> ::const_iterator iter_plane=(*iter)->planes.begin();
        !(*iter)->planes.empty() && iter_plane != (*iter)->planes.end(); ++iter_plane) {
        if (!(*iter_plane)->is_use()) {
            ALOGD_IF(LogLevel(DBG_DEBUG),"DisableUnusedPlanes plane_groups plane id=%d (%s)",
                      (*iter_plane)->id(),(*iter_plane)->name());
            AddPlaneDisable(*iter_plane);
            // break;
        }
      }
    }
  }
  return 0;
}

int DrmDisplayComposition::CreateAndAssignReleaseFences(SyncTimeline &sync_timeline) {
  ATRACE_CALL();
  AutoLock lock(&lock_, __func__);
  if (lock.Lock())
    return -1;
  signal_ = false;
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

  char acBuf[32];
  for (DrmHwcLayer *layer : comp_layers) {
    if (!layer){
      continue;
    }
    int sync_timeline_cnt = sync_timeline.IncTimeline();
    sprintf(acBuf,"RFD%" PRIu64 "-FN%" PRIu64 "-TC%d" ,display_id_, frame_no_, sync_timeline_cnt);
    layer->release_fence = sp<ReleaseFence>(new ReleaseFence(sync_timeline, sync_timeline_cnt, acBuf));
    if (layer->release_fence->isValid()){
      HWC2_ALOGD_IF_DEBUG(" Create ReleaseFence(%s) Sucess: frame = %" PRIu64 " LayerName=%s",acBuf, frame_no_, layer->sLayerName_.c_str());
#ifdef USE_LIBSVEP
      if(layer->bUseSvep_){
        layer->pSvepBuffer_->SetReleaseFence(dup(layer->release_fence->getFd()));
        HWC2_ALOGD_IF_DEBUG(" Create SvepReleaseFence(%s) Sucess: frame = %" PRIu64 " LayerName=%s",acBuf, frame_no_, layer->sLayerName_.c_str());
      }
#endif
    }else{
      HWC2_ALOGE(" Create ReleaseFence(%s) Fail!: frame = %" PRIu64 " LayerName=%s",acBuf, frame_no_, layer->sLayerName_.c_str());
      return -1;
    }
  }
  return 0;
}

sp<ReleaseFence> DrmDisplayComposition::GetReleaseFence(hwc2_layer_t layer_id) {
  ATRACE_CALL();
  AutoLock lock(&lock_, __func__);
  if (lock.Lock())
    return ReleaseFence::NO_FENCE;
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

  char acBuf[32];
  for (DrmHwcLayer *layer : comp_layers) {
    if (!layer){
      continue;
    }
    if(layer->uId_ == layer_id){
      return layer->release_fence;
    }
  }
  return ReleaseFence::NO_FENCE;
}

int DrmDisplayComposition::SignalCompositionDone() {
  ATRACE_CALL();
  AutoLock lock(&lock_, __func__);
  if (lock.Lock())
    return -1;

  if(signal_){
    HWC2_ALOGD_IF_VERBOSE("Have been signal frame = %" PRIu64", not to signal.",frame_no_);
    return 0;
  }
  HWC2_ALOGD_IF_DEBUG("Will to signal frame = %" PRIu64,frame_no_);
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

  for (DrmHwcLayer *layer : comp_layers) {
    if (!layer || !layer->release_fence->isValid()){
      continue;
    }
    int act,sig;
    if(LogLevel(DBG_DEBUG)){
      act = layer->release_fence->getActiveCount();
      sig = layer->release_fence->getSignaledCount();
    }
    int ret = layer->release_fence->signal();
    if(LogLevel(DBG_DEBUG))
      HWC2_ALOGD_IF_DEBUG("Signal %s frame = %" PRIu64 " %s Info: size=%d act=%d signal=%d err=%d LayerName=%s ",
                          act == 1 && sig == 0 && layer->release_fence->getActiveCount() == 0 && layer->release_fence->getSignaledCount() == 1 ? "Sucess" : "Fail",
                          frame_no_,layer->release_fence->getName().c_str(),layer->release_fence->getSize(),layer->release_fence->getActiveCount(),
                          layer->release_fence->getSignaledCount(),layer->release_fence->getErrorCount(),
                          layer->sLayerName_.c_str());
  }
  signal_ = true;
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
