/*
 * Copyright (C) 2019 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "GCH_HidlProfiler"
#include <log/log.h>
#include <mutex>
#include <utility>

#include "hidl_profiler.h"

namespace android {
namespace hardware {
namespace camera {
namespace implementation {
namespace hidl_profiler {
namespace {

struct HidlProfiler {
  HidlProfiler() {
    int32_t mode = property_get_int32("persist.camera.profiler.open_close", 0);
    if (mode) {
      // Use stop watch mode to print result.
      mode |= google::camera_common::Profiler::SetPropFlag::kStopWatch;
    }
    profiler = google::camera_common::Profiler::Create(mode);
    if (profiler != nullptr) {
      profiler->SetDumpFilePrefix(
          "/data/vendor/camera/profiler/hidl_open_close_");
      profiler->Start("Overall", 0);
    }
  }

  ~HidlProfiler() {
    if (profiler != nullptr) {
      profiler->End("Overall", 0);
    }
  }

  std::shared_ptr<google::camera_common::Profiler> profiler = nullptr;
  bool has_camera_open = false;
  uint8_t config_counter = 0;
  uint8_t flush_counter = 0;
  uint8_t connector_counter = 0;
};

std::unique_ptr<HidlProfiler> gHidlProfiler = nullptr;
// Mutex to make all API functions mutually exclusive.
std::mutex api_mutex;

void StartNewConnector() {
  if (gHidlProfiler != nullptr && gHidlProfiler->profiler != nullptr) {
    gHidlProfiler->profiler->Start("<-- IDLE -->",
                                   ++gHidlProfiler->connector_counter);
  }
}

void EndConnector() {
  if (gHidlProfiler != nullptr && gHidlProfiler->profiler != nullptr &&
      gHidlProfiler->connector_counter != 0) {
    gHidlProfiler->profiler->End("<-- IDLE -->",
                                 gHidlProfiler->connector_counter);
  }
}

void EndProfiler() {
  gHidlProfiler = nullptr;
}

}  // anonymous namespace

std::unique_ptr<HidlProfilerItem> OnCameraOpen() {
  std::lock_guard lock(api_mutex);
  gHidlProfiler = std::make_unique<HidlProfiler>();
  if (gHidlProfiler == nullptr || gHidlProfiler->profiler == nullptr) {
    ALOGE("%s: gHidlProfiler or profiler is nullptr.", __FUNCTION__);
    return nullptr;
  }

  gHidlProfiler->has_camera_open = true;
  gHidlProfiler->profiler->SetUseCase("Open Camera");

  return std::make_unique<HidlProfilerItem>(gHidlProfiler->profiler, "Open",
                                            StartNewConnector);
}

std::unique_ptr<HidlProfilerItem> OnCameraFlush() {
  std::lock_guard lock(api_mutex);
  EndConnector();
  if (gHidlProfiler == nullptr) {
    gHidlProfiler = std::make_unique<HidlProfiler>();
  }

  if (gHidlProfiler == nullptr || gHidlProfiler->profiler == nullptr) {
    ALOGE("%s: gHidlProfiler or profiler is nullptr.", __FUNCTION__);
    return nullptr;
  }
  gHidlProfiler->profiler->SetUseCase("Flush Camera");
  return std::make_unique<HidlProfilerItem>(gHidlProfiler->profiler, "Flush",
                                            StartNewConnector,
                                            gHidlProfiler->flush_counter++);
}

std::unique_ptr<HidlProfilerItem> OnCameraClose() {
  std::lock_guard lock(api_mutex);
  EndConnector();
  if (gHidlProfiler == nullptr) {
    gHidlProfiler = std::make_unique<HidlProfiler>();
  }

  if (gHidlProfiler == nullptr || gHidlProfiler->profiler == nullptr) {
    ALOGE("%s: gHidlProfiler or profiler is nullptr.", __FUNCTION__);
    return nullptr;
  }

  gHidlProfiler->profiler->SetUseCase("Close Camera");
  return std::make_unique<HidlProfilerItem>(gHidlProfiler->profiler, "Close",
                                            EndProfiler);
}

std::unique_ptr<HidlProfilerItem> OnCameraStreamConfigure() {
  std::lock_guard lock(api_mutex);
  EndConnector();
  if (gHidlProfiler == nullptr) {
    gHidlProfiler = std::make_unique<HidlProfiler>();
  }

  if (gHidlProfiler == nullptr || gHidlProfiler->profiler == nullptr) {
    ALOGE("%s: gHidlProfiler or profiler is nullptr.", __FUNCTION__);
    return nullptr;
  }

  if (!gHidlProfiler->has_camera_open) {
    gHidlProfiler->profiler->SetUseCase("Reconfigure Stream");
  }

  return std::make_unique<HidlProfilerItem>(
      gHidlProfiler->profiler, "configureStreams", StartNewConnector,
      gHidlProfiler->config_counter++);
}

void OnFirstFrameRequest() {
  std::lock_guard lock(api_mutex);
  EndConnector();
  if (gHidlProfiler != nullptr && gHidlProfiler->profiler != nullptr) {
    gHidlProfiler->profiler->Start(
        "First frame", google::camera_common::Profiler::kInvalidRequestId);
    gHidlProfiler->profiler->Start(
        "HAL Total", google::camera_common::Profiler::kInvalidRequestId);
  }
}

void OnFirstFrameResult() {
  std::lock_guard lock(api_mutex);
  if (gHidlProfiler != nullptr && gHidlProfiler->profiler != nullptr) {
    gHidlProfiler->profiler->End(
        "First frame", google::camera_common::Profiler::kInvalidRequestId);
    gHidlProfiler->profiler->End(
        "HAL Total", google::camera_common::Profiler::kInvalidRequestId);
    EndProfiler();
  }
}

HidlProfilerItem::HidlProfilerItem(
    std::shared_ptr<google::camera_common::Profiler> profiler,
    const std::string target, std::function<void()> on_end, int request_id)
    : profiler_(profiler), target_(std::move(target)), request_id_(request_id) {
  on_end_ = on_end;
  profiler_->Start(target_, request_id_);
  profiler_->Start("HAL Total",
                   google::camera_common::Profiler::kInvalidRequestId);
}

HidlProfilerItem::~HidlProfilerItem() {
  profiler_->End("HAL Total",
                 google::camera_common::Profiler::kInvalidRequestId);
  profiler_->End(target_, request_id_);
  if (on_end_) {
    on_end_();
  }
}

}  // namespace hidl_profiler
}  // namespace implementation
}  // namespace camera
}  // namespace hardware
}  // namespace android
