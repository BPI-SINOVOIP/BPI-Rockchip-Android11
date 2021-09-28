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

#include "osi/test/AllocationTestHarness.h"
#include "stack/gatt/gatt_int.h"
#undef LOG_TAG
#include "stack/gatt/gatt_sr.cc"
#include "types/raw_address.h"

#define MAX_UINT16 ((uint16_t)0xffff)

tGATT_CB gatt_cb;

namespace {

struct TestMutables {
  struct {
    uint8_t op_code_;
  } attp_build_sr_msg;
  struct {
    uint16_t conn_id_{0};
    uint32_t trans_id_{0};
    tGATTS_REQ_TYPE type_{0xff};
    tGATTS_DATA data_;
  } application_request_callback;
  struct {
    int access_count_{0};
    tGATT_STATUS return_status_{GATT_SUCCESS};
  } gatts_write_attr_perm_check;
};

TestMutables test_state_;
}  // namespace

namespace connection_manager {
bool background_connect_remove(uint8_t app_id, const RawAddress& address) {
  return false;
}
bool direct_connect_remove(uint8_t app_id, const RawAddress& address) {
  return false;
}
}  // namespace connection_manager

BT_HDR* attp_build_sr_msg(tGATT_TCB& tcb, uint8_t op_code,
                          tGATT_SR_MSG* p_msg) {
  test_state_.attp_build_sr_msg.op_code_ = op_code;
  return nullptr;
}
tGATT_STATUS attp_send_cl_msg(tGATT_TCB& tcb, tGATT_CLCB* p_clcb,
                              uint8_t op_code, tGATT_CL_MSG* p_msg) {
  return 0;
}
tGATT_STATUS attp_send_sr_msg(tGATT_TCB& tcb, BT_HDR* p_msg) { return 0; }
uint8_t btm_ble_read_sec_key_size(const RawAddress& bd_addr) { return 0; }
bool BTM_GetSecurityFlagsByTransport(const RawAddress& bd_addr,
                                     uint8_t* p_sec_flags,
                                     tBT_TRANSPORT transport) {
  return false;
}
void gatt_act_discovery(tGATT_CLCB* p_clcb) {}
bool gatt_disconnect(tGATT_TCB* p_tcb) { return false; }
tGATT_CH_STATE gatt_get_ch_state(tGATT_TCB* p_tcb) { return 0; }
tGATT_STATUS gatts_db_read_attr_value_by_type(
    tGATT_TCB& tcb, tGATT_SVC_DB* p_db, uint8_t op_code, BT_HDR* p_rsp,
    uint16_t s_handle, uint16_t e_handle, const Uuid& type, uint16_t* p_len,
    tGATT_SEC_FLAG sec_flag, uint8_t key_size, uint32_t trans_id,
    uint16_t* p_cur_handle) {
  return 0;
}
void gatt_set_ch_state(tGATT_TCB* p_tcb, tGATT_CH_STATE ch_state) {}
Uuid* gatts_get_service_uuid(tGATT_SVC_DB* p_db) { return nullptr; }
tGATT_STATUS GATTS_HandleValueIndication(uint16_t conn_id, uint16_t attr_handle,
                                         uint16_t val_len, uint8_t* p_val) {
  return GATT_SUCCESS;
}
tGATT_STATUS gatts_read_attr_perm_check(tGATT_SVC_DB* p_db, bool is_long,
                                        uint16_t handle,
                                        tGATT_SEC_FLAG sec_flag,
                                        uint8_t key_size) {
  return GATT_SUCCESS;
}
tGATT_STATUS gatts_read_attr_value_by_handle(
    tGATT_TCB& tcb, tGATT_SVC_DB* p_db, uint8_t op_code, uint16_t handle,
    uint16_t offset, uint8_t* p_value, uint16_t* p_len, uint16_t mtu,
    tGATT_SEC_FLAG sec_flag, uint8_t key_size, uint32_t trans_id) {
  return GATT_SUCCESS;
}
tGATT_STATUS gatts_write_attr_perm_check(tGATT_SVC_DB* p_db, uint8_t op_code,
                                         uint16_t handle, uint16_t offset,
                                         uint8_t* p_data, uint16_t len,
                                         tGATT_SEC_FLAG sec_flag,
                                         uint8_t key_size) {
  test_state_.gatts_write_attr_perm_check.access_count_++;
  return test_state_.gatts_write_attr_perm_check.return_status_;
}
void gatt_update_app_use_link_flag(tGATT_IF gatt_if, tGATT_TCB* p_tcb,
                                   bool is_add, bool check_acl_link) {}
base::MessageLoop* get_main_message_loop() { return nullptr; }
void l2cble_set_fixed_channel_tx_data_length(const RawAddress& remote_bda,
                                             uint16_t fix_cid,
                                             uint16_t tx_mtu) {}
bool SDP_AddAttribute(uint32_t handle, uint16_t attr_id, uint8_t attr_type,
                      uint32_t attr_len, uint8_t* p_val) {
  return false;
}
bool SDP_AddProtocolList(uint32_t handle, uint16_t num_elem,
                         tSDP_PROTOCOL_ELEM* p_elem_list) {
  return false;
}
bool SDP_AddServiceClassIdList(uint32_t handle, uint16_t num_services,
                               uint16_t* p_service_uuids) {
  return false;
}
bool SDP_AddUuidSequence(uint32_t handle, uint16_t attr_id, uint16_t num_uuids,
                         uint16_t* p_uuids) {
  return false;
}
uint32_t SDP_CreateRecord(void) { return 0; }

void ApplicationRequestCallback(uint16_t conn_id, uint32_t trans_id,
                                tGATTS_REQ_TYPE type, tGATTS_DATA* p_data) {
  test_state_.application_request_callback.conn_id_ = conn_id;
  test_state_.application_request_callback.trans_id_ = trans_id;
  test_state_.application_request_callback.type_ = type;
  test_state_.application_request_callback.data_ = *p_data;
}

/**
 * Test class to test selected functionality in stack/gatt/gatt_sr.cc
 */
extern void allocation_tracker_uninit(void);
namespace {
uint16_t kHandle = 1;
bt_gatt_db_attribute_type_t kGattCharacteristicType = BTGATT_DB_CHARACTERISTIC;
}  // namespace
class GattSrTest : public AllocationTestHarness {
 protected:
  void SetUp() override {
    AllocationTestHarness::SetUp();
    // Disable our allocation tracker to allow ASAN full range
    allocation_tracker_uninit();
    memset(&tcb_, 0, sizeof(tcb_));
    memset(&el_, 0, sizeof(el_));

    tcb_.trans_id = 0x12345677;
    el_.gatt_if = 1;
    gatt_cb.cl_rcb[el_.gatt_if - 1].in_use = true;
    gatt_cb.cl_rcb[el_.gatt_if - 1].app_cb.p_req_cb =
        ApplicationRequestCallback;

    test_state_ = TestMutables();
  }

  void TearDown() override { AllocationTestHarness::TearDown(); }

  tGATT_TCB tcb_;
  tGATT_SRV_LIST_ELEM el_;
};

TEST_F(GattSrTest, gatts_process_write_req_request_prepare_write_no_data) {
  gatts_process_write_req(tcb_, el_, kHandle, GATT_REQ_PREPARE_WRITE, 0,
                          nullptr, kGattCharacteristicType);
}

TEST_F(GattSrTest,
       gatts_process_write_req_request_prepare_write_max_len_no_data) {
  gatts_process_write_req(tcb_, el_, kHandle, GATT_REQ_PREPARE_WRITE,
                          MAX_UINT16, nullptr, kGattCharacteristicType);
}

TEST_F(GattSrTest,
       gatts_process_write_req_request_prepare_write_zero_len_max_data) {
  uint8_t max_mem[MAX_UINT16];
  gatts_process_write_req(tcb_, el_, kHandle, GATT_REQ_PREPARE_WRITE, 0,
                          max_mem, kGattCharacteristicType);
}

TEST_F(GattSrTest, gatts_process_write_req_request_prepare_write_typical) {
  uint8_t p_data[2] = {0x34, 0x12};
  uint16_t length = static_cast<uint16_t>(sizeof(p_data) / sizeof(p_data[0]));
  gatts_process_write_req(tcb_, el_, kHandle, GATT_REQ_PREPARE_WRITE, length,
                          p_data, kGattCharacteristicType);

  CHECK(test_state_.gatts_write_attr_perm_check.access_count_ == 1);
  CHECK(test_state_.application_request_callback.conn_id_ == el_.gatt_if);
  CHECK(test_state_.application_request_callback.trans_id_ == 0x12345678);
  CHECK(test_state_.application_request_callback.type_ ==
        GATTS_REQ_TYPE_WRITE_CHARACTERISTIC);
  CHECK(test_state_.application_request_callback.data_.write_req.offset ==
        0x1234);
  CHECK(test_state_.application_request_callback.data_.write_req.is_prep ==
        true);
  CHECK(test_state_.application_request_callback.data_.write_req.len == 0);
}

TEST_F(GattSrTest, gatts_process_write_req_signed_command_write_no_data) {
  gatts_process_write_req(tcb_, el_, kHandle, GATT_SIGN_CMD_WRITE, 0, nullptr,
                          kGattCharacteristicType);
}

TEST_F(GattSrTest,
       gatts_process_write_req_signed_command_write_max_len_no_data) {
  gatts_process_write_req(tcb_, el_, kHandle, GATT_SIGN_CMD_WRITE, MAX_UINT16,
                          nullptr, kGattCharacteristicType);
}

TEST_F(GattSrTest,
       gatts_process_write_req_signed_command_write_zero_len_max_data) {
  uint8_t max_mem[MAX_UINT16];
  gatts_process_write_req(tcb_, el_, kHandle, GATT_SIGN_CMD_WRITE, 0, max_mem,
                          kGattCharacteristicType);
}

TEST_F(GattSrTest, gatts_process_write_req_signed_command_write_typical) {
  static constexpr size_t kDataLength = 4;
  uint8_t p_data[GATT_AUTH_SIGN_LEN + kDataLength] = {
      0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
      0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x01};
  uint16_t length = static_cast<uint16_t>(sizeof(p_data) / sizeof(p_data[0]));
  gatts_process_write_req(tcb_, el_, kHandle, GATT_SIGN_CMD_WRITE, length,
                          p_data, kGattCharacteristicType);

  CHECK(test_state_.gatts_write_attr_perm_check.access_count_ == 1);
  CHECK(test_state_.application_request_callback.conn_id_ == el_.gatt_if);
  CHECK(test_state_.application_request_callback.trans_id_ == 0x12345678);
  CHECK(test_state_.application_request_callback.type_ ==
        GATTS_REQ_TYPE_WRITE_CHARACTERISTIC);
  CHECK(test_state_.application_request_callback.data_.write_req.offset == 0x0);
  CHECK(test_state_.application_request_callback.data_.write_req.is_prep ==
        false);
  CHECK(test_state_.application_request_callback.data_.write_req.len ==
        kDataLength);
}

TEST_F(GattSrTest, gatts_process_write_req_command_write_no_data) {
  gatts_process_write_req(tcb_, el_, kHandle, GATT_CMD_WRITE, 0, nullptr,
                          kGattCharacteristicType);
}

TEST_F(GattSrTest, gatts_process_write_req_command_write_max_len_no_data) {
  gatts_process_write_req(tcb_, el_, kHandle, GATT_CMD_WRITE, MAX_UINT16,
                          nullptr, kGattCharacteristicType);
}

TEST_F(GattSrTest, gatts_process_write_req_command_write_zero_len_max_data) {
  uint8_t max_mem[MAX_UINT16];
  gatts_process_write_req(tcb_, el_, kHandle, GATT_CMD_WRITE, 0, max_mem,
                          kGattCharacteristicType);
}

TEST_F(GattSrTest, gatts_process_write_req_command_write_typical) {
  uint8_t p_data[16] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                        0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x01};
  uint16_t length = static_cast<uint16_t>(sizeof(p_data) / sizeof(p_data[0]));
  gatts_process_write_req(tcb_, el_, kHandle, GATT_CMD_WRITE, length, p_data,
                          kGattCharacteristicType);

  CHECK(test_state_.gatts_write_attr_perm_check.access_count_ == 1);
  CHECK(test_state_.application_request_callback.conn_id_ == el_.gatt_if);
  CHECK(test_state_.application_request_callback.trans_id_ == 0x12345678);
  CHECK(test_state_.application_request_callback.type_ ==
        GATTS_REQ_TYPE_WRITE_CHARACTERISTIC);
  CHECK(test_state_.application_request_callback.data_.write_req.offset == 0x0);
  CHECK(test_state_.application_request_callback.data_.write_req.is_prep ==
        false);
  CHECK(test_state_.application_request_callback.data_.write_req.len == length);
}

TEST_F(GattSrTest, gatts_process_write_req_request_write_no_data) {
  gatts_process_write_req(tcb_, el_, kHandle, GATT_REQ_WRITE, 0, nullptr,
                          kGattCharacteristicType);
}

TEST_F(GattSrTest, gatts_process_write_req_request_write_max_len_no_data) {
  gatts_process_write_req(tcb_, el_, kHandle, GATT_REQ_WRITE, MAX_UINT16,
                          nullptr, kGattCharacteristicType);
}

TEST_F(GattSrTest, gatts_process_write_req_request_write_zero_len_max_data) {
  uint8_t max_mem[MAX_UINT16];
  gatts_process_write_req(tcb_, el_, kHandle, GATT_REQ_WRITE, 0, max_mem,
                          kGattCharacteristicType);
}

TEST_F(GattSrTest, gatts_process_write_req_request_write_typical) {
  uint8_t p_data[16] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                        0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x01};
  uint16_t length = static_cast<uint16_t>(sizeof(p_data) / sizeof(p_data[0]));

  gatts_process_write_req(tcb_, el_, kHandle, GATT_REQ_WRITE, length, p_data,
                          kGattCharacteristicType);

  CHECK(test_state_.gatts_write_attr_perm_check.access_count_ == 1);
  CHECK(test_state_.application_request_callback.conn_id_ == el_.gatt_if);
  CHECK(test_state_.application_request_callback.trans_id_ == 0x12345678);
  CHECK(test_state_.application_request_callback.type_ ==
        GATTS_REQ_TYPE_WRITE_CHARACTERISTIC);
  CHECK(test_state_.application_request_callback.data_.write_req.offset == 0x0);
  CHECK(test_state_.application_request_callback.data_.write_req.is_prep ==
        false);
  CHECK(test_state_.application_request_callback.data_.write_req.len == length);
}
