/*
 * Copyright 2019 The Android Open Source Project
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

#include <memory>

#include "hci/hci_packets.h"
#include "module.h"
#include "neighbor/scan_parameters.h"

namespace bluetooth {
namespace neighbor {

using InquiryLength = uint8_t;  // Range: 0x01 to 0x30, 1.28 to 61.44s
using NumResponses = uint8_t;   // Range: 0x01 to 0xff, 0x00 is unlimited
using PeriodLength = uint16_t;  // Time = N * 1.28 s

using InquiryResultCallback = std::function<void(hci::InquiryResultView view)>;
using InquiryResultWithRssiCallback = std::function<void(hci::InquiryResultWithRssiView view)>;
using ExtendedInquiryResultCallback = std::function<void(hci::ExtendedInquiryResultView view)>;
using InquiryCompleteCallback = std::function<void(hci::ErrorCode status)>;

using InquiryCallbacks = struct {
  InquiryResultCallback result;
  InquiryResultWithRssiCallback result_with_rssi;
  ExtendedInquiryResultCallback extended_result;
  InquiryCompleteCallback complete;
};

class InquiryModule : public bluetooth::Module {
 public:
  void RegisterCallbacks(InquiryCallbacks inquiry_callbacks);
  void UnregisterCallbacks();

  void StartGeneralInquiry(InquiryLength inquiry_length, NumResponses num_responses);
  void StartLimitedInquiry(InquiryLength inquiry_length, NumResponses num_responses);
  void StopInquiry();

  void StartGeneralPeriodicInquiry(InquiryLength inquiry_length, NumResponses num_responses, PeriodLength max_delay,
                                   PeriodLength min_delay);
  void StartLimitedPeriodicInquiry(InquiryLength inquiry_length, NumResponses num_responses, PeriodLength max_delay,
                                   PeriodLength min_delay);
  void StopPeriodicInquiry();

  void SetScanActivity(ScanParameters parms);

  void SetInterlacedScan();
  void SetStandardScan();

  void SetStandardInquiryResultMode();
  void SetInquiryWithRssiResultMode();
  void SetExtendedInquiryResultMode();

  static const ModuleFactory Factory;

  InquiryModule();
  ~InquiryModule();

 protected:
  void ListDependencies(ModuleList* list) override;
  void Start() override;
  void Stop() override;

 private:
  struct impl;
  std::unique_ptr<impl> pimpl_;

  DISALLOW_COPY_AND_ASSIGN(InquiryModule);
};

}  // namespace neighbor
}  // namespace bluetooth
