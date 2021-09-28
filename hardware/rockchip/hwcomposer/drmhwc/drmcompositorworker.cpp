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

#define LOG_TAG "hwc-drm-compositor-worker"

#include "drmdisplaycompositor.h"
#include "drmcompositorworker.h"
#include "worker.h"

#include <stdlib.h>

#ifdef ANDROID_P
#include <log/log.h>
#else
#include <cutils/log.h>
#endif

#include <hardware/hardware.h>

namespace android {

static const int64_t kSquashWait = 500000000LL;

DrmCompositorWorker::DrmCompositorWorker(DrmDisplayCompositor *compositor)
    : Worker("drm-compositor", HAL_PRIORITY_URGENT_DISPLAY),
      compositor_(compositor) {
}

DrmCompositorWorker::~DrmCompositorWorker() {
}

int DrmCompositorWorker::Init() {
  return InitWorker();
}

void DrmCompositorWorker::Routine() {
  ALOGD_IF(log_level(DBG_INFO),"----------------------------DrmCompositorWorker Routine start----------------------------");

  int ret;
  if (!compositor_->HaveQueuedComposites()) {
    ret = Lock();
    if (ret) {
      ALOGE("Failed to lock worker, %d", ret);
      return;
    }

    // Only use a timeout if we didn't do a SquashAll last time. This will
    // prevent wait_ret == -ETIMEDOUT which would trigger a SquashAll and be a
    // pointless drain on resources.
    int wait_ret = did_squash_all_ ? WaitForSignalOrExitLocked()
                                   : WaitForSignalOrExitLocked(kSquashWait);

    ret = Unlock();
    if (ret) {
      ALOGE("Failed to unlock worker, %d", ret);
      return;
    }

    switch (wait_ret) {
      case 0:
        break;
      case -EINTR:
        return;
//close pre-comp for static screen.
      case -ETIMEDOUT:
#if 0
        ret = compositor_->SquashAll();
        if (ret)
          ALOGD_IF(log_level(DBG_DEBUG),"Failed to squash all %d", ret);
        did_squash_all_ = true;
#endif
        return;
      default:
        ALOGE("Failed to wait for signal, %d", wait_ret);
        return;
    }
  }

  ret = compositor_->Composite();
  if (ret)
    ALOGE("Failed to composite! %d", ret);
  did_squash_all_ = false;

  ALOGD_IF(log_level(DBG_INFO),"----------------------------DrmCompositorWorker Routine end----------------------------");
}
}
