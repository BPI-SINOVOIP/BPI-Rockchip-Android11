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

#define LOG_TAG "hwc-vsync-worker"

#include "drmresources.h"
#include "vsyncworker.h"
#include "worker.h"

#include <map>
#include <stdlib.h>
#include <time.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#ifdef ANDROID_P
#include <log/log.h>
#else
#include <cutils/log.h>
#endif

#include <cutils/properties.h>
#include <hardware/hardware.h>

namespace android {

VSyncWorker::VSyncWorker()
    : Worker("vsync", HAL_PRIORITY_URGENT_DISPLAY),
      drm_(NULL),
      procs_(NULL),
      display_(-1),
      last_timestamp_(-1) {
}

VSyncWorker::~VSyncWorker() {
}

int VSyncWorker::Init(DrmResources *drm, int display) {
  drm_ = drm;
  display_ = display;

  return InitWorker();
}

int VSyncWorker::SetProcs(hwc_procs_t const *procs) {
  int ret = Lock();
  if (ret) {
    ALOGE("Failed to lock vsync worker lock %d\n", ret);
    return ret;
  }

  procs_ = procs;

  ret = Unlock();
  if (ret) {
    ALOGE("Failed to unlock vsync worker lock %d\n", ret);
    return ret;
  }
  return 0;
}

int VSyncWorker::VSyncControl(bool enabled) {
  int ret = Lock();
  if (ret) {
    ALOGE("Failed to lock vsync worker lock %d\n", ret);
    return ret;
  }

  enabled_ = enabled;
  last_timestamp_ = -1;
  int signal_ret = SignalLocked();

  ret = Unlock();
  if (ret) {
    ALOGE("Failed to unlock vsync worker lock %d\n", ret);
    return ret;
  }

  return signal_ret;
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
int64_t VSyncWorker::GetPhasedVSync(int64_t frame_ns, int64_t current) {
  if (last_timestamp_ < 0)
    return current + frame_ns;

  return frame_ns * ((current - last_timestamp_) / frame_ns + 1) +
         last_timestamp_;
}

static const int64_t kOneSecondNs = 1 * 1000 * 1000 * 1000;

int VSyncWorker::SyntheticWaitVBlank(int64_t *timestamp) {
  struct timespec vsync;
  int ret = clock_gettime(CLOCK_MONOTONIC, &vsync);

  float refresh = 60.0f;  // Default to 60Hz refresh rate
  DrmConnector *conn = drm_->GetConnectorFromType(display_);
  if (conn && conn->state() == DRM_MODE_CONNECTED) {
    if (conn->active_mode().v_refresh() > 0.0f)
      refresh = conn->active_mode().v_refresh();
  }

  int64_t phased_timestamp = GetPhasedVSync(
      kOneSecondNs / refresh, vsync.tv_sec * kOneSecondNs + vsync.tv_nsec);
  vsync.tv_sec = phased_timestamp / kOneSecondNs;
  vsync.tv_nsec = phased_timestamp - (vsync.tv_sec * kOneSecondNs);
  do {
    ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &vsync, NULL);
  } while (ret == -1 && errno == EINTR);
  if (ret)
    return ret;

  *timestamp = (int64_t)vsync.tv_sec * kOneSecondNs + (int64_t)vsync.tv_nsec;
  return 0;
}

void VSyncWorker::Routine() {
  ALOGD_IF(log_level(DBG_INFO),"----------------------------VSyncWorker Routine start----------------------------");
  int ret = Lock();
  if (ret) {
    ALOGE("Failed to lock worker %d", ret);
    return;
  }

  if (!enabled_) {
    ret = WaitForSignalOrExitLocked();
    if (ret == -EINTR) {
      return;
    }
  }

  bool enabled = enabled_;
  int display = display_;
  hwc_procs_t const *procs = procs_;

  ret = Unlock();
  if (ret) {
    ALOGE("Failed to unlock worker %d", ret);
  }

  if (!enabled)
    return;

  int64_t timestamp;
  DrmConnector *conn = drm_->GetConnectorFromType(display);
  if (!conn) {
    ALOGE("Failed to get connector for display");
    return;
  }
  DrmCrtc *crtc = NULL;
  if (conn)
    crtc = drm_->GetCrtcFromConnector(conn);

  if (!crtc) {
    ret = SyntheticWaitVBlank(&timestamp);
    if (ret)
      return;
  } else {
    uint32_t high_crtc = (crtc->pipe() << DRM_VBLANK_HIGH_CRTC_SHIFT);

    drmVBlank vblank;
    memset(&vblank, 0, sizeof(vblank));
    vblank.request.type = (drmVBlankSeqType)(
        DRM_VBLANK_RELATIVE | (high_crtc & DRM_VBLANK_HIGH_CRTC_MASK));
    vblank.request.sequence = 1;

    ret = drmWaitVBlank(drm_->fd(), &vblank);
    if (ret == -EINTR) {
      return;
    } else if (ret) {
      ret = SyntheticWaitVBlank(&timestamp);
      if (ret)
        return;
    } else {
      timestamp = (int64_t)vblank.reply.tval_sec * kOneSecondNs +
                  (int64_t)vblank.reply.tval_usec * 1000;
    }
  }
  /*
   * There's a race here where a change in procs_ will not take effect until
   * the next subsequent requested vsync. This is unavoidable since we can't
   * call the vsync hook while holding the thread lock.
   *
   * We could shorten the race window by caching procs_ right before calling
   * the hook. However, in practice, procs_ is only updated once, so it's not
   * worth the overhead.
   */
   //zxl:In VtsHalGraphicsComposerV2_1TargetTest, sometimes procs->vsync will invalid.
  if (procs && ((unsigned long)procs->vsync > 0x10))
    procs->vsync(procs, display, timestamp);
  last_timestamp_ = timestamp;

  ALOGD_IF(log_level(DBG_INFO),"----------------------------VSyncWorker Routine end----------------------------");
}
}
