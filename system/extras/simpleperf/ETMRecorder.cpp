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

#include "ETMRecorder.h"

#include <stdio.h>
#include <sys/sysinfo.h>

#include <memory>
#include <limits>
#include <string>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

#include "utils.h"

namespace simpleperf {

static constexpr bool ETM_RECORD_TIMESTAMP = false;

// Config bits from include/linux/coresight-pmu.h in the kernel
// For etm_event_config:
static constexpr int ETM_OPT_CTXTID = 14;
static constexpr int ETM_OPT_TS = 28;
// For etm_config_reg:
static constexpr int ETM4_CFG_BIT_CTXTID = 6;
static constexpr int ETM4_CFG_BIT_TS = 11;

static const std::string ETM_DIR = "/sys/bus/event_source/devices/cs_etm/";

// from coresight_get_trace_id(int cpu) in include/linux/coresight-pmu.h
static int GetTraceId(int cpu) {
  return 0x10 + cpu * 2;
}

template <typename T>
static bool ReadValueInEtmDir(const std::string& file, T* value, bool report_error = true) {
  std::string s;
  uint64_t v;
  if (!android::base::ReadFileToString(ETM_DIR + file, &s) ||
      !android::base::ParseUint(android::base::Trim(s), &v)) {
    if (report_error) {
      LOG(ERROR) << "failed to read " << ETM_DIR << file;
    }
    return false;
  }
  *value = static_cast<T>(v);
  return true;
}

static uint32_t GetBits(uint32_t value, int start, int end) {
  return (value >> start) & ((1U << (end - start + 1)) - 1);
}

int ETMPerCpu::GetMajorVersion() const {
  return GetBits(trcidr1, 8, 11);
}

bool ETMPerCpu::IsContextIDSupported() const {
  return GetBits(trcidr2, 5, 9) >= 4;
}

bool ETMPerCpu::IsTimestampSupported() const {
  return GetBits(trcidr0, 24, 28) > 0;
}

ETMRecorder& ETMRecorder::GetInstance() {
  static ETMRecorder etm;
  return etm;
}

int ETMRecorder::GetEtmEventType() {
  if (event_type_ == 0) {
    if (!IsDir(ETM_DIR) || !ReadValueInEtmDir("type", &event_type_, false)) {
      event_type_ = -1;
    }
  }
  return event_type_;
}

std::unique_ptr<EventType> ETMRecorder::BuildEventType() {
  int etm_event_type = GetEtmEventType();
  if (etm_event_type == -1) {
    return nullptr;
  }
  return std::make_unique<EventType>(
      "cs-etm", etm_event_type, 0, "CoreSight ETM instruction tracing", "arm");
}

bool ETMRecorder::CheckEtmSupport() {
  if (GetEtmEventType() == -1) {
    LOG(ERROR) << "etm event type isn't supported on device";
    return false;
  }
  if (!ReadEtmInfo()) {
    LOG(ERROR) << "etm devices are not available";
    return false;
  }
  for (const auto& p : etm_info_) {
    if (p.second.GetMajorVersion() < 4) {
      LOG(ERROR) << "etm device version is less than 4.0";
      return false;
    }
    if (!p.second.IsContextIDSupported()) {
      LOG(ERROR) << "etm device doesn't support contextID";
      return false;
    }
  }
  if (!FindSinkConfig()) {
    LOG(ERROR) << "can't find etr device, which moves etm data to memory";
    return false;
  }
  etm_supported_ = true;
  return true;
}

bool ETMRecorder::ReadEtmInfo() {
  int cpu_count = get_nprocs_conf();
  for (const auto &name : GetEntriesInDir(ETM_DIR)) {
    int cpu;
    if (sscanf(name.c_str(), "cpu%d", &cpu) == 1) {
      ETMPerCpu &cpu_info = etm_info_[cpu];
      bool success =
          ReadValueInEtmDir(name + "/trcidr/trcidr0", &cpu_info.trcidr0) &&
          ReadValueInEtmDir(name + "/trcidr/trcidr1", &cpu_info.trcidr1) &&
          ReadValueInEtmDir(name + "/trcidr/trcidr2", &cpu_info.trcidr2) &&
          ReadValueInEtmDir(name + "/trcidr/trcidr4", &cpu_info.trcidr4) &&
          ReadValueInEtmDir(name + "/trcidr/trcidr8", &cpu_info.trcidr8) &&
          ReadValueInEtmDir(name + "/mgmt/trcauthstatus", &cpu_info.trcauthstatus);
      if (!success) {
        return false;
      }
    }
  }
  return (etm_info_.size() == cpu_count);
}

bool ETMRecorder::FindSinkConfig() {
  for (const auto &name : GetEntriesInDir(ETM_DIR + "sinks")) {
    if (name.find("etr") != -1) {
      if (ReadValueInEtmDir("sinks/" + name, &sink_config_)) {
        return true;
      }
    }
  }
  return false;
}

void ETMRecorder::SetEtmPerfEventAttr(perf_event_attr* attr) {
  CHECK(etm_supported_);
  BuildEtmConfig();
  attr->config = etm_event_config_;
  attr->config2 = sink_config_;
}

void ETMRecorder::BuildEtmConfig() {
  if (etm_event_config_ == 0) {
    etm_event_config_ |= 1ULL << ETM_OPT_CTXTID;
    etm_config_reg_ |= 1U << ETM4_CFG_BIT_CTXTID;

    if (ETM_RECORD_TIMESTAMP) {
      bool ts_supported = true;
      for (auto& p : etm_info_) {
        ts_supported &= p.second.IsTimestampSupported();
      }
      if (ts_supported) {
        etm_event_config_ |= 1ULL << ETM_OPT_TS;
        etm_config_reg_ |= 1U << ETM4_CFG_BIT_TS;
      }
    }
  }
}

AuxTraceInfoRecord ETMRecorder::CreateAuxTraceInfoRecord() {
  AuxTraceInfoRecord::DataType data;
  memset(&data, 0, sizeof(data));
  data.aux_type = AuxTraceInfoRecord::AUX_TYPE_ETM;
  data.nr_cpu = etm_info_.size();
  data.pmu_type = GetEtmEventType();
  std::vector<AuxTraceInfoRecord::ETM4Info> etm4_v(etm_info_.size());
  size_t pos = 0;
  for (auto& p : etm_info_) {
    auto& e = etm4_v[pos++];
    e.magic = AuxTraceInfoRecord::MAGIC_ETM4;
    e.cpu = p.first;
    e.trcconfigr = etm_config_reg_;
    e.trctraceidr = GetTraceId(p.first);
    e.trcidr0 = p.second.trcidr0;
    e.trcidr1 = p.second.trcidr1;
    e.trcidr2 = p.second.trcidr2;
    e.trcidr8 = p.second.trcidr8;
    e.trcauthstatus = p.second.trcauthstatus;
  }
  return AuxTraceInfoRecord(data, etm4_v);
}

size_t ETMRecorder::GetAddrFilterPairs() {
  CHECK(etm_supported_);
  size_t min_pairs = std::numeric_limits<size_t>::max();
  for (auto& p : etm_info_) {
    min_pairs = std::min<size_t>(min_pairs, GetBits(p.second.trcidr4, 0, 3));
  }
  if (min_pairs > 0) {
    --min_pairs;  // One pair is used by the kernel to set default addr filter.
  }
  return min_pairs;
}

}  // namespace simpleperf