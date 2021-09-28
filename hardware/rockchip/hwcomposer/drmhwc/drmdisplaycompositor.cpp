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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#define LOG_TAG "hwc-drm-display-compositor"

#include "drmdisplaycompositor.h"

#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <time.h>
#include <sstream>
#include <vector>
#ifdef ANDROID_P
#include <log/log.h>
#else
#include <cutils/log.h>
#endif

#include <drm/drm_mode.h>
#include <sync/sync.h>
#include <utils/Trace.h>
#include <cutils/properties.h>
#include <unistd.h>
#include "autolock.h"
#include "drmcrtc.h"
#include "drmplane.h"
#include "drmresources.h"
#include "glworker.h"
#include "hwc_util.h"
#include "hwc_debug.h"
#include "hwc_rockchip.h"

#if USE_GRALLOC_4
#include "drmgralloc4.h"
#endif

#define DRM_QUEUE_USLEEP 10
#define DRM_DISPLAY_COMPOSITOR_MAX_QUEUE_DEPTH 1

namespace android {

void SquashState::Init(DrmHwcLayer *layers, size_t num_layers) {
  generation_number_++;
  valid_history_ = 0;
  regions_.clear();
  last_handles_.clear();

  std::vector<DrmHwcRect<int>> in_rects;
  for (size_t i = 0; i < num_layers; i++) {
    DrmHwcLayer *layer = &layers[i];
    in_rects.emplace_back(layer->display_frame);
    last_handles_.push_back(layer->sf_handle);
  }

  std::vector<separate_rects::RectSet<uint64_t, int>> out_regions;
  separate_rects::separate_rects_64(in_rects, &out_regions);

  for (const separate_rects::RectSet<uint64_t, int> &out_region : out_regions) {
    regions_.emplace_back();
    Region &region = regions_.back();
    region.rect = out_region.rect;
    region.layer_refs = out_region.id_set.getBits();
  }
}

void SquashState::GenerateHistory(DrmHwcLayer *layers, size_t num_layers,
                                  std::vector<bool> &changed_regions) const {
  changed_regions.resize(regions_.size());
  if (num_layers != last_handles_.size()) {
    ALOGE("SquashState::GenerateHistory expected %zu layers but got %zu layers",
          last_handles_.size(), num_layers);
    return;
  }
  std::bitset<kMaxLayers> changed_layers;
  for (size_t i = 0; i < last_handles_.size(); i++) {
    DrmHwcLayer *layer = &layers[i];
    // Protected layers can't be squashed so we treat them as constantly
    // changing.
    if (layer->protected_usage() || last_handles_[i] != layer->sf_handle)
      changed_layers.set(i);
  }

  for (size_t i = 0; i < regions_.size(); i++) {
    changed_regions[i] = (regions_[i].layer_refs & changed_layers).any();
  }
}

void SquashState::StableRegionsWithMarginalHistory(
    const std::vector<bool> &changed_regions,
    std::vector<bool> &stable_regions) const {
  stable_regions.resize(regions_.size());
  for (size_t i = 0; i < regions_.size(); i++) {
    stable_regions[i] = !changed_regions[i] && is_stable(i);
  }
}

void SquashState::RecordHistory(DrmHwcLayer *layers, size_t num_layers,
                                const std::vector<bool> &changed_regions) {
  if (num_layers != last_handles_.size()) {
    ALOGE("SquashState::RecordHistory expected %zu layers but got %zu layers",
          last_handles_.size(), num_layers);
    return;
  }
  if (changed_regions.size() != regions_.size()) {
    ALOGE("SquashState::RecordHistory expected %zu regions but got %zu regions",
          regions_.size(), changed_regions.size());
    return;
  }

  for (size_t i = 0; i < last_handles_.size(); i++) {
    DrmHwcLayer *layer = &layers[i];
    last_handles_[i] = layer->sf_handle;
  }

  for (size_t i = 0; i < regions_.size(); i++) {
    regions_[i].change_history <<= 1;
    regions_[i].change_history.set(/* LSB */ 0, changed_regions[i]);
  }

  valid_history_++;
}

bool SquashState::RecordAndCompareSquashed(
    const std::vector<bool> &squashed_regions) {
  if (squashed_regions.size() != regions_.size()) {
    ALOGE(
        "SquashState::RecordAndCompareSquashed expected %zu regions but got "
        "%zu regions",
        regions_.size(), squashed_regions.size());
    return false;
  }
  bool changed = false;
  for (size_t i = 0; i < regions_.size(); i++) {
    if (regions_[i].squashed != squashed_regions[i]) {
      regions_[i].squashed = squashed_regions[i];
      changed = true;
    }
  }
  return changed;
}

void SquashState::Dump(std::ostringstream *out) const {
  *out << "----SquashState generation=" << generation_number_
       << " history=" << valid_history_ << "\n"
       << "    Regions: count=" << regions_.size() << "\n";
  for (size_t i = 0; i < regions_.size(); i++) {
    const Region &region = regions_[i];
    *out << "      [" << i << "]"
         << " history=" << region.change_history << " rect";
    region.rect.Dump(out);
    *out << " layers=(";
    bool first = true;
    for (size_t layer_index = 0; layer_index < kMaxLayers; layer_index++) {
      if ((region.layer_refs &
           std::bitset<kMaxLayers>((size_t)1 << layer_index))
              .any()) {
        if (!first)
          *out << " ";
        first = false;
        *out << layer_index;
      }
    }
    *out << ")";
    if (region.squashed)
      *out << " squashed";
    *out << "\n";
  }
}

static bool UsesSquash(const std::vector<DrmCompositionPlane> &comp_planes) {
  return std::any_of(comp_planes.begin(), comp_planes.end(),
                     [](const DrmCompositionPlane &plane) {
    return plane.type() == DrmCompositionPlane::Type::kSquash;
  });
}

DrmDisplayCompositor::FrameWorker::FrameWorker(DrmDisplayCompositor *compositor)
    : Worker("frame-worker", HAL_PRIORITY_URGENT_DISPLAY),
      compositor_(compositor) {
}

DrmDisplayCompositor::FrameWorker::~FrameWorker() {
  pthread_cond_destroy(&frame_queue_cond_);
}

int DrmDisplayCompositor::FrameWorker::Init() {
  pthread_cond_init(&frame_queue_cond_, NULL);
  return InitWorker();
}

void DrmDisplayCompositor::FrameWorker::QueueFrame(
    std::unique_ptr<DrmDisplayComposition> composition, int status) {

  /* ----------rk modified----------
   * Block the queue if it gets too large.
   * If we don't set this limitation,sometimes it will lead many frames' acquirefence don't signal,
   * finanlly,lead fd leak out.
   */
  //while (frame_queue_.size() >= DRM_DISPLAY_COMPOSITOR_MAX_QUEUE_DEPTH) {
 //   usleep(DRM_QUEUE_USLEEP);
  //}

  Lock();
  while(frame_queue_.size() >= DRM_DISPLAY_COMPOSITOR_MAX_QUEUE_DEPTH)
  {
    pthread_cond_wait(&frame_queue_cond_,getLock());
  }

  FrameState frame;
  frame.composition = std::move(composition);
  frame.status = status;
  frame_queue_.push(std::move(frame));
  SignalLocked();
  Unlock();
}

void DrmDisplayCompositor::FrameWorker::Routine() {
  ALOGD_IF(log_level(DBG_INFO),"----------------------------FrameWorker Routine start----------------------------");

  int ret = Lock();
  if (ret) {
    ALOGE("Failed to lock worker, %d", ret);
    return;
  }

  int wait_ret = 0;
  if (frame_queue_.empty()) {
    wait_ret = WaitForSignalOrExitLocked();
  }

  FrameState frame;
  if (!frame_queue_.empty()) {
    frame = std::move(frame_queue_.front());
    frame_queue_.pop();
    pthread_cond_signal(&frame_queue_cond_);
  }

  ret = Unlock();
  if (ret) {
    ALOGE("Failed to unlock worker, %d", ret);
    return;
  }

  if (wait_ret == -EINTR) {
    return;
  } else if (wait_ret) {
    ALOGE("Failed to wait for signal, %d", wait_ret);
    return;
  }

  compositor_->ApplyFrame(std::move(frame.composition), frame.status);

  ALOGD_IF(log_level(DBG_INFO),"----------------------------FrameWorker Routine end----------------------------");
}

#if RK_DEBUG_CHECK_CRC
static int crcTable[256];
static void initCrcTable(void)
{
	unsigned int c;
	unsigned int i, j;

	for (i = 0; i < 256; i++) {
		c = (unsigned int)i;
		for (j = 0; j < 8; j++) {
			if (c & 1) {
				c = 0xedb88320L ^ (c >> 1);
			} else {
			    c = c >> 1;
			}
		}
		crcTable[i] = c;
	}
}

static unsigned int createCrc32(unsigned int crc,unsigned const char *buffer, unsigned int size)
{
	unsigned int i;
	for (i = 0; i < size; i++) {
		crc = crcTable[(crc ^ buffer[i]) & 0xff] ^ (crc >> 8);
	}
	return crc ;
}
#endif

DrmDisplayCompositor::DrmDisplayCompositor()
    : drm_(NULL),
      display_(-1),
      worker_(this),
      frame_worker_(this),
      initialized_(false),
      active_(false),
      use_hw_overlays_(true),
      framebuffer_index_(0),
#if RK_RGA_COMPSITE_SYNC
      rgaBuffer_index_(0),
      mRga_(RockchipRga::get()),
      mUseRga_(false),
#endif
      squash_framebuffer_index_(0),
      vop_bw_fd_(-1),
      dump_frames_composited_(0),
      dump_last_timestamp_ns_(0) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts))
    return;
  dump_last_timestamp_ns_ = ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;

#if USE_GRALLOC_4
    gralloc_ = NULL;
#else   // USE_GRALLOC_4
  int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                      (const hw_module_t **)&gralloc_);
  if (ret) {
    ALOGE("Failed to open gralloc module %d", ret);
  }
#endif  // USE_GRALLOC_4

#if RK_DEBUG_CHECK_CRC
    initCrcTable();
#endif

}

DrmDisplayCompositor::~DrmDisplayCompositor() {
  if (!initialized_)
    return;

  worker_.Exit();
  frame_worker_.Exit();

  int ret = pthread_mutex_lock(&lock_);
  if (ret)
    ALOGE("Failed to acquire compositor lock %d", ret);

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

  if(vop_bw_fd_ > 0)
    close(vop_bw_fd_);
}

int DrmDisplayCompositor::Init(DrmResources *drm, int display) {
  drm_ = drm;
  display_ = display;

  int ret = pthread_mutex_init(&lock_, NULL);
  if (ret) {
    ALOGE("Failed to initialize drm compositor lock %d\n", ret);
    return ret;
  }

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


  vop_bw_fd_ = open(VOP_BW_PATH, O_WRONLY);
  if(vop_bw_fd_ < 0)
  {
    char buf[80];
    strerror_r(errno, buf, sizeof(buf));
    ALOGE("vop_bw: Error opening %s: %s\n", VOP_BW_PATH, buf);
  }

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
      break;
    case DRM_COMPOSITION_TYPE_DPMS:
      break;
    case DRM_COMPOSITION_TYPE_MODESET:
      break;
    case DRM_COMPOSITION_TYPE_EMPTY:
      return 0;
    default:
      ALOGE("Unknown composition type %d/%d", composition->type(), display_);
      return -ENOENT;
  }

  int ret = pthread_mutex_lock(&lock_);
  if (ret) {
    ALOGE("Failed to acquire compositor lock %d", ret);
    return ret;
  }
  clearDisplay_ = false;

  while(composite_queue_.size() >= DRM_DISPLAY_COMPOSITOR_MAX_QUEUE_DEPTH)
  {
    pthread_cond_wait(&composite_queue_cond_,&lock_);
  }

#if 0
  // Block the queue if it gets too large. Otherwise, SurfaceFlinger will start
  // to eat our buffer handles when we get about 1 second behind.
  while (composite_queue_.size() >= DRM_DISPLAY_COMPOSITOR_MAX_QUEUE_DEPTH) {

#if 0
    pthread_mutex_unlock(&lock_);
#if RK_DRM_HWC
    //sched_yield will lead cpu schedule abnormal.
    usleep(DRM_QUEUE_USLEEP);
#else
    sched_yield();
#endif
    pthread_mutex_lock(&lock_);
#endif
  }
#endif

  composite_queue_.push(std::move(composition));

  ret = pthread_mutex_unlock(&lock_);
  if (ret) {
    ALOGE("Failed to release compositor lock %d", ret);
    return ret;
  }

  worker_.Signal();
  return 0;
}

std::tuple<uint32_t, uint32_t, int>
DrmDisplayCompositor::GetActiveModeResolution() {
  DrmConnector *connector = drm_->GetConnectorFromType(display_);
  if (connector == NULL) {
    ALOGE("Failed to determine display mode: no connector for display %d",
          display_);
    return std::make_tuple(0, 0, -ENODEV);
  }

  const DrmMode &mode = connector->active_mode();
  return std::make_tuple(mode.h_display(), mode.v_display(), 0);
}

int DrmDisplayCompositor::PrepareFramebuffer(
    DrmFramebuffer &fb, DrmDisplayComposition *display_comp) {
  int ret = fb.WaitReleased(-1);
  if (ret) {
    ALOGE("Failed to wait for framebuffer release %d", ret);
    return ret;
  }
  uint32_t width, height;
  std::tie(width, height, ret) = GetActiveModeResolution();
  if (ret) {
    ALOGE(
        "Failed to allocate framebuffer because the display resolution could "
        "not be determined %d",
        ret);
    return ret;
  }

  fb.set_release_fence_fd(-1);
  if (!fb.Allocate(width, height)) {
    ALOGE("Failed to allocate framebuffer with size %dx%d", width, height);
    return -ENOMEM;
  }

  display_comp->layers().emplace_back();
  DrmHwcLayer &pre_comp_layer = display_comp->layers().back();
  pre_comp_layer.sf_handle = fb.buffer()->handle;
  pre_comp_layer.blending = DrmHwcBlending::kPreMult;
  pre_comp_layer.source_crop = DrmHwcRect<float>(0, 0, width, height);
  pre_comp_layer.display_frame = DrmHwcRect<int>(0, 0, width, height);
  ret = pre_comp_layer.buffer.ImportBuffer(fb.buffer()->handle,
                                           display_comp->importer()
#if RK_VIDEO_SKIP_LINE
                                           , false
#endif
                                           );
  if (ret) {
    ALOGE("Failed to import framebuffer for display %d", ret);
    return ret;
  }

#if USE_AFBC_LAYER
#if USE_GRALLOC_4
            pre_comp_layer.internal_format = gralloc4::get_internal_format(fb.buffer()->handle);
#else // #if USE_GRALLOC_4
#if RK_PER_MODE
      struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)fb.buffer()->handle;
      pre_comp_layer.internal_format = drm_hnd->internal_format;
#else
    ret = gralloc_->perform(gralloc_, GRALLOC_MODULE_PERFORM_GET_INTERNAL_FORMAT,
                         fb.buffer()->handle, &pre_comp_layer.internal_format);
    if (ret) {
        ALOGE("Failed to get internal_format for buffer %p (%d)", fb.buffer()->handle, ret);
        return ret;
    }
#endif
#endif // #if USE_GRALLOC_4
#endif

  return ret;
}

#if RK_RGA_COMPSITE_SYNC
int DrmDisplayCompositor::PrepareRgaBuffer(
DrmRgaBuffer &rgaBuffer, DrmDisplayComposition *display_comp, DrmHwcLayer &layer) {
    int rga_transform = 0;
    int src_l,src_t,src_w,src_h;
    int dst_l,dst_t,dst_r,dst_b;
    int ret;
    int dst_w,dst_h,dst_stride;
    rga_info_t src, dst;
    int alloc_format = 0;

    memset(&src, 0, sizeof(rga_info_t));
    memset(&dst, 0, sizeof(rga_info_t));
    src.fd = -1;
    dst.fd = -1;

    ret = rgaBuffer.WaitReleased(-1);
    if (ret) {
        ALOGE("Failed to wait for rga buffer release %d", ret);
        return ret;
    }
    rgaBuffer.set_release_fence_fd(-1);

    src_l = (int)layer.source_crop.left;
    src_t = (int)layer.source_crop.top;
    src_w = (int)(layer.source_crop.right - layer.source_crop.left);
    src_h = (int)(layer.source_crop.bottom - layer.source_crop.top);
    src_l = ALIGN_DOWN(src_l, 2);
    src_t = ALIGN_DOWN(src_t, 2);
    dst_l = 0;
    dst_t = 0;

#if !RK_RGA_SCALE_AND_ROTATE
    if(layer.transform & DrmHwcTransform::kRotate90 || layer.transform & DrmHwcTransform::kRotate270)
    {
        dst_r = (int)(layer.source_crop.bottom - layer.source_crop.top);
        dst_b = (int)(layer.source_crop.right - layer.source_crop.left);
        src_h = ALIGN_DOWN(src_h, 8);
        src_w = ALIGN_DOWN(src_w, 2);
    }
    else
    {
        dst_r = (int)(layer.source_crop.right - layer.source_crop.left);
        dst_b = (int)(layer.source_crop.bottom - layer.source_crop.top);
        src_w = ALIGN_DOWN(src_w, 8);
        src_h = ALIGN_DOWN(src_h, 2);
    }
    dst_w = dst_r - dst_l;
    dst_h = dst_b - dst_t;
    int dst_raw_w = dst_w;
    int dst_raw_h = dst_h;
    dst_w = ALIGN_DOWN(dst_w, 8);
    dst_h = ALIGN_DOWN(dst_h, 2);
#else
    src_w = ALIGN_DOWN(src_w, 2);
    src_h = ALIGN_DOWN(src_h, 2);

    dst_w = layer.rect_merge.right - layer.rect_merge.left;
    dst_h = layer.rect_merge.bottom - layer.rect_merge.top;

    dst_w = ALIGN(dst_w, 8);
    dst_h = ALIGN(dst_h, 2);
#endif

    if(dst_w < 0 || dst_h <0 )
      ALOGE("RGA invalid dst_w=%d,dst_h=%d",dst_w,dst_h);

    //If the layer's format is NV12_10,then use RGA to switch it to NV12.
    if(layer.format == HAL_PIXEL_FORMAT_YCrCb_NV12_10)
        alloc_format = HAL_PIXEL_FORMAT_YCrCb_NV12;
    else
        alloc_format = layer.format;

    if (!rgaBuffer.Allocate(dst_w, dst_h, alloc_format)) {
        ALOGE("Failed to allocate rga buffer with size %dx%d", dst_w, dst_h);
        return -ENOMEM;
    }

    dst_stride = rgaBuffer.buffer()->getStride();

    //DumpLayer("rga", layer.sf_handle);

    if(layer.transform & DrmHwcTransform::kRotate90) {
        rga_transform = DRM_RGA_TRANSFORM_ROT_90;
    }
    else if(layer.transform & DrmHwcTransform::kRotate270) {
        rga_transform = DRM_RGA_TRANSFORM_ROT_270;
    }
    else if(layer.transform & DrmHwcTransform::kRotate180) {
        rga_transform = DRM_RGA_TRANSFORM_ROT_180;
    }
    else if(layer.transform & DrmHwcTransform::kRotate0) {
        rga_transform = DRM_RGA_TRANSFORM_ROT_0;
    }
    else if(layer.transform & DrmHwcTransform::kFlipH) {
        rga_transform = DRM_RGA_TRANSFORM_FLIP_H;
    }
    else if(layer.transform & DrmHwcTransform::kFlipV) {
        rga_transform = DRM_RGA_TRANSFORM_FLIP_V;
    }
    else {
        ALOGE("%s: line=%d, wrong transform=0x%x", __FUNCTION__, __LINE__, layer.transform);
        ret = -1;
        return ret;
    }

    if(rga_transform != DRM_RGA_TRANSFORM_FLIP_H && layer.transform & DrmHwcTransform::kFlipH)
        rga_transform |= DRM_RGA_TRANSFORM_FLIP_H;

    if (rga_transform != DRM_RGA_TRANSFORM_FLIP_V && layer.transform & DrmHwcTransform::kFlipV)
        rga_transform |= DRM_RGA_TRANSFORM_FLIP_V;

    rga_set_rect(&src.rect,
                src_l, src_t, src_w, src_h,
                layer.stride, layer.height, layer.format);
    rga_set_rect(&dst.rect, dst_l, dst_t,  dst_w, dst_h, dst_stride, dst_h, alloc_format);
    ALOGD_IF(log_level(DBG_DEBUG),"RK_RGA_COMPSITE_SYNC rgaRotateScale  : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
        src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
        dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
    ALOGD_IF(log_level(DBG_DEBUG),"RK_RGA_COMPSITE_SYNC rgaRotateScale : src hnd=%p,dst hnd=%p, format=0x%x, transform=0x%x\n",
        (void*)layer.sf_handle, (void*)(rgaBuffer.buffer()->handle), layer.format, rga_transform);

    src.hnd = layer.sf_handle;
    dst.hnd = rgaBuffer.buffer()->handle;
    src.rotation = rga_transform;
    ret = mRga_.RkRgaBlit(&src, &dst, NULL);
    if(ret) {
        ALOGE("rgaRotateScale error : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
            src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
            dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
        ALOGE("rgaRotateScale error : %s,src hnd=%p,dst hnd=%p",
            strerror(errno), (void*)layer.sf_handle, (void*)(rgaBuffer.buffer()->handle));
    }

    DumpLayer("rga", dst.hnd);

    //instead of the original DrmHwcLayer
    layer.is_rotate_by_rga = true;
    layer.buffer.Clear();
    layer.source_crop = DrmHwcRect<float>(dst_l,dst_t,dst_w,dst_h);
    //The dst layer's format is NV12.
    if(layer.format == HAL_PIXEL_FORMAT_YCrCb_NV12_10)
        layer.format = HAL_PIXEL_FORMAT_YCrCb_NV12;
    layer.sf_handle = rgaBuffer.buffer()->handle;

#if RK_VIDEO_SKIP_LINE
    layer.SkipLine = 0;
#endif

    ret = layer.buffer.ImportBuffer(rgaBuffer.buffer()->handle,
                                           display_comp->importer()
#if RK_VIDEO_SKIP_LINE
                                           , layer.SkipLine
#endif
                                           );
    if (ret) {
        ALOGE("Failed to import rga buffer ret=%d", ret);
        return ret;
    }

    ret = layer.handle.CopyBufferHandle(rgaBuffer.buffer()->handle, gralloc_);
    if (ret) {
        ALOGE("Failed to copy rga handle ret=%d", ret);
        return ret;
    }

    return ret;
}
#endif

int DrmDisplayCompositor::ApplySquash(DrmDisplayComposition *display_comp) {
  int ret = 0;

  DrmFramebuffer &fb = squash_framebuffers_[squash_framebuffer_index_];
  ret = PrepareFramebuffer(fb, display_comp);
  if (ret) {
    ALOGE("Failed to prepare framebuffer for squash %d", ret);
    return ret;
  }

  std::vector<DrmCompositionRegion> &regions = display_comp->squash_regions();
  ret = pre_compositor_->Composite(display_comp->layers().data(),
                                   regions.data(), regions.size(), fb.buffer());
  pre_compositor_->Finish();

  if (ret) {
    ALOGE("Failed to squash layers");
    return ret;
  }

  ret = display_comp->CreateNextTimelineFence("PreLayer");
  if (ret <= 0) {
    ALOGE("Failed to create PreLayer framebuffer release fence %d", ret);
    return ret;
  }

  fb.set_release_fence_fd(ret);
  display_comp->SignalSquashDone();

  return 0;
}

int DrmDisplayCompositor::ApplyPreComposite(
    DrmDisplayComposition *display_comp) {
  int ret = 0;

  DrmFramebuffer &fb = framebuffers_[framebuffer_index_];
  ret = PrepareFramebuffer(fb, display_comp);
  if (ret) {
    ALOGE("Failed to prepare framebuffer for pre-composite %d", ret);
    return ret;
  }

  std::vector<DrmCompositionRegion> &regions = display_comp->pre_comp_regions();
  ret = pre_compositor_->Composite(display_comp->layers().data(),
                                   regions.data(), regions.size(), fb.buffer());
  pre_compositor_->Finish();

  if (ret) {
    ALOGE("Failed to pre-composite layers");
    return ret;
  }

  ret = display_comp->CreateNextTimelineFence("ApplyPreComposite");
  if (ret <= 0) {
    ALOGE("Failed to create pre-composite framebuffer release fence %d", ret);
    return ret;
  }

  fb.set_release_fence_fd(ret);
  display_comp->SignalPreCompDone();

  return 0;
}

#if RK_RGA_COMPSITE_SYNC
static int fence_merge(char* value,int fd1,int fd2)
{
    int ret = -1;
    if(fd1 >= 0 && fd2 >= 0) {
        ret = sync_merge(value, fd1, fd2);
        close(fd1);close(fd2);
    } else if (fd1 >= 0) {
        ret = sync_merge(value, fd1, fd1);
        close(fd1);
    } else if (fd2 >= 0) {
        ret = sync_merge(value, fd2, fd2);
        close(fd2);
    }
    if(ret < 0) {
        ALOGD("%s:merge[%d,%d]:%s",value,fd1,fd2,strerror(errno));
    }
    ALOGD_IF(log_level(DBG_DEBUG),"merge fd[%d,%d] to fd=%d",fd1,fd2,ret);
    return ret;
}

int DrmDisplayCompositor::ApplyPreRotate(
    DrmDisplayComposition *display_comp, DrmHwcLayer &layer) {
  int ret = 0;

  ALOGD_IF(log_level(DBG_DEBUG), "%s:rgaBuffer_index_=%d", __FUNCTION__, rgaBuffer_index_);

  DrmRgaBuffer &rga_buffer = rgaBuffers_[rgaBuffer_index_];
  ret = PrepareRgaBuffer(rga_buffer, display_comp, layer);
  if (ret) {
    ALOGE("Failed to prepare rga buffer for RGA rotate %d", ret);
    return ret;
  }

  ret = display_comp->CreateNextTimelineFence("ApplyPreRotate");
  if (ret <= 0) {
    ALOGE("Failed to create RGA rotate release fence %d", ret);
    return ret;
  }

  rga_buffer.set_release_fence_fd(ret);

  return 0;
}

void DrmDisplayCompositor::freeRgaBuffers() {
    for(int i = 0; i < MaxRgaBuffers; i++) {
        rgaBuffers_[i].Clear();
    }
}
#endif

int DrmDisplayCompositor::DisablePlanes(DrmDisplayComposition *display_comp) {
  drmModeAtomicReqPtr pset = drmModeAtomicAlloc();
  if (!pset) {
    ALOGE("Failed to allocate property set");
    return -ENOMEM;
  }

  int ret;
#ifdef USE_PLANE_RESERVED
  int win1_reserved = hwc_get_int_property( PROPERTY_TYPE ".hwc.win1.reserved", "0");
#endif
  std::vector<DrmCompositionPlane> &comp_planes =
      display_comp->composition_planes();
  for (DrmCompositionPlane &comp_plane : comp_planes) {
    DrmPlane *plane = comp_plane.plane();
#ifdef USE_PLANE_RESERVED
    if (win1_reserved > 0 && plane->is_reserved()){
        continue;
    }
#endif
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

  ret = drmModeAtomicCommit(drm_->fd(), pset, 0, drm_);
  if (ret) {
    ALOGE("Failed to commit pset ret=%d\n", ret);
    drmModeAtomicFree(pset);
    return ret;
  }

  drmModeAtomicFree(pset);
  return 0;
}

int DrmDisplayCompositor::PrepareFrame(DrmDisplayComposition *display_comp) {
  int ret = 0;

  std::vector<DrmHwcLayer> &layers = display_comp->layers();
  std::vector<DrmCompositionPlane> &comp_planes =
      display_comp->composition_planes();
  std::vector<DrmCompositionRegion> &squash_regions =
      display_comp->squash_regions();
  std::vector<DrmCompositionRegion> &pre_comp_regions =
      display_comp->pre_comp_regions();

  int squash_layer_index = -1;
  if (squash_regions.size() > 0) {
    squash_framebuffer_index_ = (squash_framebuffer_index_ + 1) % 2;
    ret = ApplySquash(display_comp);
    if (ret)
      return ret;

    squash_layer_index = layers.size() - 1;
  } else {
    if (UsesSquash(comp_planes)) {
      DrmFramebuffer &fb = squash_framebuffers_[squash_framebuffer_index_];
      layers.emplace_back();
      squash_layer_index = layers.size() - 1;
      DrmHwcLayer &squash_layer = layers.back();
      ret = squash_layer.buffer.ImportBuffer(fb.buffer()->handle,
                                             display_comp->importer()
#if RK_VIDEO_SKIP_LINE
                                             , false
#endif
                                             );
      if (ret) {
        ALOGE("Failed to import old squashed framebuffer %d", ret);
        return ret;
      }
      squash_layer.sf_handle = fb.buffer()->handle;
      squash_layer.blending = DrmHwcBlending::kPreMult;
      squash_layer.source_crop = DrmHwcRect<float>(
          0, 0, squash_layer.buffer->width, squash_layer.buffer->height);
      squash_layer.display_frame = DrmHwcRect<int>(
          0, 0, squash_layer.buffer->width, squash_layer.buffer->height);
#if USE_AFBC_LAYER
#if USE_GRALLOC_4
        squash_layer.internal_format = gralloc4::get_internal_format(fb.buffer()->handle);
#else // #if USE_GRALLOC_4
#if RK_PER_MODE
    struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)fb.buffer()->handle;
    squash_layer.internal_format = drm_hnd->internal_format;
#else
    ret = gralloc_->perform(gralloc_, GRALLOC_MODULE_PERFORM_GET_INTERNAL_FORMAT,
                         fb.buffer()->handle, &squash_layer.internal_format);
    if (ret) {
        ALOGE("Failed to get internal_format for buffer %p (%d)", fb.buffer()->handle, ret);
        return ret;
    }
#endif
#endif // #if USE_GRALLOC_4
#endif
     ret = display_comp->CreateNextTimelineFence("SquashLayer");
      if (ret <= 0) {
        ALOGE("Failed to create squash framebuffer release fence %d", ret);
        return ret;
      }

      fb.set_release_fence_fd(ret);
      ret = 0;
    }
  }

  bool do_pre_comp = pre_comp_regions.size() > 0;
  int pre_comp_layer_index = -1;
  if (do_pre_comp) {
    ret = ApplyPreComposite(display_comp);
    if (ret)
      return ret;

    pre_comp_layer_index = layers.size() - 1;
    framebuffer_index_ = (framebuffer_index_ + 1) % DRM_DISPLAY_BUFFERS;
  }

#if RK_RGA_COMPSITE_SYNC
    bool bUseRga = false;
#endif

  for (DrmCompositionPlane &comp_plane : comp_planes) {
    std::vector<size_t> &source_layers = comp_plane.source_layers();

    switch (comp_plane.type()) {
      case DrmCompositionPlane::Type::kSquash:
        if (source_layers.size())
          ALOGE("Squash source_layers is expected to be empty (%zu/%d)",
                source_layers[0], squash_layer_index);
        source_layers.push_back(squash_layer_index);
        break;
      case DrmCompositionPlane::Type::kPrecomp:
        if (!do_pre_comp) {
          ALOGE(
              "Can not use pre composite framebuffer with no pre composite "
              "regions");
          return -EINVAL;
        }
        // Replace source_layers with the output of the precomposite
        source_layers.clear();
        source_layers.push_back(pre_comp_layer_index);
        break;
     case DrmCompositionPlane::Type::kLayer:
#if RK_RGA_PREPARE_ASYNC
        if(drm_->isSupportRkRga() && !source_layers.empty())
        {
            DrmHwcLayer &layer = layers[source_layers.front()];

            if((layer.is_yuv && layer.transform!=DrmHwcTransform::kRotate0))
            {
                RockchipRga& rkRga(RockchipRga::get());
                ret = rkRga.RkRgaFlush();
                if(ret)
                {
                    ALOGE("%s:line=%d flush rga fail",__FUNCTION__,__LINE__);
                }
            }
        }
#endif
#if RK_RGA_COMPSITE_SYNC
        if(drm_->isSupportRkRga() && !source_layers.empty())
        {
            DrmHwcLayer &layer = layers[source_layers.front()];

            if(/*layer.is_yuv &&*/ layer.transform!=DrmHwcTransform::kRotate0 ||
                (layer.h_scale_mul > 1.0 &&  (int)(layer.display_frame.right - layer.display_frame.left) > 2560))
            {
                ret = ApplyPreRotate(display_comp,layer);
                if (ret)
                {
                    freeRgaBuffers();
                    mUseRga_ = mUseRga_ ? false : mUseRga_;
                    return ret;
                }

                rgaBuffer_index_ = (rgaBuffer_index_ + 1) % MaxRgaBuffers;
                bUseRga = true;
                mUseRga_ = mUseRga_ ? mUseRga_ : true;
            }
        }
#endif
        break;
      default:
        break;
    }
  }

#if RK_RGA_COMPSITE_SYNC
    if(mUseRga_ && !bUseRga)
    {
        freeRgaBuffers();
        mUseRga_ = false;
    }
#endif

  return ret;
}

static const char *RotatingToString(uint64_t rotating) {
  switch (rotating) {
    case (1 << DRM_REFLECT_X):
      return "DRM_REFLECT_X";
    case (1 << DRM_REFLECT_Y):
      return "DRM_REFLECT_Y";
    case (1 << DRM_ROTATE_90):
      return "DRM_ROTATE_90";
    case (1 << DRM_ROTATE_180):
      return "DRM_ROTATE_180";
    case (1 << DRM_ROTATE_270):
      return "DRM_ROTATE_270";
    case (0):
      return "DRM_ROTATE_0";
    default:
      return "<invalid>";
  }
}

int DrmDisplayCompositor::CommitFrame(DrmDisplayComposition *display_comp,
                                      bool test_only) {
  ATRACE_CALL();

  int ret = 0;
  uint32_t afbc_plane_id = 0;
  uint32_t plane_size = 0;
  uint32_t vop_bandwidth = 0, total_bandwidth = 0;

  std::vector<DrmHwcLayer> &layers = display_comp->layers();
  std::vector<DrmCompositionPlane> &comp_planes =
      display_comp->composition_planes();

  DrmCrtc *crtc = display_comp->crtc();
  if (!crtc) {
    ALOGE("Could not locate crtc for display %d", display_);
    return -ENODEV;
  }

  drmModeAtomicReqPtr pset = drmModeAtomicAlloc();
  if (!pset) {
    ALOGE("Failed to allocate property set");
    return -ENOMEM;
  }

  if (crtc->can_overscan()) {
    char overscan[PROPERTY_VALUE_MAX];
    int left_margin = 100, right_margin= 100, top_margin = 100, bottom_margin = 100;

    DrmConnector *conn = drm_->GetConnectorFromType(display_);
    if(conn == NULL)
    {
      ALOGE("%s: line=%d display %d connector is NULL", __FUNCTION__, __LINE__, display_);
      return -ENODEV;
    }
    DrmMode mode = conn->current_mode();
    if(display_comp->mode_3d() != NON_3D || (mode.interlaced() > 0))
    {
        left_margin = 100;
        top_margin = 100;
        right_margin = 100;
        bottom_margin = 100;
    }
    else
    {
      if (display_ == HWC_DISPLAY_PRIMARY){
        if(hwc_have_baseparameter()){
          property_get("persist." PROPERTY_TYPE ".overscan.main", overscan, "use_baseparameter");
          if(!strcmp(overscan,"use_baseparameter"))
            hwc_get_baseparameter_config(overscan,display_,BP_OVERSCAN,0);
        }else{
          property_get("persist." PROPERTY_TYPE ".overscan.main", overscan, "overscan 100,100,100,100");
        }
      }else{
        if(hwc_have_baseparameter()){
          property_get("persist." PROPERTY_TYPE ".overscan.aux", overscan, "use_baseparameter");
          if(!strcmp(overscan,"use_baseparameter"))
            hwc_get_baseparameter_config(overscan,display_,BP_OVERSCAN,0);
        }else{
          property_get("persist." PROPERTY_TYPE ".overscan.aux", overscan, "overscan 100,100,100,100");
        }
      }
      sscanf(overscan, "overscan %d,%d,%d,%d", &left_margin, &top_margin,
               &right_margin, &bottom_margin);
      ALOGD_IF(log_level(DBG_VERBOSE),"vop post scale overscan(%d,%d,%d,%d)",
                left_margin,top_margin,right_margin,bottom_margin);
    }

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
      drmModeAtomicFree(pset);
      return ret;
    }
  }

#if RK_VR
  float w_scale=1.0,h_scale=1.0;
  int xxx_w =  hwc_get_int_property( PROPERTY_TYPE ".xxx.x_w","720");
  int xxx_h =  hwc_get_int_property( PROPERTY_TYPE ".xxx.x_h","1280");
  uint32_t act_w, act_h;
  std::tie(act_w, act_h, ret) = GetActiveModeResolution();
  if (ret) {
    ALOGE(
        "Failed to allocate framebuffer because the display resolution could "
        "not be determined %d",
        ret);
    return ret;
  }
  if(act_w && xxx_w)
  {
    w_scale = (float)act_w / xxx_w;
    ALOGD("xxx_w=%d,act_w=%d,w_scale=%f,w_scale=%d",xxx_w,act_w,w_scale,(int)w_scale);
  }

  if(act_h && xxx_h)
  {
    h_scale = (float)act_h / xxx_h;
  }
#endif

    //Find out the fb target for clone layer.
    int fb_target_fb_id = -1;
#ifdef USE_PLANE_RESERVED
    int win1_reserved = hwc_get_int_property( PROPERTY_TYPE ".hwc.win1.reserved", "0");
#endif

#if RK_3D_VIDEO
    if(display_comp->mode_3d() == FPS_3D)
    {
        for (DrmCompositionPlane &comp_plane : comp_planes) {
            if (comp_plane.type() != DrmCompositionPlane::Type::kDisable) {
                std::vector<size_t> &source_layers = comp_plane.source_layers();

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
                if(layer.bFbTarget_ && !layer.bClone_ && layer.buffer)
                {
                    fb_target_fb_id = layer.buffer->fb_id;
                    break;
                }
            }
        }
    }
#endif

  for (DrmCompositionPlane &comp_plane : comp_planes) {
    DrmPlane *plane = comp_plane.plane();
    DrmCrtc *crtc = comp_plane.crtc();
    std::vector<size_t> &source_layers = comp_plane.source_layers();

    int fb_id = -1;
    bool is_yuv = false;
    int frame_no = 0;
    DrmHwcRect<int> display_frame = DrmHwcRect<int>(0, 0, 0, 0);
    DrmHwcRect<float> source_crop = DrmHwcRect<float>(0.0, 0.0, 0.0, 0.0);
#if RK_VIDEO_SKIP_LINE
    uint32_t SkipLine = 0;
#endif
    uint64_t rotation = 0;
    uint64_t alpha = 0xFF;
    uint16_t eotf = TRADITIONAL_GAMMA_SDR;
    DrmHwcBlending blending = DrmHwcBlending::kNone;
    uint32_t colorspace = V4L2_COLORSPACE_DEFAULT;
#if (RK_RGA_COMPSITE_SYNC | RK_RGA_PREPARE_ASYNC)
    bool is_rotate_by_rga = false;
#endif
    int zpos = 0;
#if RK_DEBUG_CHECK_CRC
    unsigned int crc32 = 0;
#endif
#if USE_AFBC_LAYER
    bool is_afbc = false;
#endif
    int format = 0;
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

      zpos = comp_plane.get_zpos();
      if(zpos < 0)
      {
        ALOGE("The zpos(%d) is invalid", zpos);
      }

      DrmHwcLayer &layer = layers[source_layers.front()];
      if (!test_only && layer.acquire_fence.get() >= 0) {
        int acquire_fence = layer.acquire_fence.get();
#if RK_VR
        if(!(layer.gralloc_buffer_usage & 0x08000000))
#endif
        {
          ret = sync_wait(acquire_fence, 1500);
          if (ret) {
            ALOGE("Failed to wait for acquire %d/%d 1500ms", acquire_fence, ret);
            break;
          }
        }
        layer.acquire_fence.Close();
      }

      if (
#if RK_3D_VIDEO
      !layer.bClone_ &&
#endif
      !layer.buffer) {
        ALOGE("Expected a valid framebuffer for pset");
        break;
      }

     //DumpLayer(layer.name.c_str(),layer.get_usable_handle());

#if RK_VIDEO_SKIP_LINE
      SkipLine = layer.SkipLine;
#endif
      if(layer.bClone_)
      {
        if(fb_target_fb_id > 0)
            fb_id = fb_target_fb_id;
        else
            ALOGE("Invalid fb_target_fb_id=%d in 3D FPS mode", fb_target_fb_id);
      }
      else
        fb_id = layer.buffer->fb_id;
      display_frame = layer.display_frame;
      source_crop = layer.source_crop;
      is_yuv = layer.is_yuv;
      frame_no = layer.frame_no;
      if (layer.blending == DrmHwcBlending::kPreMult)
        alpha = layer.alpha;


        eotf = layer.eotf;
        colorspace = layer.colorspace;
        blending = layer.blending;

#if RK_DEBUG_CHECK_CRC
    void* cpu_addr;
    gralloc_->lock(gralloc_, layer.sf_handle, GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                    0, 0, layer.width, layer.height, (void **)&cpu_addr);
    crc32 = createCrc32(0xFFFFFFFF,(unsigned const char *)cpu_addr,sizeof(layer.width*layer.height));
    ALOGD("layer=%s, w=%d, h=%d, crc32=0x%x",layer.name.c_str(),layer.width,layer.height,crc32);
    gralloc_->unlock(gralloc_, layer.sf_handle);
#endif

#if USE_AFBC_LAYER
    is_afbc = layer.is_afbc;
    if((afbc_plane_id== 0) && is_afbc)
    {
        afbc_plane_id = plane->id();
        ALOGD_IF(log_level(DBG_VERBOSE),"fbdc layer %s,plane id=%d",layer.name.c_str(),afbc_plane_id);
    }
#else
    UN_USED(afbc_plane_id);
#endif
    format = layer.format;

#if (RK_RGA_COMPSITE_SYNC | RK_RGA_PREPARE_ASYNC)
      is_rotate_by_rga = layer.is_rotate_by_rga;
#endif
#if DRM_DRIVER_VERSION==2
      rotation = kRotate0;
#else
      rotation = 0;
#endif
      if (layer.transform & DrmHwcTransform::kFlipH)
        rotation |= 1 << DRM_REFLECT_X;
      if (layer.transform & DrmHwcTransform::kFlipV)
        rotation |= 1 << DRM_REFLECT_Y;
      if (layer.transform & DrmHwcTransform::kRotate90)
        rotation |= 1 << DRM_ROTATE_90;
      else if (layer.transform & DrmHwcTransform::kRotate180)
        rotation |= 1 << DRM_ROTATE_180;
      else if (layer.transform & DrmHwcTransform::kRotate270)
        rotation |= 1 << DRM_ROTATE_270;
    }

    // Disable the plane if there's no framebuffer
#ifdef USE_PLANE_RESERVED
    if (fb_id < 0 && win1_reserved > 0 && plane->is_reserved()){
        continue;
    }
#endif

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

    // TODO: Once we have atomic test, this should fall back to GL
    if (
#if (RK_RGA_COMPSITE_SYNC | RK_RGA_PREPARE_ASYNC)
    !is_rotate_by_rga &&
#endif
#if DRM_DRIVER_VERSION==2
    rotation != kRotate0) {
#else
    rotation && !(rotation & plane->get_rotate())) {
#endif

      ALOGE("Rotation is not supported on plane %d", plane->id());
      ret = -EINVAL;
      break;
    }
    // TODO: Once we have atomic test, this should fall back to GL
    if (alpha != 0xFF && plane->alpha_property().id() == 0) {
      ALOGE("Alpha is not supported on plane %d", plane->id());
      ret = -EINVAL;
      break;
    }

    int dst_l,dst_t,dst_w,dst_h;
    int src_l,src_t,src_w,src_h;
    float hfactor;
    int scale_factor;
    float src_bpp;

    src_l = (int)source_crop.left;
    src_t = (int)source_crop.top;
    src_w = (int)(source_crop.right - source_crop.left);
#if RK_VIDEO_SKIP_LINE
    if(SkipLine)
    {
        src_h = (int)(source_crop.bottom - source_crop.top) / SkipLine + \
                ((int)(source_crop.bottom - source_crop.top) / SkipLine) % 2;
        src_t = (int)source_crop.top / SkipLine - ((int)source_crop.top / SkipLine) % 2;
    }
    else
#endif
        src_h = (int)(source_crop.bottom - source_crop.top);

    dst_l = display_frame.left;
    dst_t = display_frame.top;
    dst_w = display_frame.right - display_frame.left;
    dst_h = display_frame.bottom - display_frame.top;

#if RK_VR
    dst_l = dst_l * w_scale;
    dst_t = dst_t * h_scale;
    dst_w = dst_w * w_scale;
    dst_h = dst_h * h_scale;
    ALOGD_IF(log_level(DBG_VERBOSE),"scale dst: w_scale=%f,h_scale=%f",w_scale,h_scale);
#endif

//zxl: src_l/src_w need 16 pixels aligned and src_t/src_h need 4 pixels aligned in FBDC area.
#if USE_AFBC_LAYER
    if(afbc_plane_id == plane->id())
    {
        src_l = IS_ALIGN(src_l, 16)?src_l:ALIGN(src_l, 16);
        src_t = IS_ALIGN(src_t, 4)?src_t:ALIGN(src_t, 4);
        src_w = IS_ALIGN(src_w, 16)?src_w:(ALIGN(src_w, 16)-16);
        src_h = IS_ALIGN(src_h, 4)?src_h:(ALIGN(src_h, 4)-4);

        dst_l = IS_ALIGN(dst_l, 16)?dst_l:ALIGN(dst_l, 16);
        dst_t = IS_ALIGN(dst_t, 4)?dst_t:ALIGN(dst_t, 4);
        dst_w = IS_ALIGN(dst_w, 16)?dst_w:(ALIGN(dst_w, 16)-16);
        dst_h = IS_ALIGN(dst_h, 4)?dst_h:(ALIGN(dst_h, 4)-4);
    }
#endif
    if(is_yuv){
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
    ret |= drmModeAtomicAddProperty(
               pset, plane->id(), plane->crtc_w_property().id(),
               dst_w) < 0;
    ret |= drmModeAtomicAddProperty(
               pset, plane->id(), plane->crtc_h_property().id(),
               dst_h) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->src_x_property().id(),
                                    src_l << 16) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->src_y_property().id(),
                                    src_t << 16) < 0;
    ret |= drmModeAtomicAddProperty(
               pset, plane->id(), plane->src_w_property().id(),
               src_w << 16) < 0;
    ret |= drmModeAtomicAddProperty(
               pset, plane->id(), plane->src_h_property().id(),
               src_h << 16) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                   plane->zpos_property().id(), zpos) < 0;
    if (ret) {
      ALOGE("Failed to add plane %d to set", plane->id());
      break;
    }

    hfactor = (float)src_w/dst_w;
    scale_factor = hfactor > 1.0 ? 2:1;
    src_bpp = getPixelWidthByAndroidFormat(format);
    vop_bandwidth = src_w * src_h * src_bpp * scale_factor;
    total_bandwidth += vop_bandwidth;
    ALOGD_IF(log_level(DBG_VERBOSE),"vop_bw: plane=%d,w=%d,h=%d,bpp=%f,scale_factor=%d,vop_bandwidth=%d bytes",
                (plane ? plane->id() : -1),src_w,src_h,src_bpp,scale_factor,vop_bandwidth);

    plane_size++;

    size_t index=0;
    std::ostringstream out_log;

    out_log << "DrmDisplayCompositor[" << index << "]"
            << " display=" << display_
            << " frame_no=" << frame_no
            << " plane=" << (plane ? plane->id() : -1)
            << " crct id=" << crtc->id()
            << " fb id=" << fb_id
            << " display_frame[" << dst_l << ","
            << dst_t << "," << dst_w
            << "," << dst_h << "]"
            << " source_crop[" << src_l << ","
            << src_t << "," << src_w
            << "," << src_h << "]"
            << ", zpos=" << zpos
#if USE_AFBC_LAYER
            << ", is_afbc=" << is_afbc
#endif
            << ", vop_bandwidth=" << vop_bandwidth
            ;
    index++;

    if (
#if (RK_RGA_COMPSITE_SYNC | RK_RGA_PREPARE_ASYNC)
    !is_rotate_by_rga &&
#endif
    plane->rotation_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->rotation_property().id(),
                                     rotation) < 0;
      if (ret) {
        ALOGE("Failed to add rotation property %d to plane %d",
              plane->rotation_property().id(), plane->id());
        break;
      }
      out_log << " rotation=" << RotatingToString(rotation);
    }

    if (plane->alpha_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->alpha_property().id(),
                                     alpha) < 0;
      if (ret) {
        ALOGE("Failed to add alpha property %d to plane %d",
              plane->alpha_property().id(), plane->id());
        break;
      }
      out_log << " alpha=" << std::hex <<  alpha;
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

    if(plane->blend_mode_property().id()) {
      int pre_mult = (blending == DrmHwcBlending::kPreMult) ? 1:0;
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->blend_mode_property().id(),
                                     pre_mult) < 0;
      if (ret) {
        ALOGE("Failed to add blend mode property %d to plane %d",
              plane->blend_mode_property().id(), plane->id());
        break;
      }
      out_log << " blend mode =" << BlendingToString(blending) <<  eotf;
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

    out_log << "\n";
    ALOGD_IF(log_level(DBG_VERBOSE),"%s",out_log.str().c_str());
    out_log.clear();
  }

  if (!ret) {
    uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET;
    char vop_bw_str[50];
    int w_len = 0;
    char buf[80];

    total_bandwidth = total_bandwidth /(1024.0 * 1024.0) * 60;
    sprintf(vop_bw_str,"%d,%d", plane_size, total_bandwidth);
    ALOGD_IF(log_level(DBG_VERBOSE),"vop_bw: plane_size=%d, total_bandwidth=%d M, vop_bw_str=%s", plane_size, total_bandwidth, vop_bw_str);
    if(vop_bw_fd_ > 0)
    {
        w_len = write(vop_bw_fd_, vop_bw_str, strlen(vop_bw_str));
        if(w_len < 0)
        {
            strerror_r(errno, buf, sizeof(buf));
            ALOGE("vop_bw: Error writing to fd=%d: %s\n", vop_bw_fd_, buf);
        }
    }

    if (test_only)
      flags |= DRM_MODE_ATOMIC_TEST_ONLY;

PRINT_TIME_START;
    char value[PROPERTY_VALUE_MAX];
    int new_value;
    property_get( PROPERTY_TYPE ".hwc.msleep", value, "0");
    new_value = atoi(value);
    usleep(new_value*1000);

    ret = drmModeAtomicCommit(drm_->fd(), pset, flags, drm_);
    if (ret) {
      if (test_only)
        ALOGI("Commit test pset failed ret=%d\n", ret);
      else
        ALOGE("Failed to commit pset ret=%d\n", ret);
      drmModeAtomicFree(pset);
      return ret;
    }
PRINT_TIME_END("commit");
  }

  if (pset)
    drmModeAtomicFree(pset);

  return ret;
}

int DrmDisplayCompositor::ApplyDpms(DrmDisplayComposition *display_comp) {
  DrmConnector *conn = drm_->GetConnectorFromType(display_);
  if (!conn) {
    ALOGE("Failed to get DrmConnector for display %d", display_);
    return -ENODEV;
  }

  const DrmProperty &prop = conn->dpms_property();
  int ret = drmModeConnectorSetProperty(drm_->fd(), conn->id(), prop.id(),
                                        display_comp->dpms_mode());
  if (ret) {
    ALOGE("Failed to set DPMS property for connector %d", conn->id());
    return ret;
  }
  return 0;
}

void DrmDisplayCompositor::SingalCompsition(std::unique_ptr<DrmDisplayComposition> composition) {
  int ret;

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
          if (layer.acquire_fence.get() >= 0) {
              int acquire_fence = layer.acquire_fence.get();
              int total_fence_timeout = 0;
#if RK_VR
              if(!(layer.gralloc_buffer_usage & 0x08000000))
#endif
              {
#if 0

                  for (int i = 0; i < kAcquireWaitTries; ++i) {
                      int fence_timeout = kAcquireWaitTimeoutMs * (1 << i);
                      total_fence_timeout += fence_timeout;
                      ret = sync_wait(acquire_fence, -1);
                      if (ret)
                          ALOGW("Acquire fence %d wait %d failed (%d). Total time %d",
                            acquire_fence, i, ret, total_fence_timeout);
                      else
                      {
                          ALOGV("Wait Acquire fence %d successful", acquire_fence);
                          break;
                      }
                  }
                  if (ret) {
                      ALOGE("Failed to wait for acquire %d/%d", acquire_fence, ret);
                      break;
                  }
#endif
                  ret = sync_wait(acquire_fence, 1500);
                  if (ret) {
                    ALOGE("Failed to wait for acquire %d/%d 1500ms", acquire_fence, ret);
                    break;
                  }

              }
              layer.acquire_fence.Close();
          }

      }
  }

  composition->SignalCompositionDone();

  composition.reset(NULL);
}

void DrmDisplayCompositor::ClearDisplay() {
  AutoLock lock(&lock_, "compositor");
  int ret = lock.Lock();
  if (ret)
    return;

  SingalCompsition(std::move(active_composition_));

  //Singal the remainder fences in composite queue.
  while(!composite_queue_.empty())
  {
    std::unique_ptr<DrmDisplayComposition> remain_composition(
      std::move(composite_queue_.front()));

    if(remain_composition)
      ALOGD_IF(log_level(DBG_DEBUG),"ClearDisplay: composite_queue_ size=%zu frame_no=%" PRIu64 "",composite_queue_.size(), remain_composition->frame_no());

    SingalCompsition(std::move(remain_composition));
    composite_queue_.pop();
    pthread_cond_signal(&composite_queue_cond_);
  }
  clearDisplay_ = true;
}

void DrmDisplayCompositor::ApplyFrame(
    std::unique_ptr<DrmDisplayComposition> composition, int status) {
  int ret = status;
  if (!ret)
    ret = CommitFrame(composition.get(), false);

  if (ret) {
    ALOGE("Composite failed for display %d", display_);
    // Disable the hw used by the last active composition. This allows us to
    // signal the release fences from that composition to avoid hanging.
    ClearDisplay();
    return;
  }
  ++dump_frames_composited_;

  if (active_composition_)
    active_composition_->SignalCompositionDone();


  ret = pthread_mutex_lock(&lock_);
  if (ret)
    ALOGE("Failed to acquire lock for active_composition swap");

  active_composition_.swap(composition);
  if(clearDisplay_){
       usleep(16 * 1000);
       active_composition_->SignalCompositionDone();
  }
  if (!ret)
    ret = pthread_mutex_unlock(&lock_);
  if (ret)
    ALOGE("Failed to release lock for active_composition swap");
}

int DrmDisplayCompositor::Composite() {
  ATRACE_CALL();

#if USE_GL_WORKER
  if (!pre_compositor_) {
    pre_compositor_.reset(new GLWorkerCompositor());
    int ret = pre_compositor_->Init();
    if (ret) {
      ALOGE("Failed to initialize OpenGL compositor %d", ret);
      return ret;
    }
  }
#endif

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
      ret = PrepareFrame(composition.get());
      if (ret) {
        ALOGE("Failed to prepare frame for display %d", display_);
        return ret;
      }

      if (composition->geometry_changed()) {
        // Send the composition to the kernel to ensure we can commit it. This
        // is just a test, it won't actually commit the frame. If rejected,
        // squash the frame into one layer and use the squashed composition
        ret = CommitFrame(composition.get(), true);
        if (ret)
          ALOGI("Commit test failed, squashing frame for display %d(skip squashing by libin)", display_);
        use_hw_overlays_ = !ret;
      }

      // If use_hw_overlays_ is false, we can't use hardware to composite the
      // frame. So squash all layers into a single composition and queue that
      // instead.
      if ( 0 && !use_hw_overlays_) {
        std::unique_ptr<DrmDisplayComposition> squashed = CreateComposition();
        ret = SquashFrame(composition.get(), squashed.get());
        if (!ret) {
          composition = std::move(squashed);
        } else {
          ALOGE("Failed to squash frame for display %d", display_);
          // Disable the hw used by the last active composition. This allows us
          // to signal the release fences from that composition to avoid
          // hanging.
          ClearDisplay();
          return ret;
        }
      }
      frame_worker_.QueueFrame(std::move(composition), ret);
      break;
    case DRM_COMPOSITION_TYPE_DPMS:
      ret = ApplyDpms(composition.get());
      if (ret)
        ALOGE("Failed to apply dpms for display %d", display_);

#if 0
        //zxl:Fix fence timeout bug when plug out HDMI.
        if(composition.get()->dpms_mode() == DRM_MODE_DPMS_OFF && active_composition_)
        {
                active_composition_->SignalCompositionDone();
        }
#else
        if(composition.get()->dpms_mode() == DRM_MODE_DPMS_OFF)
            ClearDisplay();
#endif
      return ret;
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

int DrmDisplayCompositor::SquashAll() {
  AutoLock lock(&lock_, "compositor");
  int ret = lock.Lock();
  if (ret)
    return ret;

  if (!active_composition_)
    return 0;

  std::unique_ptr<DrmDisplayComposition> comp = CreateComposition();
  ret = SquashFrame(active_composition_.get(), comp.get());

  // ApplyFrame needs the lock
  lock.Unlock();

  if (!ret)
    ApplyFrame(std::move(comp), 0);

  return ret;
}

// Returns:
//   - 0 if src is successfully squashed into dst
//   - -EALREADY if the src is already squashed
//   - Appropriate error if the squash fails
int DrmDisplayCompositor::SquashFrame(DrmDisplayComposition *src,
                                      DrmDisplayComposition *dst) {
  if (src->type() != DRM_COMPOSITION_TYPE_FRAME)
    return -ENOTSUP;

  std::vector<DrmCompositionPlane> &src_planes = src->composition_planes();
  std::vector<DrmHwcLayer> &src_layers = src->layers();

  // Make sure there is more than one layer to squash.
  size_t src_planes_with_layer = std::count_if(
      src_planes.begin(), src_planes.end(), [](DrmCompositionPlane &p) {
        return p.type() != DrmCompositionPlane::Type::kDisable;
      });
  if (src_planes_with_layer <= 1)
    return -EALREADY;

  int pre_comp_layer_index;

  int ret = dst->Init(drm_, src->crtc(), src->importer(), src->planner(),
                      src->frame_no());
  if (ret) {
    ALOGE("Failed to init squash all composition %d", ret);
    return ret;
  }

  DrmCompositionPlane squashed_comp(DrmCompositionPlane::Type::kPrecomp, NULL,
                                    src->crtc());
  std::vector<DrmHwcLayer> dst_layers;
  for (DrmCompositionPlane &comp_plane : src_planes) {
    // Composition planes without DRM planes should never happen
    if (comp_plane.plane() == NULL) {
      ALOGE("Skipping squash all because of NULL plane");
      ret = -EINVAL;
      goto move_layers_back;
    }

    if (comp_plane.type() == DrmCompositionPlane::Type::kDisable) {
      dst->AddPlaneDisable(comp_plane.plane());
      continue;
    }

    for (auto i : comp_plane.source_layers()) {
      DrmHwcLayer &layer = src_layers[i];

      // Squashing protected layers is impossible.
      if (layer.protected_usage()) {
        ret = -ENOTSUP;
        goto move_layers_back;
      }

      // The OutputFds point to freed memory after hwc_set returns. They are
      // returned to the default to prevent DrmDisplayComposition::Plan from
      // filling the OutputFds.
      layer.release_fence = OutputFd();
      dst_layers.emplace_back(std::move(layer));
      squashed_comp.source_layers().push_back(
          squashed_comp.source_layers().size());
    }

    if (!squashed_comp.plane())
    {
      squashed_comp.set_plane(comp_plane.plane());
    }
    else
      dst->AddPlaneDisable(comp_plane.plane());
  }

  ret = dst->SetLayers(dst_layers.data(), dst_layers.size(), false);
  if (ret) {
    ALOGE("Failed to set layers for squash all composition %d", ret);
    goto move_layers_back;
  }

  ret = dst->AddPlaneComposition(std::move(squashed_comp));
  if (ret) {
    ALOGE("Failed to add squashed plane composition %d", ret);
    goto move_layers_back;
  }

  ret = dst->FinalizeComposition();
  if (ret) {
    ALOGE("Failed to plan for squash all composition %d", ret);
    goto move_layers_back;
  }

  ret = ApplyPreComposite(dst);
  if (ret) {
    ALOGE("Failed to pre-composite for squash all composition %d", ret);
    goto move_layers_back;
  }

  pre_comp_layer_index = dst->layers().size() - 1;
  framebuffer_index_ = (framebuffer_index_ + 1) % DRM_DISPLAY_BUFFERS;

  for (DrmCompositionPlane &plane : dst->composition_planes()) {
    if (plane.type() == DrmCompositionPlane::Type::kPrecomp) {
      // Replace source_layers with the output of the precomposite
      plane.source_layers().clear();
      plane.source_layers().push_back(pre_comp_layer_index);
      break;
    }
  }

  return 0;

// TODO(zachr): think of a better way to transfer ownership back to the active
// composition.
move_layers_back:
  for (size_t plane_index = 0;
       plane_index < src_planes.size() && plane_index < dst_layers.size();) {
    if (src_planes[plane_index].source_layers().empty()) {
      plane_index++;
      continue;
    }
    for (auto i : src_planes[plane_index].source_layers())
      src_layers[i] = std::move(dst_layers[plane_index++]);
  }

  return ret;
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

  if (active_composition_)
    active_composition_->Dump(out);

  squash_state_.Dump(out);

  pthread_mutex_unlock(&lock_);
}
}
