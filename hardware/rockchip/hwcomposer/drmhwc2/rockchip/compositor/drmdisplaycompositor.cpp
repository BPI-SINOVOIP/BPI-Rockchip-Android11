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
#define LOG_TAG "hwc-drm-display-compositor"

#include "drmdisplaycompositor.h"

#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <time.h>
#include <sstream>
#include <vector>

#include "drm/drm_mode.h"
#include <log/log.h>
#include <sync/sync.h>
#include <utils/Trace.h>

// System property
#include <cutils/properties.h>

#include "utils/autolock.h"
#include "drmcrtc.h"
#include "drmdevice.h"
#include "drmplane.h"
#include "rockchip/drmtype.h"
#include "rockchip/utils/drmdebug.h"

#define DRM_DISPLAY_COMPOSITOR_MAX_QUEUE_DEPTH 1

static const uint32_t kWaitWritebackFence = 100;  // ms

#define hwcMIN(x, y)			(((x) <= (y)) ?  (x) :  (y))
#define hwcMAX(x, y)			(((x) >= (y)) ?  (x) :  (y))
#define IS_ALIGN(val,align)    (((val)&(align-1))==0)
#ifndef ALIGN
#define ALIGN( value, base ) (((value) + ((base) - 1)) & ~((base) - 1))
#endif
#define ALIGN_DOWN( value, base)	(value & (~(base-1)) )


namespace android {

class CompositorVsyncCallback : public VsyncCallback {
 public:
  CompositorVsyncCallback(DrmDisplayCompositor *compositor)
      : compositor_(compositor) {
  }

  void Callback(int display, int64_t timestamp) {
    compositor_->Vsync(display, timestamp);
  }

 private:
  DrmDisplayCompositor *compositor_;
};

DrmDisplayCompositor::FrameWorker::FrameWorker(DrmDisplayCompositor *compositor)
    : Worker("frame-worker", HAL_PRIORITY_URGENT_DISPLAY),
      compositor_(compositor) {
}

DrmDisplayCompositor::FrameWorker::~FrameWorker() {
}

int DrmDisplayCompositor::FrameWorker::Init() {
  return InitWorker();
}

void DrmDisplayCompositor::FrameWorker::QueueFrame(
    std::unique_ptr<DrmDisplayComposition> composition, int status) {
  Lock();

  FrameState frame;
  frame.composition = std::move(composition);
  frame.status = status;
  frame_queue_.push(std::move(frame));
  /*
   * Fix a null-point crash bug.
   * --------- beginning of crash
   * Fatal signal 11 (SIGSEGV), code 1 (SEGV_MAPERR), fault addr 0xb8 in tid 6074 (frame-worker), pid 6049 (composer@2.1-se)
   * pid: 6049, tid: 6074, name: frame-worker  >>> /vendor/bin/hw/android.hardware.graphics.composer@2.1-service <<<
   * uid: 1000
   * signal 11 (SIGSEGV), code 1 (SEGV_MAPERR), fault addr 0xb8
   * backtrace:
   *       #00 pc 000000000003230c  /vendor/lib64/hw/hwcomposer.rk30board.so (android::DrmDisplayCompositor::CommitFrame(..))
   *       #01 pc 0000000000030e70  /vendor/lib64/hw/hwcomposer.rk30board.so (android::DrmDisplayCompositor::ApplyFrame(..))
   *       #02 pc 0000000000030d0c  /vendor/lib64/hw/hwcomposer.rk30board.so (android::DrmDisplayCompositor::FrameWorker::Routine()+308)
   */
  Signal();
  Unlock();
}

void DrmDisplayCompositor::FrameWorker::Routine() {
  int wait_ret = 0;

  Lock();
  if (frame_queue_.empty()) {
    wait_ret = WaitForSignalOrExitLocked();
  }

  FrameState frame;
  std::queue<FrameState> frame_queue_temp;
  std::set<int> exist_display;
  exist_display.clear();
  if (!frame_queue_.empty()) {
    while(frame_queue_.size() > 0){
      frame = std::move(frame_queue_.front());
      frame_queue_.pop();
      if(exist_display.count(frame.composition->display())){
        frame_queue_temp.push(std::move(frame));
        continue;
      }
      exist_display.insert(frame.composition->display());
      compositor_->CollectInfo(std::move(frame.composition), frame.status);
    }
    while(frame_queue_temp.size()){
      frame_queue_.push(std::move(frame_queue_temp.front()));
      frame_queue_temp.pop();
    }
  }else{ // frame_queue_ is empty
    ALOGW_IF(LogLevel(DBG_DEBUG),"%s,line=%d frame_queue_ is empty, skip ApplyFrame",__FUNCTION__,__LINE__);
    Unlock();
    return;
  }
  Unlock();

  if (wait_ret == -EINTR) {
    return;
  } else if (wait_ret) {
    ALOGE("Failed to wait for signal, %d", wait_ret);
    return;
  }
  compositor_->Commit();
  compositor_->SyntheticWaitVBlank();
}


DrmDisplayCompositor::DrmDisplayCompositor()
    : resource_manager_(NULL),
      display_(-1),
      worker_(this),
      frame_worker_(this),
      initialized_(false),
      active_(false),
      use_hw_overlays_(true),
      dump_frames_composited_(0),
      dump_last_timestamp_ns_(0),
      flatten_countdown_(FLATTEN_COUNTDOWN_INIT),
      writeback_fence_(-1) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts))
    return;
  dump_last_timestamp_ns_ = ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
}

DrmDisplayCompositor::~DrmDisplayCompositor() {
  if (!initialized_)
    return;

  //vsync_worker_.Exit();
  int ret = pthread_mutex_lock(&lock_);
  if (ret)
    ALOGE("Failed to acquire compositor lock %d", ret);

  worker_.Exit();
  frame_worker_.Exit();

  DrmDevice *drm = resource_manager_->GetDrmDevice(display_);
  if (mode_.blob_id)
    drm->DestroyPropertyBlob(mode_.blob_id);
  if (mode_.old_blob_id)
    drm->DestroyPropertyBlob(mode_.old_blob_id);


  while (!composite_queue_.empty()) {
    composite_queue_.front().reset();
    composite_queue_.pop();
  }

  active_composition_.reset();

  ret = pthread_mutex_unlock(&lock_);
  if (ret)
    ALOGE("Failed to acquire compositor lock %d", ret);

  pthread_mutex_destroy(&lock_);
  pthread_cond_destroy(&composite_queue_cond_);
}

int DrmDisplayCompositor::Init(ResourceManager *resource_manager, int display) {
  if(initialized_)
    return 0;

  resource_manager_ = resource_manager;
  display_ = display;
  DrmDevice *drm = resource_manager_->GetDrmDevice(display);
  if (!drm) {
    ALOGE("Could not find drmdevice for display %d",display);
    return -EINVAL;
  }
  int ret = pthread_mutex_init(&lock_, NULL);
  if (ret) {
    ALOGE("Failed to initialize drm compositor lock %d\n", ret);
    return ret;
  }
  planner_ = Planner::CreateInstance(drm);

  ret = worker_.Init();
  if (ret) {
    pthread_mutex_destroy(&lock_);
    ALOGE("Failed to initialize compositor worker %d\n", ret);
    return ret;
  }
  ret = frame_worker_.Init();
  if (ret) {
    pthread_mutex_destroy(&lock_);
    ALOGE("Failed to initialize frame worker %d\n", ret);
    return ret;
  }

  pthread_cond_init(&composite_queue_cond_, NULL);

//  vsync_worker_.Init(drm, display_);
//  auto callback = std::make_shared<CompositorVsyncCallback>(this);
//  vsync_worker_.RegisterCallback(callback);

  initialized_ = true;
  return 0;
}

std::unique_ptr<DrmDisplayComposition> DrmDisplayCompositor::CreateComposition()
    const {
  return std::unique_ptr<DrmDisplayComposition>(new DrmDisplayComposition());
}

int DrmDisplayCompositor::QueueComposition(
    std::unique_ptr<DrmDisplayComposition> composition) {

  switch (composition->type()) {
    case DRM_COMPOSITION_TYPE_FRAME:
      if (!active_){
        HWC2_ALOGD_IF_INFO("active_=%d skip frame_no=%" PRIu64 , active_, composition->frame_no());
        return -ENODEV;
      }
      break;
    case DRM_COMPOSITION_TYPE_DPMS:
      /*
       * Update the state as soon as we get it so we can start/stop queuing
       * frames asap.
       */
      active_ = (composition->dpms_mode() == DRM_MODE_DPMS_ON);
      return 0;
    case DRM_COMPOSITION_TYPE_MODESET:
      break;
    case DRM_COMPOSITION_TYPE_EMPTY:
      return 0;
    default:
      ALOGE("Unknown composition type %d/%d", composition->type(), display_);
      return -ENOENT;
  }

  if(!initialized_)
    return -EPERM;

  int ret = pthread_mutex_lock(&lock_);
  if (ret) {
    ALOGE("Failed to acquire compositor lock %d", ret);
    return ret;
  }

  // Block the queue if it gets too large. Otherwise, SurfaceFlinger will start
  // to eat our buffer handles when we get about 1 second behind.
  while(composite_queue_.size() >= DRM_DISPLAY_COMPOSITOR_MAX_QUEUE_DEPTH){
    pthread_cond_wait(&composite_queue_cond_,&lock_);
  }

  composite_queue_.push(std::move(composition));
  clear_ = false;

  ret = pthread_mutex_unlock(&lock_);
  if (ret) {
    ALOGE("Failed to release compositor lock %d", ret);
    return ret;
  }
  worker_.Signal();
  return 0;
}


std::unique_ptr<DrmDisplayComposition>
DrmDisplayCompositor::CreateInitializedComposition() const {
  DrmDevice *drm = resource_manager_->GetDrmDevice(display_);
  DrmCrtc *crtc = drm->GetCrtcForDisplay(display_);
  if (!crtc) {
    ALOGE("Failed to find crtc for display = %d", display_);
    return std::unique_ptr<DrmDisplayComposition>();
  }
  std::unique_ptr<DrmDisplayComposition> comp = CreateComposition();
  std::shared_ptr<Importer> importer = resource_manager_->GetImporter(display_);
  if (!importer) {
    ALOGE("Failed to find resources for display = %d", display_);
    return std::unique_ptr<DrmDisplayComposition>();
  }
  int ret = comp->Init(drm, crtc, importer.get(), planner_.get(), 0, -1);
  if (ret) {
    ALOGE("Failed to init composition for display = %d", display_);
    return std::unique_ptr<DrmDisplayComposition>();
  }
  return comp;
}

std::tuple<uint32_t, uint32_t, int>
DrmDisplayCompositor::GetActiveModeResolution() {
  DrmDevice *drm = resource_manager_->GetDrmDevice(display_);
  DrmConnector *connector = drm->GetConnectorForDisplay(display_);
  if (connector == NULL) {
    ALOGE("Failed to determine display mode: no connector for display %d",
          display_);
    return std::make_tuple(0, 0, -ENODEV);
  }

  const DrmMode &mode = connector->active_mode();
  return std::make_tuple(mode.h_display(), mode.v_display(), 0);
}

int DrmDisplayCompositor::DisablePlanes(DrmDisplayComposition *display_comp) {
  drmModeAtomicReqPtr pset = drmModeAtomicAlloc();
  if (!pset) {
    ALOGE("Failed to allocate property set");
    return -ENOMEM;
  }

  int ret;
  std::vector<DrmCompositionPlane> &comp_planes = display_comp
                                                      ->composition_planes();
  for (DrmCompositionPlane &comp_plane : comp_planes) {
    DrmPlane *plane = comp_plane.plane();
    if(!plane)
      continue;
    ret = drmModeAtomicAddProperty(pset, plane->id(),
                                   plane->crtc_property().id(), 0) < 0 ||
          drmModeAtomicAddProperty(pset, plane->id(), plane->fb_property().id(),
                                   0) < 0;
    if (ret) {
      ALOGE("Failed to add plane %d disable to pset", plane->id());
      drmModeAtomicFree(pset);
      return ret;
    }
  }
  DrmDevice *drm = resource_manager_->GetDrmDevice(display_);
  ret = drmModeAtomicCommit(drm->fd(), pset, 0, drm);
  if (ret) {
    ALOGE("Failed to commit pset ret=%d\n", ret);
    drmModeAtomicFree(pset);
    return ret;
  }

  drmModeAtomicFree(pset);
  return 0;
}

int DrmDisplayCompositor::SetupWritebackCommit(drmModeAtomicReqPtr pset,
                                               uint32_t crtc_id,
                                               DrmConnector *writeback_conn,
                                               DrmHwcBuffer *writeback_buffer) {
  int ret = 0;
  if (writeback_conn->writeback_fb_id().id() == 0 ||
      writeback_conn->writeback_out_fence().id() == 0) {
    ALOGE("Writeback properties don't exit");
    return -EINVAL;
  }
  if ((*writeback_buffer)->fb_id == 0) {
    ALOGE("Invalid writeback buffer");
    return -EINVAL;
  }
  ret = drmModeAtomicAddProperty(pset, writeback_conn->id(),
                                 writeback_conn->writeback_fb_id().id(),
                                 (*writeback_buffer)->fb_id);
  if (ret < 0) {
    ALOGE("Failed to add writeback_fb_id");
    return ret;
  }
  ret = drmModeAtomicAddProperty(pset, writeback_conn->id(),
                                 writeback_conn->writeback_out_fence().id(),
                                 (uint64_t)&writeback_fence_);
  if (ret < 0) {
    ALOGE("Failed to add writeback_out_fence");
    return ret;
  }

  ret = drmModeAtomicAddProperty(pset, writeback_conn->id(),
                                 writeback_conn->crtc_id_property().id(),
                                 crtc_id);
  if (ret < 0) {
    ALOGE("Failed to  attach writeback");
    return ret;
  }
  return 0;
}

int DrmDisplayCompositor::CheckOverscan(drmModeAtomicReqPtr pset, DrmCrtc* crtc, int display, const char *unique_name){
  int ret = 0;
  char overscan_value[PROPERTY_VALUE_MAX]={0};
  char overscan_pro[PROPERTY_VALUE_MAX]={0};
  int left_margin = 100, right_margin= 100, top_margin = 100, bottom_margin = 100;

  snprintf(overscan_pro,PROPERTY_VALUE_MAX,"persist.vendor.overscan.%s",unique_name);
  ret = property_get(overscan_pro,overscan_value,"");
  if(!ret){
    if(display == HWC_DISPLAY_PRIMARY){
      property_get("persist.vendor.overscan.main", overscan_value, "overscan 100,100,100,100");
    }else{
      property_get("persist.vendor.overscan.aux", overscan_value, "overscan 100,100,100,100");
    }
  }

  sscanf(overscan_value, "overscan %d,%d,%d,%d", &left_margin, &top_margin,
           &right_margin, &bottom_margin);
  ALOGD_IF(LogLevel(DBG_DEBUG),"display=%d , overscan(%d,%d,%d,%d)",display,
            left_margin,top_margin,right_margin,bottom_margin);

  if (left_margin   < OVERSCAN_MIN_VALUE) left_margin   = OVERSCAN_MIN_VALUE;
  if (top_margin    < OVERSCAN_MIN_VALUE) top_margin    = OVERSCAN_MIN_VALUE;
  if (right_margin  < OVERSCAN_MIN_VALUE) right_margin  = OVERSCAN_MIN_VALUE;
  if (bottom_margin < OVERSCAN_MIN_VALUE) bottom_margin = OVERSCAN_MIN_VALUE;

  if (left_margin   > OVERSCAN_MAX_VALUE) left_margin   = OVERSCAN_MAX_VALUE;
  if (top_margin    > OVERSCAN_MAX_VALUE) top_margin    = OVERSCAN_MAX_VALUE;
  if (right_margin  > OVERSCAN_MAX_VALUE) right_margin  = OVERSCAN_MAX_VALUE;
  if (bottom_margin > OVERSCAN_MAX_VALUE) bottom_margin = OVERSCAN_MAX_VALUE;

  ret = drmModeAtomicAddProperty(pset, crtc->id(), crtc->left_margin_property().id(), left_margin) < 0 ||
        drmModeAtomicAddProperty(pset, crtc->id(), crtc->right_margin_property().id(), right_margin) < 0 ||
        drmModeAtomicAddProperty(pset, crtc->id(), crtc->top_margin_property().id(), top_margin) < 0 ||
        drmModeAtomicAddProperty(pset, crtc->id(), crtc->bottom_margin_property().id(), bottom_margin) < 0;
  if (ret) {
    ALOGE("Failed to add overscan to pset");
    return ret;
  }

  return ret;
}

static const int64_t kOneSecondNs = 1 * 1000 * 1000 * 1000;
int DrmDisplayCompositor::GetTimestamp() {
  struct timespec current_time;
  int ret = clock_gettime(CLOCK_MONOTONIC, &current_time);
  last_timestamp_ = current_time.tv_sec * kOneSecondNs + current_time.tv_nsec;
  return 0;
}

/*
 * Returns the timestamp of the next vsync in phase with last_timestamp_.
 * For example:
 *  last_timestamp_ = 137
 *  frame_ns = 50
 *  current = 683
 *
 *  ret = (50 * ((683 - 137)/50 + 1)) + 137
 *  ret = 687
 *
 *  Thus, we must sleep until timestamp 687 to maintain phase with the last
 *  timestamp.
 */
int64_t DrmDisplayCompositor::GetPhasedVSync(int64_t frame_ns, int64_t current) {
  if (last_timestamp_ < 0)
    return current + frame_ns;

  return frame_ns * ((current - last_timestamp_) / frame_ns + 1) +
         last_timestamp_;
}

int DrmDisplayCompositor::SyntheticWaitVBlank() {
  ATRACE_CALL();
  int ret = clock_gettime(CLOCK_MONOTONIC, &vsync_);
  float refresh = 60.0f;  // Default to 60Hz refresh rate
  DrmDevice *drm = resource_manager_->GetDrmDevice(display_);
  DrmConnector *conn = drm->GetConnectorForDisplay(display_);
  if (conn && conn->state() == DRM_MODE_CONNECTED) {
    if (conn->active_mode().v_refresh() > 0.0f)
      refresh = conn->active_mode().v_refresh();
  }

  float percentage = 0.7f; // 30% Remaining Time to the drm driver。
  int64_t phased_timestamp = GetPhasedVSync(kOneSecondNs / refresh * percentage,
                                            vsync_.tv_sec * kOneSecondNs +
                                                vsync_.tv_nsec);
  vsync_.tv_sec = phased_timestamp / kOneSecondNs;
  vsync_.tv_nsec = phased_timestamp - (vsync_.tv_sec * kOneSecondNs);
  do {
    ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &vsync_, NULL);
  } while (ret == -1 && errno == EINTR);
  if (ret)
    return ret;
  return 0;
}

int DrmDisplayCompositor::CommitSidebandStream(drmModeAtomicReqPtr pset,
                                               DrmPlane* plane,
                                               DrmHwcLayer &layer,
                                               int zpos){
  uint16_t eotf       = TRADITIONAL_GAMMA_SDR;
  uint32_t colorspace = V4L2_COLORSPACE_DEFAULT;
  bool     afbcd      = layer.bAfbcd_;
  bool     yuv        = layer.bYuv_;
  uint32_t rotation   = layer.transform;
  bool     sideband   = layer.bSidebandStreamLayer_;
  uint64_t blend      = 0;
  uint64_t alpha      = 0xFFFF;

  int ret = -1;
  if (layer.blending == DrmHwcBlending::kPreMult)
    alpha             = layer.alpha << 8;

  eotf                = layer.uEOTF;
  colorspace          = layer.uColorSpace;

  static char last_prop[100] = {0};
  char prop[100] = {0};
  sprintf(prop, "%d-%d-%d-%d-%d-%d-%d-%d", \
      (int)layer.source_crop.left,\
      (int)layer.source_crop.top,\
      (int)layer.source_crop.right,\
      (int)layer.source_crop.bottom,\
      layer.display_frame.left,\
      layer.display_frame.top,\
      layer.display_frame.right,\
      layer.display_frame.bottom);
  if(strcmp(prop,last_prop)){
    property_set("vendor.hwc.sideband.crop", prop);
    strncpy(last_prop, prop, sizeof(last_prop));
  }

  if (plane->blend_property().id()) {
    switch (layer.blending) {
      case DrmHwcBlending::kPreMult:
        std::tie(blend, ret) = plane->blend_property().GetEnumValueWithName(
            "Pre-multiplied");
        break;
      case DrmHwcBlending::kCoverage:
        std::tie(blend, ret) = plane->blend_property().GetEnumValueWithName(
            "Coverage");
        break;
      case DrmHwcBlending::kNone:
      default:
        std::tie(blend, ret) = plane->blend_property().GetEnumValueWithName(
            "None");
        break;
    }
  }

  ret = drmModeAtomicAddProperty(pset, plane->id(),
                                plane->zpos_property().id(),
                                zpos) < 0;

  if(plane->async_commit_property().id()) {
    ret = drmModeAtomicAddProperty(pset, plane->id(),
                                  plane->async_commit_property().id(),
                                  sideband == true ? 1 : 0) < 0;
    if (ret) {
      ALOGE("Failed to add async_commit_property property %d to plane %d",
            plane->async_commit_property().id(), plane->id());
      return ret;
    }
  }

  if (plane->rotation_property().id()) {
    ret = drmModeAtomicAddProperty(pset, plane->id(),
                                  plane->rotation_property().id(),
                                  rotation) < 0;
    if (ret) {
      ALOGE("Failed to add rotation property %d to plane %d",
            plane->rotation_property().id(), plane->id());
      return ret;
    }
  }

  if (plane->alpha_property().id()) {
    ret = drmModeAtomicAddProperty(pset, plane->id(),
                                  plane->alpha_property().id(),
                                  alpha) < 0;
    if (ret) {
      ALOGE("Failed to add alpha property %d to plane %d",
            plane->alpha_property().id(), plane->id());
      return ret;
    }
  }

  if (plane->blend_property().id()) {
    ret = drmModeAtomicAddProperty(pset, plane->id(),
                                  plane->blend_property().id(),
                                  blend) < 0;
    if (ret) {
      ALOGE("Failed to add pixel blend mode property %d to plane %d",
            plane->blend_property().id(), plane->id());
      return ret;
    }
  }

  if(plane->get_hdr2sdr() && plane->eotf_property().id()) {
    ret = drmModeAtomicAddProperty(pset, plane->id(),
                                  plane->eotf_property().id(),
                                  eotf) < 0;
    if (ret) {
      ALOGE("Failed to add eotf property %d to plane %d",
            plane->eotf_property().id(), plane->id());
      return ret;
    }
  }

  if(plane->colorspace_property().id()) {
    ret = drmModeAtomicAddProperty(pset, plane->id(),
                                  plane->colorspace_property().id(),
                                  colorspace) < 0;
    if (ret) {
      ALOGE("Failed to add colorspace property %d to plane %d",
            plane->colorspace_property().id(), plane->id());
      return ret;
    }
  }

  HWC2_ALOGD_IF_INFO("SidebandStreamLayer zpos=%d not to commit frame.", zpos);
  return 0;
}

int DrmDisplayCompositor::CollectCommitInfo(drmModeAtomicReqPtr pset,
                                            DrmDisplayComposition *display_comp,
                                            bool test_only,
                                            DrmConnector *writeback_conn,
                                            DrmHwcBuffer *writeback_buffer) {
  ATRACE_CALL();

  int ret = 0;

  std::vector<DrmHwcLayer> &layers = display_comp->layers();
  std::vector<DrmCompositionPlane> &comp_planes = display_comp
                                                      ->composition_planes();
  DrmDevice *drm = resource_manager_->GetDrmDevice(display_);
  //uint64_t out_fences[drm->crtcs().size()];

  DrmConnector *connector = drm->GetConnectorForDisplay(display_);
  if (!connector) {
    ALOGE("Could not locate connector for display %d", display_);
    return -ENODEV;
  }
  DrmCrtc *crtc = drm->GetCrtcForDisplay(display_);
  if (!crtc) {
    ALOGE("Could not locate crtc for display %d", display_);
    return -ENODEV;
  }

  if (writeback_buffer != NULL) {
    if (writeback_conn == NULL) {
      ALOGE("Invalid arguments requested writeback without writeback conn");
      return -EINVAL;
    }
    ret = SetupWritebackCommit(pset, crtc->id(), writeback_conn,
                               writeback_buffer);
    if (ret < 0) {
      ALOGE("Failed to Setup Writeback Commit ret = %d", ret);
      return ret;
    }
  }

  if (crtc->can_overscan()) {
    int ret = CheckOverscan(pset,crtc,display_,connector->unique_name());
    if(ret < 0){
      drmModeAtomicFree(pset);
      return ret;
    }
  }

  // RK3566 mirror commit
  bool mirror_commit = false;
  DrmCrtc *mirror_commit_crtc = NULL;
  for (DrmCompositionPlane &comp_plane : comp_planes) {
    if(comp_plane.mirror()){
      mirror_commit = true;
      mirror_commit_crtc = comp_plane.crtc();
      break;
    }
  }
  if(mirror_commit){
    if (mirror_commit_crtc->can_overscan()) {
      int mirror_display_id = mirror_commit_crtc->display();
      DrmConnector *mirror_connector = drm->GetConnectorForDisplay(mirror_display_id);
      if (!mirror_connector) {
        ALOGE("Could not locate connector for display %d", mirror_display_id);
      }
      int ret = CheckOverscan(pset,mirror_commit_crtc,mirror_display_id,mirror_connector->unique_name());
      if(ret < 0){
        drmModeAtomicFree(pset);
        return ret;
      }
    }
  }

  uint64_t zpos = 0;

  for (DrmCompositionPlane &comp_plane : comp_planes) {
    DrmPlane *plane = comp_plane.plane();
    DrmCrtc *crtc = comp_plane.crtc();
    std::vector<size_t> &source_layers = comp_plane.source_layers();

    int fb_id = -1;
    hwc_rect_t display_frame;
    // Commit mirror function
    hwc_rect_t display_frame_mirror;
    hwc_frect_t source_crop;
    uint64_t rotation = 0;
    uint64_t alpha = 0xFFFF;
    uint64_t blend = 0;
    uint16_t eotf = TRADITIONAL_GAMMA_SDR;
    uint32_t colorspace = V4L2_COLORSPACE_DEFAULT;

    int dst_l,dst_t,dst_w,dst_h;
    int src_l,src_t,src_w,src_h;
    bool afbcd = false, yuv = false, sideband = false;;
    if (comp_plane.type() != DrmCompositionPlane::Type::kDisable) {

      if(source_layers.empty()){
        ALOGE("Can't handle empty source layer CompositionPlane.");
        continue;
      }

      if (source_layers.size() > 1) {
        ALOGE("Can't handle more than one source layer sz=%zu type=%d",
              source_layers.size(), comp_plane.type());
        continue;
      }

      if (source_layers.front() >= layers.size()) {
        ALOGE("Source layer index %zu out of bounds %zu type=%d",
              source_layers.front(), layers.size(), comp_plane.type());
        break;
      }

      DrmHwcLayer &layer = layers[source_layers.front()];

      if (!test_only && layer.acquire_fence->isValid()){
        if(layer.acquire_fence->wait(1500)){
          HWC2_ALOGE("Wait AcquireFence failed! frame = %" PRIu64 " Info: size=%d act=%d signal=%d err=%d ,LayerName=%s ",
                            display_comp->frame_no(), layer.acquire_fence->getSize(),
                            layer.acquire_fence->getActiveCount(), layer.acquire_fence->getSignaledCount(),
                            layer.acquire_fence->getErrorCount(),layer.sLayerName_.c_str());
          break;
        }
        layer.acquire_fence->destroy();
      }
      if (!layer.buffer) {
        ALOGE("Expected a valid framebuffer for pset");
        break;
      }
      fb_id = layer.buffer->fb_id;
      display_frame = layer.display_frame;
      display_frame_mirror = layer.display_frame_mirror;
      source_crop = layer.source_crop;
      if (layer.blending == DrmHwcBlending::kPreMult) alpha = layer.alpha << 8;
      eotf = layer.uEOTF;
      colorspace = layer.uColorSpace;
      afbcd = layer.bAfbcd_;
      yuv = layer.bYuv_;

      if (plane->blend_property().id()) {
        switch (layer.blending) {
          case DrmHwcBlending::kPreMult:
            std::tie(blend, ret) = plane->blend_property().GetEnumValueWithName(
                "Pre-multiplied");
            break;
          case DrmHwcBlending::kCoverage:
            std::tie(blend, ret) = plane->blend_property().GetEnumValueWithName(
                "Coverage");
            break;
          case DrmHwcBlending::kNone:
          default:
            std::tie(blend, ret) = plane->blend_property().GetEnumValueWithName(
                "None");
            break;
        }
      }

      zpos = comp_plane.get_zpos();
      if(display_comp->display() > 0xf)
        zpos=1;
      if(zpos < 0)
        ALOGE("The zpos(%" PRIu64 ") is invalid", zpos);

      rotation = layer.transform;

      sideband = layer.bSidebandStreamLayer_;
      if(sideband){
        ret = CommitSidebandStream(pset, plane, layer, zpos);
        if(ret){
          HWC2_ALOGE("CommitSidebandStream fail");
        }
        continue;
      }

    }

    // Disable the plane if there's no framebuffer
    if (fb_id < 0) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->crtc_property().id(), 0) < 0 ||
            drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->fb_property().id(), 0) < 0;
      if (ret) {
        ALOGE("Failed to add plane %d disable to pset", plane->id());
        break;
      }
      continue;
    }
    src_l = (int)source_crop.left;
    src_t = (int)source_crop.top;
    src_w = (int)(source_crop.right - source_crop.left);
    src_h = (int)(source_crop.bottom - source_crop.top);

    // Commit mirror function
    if(comp_plane.mirror()){
      dst_l = display_frame_mirror.left;
      dst_t = display_frame_mirror.top;
      dst_w = display_frame_mirror.right - display_frame_mirror.left;
      dst_h = display_frame_mirror.bottom - display_frame_mirror.top;
    }else{
      dst_l = display_frame.left;
      dst_t = display_frame.top;
      dst_w = display_frame.right - display_frame.left;
      dst_h = display_frame.bottom - display_frame.top;
    }

    if(yuv){
      src_l = ALIGN_DOWN(src_l, 2);
      src_t = ALIGN_DOWN(src_t, 2);
    }


    ret = drmModeAtomicAddProperty(pset, plane->id(),
                                   plane->crtc_property().id(), crtc->id()) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->fb_property().id(), fb_id) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->crtc_x_property().id(),
                                    dst_l) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->crtc_y_property().id(),
                                    dst_t) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->crtc_w_property().id(),
                                    dst_w) <
           0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->crtc_h_property().id(),
                                    dst_h) <
           0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->src_x_property().id(),
                                    (int)(src_l) << 16) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->src_y_property().id(),
                                    (int)(src_t) << 16) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->src_w_property().id(),
                                    (int)(src_w)
                                        << 16) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->src_h_property().id(),
                                    (int)(src_h)
                                        << 16) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                   plane->zpos_property().id(), zpos) < 0;
    if (ret) {
      ALOGE("Failed to add plane %d to set", plane->id());
      break;
    }

    size_t index=0;
    std::ostringstream out_log;

    out_log << "DrmDisplayCompositor[" << index << "]"
            << " frame_no=" << display_comp->frame_no()
            << " display=" << display_comp->display()
            << " plane=" << (plane ? plane->name() : "Unknow")
            << " crct id=" << crtc->id()
            << " fb id=" << fb_id
            << " display_frame[" << dst_l << ","
            << dst_t << "," << dst_w
            << "," << dst_h << "]"
            << " source_crop[" << src_l << ","
            << src_t << "," << src_w
            << "," << src_h << "]"
            << ", zpos=" << zpos
            ;
    index++;

    if (plane->rotation_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->rotation_property().id(),
                                     rotation) < 0;
      if (ret) {
        ALOGE("Failed to add rotation property %d to plane %d",
              plane->rotation_property().id(), plane->id());
        break;
      }
      out_log << " rotation=" << rotation;
    }

    if (plane->alpha_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->alpha_property().id(), alpha) < 0;
      if (ret) {
        ALOGE("Failed to add alpha property %d to plane %d",
              plane->alpha_property().id(), plane->id());
        break;
      }
      out_log << " alpha=" << std::hex <<  alpha;
    }

    if (plane->blend_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->blend_property().id(), blend) < 0;
      if (ret) {
        ALOGE("Failed to add pixel blend mode property %d to plane %d",
              plane->blend_property().id(), plane->id());
        break;
      }
      out_log << " blend mode =" << blend;
    }

    if(plane->get_hdr2sdr() && plane->eotf_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->eotf_property().id(),
                                     eotf) < 0;
      if (ret) {
        ALOGE("Failed to add eotf property %d to plane %d",
              plane->eotf_property().id(), plane->id());
        break;
      }
      out_log << " eotf=" << std::hex <<  eotf;
    }

    if(plane->colorspace_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->colorspace_property().id(),
                                     colorspace) < 0;
      if (ret) {
        ALOGE("Failed to add colorspace property %d to plane %d",
              plane->colorspace_property().id(), plane->id());
        break;
      }
      out_log << " colorspace=" << std::hex <<  colorspace;
    }

    if(plane->async_commit_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->async_commit_property().id(),
                                    sideband == true ? 1 : 0) < 0;
      if (ret) {
        ALOGE("Failed to add async_commit_property property %d to plane %d",
              plane->async_commit_property().id(), plane->id());
        break;
      }

      out_log << " async_commit=" << sideband;
    }

    ALOGD_IF(LogLevel(DBG_INFO),"%s",out_log.str().c_str());
    out_log.clear();
  }
  return ret;
}

void DrmDisplayCompositor::CollectInfo(
    std::unique_ptr<DrmDisplayComposition> composition,
    int status, bool writeback) {
  ATRACE_CALL();

  if(!pset_){
    pset_ = drmModeAtomicAlloc();
    if (!pset_) {
      ALOGE("Failed to allocate property set");
      return ;
    }
  }

  int ret = status;
  if (!ret && !clear_) {
    if (writeback && !CountdownExpired()) {
      ALOGE("Abort playing back scene");
      return;
    }
    ret = CollectCommitInfo(pset_, composition.get(), false);
  }

  if (ret) {
    ALOGE("Composite failed for display %d", display_);
    // Disable the hw used by the last active composition. This allows us to
    // signal the release fences from that composition to avoid hanging.
    ClearDisplay();
    return;
  }
  collect_composition_map_.insert(std::make_pair<int,std::unique_ptr<DrmDisplayComposition>>(composition->display(),std::move(composition)));
}

void DrmDisplayCompositor::Commit() {
  ATRACE_CALL();
  if(!pset_){
    ALOGE("pset_ is NULL");
    return;
  }
  DrmDevice *drm = resource_manager_->GetDrmDevice(display_);
  uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET;
  int ret = drmModeAtomicCommit(drm->fd(), pset_, flags, drm);
  if (ret) {
    ALOGE("Failed to commit pset ret=%d\n", ret);
    drmModeAtomicFree(pset_);
    pset_=NULL;
  }else{
    GetTimestamp();
  }

  if (pset_){
    drmModeAtomicFree(pset_);
    pset_=NULL;
  }

  AutoLock lock(&lock_, __func__);
  if (lock.Lock())
    return;
  ++dump_frames_composited_;
  for(auto &collect_composition : collect_composition_map_){
    auto active_composition = active_composition_map_.find(collect_composition.first);
    if(active_composition != active_composition_map_.end()){
      active_composition->second->SignalCompositionDone();
      active_composition_map_.erase(active_composition);
    }
  }

  for(auto &collect_composition : collect_composition_map_){
    active_composition_map_.insert(std::move(collect_composition));
  }
  collect_composition_map_.clear();
  //flatten_countdown_ = FLATTEN_COUNTDOWN_INIT;
  //vsync_worker_.VSyncControl(!writeback);
}

int DrmDisplayCompositor::CommitFrame(DrmDisplayComposition *display_comp,
                                      bool test_only,
                                      DrmConnector *writeback_conn,
                                      DrmHwcBuffer *writeback_buffer) {
  ATRACE_CALL();

  int ret = 0;
  std::vector<DrmHwcLayer> &layers = display_comp->layers();
  std::vector<DrmCompositionPlane> &comp_planes = display_comp
                                                      ->composition_planes();
  DrmDevice *drm = resource_manager_->GetDrmDevice(display_);
  //uint64_t out_fences[drm->crtcs().size()];

  DrmConnector *connector = drm->GetConnectorForDisplay(display_);
  if (!connector) {
    ALOGE("Could not locate connector for display %d", display_);
    return -ENODEV;
  }
  DrmCrtc *crtc = drm->GetCrtcForDisplay(display_);
  if (!crtc) {
    ALOGE("Could not locate crtc for display %d", display_);
    return -ENODEV;
  }

  drmModeAtomicReqPtr pset = drmModeAtomicAlloc();
  if (!pset) {
    ALOGE("Failed to allocate property set");
    return -ENOMEM;
  }

  if (writeback_buffer != NULL) {
    if (writeback_conn == NULL) {
      ALOGE("Invalid arguments requested writeback without writeback conn");
      return -EINVAL;
    }
    ret = SetupWritebackCommit(pset, crtc->id(), writeback_conn,
                               writeback_buffer);
    if (ret < 0) {
      ALOGE("Failed to Setup Writeback Commit ret = %d", ret);
      return ret;
    }
  }

  if (crtc->can_overscan()) {
    int ret = CheckOverscan(pset,crtc,display_,connector->unique_name());
    if(ret < 0){
      drmModeAtomicFree(pset);
      return ret;
    }
  }

  // RK3566 mirror commit
  bool mirror_commit = false;
  DrmCrtc *mirror_commit_crtc = NULL;
  for (DrmCompositionPlane &comp_plane : comp_planes) {
    if(comp_plane.mirror()){
      mirror_commit = true;
      mirror_commit_crtc = comp_plane.crtc();
      break;
    }
  }
  if(mirror_commit){
    if (mirror_commit_crtc->can_overscan()) {
      int mirror_display_id = mirror_commit_crtc->display();
      DrmConnector *mirror_connector = drm->GetConnectorForDisplay(mirror_display_id);
      if (!mirror_connector) {
        ALOGE("Could not locate connector for display %d", mirror_display_id);
      }
      int ret = CheckOverscan(pset,mirror_commit_crtc,mirror_display_id,mirror_connector->unique_name());
      if(ret < 0){
        drmModeAtomicFree(pset);
        return ret;
      }
    }
  }

  uint64_t zpos = 0;

  for (DrmCompositionPlane &comp_plane : comp_planes) {
    DrmPlane *plane = comp_plane.plane();
    DrmCrtc *crtc = comp_plane.crtc();
    std::vector<size_t> &source_layers = comp_plane.source_layers();

    int fb_id = -1;
    hwc_rect_t display_frame;
    // Commit mirror function
    hwc_rect_t display_frame_mirror;
    hwc_frect_t source_crop;
    uint64_t rotation = 0;
    uint64_t alpha = 0xFFFF;
    uint64_t blend = 0;
    uint16_t eotf = TRADITIONAL_GAMMA_SDR;
    uint32_t colorspace = V4L2_COLORSPACE_DEFAULT;

    int dst_l,dst_t,dst_w,dst_h;
    int src_l,src_t,src_w,src_h;
    bool afbcd = false, yuv = false;
    if (comp_plane.type() != DrmCompositionPlane::Type::kDisable) {

      if(source_layers.empty()){
        ALOGE("Can't handle empty source layer CompositionPlane.");
        continue;
      }

      if (source_layers.size() > 1) {
        ALOGE("Can't handle more than one source layer sz=%zu type=%d",
              source_layers.size(), comp_plane.type());
        continue;
      }

      if (source_layers.front() >= layers.size()) {
        ALOGE("Source layer index %zu out of bounds %zu type=%d",
              source_layers.front(), layers.size(), comp_plane.type());
        break;
      }

      DrmHwcLayer &layer = layers[source_layers.front()];

      if (!test_only && layer.acquire_fence->isValid()){
        if(layer.acquire_fence->wait(1500)){
          HWC2_ALOGE("Wait AcquireFence failed! frame = %" PRIu64 " Info: size=%d act=%d signal=%d err=%d ,LayerName=%s ",
                            display_comp->frame_no(), layer.acquire_fence->getSize(),
                            layer.acquire_fence->getActiveCount(), layer.acquire_fence->getSignaledCount(),
                            layer.acquire_fence->getErrorCount(),layer.sLayerName_.c_str());
          break;
        }
        layer.acquire_fence->destroy();
      }
      if (!layer.buffer) {
        ALOGE("Expected a valid framebuffer for pset");
        break;
      }
      fb_id = layer.buffer->fb_id;
      display_frame = layer.display_frame;
      display_frame_mirror = layer.display_frame_mirror;
      source_crop = layer.source_crop;
      if (layer.blending == DrmHwcBlending::kPreMult) alpha = layer.alpha << 8;
      eotf = layer.uEOTF;
      colorspace = layer.uColorSpace;
      afbcd = layer.bAfbcd_;
      yuv = layer.bYuv_;

      if (plane->blend_property().id()) {
        switch (layer.blending) {
          case DrmHwcBlending::kPreMult:
            std::tie(blend, ret) = plane->blend_property().GetEnumValueWithName(
                "Pre-multiplied");
            break;
          case DrmHwcBlending::kCoverage:
            std::tie(blend, ret) = plane->blend_property().GetEnumValueWithName(
                "Coverage");
            break;
          case DrmHwcBlending::kNone:
          default:
            std::tie(blend, ret) = plane->blend_property().GetEnumValueWithName(
                "None");
            break;
        }
      }
      zpos = comp_plane.get_zpos();
      if(display_comp->display() > 0xf)
        zpos = 1;

      if(zpos < 0)
        ALOGE("The zpos(%" PRIu64 ") is invalid", zpos);

      rotation = layer.transform;
    }

    // Disable the plane if there's no framebuffer
    if (fb_id < 0) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->crtc_property().id(), 0) < 0 ||
            drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->fb_property().id(), 0) < 0;
      if (ret) {
        ALOGE("Failed to add plane %d disable to pset", plane->id());
        break;
      }
      continue;
    }
    src_l = (int)source_crop.left;
    src_t = (int)source_crop.top;
    src_w = (int)(source_crop.right - source_crop.left);
    src_h = (int)(source_crop.bottom - source_crop.top);

    // Commit mirror function
    if(comp_plane.mirror()){
      dst_l = display_frame_mirror.left;
      dst_t = display_frame_mirror.top;
      dst_w = display_frame_mirror.right - display_frame_mirror.left;
      dst_h = display_frame_mirror.bottom - display_frame_mirror.top;
    }else{
      dst_l = display_frame.left;
      dst_t = display_frame.top;
      dst_w = display_frame.right - display_frame.left;
      dst_h = display_frame.bottom - display_frame.top;
    }

    if(yuv){
      src_l = ALIGN_DOWN(src_l, 2);
      src_t = ALIGN_DOWN(src_t, 2);
    }


    ret = drmModeAtomicAddProperty(pset, plane->id(),
                                   plane->crtc_property().id(), crtc->id()) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->fb_property().id(), fb_id) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->crtc_x_property().id(),
                                    dst_l) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->crtc_y_property().id(),
                                    dst_t) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->crtc_w_property().id(),
                                    dst_w) <
           0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->crtc_h_property().id(),
                                    dst_h) <
           0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->src_x_property().id(),
                                    (int)(src_l) << 16) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->src_y_property().id(),
                                    (int)(src_t) << 16) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->src_w_property().id(),
                                    (int)(src_w)
                                        << 16) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->src_h_property().id(),
                                    (int)(src_h)
                                        << 16) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                   plane->zpos_property().id(), zpos) < 0;
    if (ret) {
      ALOGE("Failed to add plane %d to set", plane->id());
      break;
    }

    size_t index=0;
    std::ostringstream out_log;

    out_log << "DrmDisplayCompositor[" << index << "]"
            << " frame_no=" << display_comp->frame_no()
            << " display=" << display_comp->display()
            << " plane=" << (plane ? plane->name() : "Unknow")
            << " crct id=" << crtc->id()
            << " fb id=" << fb_id
            << " display_frame[" << dst_l << ","
            << dst_t << "," << dst_w
            << "," << dst_h << "]"
            << " source_crop[" << src_l << ","
            << src_t << "," << src_w
            << "," << src_h << "]"
            << ", zpos=" << zpos
            ;
    index++;

    if (plane->rotation_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->rotation_property().id(),
                                     rotation) < 0;
      if (ret) {
        ALOGE("Failed to add rotation property %d to plane %d",
              plane->rotation_property().id(), plane->id());
        break;
      }
      out_log << " rotation=" << rotation;
    }

    if (plane->alpha_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->alpha_property().id(), alpha) < 0;
      if (ret) {
        ALOGE("Failed to add alpha property %d to plane %d",
              plane->alpha_property().id(), plane->id());
        break;
      }
      out_log << " alpha=" << std::hex <<  alpha;
    }

    if (plane->blend_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->blend_property().id(), blend) < 0;
      if (ret) {
        ALOGE("Failed to add pixel blend mode property %d to plane %d",
              plane->blend_property().id(), plane->id());
        break;
      }
      out_log << " blend mode =" << blend;
    }

    if(plane->get_hdr2sdr() && plane->eotf_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->eotf_property().id(),
                                     eotf) < 0;
      if (ret) {
        ALOGE("Failed to add eotf property %d to plane %d",
              plane->eotf_property().id(), plane->id());
        break;
      }
      out_log << " eotf=" << std::hex <<  eotf;
    }

    if(plane->colorspace_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->colorspace_property().id(),
                                     colorspace) < 0;
      if (ret) {
        ALOGE("Failed to add colorspace property %d to plane %d",
              plane->colorspace_property().id(), plane->id());
        break;
      }
      out_log << " colorspace=" << std::hex <<  colorspace;
    }

    ALOGD_IF(LogLevel(DBG_INFO),"%s",out_log.str().c_str());
    out_log.clear();
  }

  if (!ret) {
    uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET;
    if (test_only)
      flags |= DRM_MODE_ATOMIC_TEST_ONLY;

    ret = drmModeAtomicCommit(drm->fd(), pset, flags, drm);
    if (ret) {
      if (!test_only)
        ALOGE("Failed to commit pset ret=%d\n", ret);
      drmModeAtomicFree(pset);
      return ret;
    }
  }
  if (pset)
    drmModeAtomicFree(pset);

  return ret;
}

int DrmDisplayCompositor::ApplyDpms(DrmDisplayComposition *display_comp) {
  DrmDevice *drm = resource_manager_->GetDrmDevice(display_);
  DrmConnector *conn = drm->GetConnectorForDisplay(display_);
  if (!conn) {
    ALOGE("Failed to get DrmConnector for display %d", display_);
    return -ENODEV;
  }

  const DrmProperty &prop = conn->dpms_property();
  int ret = drmModeConnectorSetProperty(drm->fd(), conn->id(), prop.id(),
                                        display_comp->dpms_mode());
  if (ret) {
    ALOGE("Failed to set DPMS property for connector %d", conn->id());
    return ret;
  }
  return 0;
}

std::tuple<int, uint32_t> DrmDisplayCompositor::CreateModeBlob(
    const DrmMode &mode) {
  struct drm_mode_modeinfo drm_mode;
  memset(&drm_mode, 0, sizeof(drm_mode));
  mode.ToDrmModeModeInfo(&drm_mode);

  uint32_t id = 0;
  DrmDevice *drm = resource_manager_->GetDrmDevice(display_);
  int ret = drm->CreatePropertyBlob(&drm_mode, sizeof(struct drm_mode_modeinfo),
                                    &id);
  if (ret) {
    ALOGE("Failed to create mode property blob %d", ret);
    return std::make_tuple(ret, 0);
  }
  ALOGE("Create blob_id %" PRIu32 "\n", id);
  return std::make_tuple(ret, id);
}


void DrmDisplayCompositor::SingalCompsition(std::unique_ptr<DrmDisplayComposition> composition) {

  if(!composition)
    return;

  if (DisablePlanes(composition.get()))
    return;

  //wait and close acquire fence.
  std::vector<DrmHwcLayer> &layers = composition->layers();
  std::vector<DrmCompositionPlane> &comp_planes = composition->composition_planes();

  for (DrmCompositionPlane &comp_plane : comp_planes) {
      std::vector<size_t> &source_layers = comp_plane.source_layers();
      if (comp_plane.type() != DrmCompositionPlane::Type::kDisable) {
          if (source_layers.size() > 1) {
              ALOGE("Can't handle more than one source layer sz=%zu type=%d",
              source_layers.size(), comp_plane.type());
              continue;
          }

          if (source_layers.empty() || source_layers.front() >= layers.size()) {
              ALOGE("Source layer index %zu out of bounds %zu type=%d",
              source_layers.front(), layers.size(), comp_plane.type());
              break;
          }
          DrmHwcLayer &layer = layers[source_layers.front()];
          if (layer.acquire_fence->isValid()) {
            if(layer.acquire_fence->wait(1500)){
              ALOGE("Failed to wait for acquire %d 1500ms", layer.acquire_fence->getFd());
              break;
            }
            layer.acquire_fence->destroy();
          }
      }
  }

  composition->SignalCompositionDone();

  composition.reset(NULL);
}

void DrmDisplayCompositor::ClearDisplay() {
  if(!initialized_)
    return;

  AutoLock lock(&lock_, __func__);
  if (lock.Lock())
    return;

  active_composition_map_.clear();

  //Singal the remainder fences in composite queue.
  while(!composite_queue_.empty())
  {
    std::unique_ptr<DrmDisplayComposition> remain_composition(
      std::move(composite_queue_.front()));

    if(remain_composition)
      ALOGD_IF(LogLevel(DBG_DEBUG),"ClearDisplay: composite_queue_ size=%zu frame_no=%" PRIu64 "",composite_queue_.size(), remain_composition->frame_no());

    SingalCompsition(std::move(remain_composition));
    composite_queue_.pop();
    pthread_cond_signal(&composite_queue_cond_);
  }

  clear_ = true;
  //vsync_worker_.VSyncControl(false);
}

void DrmDisplayCompositor::ApplyFrame(
    std::unique_ptr<DrmDisplayComposition> composition, int status,
    bool writeback) {
  ATRACE_CALL();
  int ret = status;

  if (!ret && !clear_) {
    if (writeback && !CountdownExpired()) {
      ALOGE("Abort playing back scene");
      return;
    }
    ret = CommitFrame(composition.get(), false);
  }

  if (ret) {
    ALOGE("Composite failed for display %d", display_);
    // Disable the hw used by the last active composition. This allows us to
    // signal the release fences from that composition to avoid hanging.
    ClearDisplay();
    return;
  }

  AutoLock lock(&lock_, __func__);
  if (lock.Lock())
    return;
  ++dump_frames_composited_;
  if(active_composition_){
      active_composition_->SignalCompositionDone();
  }

  // Enter ClearDisplay state must to SignalCompositionDone
  if(clear_){
    SingalCompsition(std::move(composition));
  }else{
    active_composition_.swap(composition);
  }

  //flatten_countdown_ = FLATTEN_COUNTDOWN_INIT;
  //vsync_worker_.VSyncControl(!writeback);
}
int DrmDisplayCompositor::Composite() {
  ATRACE_CALL();

  int ret = pthread_mutex_lock(&lock_);
  if (ret) {
    ALOGE("Failed to acquire compositor lock %d", ret);
    return ret;
  }
  if (composite_queue_.empty()) {
    ret = pthread_mutex_unlock(&lock_);
    if (ret)
      ALOGE("Failed to release compositor lock %d", ret);
    return ret;
  }

  std::unique_ptr<DrmDisplayComposition> composition(
      std::move(composite_queue_.front()));

  composite_queue_.pop();
  pthread_cond_signal(&composite_queue_cond_);

  ret = pthread_mutex_unlock(&lock_);
  if (ret) {
    ALOGE("Failed to release compositor lock %d", ret);
    return ret;
  }

  switch (composition->type()) {
    case DRM_COMPOSITION_TYPE_FRAME:
#if 0 // Internal process optimization for CPU utilisation
      if (composition->geometry_changed()) {
        // Send the composition to the kernel to ensure we can commit it. This
        // is just a test, it won't actually commit the frame.
        ret = CommitFrame(composition.get(), true);
        if (ret) {
          ALOGE("Commit test failed for display %d, FIXME", display_);
          ClearDisplay();
          return ret;
        }
      }
#endif
      frame_worker_.QueueFrame(std::move(composition), ret);
      break;
    case DRM_COMPOSITION_TYPE_DPMS:
      if(composition.get()->dpms_mode() == DRM_MODE_DPMS_OFF)
          ClearDisplay();
      return 0;
    case DRM_COMPOSITION_TYPE_MODESET:
      return 0;
    default:
      ALOGE("Unknown composition type %d", composition->type());
      return -EINVAL;
  }

  return ret;
}

bool DrmDisplayCompositor::HaveQueuedComposites() const {
  int ret = pthread_mutex_lock(&lock_);
  if (ret) {
    ALOGE("Failed to acquire compositor lock %d", ret);
    return false;
  }

  bool empty_ret = !composite_queue_.empty();

  ret = pthread_mutex_unlock(&lock_);
  if (ret) {
    ALOGE("Failed to release compositor lock %d", ret);
    return false;
  }

  return empty_ret;
}


int DrmDisplayCompositor::TestComposition(DrmDisplayComposition *composition) {
  return CommitFrame(composition, true);
}

// Flatten a scene on the display by using a writeback connector
// and returns the composition result as a DrmHwcLayer.
int DrmDisplayCompositor::FlattenOnDisplay(
    std::unique_ptr<DrmDisplayComposition> &src, DrmConnector *writeback_conn,
    DrmMode &src_mode, DrmHwcLayer *writeback_layer) {
  int ret = 0;
  DrmDevice *drm = resource_manager_->GetDrmDevice(display_);
  ret = writeback_conn->UpdateModes();
  if (ret) {
    ALOGE("Failed to update modes %d", ret);
    return ret;
  }
  for (const DrmMode &mode : writeback_conn->modes()) {
    if (mode.h_display() == src_mode.h_display() &&
        mode.v_display() == src_mode.v_display()) {
      mode_.mode = mode;
      if (mode_.blob_id)
        drm->DestroyPropertyBlob(mode_.blob_id);
      std::tie(ret, mode_.blob_id) = CreateModeBlob(mode_.mode);
      if (ret) {
        ALOGE("Failed to create mode blob for display %d", display_);
        return ret;
      }
      mode_.needs_modeset = true;
      break;
    }
  }
  if (mode_.blob_id <= 0) {
    ALOGE("Failed to find similar mode");
    return -EINVAL;
  }

  DrmCrtc *crtc = drm->GetCrtcForDisplay(display_);
  if (!crtc) {
    ALOGE("Failed to find crtc for display %d", display_);
    return -EINVAL;
  }
  // TODO what happens if planes could go to both CRTCs, I don't think it's
  // handled anywhere
  std::vector<DrmPlane *> primary_planes;
  std::vector<DrmPlane *> overlay_planes;
  for (auto &plane : drm->planes()) {
    if (!plane->GetCrtcSupported(*crtc))
      continue;
    if (plane->type() == DRM_PLANE_TYPE_PRIMARY)
      primary_planes.push_back(plane.get());
    else if (plane->type() == DRM_PLANE_TYPE_OVERLAY)
      overlay_planes.push_back(plane.get());
  }

  ret = src->DisableUnusedPlanes();
  if (ret) {
    ALOGE("Failed to plan the composition ret = %d", ret);
    return ret;
  }

  AutoLock lock(&lock_, __func__);
  ret = lock.Lock();
  if (ret)
    return ret;
  DrmFramebuffer *writeback_fb = &framebuffers_[framebuffer_index_];
  framebuffer_index_ = (framebuffer_index_ + 1) % DRM_DISPLAY_BUFFERS;
  if (!writeback_fb->Allocate(mode_.mode.h_display(), mode_.mode.v_display())) {
    ALOGE("Failed to allocate writeback buffer");
    return -ENOMEM;
  }
  DrmHwcBuffer *writeback_buffer = &writeback_layer->buffer;
  writeback_layer->sf_handle = writeback_fb->buffer()->handle;
  ret = writeback_layer->ImportBuffer(
      resource_manager_->GetImporter(display_).get());
  if (ret) {
    ALOGE("Failed to import writeback buffer");
    return ret;
  }

  ret = CommitFrame(src.get(), true, writeback_conn, writeback_buffer);
  if (ret) {
    ALOGE("Atomic check failed");
    return ret;
  }
  ret = CommitFrame(src.get(), false, writeback_conn, writeback_buffer);
  if (ret) {
    ALOGE("Atomic commit failed");
    return ret;
  }

  ret = sync_wait(writeback_fence_, kWaitWritebackFence);
  writeback_layer->acquire_fence = sp<AcquireFence>(new AcquireFence(writeback_fence_));
  writeback_fence_ = -1;
  if (ret) {
    ALOGE("Failed to wait on writeback fence");
    return ret;
  }
  return 0;
}

// Flatten a scene by enabling the writeback connector attached
// to the same CRTC as the one driving the display.
int DrmDisplayCompositor::FlattenSerial(DrmConnector *writeback_conn) {
  ALOGV("FlattenSerial by enabling writeback connector to the same crtc");
  // Flattened composition with only one layer that is obtained
  // using the writeback connector
  std::unique_ptr<DrmDisplayComposition>
      writeback_comp = CreateInitializedComposition();
  if (!writeback_comp)
    return -EINVAL;

  AutoLock lock(&lock_, __func__);
  int ret = lock.Lock();
  if (ret)
    return ret;
  if (!CountdownExpired() || active_composition_->layers().size() < 2) {
    ALOGV("Flattening is not needed");
    return -EALREADY;
  }

  DrmFramebuffer *writeback_fb = &framebuffers_[framebuffer_index_];
  framebuffer_index_ = (framebuffer_index_ + 1) % DRM_DISPLAY_BUFFERS;
  lock.Unlock();

  if (!writeback_fb->Allocate(mode_.mode.h_display(), mode_.mode.v_display())) {
    ALOGE("Failed to allocate writeback buffer");
    return -ENOMEM;
  }
  writeback_comp->layers().emplace_back();

  DrmHwcLayer &writeback_layer = writeback_comp->layers().back();
  writeback_layer.sf_handle = writeback_fb->buffer()->handle;
  writeback_layer.source_crop = {0, 0, (float)mode_.mode.h_display(),
                                 (float)mode_.mode.v_display()};
  writeback_layer.display_frame = {0, 0, (int)mode_.mode.h_display(),
                                   (int)mode_.mode.v_display()};
  ret = writeback_layer.ImportBuffer(
      resource_manager_->GetImporter(display_).get());
  if (ret || writeback_comp->layers().size() != 1) {
    ALOGE("Failed to import writeback buffer");
    return ret;
  }

  drmModeAtomicReqPtr pset = drmModeAtomicAlloc();
  if (!pset) {
    ALOGE("Failed to allocate property set");
    return -ENOMEM;
  }
  DrmDevice *drm = resource_manager_->GetDrmDevice(display_);
  DrmCrtc *crtc = drm->GetCrtcForDisplay(display_);
  if (!crtc) {
    ALOGE("Failed to find crtc for display %d", display_);
    return -EINVAL;
  }
  ret = SetupWritebackCommit(pset, crtc->id(), writeback_conn,
                             &writeback_layer.buffer);
  if (ret < 0) {
    ALOGE("Failed to Setup Writeback Commit");
    return ret;
  }
  ret = drmModeAtomicCommit(drm->fd(), pset, 0, drm);
  if (ret) {
    ALOGE("Failed to enable writeback %d", ret);
    return ret;
  }
  ret = sync_wait(writeback_fence_, kWaitWritebackFence);
  writeback_layer.acquire_fence = sp<AcquireFence>(new AcquireFence(writeback_fence_));
  writeback_fence_ = -1;
  if (ret) {
    ALOGE("Failed to wait on writeback fence");
    return ret;
  }

  DrmCompositionPlane squashed_comp(DrmCompositionPlane::Type::kLayer, NULL,
                                    crtc);
  for (auto &drmplane : drm->planes()) {
    if (!drmplane->GetCrtcSupported(*crtc))
      continue;
    if (!squashed_comp.plane() && drmplane->type() == DRM_PLANE_TYPE_PRIMARY)
      squashed_comp.set_plane(drmplane.get());
    else
      writeback_comp->AddPlaneDisable(drmplane.get());
  }
  squashed_comp.source_layers().push_back(0);
  ret = writeback_comp->AddPlaneComposition(std::move(squashed_comp));
  if (ret) {
    ALOGE("Failed to add flatten scene");
    return ret;
  }

  ApplyFrame(std::move(writeback_comp), 0, true);
  return 0;
}

// Flatten a scene by using a crtc which works concurrent with
// the one driving the display.
int DrmDisplayCompositor::FlattenConcurrent(DrmConnector *writeback_conn) {
  ALOGV("FlattenConcurrent by using an unused crtc/display");
  int ret = 0;
  DrmDisplayCompositor drmdisplaycompositor;
  ret = drmdisplaycompositor.Init(resource_manager_, writeback_conn->display());
  if (ret) {
    ALOGE("Failed to init  drmdisplaycompositor = %d", ret);
    return ret;
  }
  // Copy of the active_composition, needed because of two things:
  // 1) Not to hold the lock for the whole time we are accessing
  //    active_composition
  // 2) It will be committed on a crtc that might not be on the same
  //     dri node, so buffers need to be imported on the right node.
  std::unique_ptr<DrmDisplayComposition>
      copy_comp = drmdisplaycompositor.CreateInitializedComposition();

  // Writeback composition that will be committed to the display.
  std::unique_ptr<DrmDisplayComposition>
      writeback_comp = CreateInitializedComposition();

  if (!copy_comp || !writeback_comp)
    return -EINVAL;
  AutoLock lock(&lock_, __func__);
  ret = lock.Lock();
  if (ret)
    return ret;
  if (!CountdownExpired() || active_composition_->layers().size() < 2) {
    ALOGV("Flattening is not needed");
    return -EALREADY;
  }
  DrmCrtc *crtc = active_composition_->crtc();

  std::vector<DrmHwcLayer> copy_layers;
  for (DrmHwcLayer &src_layer : active_composition_->layers()) {
    DrmHwcLayer copy;
    ret = copy.InitFromDrmHwcLayer(&src_layer,
                                   resource_manager_
                                       ->GetImporter(writeback_conn->display())
                                       .get());
    if (ret) {
      ALOGE("Failed to import buffer ret = %d", ret);
      return -EINVAL;
    }
    copy_layers.emplace_back(std::move(copy));
  }
  ret = copy_comp->SetLayers(copy_layers.data(), copy_layers.size(), true);
  if (ret) {
    ALOGE("Failed to set copy_comp layers");
    return ret;
  }

  lock.Unlock();
  DrmHwcLayer writeback_layer;
  ret = drmdisplaycompositor.FlattenOnDisplay(copy_comp, writeback_conn,
                                              mode_.mode, &writeback_layer);
  if (ret) {
    ALOGE("Failed to flatten on display ret = %d", ret);
    return ret;
  }

  DrmCompositionPlane squashed_comp(DrmCompositionPlane::Type::kLayer, NULL,
                                    crtc);
  for (auto &drmplane : resource_manager_->GetDrmDevice(display_)->planes()) {
    if (!drmplane->GetCrtcSupported(*crtc))
      continue;
    if (drmplane->type() == DRM_PLANE_TYPE_PRIMARY)
      squashed_comp.set_plane(drmplane.get());
    else
      writeback_comp->AddPlaneDisable(drmplane.get());
  }
  writeback_comp->layers().emplace_back();
  DrmHwcLayer &next_layer = writeback_comp->layers().back();
  next_layer.sf_handle = writeback_layer.get_usable_handle();
  next_layer.blending = DrmHwcBlending::kPreMult;
  next_layer.source_crop = {0, 0, (float)mode_.mode.h_display(),
                            (float)mode_.mode.v_display()};
  next_layer.display_frame = {0, 0, (int)mode_.mode.h_display(),
                              (int)mode_.mode.v_display()};
  ret = next_layer.ImportBuffer(resource_manager_->GetImporter(display_).get());
  if (ret) {
    ALOGE("Failed to import framebuffer for display %d", ret);
    return ret;
  }
  squashed_comp.source_layers().push_back(0);
  ret = writeback_comp->AddPlaneComposition(std::move(squashed_comp));
  if (ret) {
    ALOGE("Failed to add plane composition %d", ret);
    return ret;
  }
  ApplyFrame(std::move(writeback_comp), 0, true);
  return ret;
}

int DrmDisplayCompositor::FlattenActiveComposition() {
  DrmConnector *writeback_conn = resource_manager_->AvailableWritebackConnector(
      display_);
  if (!active_composition_ || !writeback_conn) {
    ALOGV("No writeback connector available");
    return -EINVAL;
  }


  if (writeback_conn->display() != display_) {
    return FlattenConcurrent(writeback_conn);
  } else {
    return FlattenSerial(writeback_conn);
  }

  return 0;
}

bool DrmDisplayCompositor::CountdownExpired() const {
  return flatten_countdown_ <= 0;
}

void DrmDisplayCompositor::Vsync(int display, int64_t timestamp) {
  AutoLock lock(&lock_, __func__);
  if (lock.Lock())
    return;
  flatten_countdown_--;
  if (!CountdownExpired())
    return;
  lock.Unlock();
  int ret = FlattenActiveComposition();
  ALOGV("scene flattening triggered for display %d at timestamp %" PRIu64
        " result = %d \n",
        display, timestamp, ret);
}

void DrmDisplayCompositor::Dump(std::ostringstream *out) const {
  int ret = pthread_mutex_lock(&lock_);
  if (ret)
    return;

  uint64_t num_frames = dump_frames_composited_;
  dump_frames_composited_ = 0;

  struct timespec ts;
  ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  if (ret) {
    pthread_mutex_unlock(&lock_);
    return;
  }

  uint64_t cur_ts = ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
  uint64_t num_ms = (cur_ts - dump_last_timestamp_ns_) / (1000 * 1000);
  float fps = num_ms ? (num_frames * 1000.0f) / (num_ms) : 0.0f;

  *out << "--DrmDisplayCompositor[" << display_
       << "]: num_frames=" << num_frames << " num_ms=" << num_ms
       << " fps=" << fps << "\n";

  dump_last_timestamp_ns_ = cur_ts;

  pthread_mutex_unlock(&lock_);
}
}  // namespace android
