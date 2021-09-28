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

#define LOG_TAG "bt_shim_controller"

#include "main/shim/controller.h"
#include "btcore/include/module.h"
#include "main/shim/entry.h"
#include "main/shim/shim.h"
#include "osi/include/future.h"
#include "osi/include/log.h"

#include "hci/controller.h"

using ::bluetooth::shim::GetController;

constexpr uint8_t kPageZero = 0;
constexpr uint8_t kPageOne = 1;
constexpr uint8_t kPageTwo = 2;
constexpr uint8_t kMaxFeaturePage = 3;

constexpr int kMaxSupportedCodecs = 8;  // MAX_LOCAL_SUPPORTED_CODECS_SIZE

constexpr uint8_t kPhyLe1M = 0x01;

/**
 * Interesting commands supported by controller
 */
constexpr int kReadRemoteExtendedFeatures = 0x41c;
constexpr int kEnhancedSetupSynchronousConnection = 0x428;
constexpr int kEnhancedAcceptSynchronousConnection = 0x429;
constexpr int kLeSetPrivacyMode = 0x204e;

constexpr int kHciDataPreambleSize = 4;  // #define HCI_DATA_PREAMBLE_SIZE 4

// Module lifecycle functions
static future_t* start_up(void);
static future_t* shut_down(void);

EXPORT_SYMBOL extern const module_t gd_controller_module = {
    .name = GD_CONTROLLER_MODULE,
    .init = nullptr,
    .start_up = start_up,
    .shut_down = shut_down,
    .clean_up = nullptr,
    .dependencies = {GD_SHIM_MODULE, nullptr}};

struct {
  bool ready;
  uint64_t feature[kMaxFeaturePage];
  uint64_t le_feature[kMaxFeaturePage];
  RawAddress raw_address;
  bt_version_t bt_version;
  uint8_t local_supported_codecs[kMaxSupportedCodecs];
  uint8_t number_of_local_supported_codecs;
  uint64_t le_supported_states;
  uint8_t phy;
} data_;

static future_t* start_up(void) {
  LOG_INFO(LOG_TAG, "%s Starting up", __func__);
  data_.ready = true;

  std::string string_address =
      GetController()->GetControllerMacAddress().ToString();
  RawAddress::FromString(string_address, data_.raw_address);

  data_.le_supported_states =
      bluetooth::shim::GetController()->GetControllerLeSupportedStates();

  LOG_INFO(LOG_TAG, "Mac address:%s", string_address.c_str());

  data_.phy = kPhyLe1M;

  return future_new_immediate(FUTURE_SUCCESS);
}

static future_t* shut_down(void) {
  data_.ready = false;
  return future_new_immediate(FUTURE_SUCCESS);
}

/**
 * Module methods
 */
#define BIT(x) (0x1ULL << (x))

static bool get_is_ready(void) { return data_.ready; }

static const RawAddress* get_address(void) { return &data_.raw_address; }

static const bt_version_t* get_bt_version(void) { return &data_.bt_version; }

static const bt_device_features_t* get_features_classic(int index) {
  CHECK(index >= 0 && index < kMaxFeaturePage);
  data_.feature[index] =
      bluetooth::shim::GetController()->GetControllerLocalExtendedFeatures(
          index);
  return (const bt_device_features_t*)&data_.feature[index];
}

static uint8_t get_last_features_classic_index(void) {
  return bluetooth::shim::GetController()
      ->GetControllerLocalExtendedFeaturesMaxPageNumber();
}

static uint8_t* get_local_supported_codecs(uint8_t* number_of_codecs) {
  CHECK(number_of_codecs != nullptr);
  if (data_.number_of_local_supported_codecs != 0) {
    *number_of_codecs = data_.number_of_local_supported_codecs;
    return data_.local_supported_codecs;
  }
  return (uint8_t*)nullptr;
}

static const bt_device_features_t* get_features_ble(void) {
  return (const bt_device_features_t*)&data_.le_feature[0];
}

static const uint8_t* get_ble_supported_states(void) {
  return (const uint8_t*)&data_.le_supported_states;
}

static bool supports_simple_pairing(void) {
  return GetController()->GetControllerLocalExtendedFeatures(kPageOne) &
         BIT(51);
}

static bool supports_secure_connections(void) {
  return GetController()->GetControllerLocalExtendedFeatures(kPageTwo) & BIT(8);
}

static bool supports_simultaneous_le_bredr(void) {
  return GetController()->GetControllerLocalExtendedFeatures(kPageZero) &
         BIT(49);
}

static bool supports_reading_remote_extended_features(void) {
  return GetController()->IsSupported(
      (bluetooth::hci::OpCode)kReadRemoteExtendedFeatures);
}

static bool supports_interlaced_inquiry_scan(void) {
  return GetController()->GetControllerLocalExtendedFeatures(kPageZero) &
         BIT(28);
}

static bool supports_rssi_with_inquiry_results(void) {
  return GetController()->GetControllerLocalExtendedFeatures(kPageZero) &
         BIT(28);
}

static bool supports_extended_inquiry_response(void) {
  return GetController()->GetControllerLocalExtendedFeatures(kPageZero) &
         BIT(48);
}

static bool supports_master_slave_role_switch(void) {
  return GetController()->GetControllerLocalExtendedFeatures(kPageZero) &
         BIT(5);
}

static bool supports_enhanced_setup_synchronous_connection(void) {
  return GetController()->IsSupported(
      (bluetooth::hci::OpCode)kEnhancedSetupSynchronousConnection);
}

static bool supports_enhanced_accept_synchronous_connection(void) {
  return GetController()->IsSupported(
      (bluetooth::hci::OpCode)kEnhancedAcceptSynchronousConnection);
}

static bool supports_ble(void) {
  return GetController()->GetControllerLocalExtendedFeatures(kPageOne) & BIT(1);
}

static bool supports_ble_privacy(void) {
  return GetController()->GetControllerLeLocalSupportedFeatures() & BIT(6);
}

static bool supports_ble_set_privacy_mode() {
  return GetController()->IsSupported(
      (bluetooth::hci::OpCode)kLeSetPrivacyMode);
}

static bool supports_ble_packet_extension(void) {
  return GetController()->GetControllerLeLocalSupportedFeatures() & BIT(5);
}

static bool supports_ble_connection_parameters_request(void) {
  return GetController()->GetControllerLeLocalSupportedFeatures() & BIT(2);
}

static bool supports_ble_2m_phy(void) {
  return GetController()->GetControllerLeLocalSupportedFeatures() & BIT(8);
}

static bool supports_ble_coded_phy(void) {
  return GetController()->GetControllerLeLocalSupportedFeatures() & BIT(11);
}

static bool supports_ble_extended_advertising(void) {
  return GetController()->GetControllerLeLocalSupportedFeatures() & BIT(12);
}

static bool supports_ble_periodic_advertising(void) {
  return GetController()->GetControllerLeLocalSupportedFeatures() & BIT(13);
}

static uint16_t get_acl_data_size_classic(void) {
  return GetController()->GetControllerAclPacketLength();
}

static uint16_t get_acl_data_size_ble(void) {
  ::bluetooth::hci::LeBufferSize le_buffer_size =
      GetController()->GetControllerLeBufferSize();
  return le_buffer_size.le_data_packet_length_;
}

static uint16_t get_acl_packet_size_classic(void) {
  return get_acl_data_size_classic() + kHciDataPreambleSize;
}

static uint16_t get_acl_packet_size_ble(void) {
  return get_acl_data_size_ble() + kHciDataPreambleSize;
}

static uint16_t get_ble_suggested_default_data_length(void) {
  LOG_WARN(LOG_TAG, "%s TODO Unimplemented", __func__);
  return 0;
}

static uint16_t get_ble_maximum_tx_data_length(void) {
  ::bluetooth::hci::LeMaximumDataLength le_maximum_data_length =
      GetController()->GetControllerLeMaximumDataLength();
  return le_maximum_data_length.supported_max_tx_octets_;
}

static uint16_t get_ble_maxium_advertising_data_length(void) {
  LOG_WARN(LOG_TAG, "%s TODO Unimplemented", __func__);
  return 0;
}

static uint8_t get_ble_number_of_supported_advertising_sets(void) {
  return GetController()->GetControllerLeNumberOfSupportedAdverisingSets();
}

static uint16_t get_acl_buffer_count_classic(void) {
  return GetController()->GetControllerNumAclPacketBuffers();
}

static uint8_t get_acl_buffer_count_ble(void) {
  LOG_WARN(LOG_TAG, "%s TODO Unimplemented", __func__);
  return 0;
}

static uint8_t get_ble_white_list_size(void) {
  LOG_WARN(LOG_TAG, "%s TODO Unimplemented", __func__);
  return 0;
}

static uint8_t get_ble_resolving_list_max_size(void) {
  LOG_WARN(LOG_TAG, "%s TODO Unimplemented", __func__);
  return 0;
}

static void set_ble_resolving_list_max_size(int resolving_list_max_size) {
  LOG_WARN(LOG_TAG, "%s TODO Unimplemented", __func__);
}

static uint8_t get_le_all_initiating_phys() { return data_.phy; }

static const controller_t interface = {
    get_is_ready,

    get_address,
    get_bt_version,

    get_features_classic,
    get_last_features_classic_index,

    get_features_ble,
    get_ble_supported_states,

    supports_simple_pairing,
    supports_secure_connections,
    supports_simultaneous_le_bredr,
    supports_reading_remote_extended_features,
    supports_interlaced_inquiry_scan,
    supports_rssi_with_inquiry_results,
    supports_extended_inquiry_response,
    supports_master_slave_role_switch,
    supports_enhanced_setup_synchronous_connection,
    supports_enhanced_accept_synchronous_connection,

    supports_ble,
    supports_ble_packet_extension,
    supports_ble_connection_parameters_request,
    supports_ble_privacy,
    supports_ble_set_privacy_mode,
    supports_ble_2m_phy,
    supports_ble_coded_phy,
    supports_ble_extended_advertising,
    supports_ble_periodic_advertising,

    get_acl_data_size_classic,
    get_acl_data_size_ble,

    get_acl_packet_size_classic,
    get_acl_packet_size_ble,
    get_ble_suggested_default_data_length,
    get_ble_maximum_tx_data_length,
    get_ble_maxium_advertising_data_length,
    get_ble_number_of_supported_advertising_sets,

    get_acl_buffer_count_classic,
    get_acl_buffer_count_ble,

    get_ble_white_list_size,

    get_ble_resolving_list_max_size,
    set_ble_resolving_list_max_size,
    get_local_supported_codecs,
    get_le_all_initiating_phys};

const controller_t* bluetooth::shim::controller_get_interface() {
  static bool loaded = false;
  if (!loaded) {
    loaded = true;
  }
  return &interface;
}
