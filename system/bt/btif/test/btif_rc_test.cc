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

#include <base/logging.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include <cstdint>

#include "bta/include/bta_av_api.h"
#include "btif/include/btif_common.h"
#include "device/include/interop.h"
#include "include/hardware/bt_rc.h"
#include "osi/test/AllocationTestHarness.h"
#include "stack/include/btm_api_types.h"
#include "types/raw_address.h"
#undef LOG_TAG
#include "btif/src/btif_rc.cc"

extern void allocation_tracker_uninit(void);

namespace {
int AVRC_BldResponse_ = 0;
}  // namespace

uint8_t appl_trace_level = BT_TRACE_LEVEL_WARNING;
uint8_t btif_trace_level = BT_TRACE_LEVEL_WARNING;

tAVRC_STS AVRC_BldCommand(tAVRC_COMMAND* p_cmd, BT_HDR** pp_pkt) { return 0; }
tAVRC_STS AVRC_BldResponse(uint8_t handle, tAVRC_RESPONSE* p_rsp,
                           BT_HDR** pp_pkt) {
  AVRC_BldResponse_++;
  return 0;
}
tAVRC_STS AVRC_Ctrl_ParsCommand(tAVRC_MSG* p_msg, tAVRC_COMMAND* p_result) {
  return 0;
}
tAVRC_STS AVRC_Ctrl_ParsResponse(tAVRC_MSG* p_msg, tAVRC_RESPONSE* p_result,
                                 uint8_t* p_buf, uint16_t* buf_len) {
  return 0;
}
tAVRC_STS AVRC_ParsCommand(tAVRC_MSG* p_msg, tAVRC_COMMAND* p_result,
                           uint8_t* p_buf, uint16_t buf_len) {
  return 0;
}
tAVRC_STS AVRC_ParsResponse(tAVRC_MSG* p_msg, tAVRC_RESPONSE* p_result,
                            UNUSED_ATTR uint8_t* p_buf,
                            UNUSED_ATTR uint16_t buf_len) {
  return 0;
}
void BTA_AvCloseRc(uint8_t rc_handle) {}
void BTA_AvMetaCmd(uint8_t rc_handle, uint8_t label, tBTA_AV_CMD cmd_code,
                   BT_HDR* p_pkt) {}
void BTA_AvMetaRsp(uint8_t rc_handle, uint8_t label, tBTA_AV_CODE rsp_code,
                   BT_HDR* p_pkt) {}
void BTA_AvRemoteCmd(uint8_t rc_handle, uint8_t label, tBTA_AV_RC rc_id,
                     tBTA_AV_STATE key_state) {}
void BTA_AvRemoteVendorUniqueCmd(uint8_t rc_handle, uint8_t label,
                                 tBTA_AV_STATE key_state, uint8_t* p_msg,
                                 uint8_t buf_len) {}
void BTA_AvVendorCmd(uint8_t rc_handle, uint8_t label, tBTA_AV_CODE cmd_code,
                     uint8_t* p_data, uint16_t len) {}
void BTA_AvVendorRsp(uint8_t rc_handle, uint8_t label, tBTA_AV_CODE rsp_code,
                     uint8_t* p_data, uint16_t len, uint32_t company_id) {}
void btif_av_clear_remote_suspend_flag(void) {}
bool btif_av_is_connected(void) { return false; }
bool btif_av_is_sink_enabled(void) { return false; }
RawAddress btif_av_sink_active_peer(void) { return RawAddress(); }
RawAddress btif_av_source_active_peer(void) { return RawAddress(); }
bool btif_av_stream_started_ready(void) { return false; }
bt_status_t btif_transfer_context(tBTIF_CBACK* p_cback, uint16_t event,
                                  char* p_params, int param_len,
                                  tBTIF_COPY_CBACK* p_copy_cback) {
  return BT_STATUS_SUCCESS;
}
const char* dump_rc_event(uint8_t event) { return nullptr; }
const char* dump_rc_notification_event_id(uint8_t event_id) { return nullptr; }
const char* dump_rc_pdu(uint8_t pdu) { return nullptr; }
bt_status_t do_in_jni_thread(const base::Location& from_here,
                             base::OnceClosure task) {
  return BT_STATUS_SUCCESS;
}
base::MessageLoop* get_main_message_loop() { return nullptr; }
bool interop_match_addr(const interop_feature_t feature,
                        const RawAddress* addr) {
  return false;
}
void LogMsg(uint32_t trace_set_mask, const char* fmt_str, ...) {}

/**
 * Test class to test selected functionality in hci/src/hci_layer.cc
 */
class BtifRcTest : public AllocationTestHarness {
 protected:
  void SetUp() override {
    AllocationTestHarness::SetUp();
    // Disable our allocation tracker to allow ASAN full range
    allocation_tracker_uninit();
  }

  void TearDown() override { AllocationTestHarness::TearDown(); }
};

TEST_F(BtifRcTest, get_element_attr_rsp) {
  RawAddress bd_addr;

  btif_rc_cb.rc_multi_cb[0].rc_addr = bd_addr;
  btif_rc_cb.rc_multi_cb[0].rc_connected = true;
  btif_rc_cb.rc_multi_cb[0]
      .rc_pdu_info[IDX_GET_ELEMENT_ATTR_RSP]
      .is_rsp_pending = true;
  btif_rc_cb.rc_multi_cb[0].rc_state = BTRC_CONNECTION_STATE_CONNECTED;

  btrc_element_attr_val_t p_attrs[BTRC_MAX_ELEM_ATTR_SIZE];
  uint8_t num_attr = BTRC_MAX_ELEM_ATTR_SIZE + 1;

  CHECK(get_element_attr_rsp(bd_addr, num_attr, p_attrs) == BT_STATUS_SUCCESS);
  CHECK(AVRC_BldResponse_ == 1);
}
