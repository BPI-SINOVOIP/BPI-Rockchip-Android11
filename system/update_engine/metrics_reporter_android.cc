//
// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "update_engine/metrics_reporter_android.h"

#include <stdint.h>

#include <memory>
#include <string>

#include <android-base/properties.h>
#include <base/strings/string_util.h>
#include <fs_mgr.h>
#include <libdm/dm.h>
#include <liblp/builder.h>
#include <liblp/liblp.h>
#include <statslog.h>

#include "update_engine/common/constants.h"

using android::fs_mgr::GetPartitionGroupName;
using android::fs_mgr::LpMetadata;
using android::fs_mgr::MetadataBuilder;
using android::fs_mgr::ReadMetadata;
using android::fs_mgr::SlotNumberForSlotSuffix;
using base::EndsWith;

namespace {
// A number offset adds on top of the enum value. e.g. ErrorCode::SUCCESS will
// be reported as 10000, and AttemptResult::UPDATE_CANCELED will be reported as
// 10011. This keeps the ordering of update engine's enum definition when statsd
// atoms reserve the value 0 for unknown state.
constexpr auto kMetricsReporterEnumOffset = 10000;

int32_t GetStatsdEnumValue(int32_t value) {
  return kMetricsReporterEnumOffset + value;
}
}  // namespace

namespace chromeos_update_engine {

namespace metrics {

std::unique_ptr<MetricsReporterInterface> CreateMetricsReporter() {
  return std::make_unique<MetricsReporterAndroid>();
}

}  // namespace metrics

void MetricsReporterAndroid::ReportUpdateAttemptMetrics(
    SystemState* /* system_state */,
    int attempt_number,
    PayloadType payload_type,
    base::TimeDelta duration,
    base::TimeDelta duration_uptime,
    int64_t payload_size,
    metrics::AttemptResult attempt_result,
    ErrorCode error_code) {
  int64_t payload_size_mib = payload_size / kNumBytesInOneMiB;

  int64_t super_partition_size_bytes = 0;
  int64_t super_free_space = 0;
  int64_t slot_size_bytes = 0;

  if (android::base::GetBoolProperty("ro.boot.dynamic_partitions", false)) {
    uint32_t slot = SlotNumberForSlotSuffix(fs_mgr_get_slot_suffix());
    auto super_device = fs_mgr_get_super_partition_name();
    std::unique_ptr<LpMetadata> metadata = ReadMetadata(super_device, slot);
    if (metadata) {
      super_partition_size_bytes = GetTotalSuperPartitionSize(*metadata);

      for (const auto& group : metadata->groups) {
        if (EndsWith(GetPartitionGroupName(group),
                     fs_mgr_get_slot_suffix(),
                     base::CompareCase::SENSITIVE)) {
          slot_size_bytes += group.maximum_size;
        }
      }

      auto metadata_builder = MetadataBuilder::New(*metadata);
      if (metadata_builder) {
        auto free_regions = metadata_builder->GetFreeRegions();
        for (const auto& interval : free_regions) {
          super_free_space += interval.length();
        }
        super_free_space *= android::dm::kSectorSize;
      } else {
        LOG(ERROR) << "Cannot create metadata builder.";
      }
    } else {
      LOG(ERROR) << "Could not read dynamic partition metadata for device: "
                 << super_device;
    }
  }

  android::util::stats_write(
      android::util::UPDATE_ENGINE_UPDATE_ATTEMPT_REPORTED,
      attempt_number,
      GetStatsdEnumValue(static_cast<int32_t>(payload_type)),
      duration.InMinutes(),
      duration_uptime.InMinutes(),
      payload_size_mib,
      GetStatsdEnumValue(static_cast<int32_t>(attempt_result)),
      GetStatsdEnumValue(static_cast<int32_t>(error_code)),
      android::base::GetProperty("ro.build.fingerprint", "").c_str(),
      super_partition_size_bytes,
      slot_size_bytes,
      super_free_space);
}

void MetricsReporterAndroid::ReportUpdateAttemptDownloadMetrics(
    int64_t payload_bytes_downloaded,
    int64_t /* payload_download_speed_bps */,
    DownloadSource /* download_source */,
    metrics::DownloadErrorCode /* payload_download_error_code */,
    metrics::ConnectionType /* connection_type */) {
  // TODO(xunchang) add statsd reporting
  LOG(INFO) << "Current update attempt downloads "
            << payload_bytes_downloaded / kNumBytesInOneMiB << " bytes data";
}

void MetricsReporterAndroid::ReportSuccessfulUpdateMetrics(
    int attempt_count,
    int /* updates_abandoned_count */,
    PayloadType payload_type,
    int64_t payload_size,
    int64_t num_bytes_downloaded[kNumDownloadSources],
    int download_overhead_percentage,
    base::TimeDelta total_duration,
    base::TimeDelta /* total_duration_uptime */,
    int reboot_count,
    int /* url_switch_count */) {
  int64_t payload_size_mib = payload_size / kNumBytesInOneMiB;
  int64_t total_bytes_downloaded = 0;
  for (size_t i = 0; i < kNumDownloadSources; i++) {
    total_bytes_downloaded += num_bytes_downloaded[i] / kNumBytesInOneMiB;
  }

  android::util::stats_write(
      android::util::UPDATE_ENGINE_SUCCESSFUL_UPDATE_REPORTED,
      static_cast<int32_t>(attempt_count),
      GetStatsdEnumValue(static_cast<int32_t>(payload_type)),
      static_cast<int32_t>(payload_size_mib),
      static_cast<int32_t>(total_bytes_downloaded),
      static_cast<int32_t>(download_overhead_percentage),
      static_cast<int32_t>(total_duration.InMinutes()),
      static_cast<int32_t>(reboot_count));
}

void MetricsReporterAndroid::ReportAbnormallyTerminatedUpdateAttemptMetrics() {
  int attempt_result =
      static_cast<int>(metrics::AttemptResult::kAbnormalTermination);
  // TODO(xunchang) add statsd reporting
  LOG(INFO) << "Abnormally terminated update attempt result " << attempt_result;
}

};  // namespace chromeos_update_engine
