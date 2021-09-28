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

#define LOG_TAG "hwc-invalidate-worker"

#include "rockchip/invalidateworker.h"
#include "worker.h"

#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <hardware/hardware.h>
#include <log/log.h>

namespace android {

InvalidateWorker::InvalidateWorker()
    : Worker("vsync", HAL_PRIORITY_URGENT_DISPLAY),
      enable_(false),
      display_(-1),
      refresh_(0),
      refresh_cnt_(0),
      last_timestamp_(-1){
}

InvalidateWorker::~InvalidateWorker() {
}

int InvalidateWorker::Init(int display) {
  display_ = display;
  return InitWorker();
}

void InvalidateWorker::RegisterCallback(std::shared_ptr<InvalidateCallback> callback) {
  Lock();
  callback_ = callback;
  Unlock();
}

void InvalidateWorker::InvalidateControl(uint64_t refresh, int refresh_cnt) {
  Lock();
  refresh_ = refresh;
  refresh_cnt_ = refresh_cnt;
  Unlock();
  Signal();
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
int64_t InvalidateWorker::GetPhasedVSync(int64_t frame_ns, int64_t current) {
  if (last_timestamp_ < 0)
    return current + frame_ns;

  return frame_ns * ((current - last_timestamp_) / frame_ns + 1) +
         last_timestamp_;
}

static const int64_t kOneSecondNs = 1 * 1000 * 1000 * 1000;

int InvalidateWorker::SyntheticWaitVBlank(int64_t *timestamp) {
  struct timespec vsync;
  int ret = clock_gettime(CLOCK_MONOTONIC, &vsync);

  int64_t phased_timestamp = GetPhasedVSync(kOneSecondNs / refresh_,
                                            vsync.tv_sec * kOneSecondNs +
                                                vsync.tv_nsec);
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


void InvalidateWorker::Routine() {
  int ret;

  Lock();
  if (refresh_cnt_ == 0 || refresh_ == 0) {
    ret = WaitForSignalOrExitLocked();
    if (ret == -EINTR) {
      Unlock();
      return;
    }
  }

  int64_t timestamp;
  ret = SyntheticWaitVBlank(&timestamp);
  last_timestamp_ = timestamp;

  bool enable = (refresh_cnt_ > 0 || refresh_cnt_ < 0);

  if(refresh_cnt_ > 0)
    refresh_cnt_--;
  std::shared_ptr<InvalidateCallback> callback(callback_);
  Unlock();

  if (!enable)
    return;

  if (callback)
    callback->Callback(display_);
}
}  // namespace android
