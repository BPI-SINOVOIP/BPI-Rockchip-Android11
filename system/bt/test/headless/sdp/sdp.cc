/*
 * Copyright 2020 The Android Open Source Project
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

#define LOG_TAG "bt_headless_sdp"

#include <future>

#include "base/logging.h"     // LOG() stdout and android log
#include "osi/include/log.h"  // android log only
#include "stack/include/sdp_api.h"
#include "test/headless/get_options.h"
#include "test/headless/headless.h"
#include "test/headless/sdp/sdp.h"
#include "test/headless/sdp/sdp_db.h"
#include "types/raw_address.h"

using namespace bluetooth::test::headless;

static void bta_jv_start_discovery_callback(uint16_t result, void* user_data) {
  auto promise = static_cast<std::promise<uint16_t>*>(user_data);
  promise->set_value(result);
}

namespace {

struct sdp_error_code_s {
  const char* name;
  uint16_t error_code;
} sdp_error_code[] = {
    {"KsdpSuccess", 0},
    {"KsdpInvalidVersion", 0x0001},
    {"KsdpInvalidServRecHdl", 0x0002},
    {"KsdpInvalidReqSyntax", 0x0003},
    {"KsdpInvalidPduSize", 0x0004},
    {"KsdpInvalidContState", 0x0005},
    {"KsdpNoResources", 0x0006},
    {"KsdpDiRegFailed", 0x0007},
    {"KsdpDiDiscFailed", 0x0008},
    {"KsdpNoDiRecordFound", 0x0009},
    {"KsdpErrAttrNotPresent", 0x000a},
    {"KsdpIllegalParameter", 0x000b},
    {"KsdpNoRecsMatch", 0xFFF0},
    {"KsdpConnFailed", 0xFFF1},
    {"KsdpCfgFailed", 0xFFF2},
    {"KsdpGenericError", 0xFFF3},
    {"KsdpDbFull", 0xFFF4},
    {"KsdpInvalidPdu", 0xFFF5},
    {"KsdpSecurityErr", 0xFFF6},
    {"KsdpConnRejected", 0xFFF7},
    {"KsdpCancel", 0xFFF8},
};

const char* kUnknownText = "Unknown";

const char* SdpErrorCodeToString(uint16_t code) {
  for (size_t i = 0; i < sizeof(sdp_error_code) / sizeof(sdp_error_code_s);
       ++i) {
    if (sdp_error_code[i].error_code == code) {
      return sdp_error_code[i].name;
    }
  }
  return kUnknownText;
}

constexpr size_t kMaxDiscoveryRecords = 64;

int sdp_query_uuid(unsigned int num_loops, const RawAddress& raw_address,
                   const bluetooth::Uuid& uuid) {
  SdpDb sdp_discovery_db(kMaxDiscoveryRecords);

  if (!SDP_InitDiscoveryDb(sdp_discovery_db.RawPointer(),
                           sdp_discovery_db.Length(),
                           1,  // num_uuid,
                           &uuid, 0, nullptr)) {
    fprintf(stdout, "%s Unable to initialize sdp discovery\n", __func__);
    return -1;
  }

  std::promise<uint16_t> promise;
  auto future = promise.get_future();

  sdp_discovery_db.Print(stdout);

  if (!SDP_ServiceSearchAttributeRequest2(
          raw_address, sdp_discovery_db.RawPointer(),
          bta_jv_start_discovery_callback, (void*)&promise)) {
    fprintf(stdout, "%s Failed to start search attribute request\n", __func__);
    return -2;
  }

  uint16_t result = future.get();
  if (result != 0) {
    fprintf(stdout, "Failed search discovery result:%s\n",
            SdpErrorCodeToString(result));
    return result;
  }

  tSDP_DISC_REC* rec = SDP_FindServiceInDb(sdp_discovery_db.RawPointer(),
                                           uuid.As16Bit(), nullptr);
  if (rec == nullptr) {
    fprintf(stdout, "discovery record is null from:%s uuid:%s\n",
            raw_address.ToString().c_str(), uuid.ToString().c_str());
  } else {
    fprintf(stdout, "result:%d attr_id:%x from:%s uuid:%s\n", result,
            rec->p_first_attr->attr_id, rec->remote_bd_addr.ToString().c_str(),
            uuid.ToString().c_str());
  }
  return 0;
}

}  // namespace

int bluetooth::test::headless::Sdp::Run() {
  if (options_.loop_ < 1) {
    printf("This test requires at least a single loop\n");
    options_.Usage();
    return -1;
  }
  if (options_.device_.size() != 1) {
    printf("This test requires a single device specified\n");
    options_.Usage();
    return -1;
  }
  if (options_.uuid_.size() != 1) {
    printf("This test requires a single uuid specified\n");
    options_.Usage();
    return -1;
  }

  return RunOnHeadlessStack<int>([this]() {
    return sdp_query_uuid(options_.loop_, options_.device_.front(),
                          options_.uuid_.front());
  });
}
