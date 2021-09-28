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

#ifndef DRM_INVALIDATE_WORKER_H_
#define DRM_INVALIDATE_WORKER_H_

#include "worker.h"

#include <stdint.h>
#include <map>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

namespace android {

class InvalidateCallback {
 public:
  virtual ~InvalidateCallback() {
  }
  virtual void Callback(int display) = 0;
};

class InvalidateWorker : public Worker {
 public:
  InvalidateWorker();
  ~InvalidateWorker() override;

  int Init(int display);
  void RegisterCallback(std::shared_ptr<InvalidateCallback> callback);

  void InvalidateControl(uint64_t refresh, int refresh_cnt);

 protected:
  void Routine() override;

 private:
  int64_t GetPhasedVSync(int64_t frame_ns, int64_t current);
  int SyntheticWaitVBlank(int64_t *timestamp);

  // shared_ptr since we need to use this outside of the thread lock (to
  // actually call the hook) and we don't want the memory freed until we're
  // done
  std::shared_ptr<InvalidateCallback> callback_ = NULL;
  bool enable_;
  int display_;
  uint64_t refresh_;
  int refresh_cnt_;
  int64_t last_timestamp_;
};
}  // namespace android

#endif
