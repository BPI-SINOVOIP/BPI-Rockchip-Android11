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

#define LOG_TAG "hwc-drm-compositor-worker"

#include "drmdisplaycompositor.h"
#include "drmcompositorworker.h"
#include "utils/worker.h"

#include <stdlib.h>

#include <log/log.h>
#include <hardware/hardware.h>

namespace android {

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
  int ret;
  if (!compositor_->HaveQueuedComposites()) {
    Lock();
    int wait_ret = WaitForSignalOrExitLocked(kWaitTimeOut_);
    Unlock();

    switch (wait_ret) {
      case 0:
        break;
      case -EINTR:
        return;
      //close pre-comp for static screen.
      case -ETIMEDOUT:
        kWaitTimeOut_ = kWaitTimeOut_ * 2 > 500000000LL? 500000000LL : kWaitTimeOut_ * 2;
        return;
      default:
        ALOGE("Failed to wait for signal, %d", wait_ret);
        return;
    }
  }
  kWaitTimeOut_ = 2000000LL;

  ret = compositor_->Composite();
  if (ret)
    ALOGE("Failed to composite! %d", ret);
}
}
