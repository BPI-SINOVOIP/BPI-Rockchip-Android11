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

#ifndef ANDROID_DRM_DISPLAY_COMPOSITOR_H_
#define ANDROID_DRM_DISPLAY_COMPOSITOR_H_


#include "drmdisplaycomposition.h"
#include "drmframebuffer.h"
#include "drmhwcomposer.h"
#include "resourcemanager.h"
#include "vsyncworker.h"
#include "drmcompositorworker.h"

#include <pthread.h>
#include <memory>
#include <sstream>
#include <tuple>
#include <queue>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

#define OVERSCAN_MIN_VALUE              (80)
#define OVERSCAN_MAX_VALUE              (100)

// One for the front, one for the back, and one for cases where we need to
// squash a frame that the hw can't display with hw overlays.
#define DRM_DISPLAY_BUFFERS 3

// If a scene is still for this number of vblanks flatten it to reduce power
// consumption.
#define FLATTEN_COUNTDOWN_INIT 60

namespace android {

class DrmDisplayCompositor {
 public:
  DrmDisplayCompositor();
  ~DrmDisplayCompositor();

  int Init(ResourceManager *resource_manager, int display);

  std::unique_ptr<DrmDisplayComposition> CreateComposition() const;
  std::unique_ptr<DrmDisplayComposition> CreateInitializedComposition() const;
  int QueueComposition(std::unique_ptr<DrmDisplayComposition> composition);
  int TestComposition(DrmDisplayComposition *composition);
  int Composite();
  void Dump(std::ostringstream *out) const;
  void Vsync(int display, int64_t timestamp);
  void SingalCompsition(std::unique_ptr<DrmDisplayComposition> composition);
  void ClearDisplay();

  std::tuple<uint32_t, uint32_t, int> GetActiveModeResolution();

  bool HaveQueuedComposites() const;

 private:

  struct FrameState {
    std::unique_ptr<DrmDisplayComposition> composition;
    int status = 0;
  };

  class FrameWorker : public Worker {
   public:
    FrameWorker(DrmDisplayCompositor *compositor);
    ~FrameWorker() override;

    int Init();
    void QueueFrame(std::unique_ptr<DrmDisplayComposition> composition,
                    int status);

   protected:
    void Routine() override;

   private:
    DrmDisplayCompositor *compositor_;
    std::queue<FrameState> frame_queue_;
  };
  struct ModeState {
    bool needs_modeset = false;
    DrmMode mode;
    uint32_t blob_id = 0;
    uint32_t old_blob_id = 0;
  };

  DrmDisplayCompositor(const DrmDisplayCompositor &) = delete;

  // We'll wait for acquire fences to fire for kAcquireWaitTimeoutMs,
  // kAcquireWaitTries times, logging a warning in between.
  static const int kAcquireWaitTries = 5;
  static const int kAcquireWaitTimeoutMs = 100;
  int CheckOverscan(drmModeAtomicReqPtr pset, DrmCrtc* crtc, int display, const char *UniqueName);
  int CommitFrame(DrmDisplayComposition *display_comp, bool test_only,
                  DrmConnector *writeback_conn = NULL,
                  DrmHwcBuffer *writeback_buffer = NULL);
  int SetupWritebackCommit(drmModeAtomicReqPtr pset, uint32_t crtc_id,
                           DrmConnector *writeback_conn,
                           DrmHwcBuffer *writeback_buffer);
  int ApplyDpms(DrmDisplayComposition *display_comp);
  int DisablePlanes(DrmDisplayComposition *display_comp);

  void ApplyFrame(std::unique_ptr<DrmDisplayComposition> composition,
                  int status, bool writeback = false);
  int FlattenActiveComposition();
  int FlattenSerial(DrmConnector *writeback_conn);
  int FlattenConcurrent(DrmConnector *writeback_conn);
  int FlattenOnDisplay(std::unique_ptr<DrmDisplayComposition> &src,
                       DrmConnector *writeback_conn, DrmMode &src_mode,
                       DrmHwcLayer *writeback_layer);

  bool CountdownExpired() const;

  std::tuple<int, uint32_t> CreateModeBlob(const DrmMode &mode);

  ResourceManager *resource_manager_;
  int display_;
  DrmCompositorWorker worker_;
  FrameWorker frame_worker_;

  std::queue<std::unique_ptr<DrmDisplayComposition>> composite_queue_;

  std::unique_ptr<DrmDisplayComposition> active_composition_;

  pthread_cond_t composite_queue_cond_;

  bool initialized_;
  bool active_;
  bool use_hw_overlays_;
  // Enter ClearDisplay state must SignalCompositionDone to signal releaseFence
  bool clear_;

  ModeState mode_;

  int framebuffer_index_;
  DrmFramebuffer framebuffers_[DRM_DISPLAY_BUFFERS];

  // mutable since we need to acquire in HaveQueuedComposites
  mutable pthread_mutex_t lock_;

  // State tracking progress since our last Dump(). These are mutable since
  // we need to reset them on every Dump() call.
  mutable uint64_t dump_frames_composited_;
  mutable uint64_t dump_last_timestamp_ns_;
  VSyncWorker vsync_worker_;
  int64_t flatten_countdown_;
  std::unique_ptr<Planner> planner_;
  int writeback_fence_;
};
}  // namespace android

#endif  // ANDROID_DRM_DISPLAY_COMPOSITOR_H_
