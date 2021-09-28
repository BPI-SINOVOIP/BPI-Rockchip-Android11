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

#pragma once

#include <inttypes.h>

#include <map>
#include <memory>

#include "event_type.h"
#include "record.h"
#include "perf_event.h"

namespace simpleperf {

struct ETMPerCpu {
  uint32_t trcidr0;
  uint32_t trcidr1;
  uint32_t trcidr2;
  uint32_t trcidr4;
  uint32_t trcidr8;
  uint32_t trcauthstatus;

  int GetMajorVersion() const;
  bool IsContextIDSupported() const;
  bool IsTimestampSupported() const;
};

// Help recording Coresight ETM data on ARM devices.
// 1. Get etm event type on device.
// 2. Get sink config, which selects the ETR device moving etm data to memory.
// 3. Get etm info on each cpu.
// The etm event type and sink config are used to build perf_event_attr for etm data tracing.
// The etm info is kept in perf.data to help etm decoding.
class ETMRecorder {
 public:
  static ETMRecorder& GetInstance();

  // If not found, return -1.
  int GetEtmEventType();
  std::unique_ptr<EventType> BuildEventType();
  bool CheckEtmSupport();
  void SetEtmPerfEventAttr(perf_event_attr* attr);
  AuxTraceInfoRecord CreateAuxTraceInfoRecord();
  size_t GetAddrFilterPairs();

 private:
  bool ReadEtmInfo();
  bool FindSinkConfig();
  void BuildEtmConfig();

  int event_type_ = 0;
  bool etm_supported_ = false;
  // select ETR device, setting in perf_event_attr->config2
  uint32_t sink_config_ = 0;
  // select etm options (timestamp, context_id, ...), setting in perf_event_attr->config
  uint64_t etm_event_config_ = 0;
  // record etm options in AuxTraceInfoRecord
  uint32_t etm_config_reg_ = 0;
  std::map<int, ETMPerCpu> etm_info_;
};

} // namespace simpleperf