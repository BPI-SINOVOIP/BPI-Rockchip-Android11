/*
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co.Ltd.
 *
 * Modification based on code covered by the Apache License, Version 2.0 (the "License").
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS TO YOU ON AN "AS IS" BASIS
 * AND ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
 * NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.
 *
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
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

#include "drmhwcomposer.h"
#include "drmcomposition.h"
#include "drmcompositorworker.h"
#include "drmframebuffer.h"
#include "separate_rects.h"

#include <pthread.h>
#include <memory>
#include <queue>
#include <sstream>
#include <tuple>

#if (RK_RGA_COMPSITE_SYNC | RK_RGA_PREPARE_ASYNC)
#include <RockchipRga.h>
#endif

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

// One for the front, one for the back, and one for cases where we need to
// squash a frame that the hw can't display with hw overlays.
#define DRM_DISPLAY_BUFFERS             (3)
#define MaxRgaBuffers                   (5)
#define RGA_MAX_WIDTH                   (4096)
#define RGA_MAX_HEIGHT                  (2304)
#define VOP_BW_PATH			"/sys/class/devfreq/dmc/vop_bandwidth"
#define OVERSCAN_MIN_VALUE              (80)
#define OVERSCAN_MAX_VALUE              (100)


namespace android {

class GLWorkerCompositor;

class SquashState {
 public:
  static const unsigned kHistoryLength = 6;  // TODO: make this number not magic
  static const unsigned kMaxLayers = 64;

  struct Region {
    DrmHwcRect<int> rect;
    std::bitset<kMaxLayers> layer_refs;
    std::bitset<kHistoryLength> change_history;
    bool squashed = false;
  };

  bool is_stable(int region_index) const {
    return valid_history_ >= kHistoryLength &&
           regions_[region_index].change_history.none();
  }

  const std::vector<Region> &regions() const {
    return regions_;
  }

  void Init(DrmHwcLayer *layers, size_t num_layers);
  void GenerateHistory(DrmHwcLayer *layers, size_t num_layers,
                       std::vector<bool> &changed_regions) const;
  void StableRegionsWithMarginalHistory(
      const std::vector<bool> &changed_regions,
      std::vector<bool> &stable_regions) const;
  void RecordHistory(DrmHwcLayer *layers, size_t num_layers,
                     const std::vector<bool> &changed_regions);
  bool RecordAndCompareSquashed(const std::vector<bool> &squashed_regions);

  void Dump(std::ostringstream *out) const;

 private:
  size_t generation_number_ = 0;
  unsigned valid_history_ = 0;
  std::vector<buffer_handle_t> last_handles_;

  std::vector<Region> regions_;
};

class DrmDisplayCompositor {
 public:
  DrmDisplayCompositor();
  ~DrmDisplayCompositor();

  int Init(DrmResources *drm, int display);

  std::unique_ptr<DrmDisplayComposition> CreateComposition() const;
  int QueueComposition(std::unique_ptr<DrmDisplayComposition> composition);
  int Composite();
  void ClearDisplay();
  int SquashAll();
  void Dump(std::ostringstream *out) const;

  std::tuple<uint32_t, uint32_t, int> GetActiveModeResolution();

  bool HaveQueuedComposites() const;

  SquashState *squash_state() {
    return &squash_state_;
  }

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
    pthread_cond_t frame_queue_cond_;  //rk: limit frame queue size;
    DrmDisplayCompositor *compositor_;
    std::queue<FrameState> frame_queue_;
  };

  struct ModeState {
    bool needs_modeset = false;
    DrmMode mode;
    uint32_t blob_id = 0;
  };

  DrmDisplayCompositor(const DrmDisplayCompositor &) = delete;

  // We'll wait for acquire fences to fire for kAcquireWaitTimeoutMs,
  // kAcquireWaitTries times, logging a warning in between.
  static const int kAcquireWaitTries = 5;
  static const int kAcquireWaitTimeoutMs = 100;

  int PrepareFramebuffer(DrmFramebuffer &fb,
                         DrmDisplayComposition *display_comp);
#if RK_RGA_COMPSITE_SYNC
  int PrepareRgaBuffer(DrmRgaBuffer &rgaBuffer,
                         DrmDisplayComposition *display_comp, DrmHwcLayer &layer);
#endif
  int ApplySquash(DrmDisplayComposition *display_comp);
  int ApplyPreComposite(DrmDisplayComposition *display_comp);
#if RK_RGA_COMPSITE_SYNC
  int ApplyPreRotate(DrmDisplayComposition *display_comp,
                DrmHwcLayer &layer);
  void freeRgaBuffers();
#endif
  int PrepareFrame(DrmDisplayComposition *display_comp);
  int CommitFrame(DrmDisplayComposition *display_comp, bool test_only);
  int SquashFrame(DrmDisplayComposition *src, DrmDisplayComposition *dst);
  int ApplyDpms(DrmDisplayComposition *display_comp);
  int DisablePlanes(DrmDisplayComposition *display_comp);
  void SingalCompsition(std::unique_ptr<DrmDisplayComposition> composition);

  void ApplyFrame(std::unique_ptr<DrmDisplayComposition> composition,
                  int status);

  std::tuple<int, uint32_t> CreateModeBlob(const DrmMode &mode);

  DrmResources *drm_;
  int display_;

  DrmCompositorWorker worker_;
  FrameWorker frame_worker_;

  std::queue<std::unique_ptr<DrmDisplayComposition>> composite_queue_;
  std::unique_ptr<DrmDisplayComposition> active_composition_;

  pthread_cond_t composite_queue_cond_;  //rk: limit composite queue size;

  bool initialized_;
  bool active_;
  bool use_hw_overlays_;
  /*
   *  Currently, ClearDisplay only to clear composition in DrmCompositorWorker,
   *  but sometime some compositions exist in FrameWorker, so, we must set
   *  clearDisplay_ to notify FrameWorker to clear compositions.
   */
  bool clearDisplay_;

  mutable pthread_mutex_t mode_lock_;
  ModeState mode_;

  int framebuffer_index_;
  DrmFramebuffer framebuffers_[DRM_DISPLAY_BUFFERS];
#if RK_RGA_COMPSITE_SYNC
  int rgaBuffer_index_;
  DrmRgaBuffer rgaBuffers_[MaxRgaBuffers];
  RockchipRga& mRga_;
  bool mUseRga_;
#endif
  std::unique_ptr<GLWorkerCompositor> pre_compositor_;

  SquashState squash_state_;
  int squash_framebuffer_index_;
  DrmFramebuffer squash_framebuffers_[2];

  // mutable since we need to acquire in HaveQueuedComposites
  mutable pthread_mutex_t lock_;
  int vop_bw_fd_;

  // State tracking progress since our last Dump(). These are mutable since
  // we need to reset them on every Dump() call.
  mutable uint64_t dump_frames_composited_;
  mutable uint64_t dump_last_timestamp_ns_;

  const gralloc_module_t *gralloc_;
};
}

#endif  // ANDROID_DRM_DISPLAY_COMPOSITOR_H_
