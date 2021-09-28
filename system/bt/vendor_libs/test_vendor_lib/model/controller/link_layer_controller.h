/*
 * Copyright 2017 The Android Open Source Project
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

#include "acl_connection_handler.h"
#include "hci/address.h"
#include "hci/hci_packets.h"
#include "include/hci.h"
#include "include/inquiry.h"
#include "include/phy.h"
#include "model/devices/device_properties.h"
#include "model/setup/async_manager.h"
#include "packets/link_layer_packets.h"
#include "security_manager.h"

namespace test_vendor_lib {

using ::bluetooth::hci::Address;
using ::bluetooth::hci::ErrorCode;
using ::bluetooth::hci::OpCode;

class LinkLayerController {
 public:
  static constexpr size_t kIrk_size = 16;

  LinkLayerController(const DeviceProperties& properties) : properties_(properties) {}
  ErrorCode SendCommandToRemoteByAddress(
      OpCode opcode, bluetooth::packet::PacketView<true> args,
      const Address& remote);
  ErrorCode SendCommandToRemoteByHandle(
      OpCode opcode, bluetooth::packet::PacketView<true> args, uint16_t handle);
  ErrorCode SendScoToRemote(bluetooth::hci::ScoPacketView sco_packet);
  ErrorCode SendAclToRemote(bluetooth::hci::AclPacketView acl_packet);

  void WriteSimplePairingMode(bool enabled);
  void StartSimplePairing(const Address& address);
  void AuthenticateRemoteStage1(const Address& address, PairingType pairing_type);
  void AuthenticateRemoteStage2(const Address& address);
  ErrorCode LinkKeyRequestReply(const Address& address,
                                const std::array<uint8_t, 16>& key);
  ErrorCode LinkKeyRequestNegativeReply(const Address& address);
  ErrorCode IoCapabilityRequestReply(const Address& peer, uint8_t io_capability,
                                     uint8_t oob_data_present_flag,
                                     uint8_t authentication_requirements);
  ErrorCode IoCapabilityRequestNegativeReply(const Address& peer,
                                             ErrorCode reason);
  ErrorCode UserConfirmationRequestReply(const Address& peer);
  ErrorCode UserConfirmationRequestNegativeReply(const Address& peer);
  ErrorCode UserPasskeyRequestReply(const Address& peer,
                                    uint32_t numeric_value);
  ErrorCode UserPasskeyRequestNegativeReply(const Address& peer);
  ErrorCode RemoteOobDataRequestReply(const Address& peer,
                                      const std::vector<uint8_t>& c,
                                      const std::vector<uint8_t>& r);
  ErrorCode RemoteOobDataRequestNegativeReply(const Address& peer);
  void HandleSetConnectionEncryption(const Address& address, uint16_t handle, uint8_t encryption_enable);
  ErrorCode SetConnectionEncryption(uint16_t handle, uint8_t encryption_enable);
  void HandleAuthenticationRequest(const Address& address, uint16_t handle);
  ErrorCode AuthenticationRequested(uint16_t handle);

  ErrorCode AcceptConnectionRequest(const Address& addr, bool try_role_switch);
  void MakeSlaveConnection(const Address& addr, bool try_role_switch);
  ErrorCode RejectConnectionRequest(const Address& addr, uint8_t reason);
  void RejectSlaveConnection(const Address& addr, uint8_t reason);
  ErrorCode CreateConnection(const Address& addr, uint16_t packet_type,
                             uint8_t page_scan_mode, uint16_t clock_offset,
                             uint8_t allow_role_switch);
  ErrorCode CreateConnectionCancel(const Address& addr);
  ErrorCode Disconnect(uint16_t handle, uint8_t reason);

 private:
  void DisconnectCleanup(uint16_t handle, uint8_t reason);

 public:
  void IncomingPacket(model::packets::LinkLayerPacketView incoming);

  void TimerTick();

  AsyncTaskId ScheduleTask(std::chrono::milliseconds delay_ms, const TaskCallback& task);

  void CancelScheduledTask(AsyncTaskId task);

  // Set the callbacks for sending packets to the HCI.
  void RegisterEventChannel(
      const std::function<void(
          std::shared_ptr<bluetooth::hci::EventPacketBuilder>)>& send_event);

  void RegisterAclChannel(
      const std::function<
          void(std::shared_ptr<bluetooth::hci::AclPacketBuilder>)>& send_acl);

  void RegisterScoChannel(const std::function<void(std::shared_ptr<std::vector<uint8_t>>)>& send_sco);

  void RegisterIsoChannel(
      const std::function<void(std::shared_ptr<std::vector<uint8_t>>)>&
          send_iso);

  void RegisterRemoteChannel(
      const std::function<void(
          std::shared_ptr<model::packets::LinkLayerPacketBuilder>, Phy::Type)>&
          send_to_remote);

  // Set the callbacks for scheduling tasks.
  void RegisterTaskScheduler(
      std::function<AsyncTaskId(std::chrono::milliseconds, const TaskCallback&)> event_scheduler);

  void RegisterPeriodicTaskScheduler(
      std::function<AsyncTaskId(std::chrono::milliseconds, std::chrono::milliseconds, const TaskCallback&)>
          periodic_event_scheduler);

  void RegisterTaskCancel(std::function<void(AsyncTaskId)> cancel);
  void Reset();
  void AddControllerEvent(std::chrono::milliseconds delay, const TaskCallback& task);

  void PageScan();
  void Connections();

  void LeAdvertising();

  void HandleLeConnection(AddressWithType addr, AddressWithType own_addr,
                          uint8_t role, uint16_t connection_interval,
                          uint16_t connection_latency,
                          uint16_t supervision_timeout);

  void LeWhiteListClear();
  void LeWhiteListAddDevice(Address addr, uint8_t addr_type);
  void LeWhiteListRemoveDevice(Address addr, uint8_t addr_type);
  bool LeWhiteListContainsDevice(Address addr, uint8_t addr_type);
  bool LeWhiteListFull();
  void LeResolvingListClear();
  void LeResolvingListAddDevice(Address addr, uint8_t addr_type,
                                std::array<uint8_t, kIrk_size> peerIrk,
                                std::array<uint8_t, kIrk_size> localIrk);
  void LeResolvingListRemoveDevice(Address addr, uint8_t addr_type);
  bool LeResolvingListContainsDevice(Address addr, uint8_t addr_type);
  bool LeResolvingListFull();
  void LeSetPrivacyMode(uint8_t address_type, Address addr, uint8_t mode);

  ErrorCode SetLeAdvertisingEnable(uint8_t le_advertising_enable) {
    le_advertising_enable_ = le_advertising_enable;
    // TODO: Check properties and return errors
    return ErrorCode::SUCCESS;
  }

  void SetLeScanEnable(bluetooth::hci::OpCode enabling_opcode) {
    le_scan_enable_ = enabling_opcode;
  }
  void SetLeScanType(uint8_t le_scan_type) {
    le_scan_type_ = le_scan_type;
  }
  void SetLeScanInterval(uint16_t le_scan_interval) {
    le_scan_interval_ = le_scan_interval;
  }
  void SetLeScanWindow(uint16_t le_scan_window) {
    le_scan_window_ = le_scan_window;
  }
  void SetLeScanFilterPolicy(uint8_t le_scan_filter_policy) {
    le_scan_filter_policy_ = le_scan_filter_policy;
  }
  void SetLeFilterDuplicates(uint8_t le_scan_filter_duplicates) {
    le_scan_filter_duplicates_ = le_scan_filter_duplicates;
  }
  void SetLeAddressType(uint8_t le_address_type) {
    le_address_type_ = le_address_type;
  }
  ErrorCode SetLeConnect(bool le_connect) {
    le_connect_ = le_connect;
    return ErrorCode::SUCCESS;
  }
  void SetLeConnectionIntervalMin(uint16_t min) {
    le_connection_interval_min_ = min;
  }
  void SetLeConnectionIntervalMax(uint16_t max) {
    le_connection_interval_max_ = max;
  }
  void SetLeConnectionLatency(uint16_t latency) {
    le_connection_latency_ = latency;
  }
  void SetLeSupervisionTimeout(uint16_t timeout) {
    le_connection_supervision_timeout_ = timeout;
  }
  void SetLeMinimumCeLength(uint16_t min) {
    le_connection_minimum_ce_length_ = min;
  }
  void SetLeMaximumCeLength(uint16_t max) {
    le_connection_maximum_ce_length_ = max;
  }
  void SetLeInitiatorFilterPolicy(uint8_t le_initiator_filter_policy) {
    le_initiator_filter_policy_ = le_initiator_filter_policy;
  }
  void SetLePeerAddressType(uint8_t peer_address_type) {
    le_peer_address_type_ = peer_address_type;
  }
  void SetLePeerAddress(const Address& peer_address) {
    le_peer_address_ = peer_address;
  }

  // Classic
  void StartInquiry(std::chrono::milliseconds timeout);
  void InquiryCancel();
  void InquiryTimeout();
  void SetInquiryMode(uint8_t mode);
  void SetInquiryLAP(uint64_t lap);
  void SetInquiryMaxResponses(uint8_t max);
  void Inquiry();

  void SetInquiryScanEnable(bool enable);
  void SetPageScanEnable(bool enable);

  ErrorCode ChangeConnectionPacketType(uint16_t handle, uint16_t types);
  ErrorCode ChangeConnectionLinkKey(uint16_t handle);
  ErrorCode MasterLinkKey(uint8_t key_flag);
  ErrorCode HoldMode(uint16_t handle, uint16_t hold_mode_max_interval,
                     uint16_t hold_mode_min_interval);
  ErrorCode SniffMode(uint16_t handle, uint16_t sniff_max_interval,
                      uint16_t sniff_min_interval, uint16_t sniff_attempt,
                      uint16_t sniff_timeout);
  ErrorCode ExitSniffMode(uint16_t handle);
  ErrorCode QosSetup(uint16_t handle, uint8_t service_type, uint32_t token_rate,
                     uint32_t peak_bandwidth, uint32_t latency,
                     uint32_t delay_variation);
  ErrorCode SwitchRole(Address bd_addr, uint8_t role);
  ErrorCode WriteLinkPolicySettings(uint16_t handle, uint16_t settings);
  ErrorCode FlowSpecification(uint16_t handle, uint8_t flow_direction,
                              uint8_t service_type, uint32_t token_rate,
                              uint32_t token_bucket_size,
                              uint32_t peak_bandwidth, uint32_t access_latency);
  ErrorCode WriteLinkSupervisionTimeout(uint16_t handle, uint16_t timeout);

 protected:
  void SendLeLinkLayerPacket(
      std::unique_ptr<model::packets::LinkLayerPacketBuilder> packet);
  void SendLinkLayerPacket(
      std::unique_ptr<model::packets::LinkLayerPacketBuilder> packet);
  void IncomingAclPacket(model::packets::LinkLayerPacketView packet);
  void IncomingAclAckPacket(model::packets::LinkLayerPacketView packet);
  void IncomingCreateConnectionPacket(
      model::packets::LinkLayerPacketView packet);
  void IncomingDisconnectPacket(model::packets::LinkLayerPacketView packet);
  void IncomingEncryptConnection(model::packets::LinkLayerPacketView packet);
  void IncomingEncryptConnectionResponse(
      model::packets::LinkLayerPacketView packet);
  void IncomingInquiryPacket(model::packets::LinkLayerPacketView packet);
  void IncomingInquiryResponsePacket(
      model::packets::LinkLayerPacketView packet);
  void IncomingIoCapabilityRequestPacket(
      model::packets::LinkLayerPacketView packet);
  void IncomingIoCapabilityResponsePacket(
      model::packets::LinkLayerPacketView packet);
  void IncomingIoCapabilityNegativeResponsePacket(
      model::packets::LinkLayerPacketView packet);
  void IncomingLeAdvertisementPacket(
      model::packets::LinkLayerPacketView packet);
  void IncomingLeConnectPacket(model::packets::LinkLayerPacketView packet);
  void IncomingLeConnectCompletePacket(
      model::packets::LinkLayerPacketView packet);
  void IncomingLeScanPacket(model::packets::LinkLayerPacketView packet);
  void IncomingLeScanResponsePacket(model::packets::LinkLayerPacketView packet);
  void IncomingPagePacket(model::packets::LinkLayerPacketView packet);
  void IncomingPageRejectPacket(model::packets::LinkLayerPacketView packet);
  void IncomingPageResponsePacket(model::packets::LinkLayerPacketView packet);
  void IncomingReadRemoteLmpFeatures(
      model::packets::LinkLayerPacketView packet);
  void IncomingReadRemoteLmpFeaturesResponse(
      model::packets::LinkLayerPacketView packet);
  void IncomingReadRemoteSupportedFeatures(
      model::packets::LinkLayerPacketView packet);
  void IncomingReadRemoteSupportedFeaturesResponse(
      model::packets::LinkLayerPacketView packet);
  void IncomingReadRemoteExtendedFeatures(
      model::packets::LinkLayerPacketView packet);
  void IncomingReadRemoteExtendedFeaturesResponse(
      model::packets::LinkLayerPacketView packet);
  void IncomingReadRemoteVersion(model::packets::LinkLayerPacketView packet);
  void IncomingReadRemoteVersionResponse(
      model::packets::LinkLayerPacketView packet);
  void IncomingReadClockOffset(model::packets::LinkLayerPacketView packet);
  void IncomingReadClockOffsetResponse(
      model::packets::LinkLayerPacketView packet);
  void IncomingRemoteNameRequest(model::packets::LinkLayerPacketView packet);
  void IncomingRemoteNameRequestResponse(
      model::packets::LinkLayerPacketView packet);

 private:
  const DeviceProperties& properties_;
  AclConnectionHandler connections_;
  // Add timestamps?
  std::vector<std::shared_ptr<model::packets::LinkLayerPacketBuilder>>
      commands_awaiting_responses_;

  // Timing related state
  std::vector<AsyncTaskId> controller_events_;
  AsyncTaskId timer_tick_task_;
  std::chrono::milliseconds timer_period_ = std::chrono::milliseconds(100);

  // Callbacks to schedule tasks.
  std::function<AsyncTaskId(std::chrono::milliseconds, const TaskCallback&)> schedule_task_;
  std::function<AsyncTaskId(std::chrono::milliseconds, std::chrono::milliseconds, const TaskCallback&)>
      schedule_periodic_task_;
  std::function<void(AsyncTaskId)> cancel_task_;

  // Callbacks to send packets back to the HCI.
  std::function<void(std::shared_ptr<bluetooth::hci::AclPacketBuilder>)>
      send_acl_;
  std::function<void(std::shared_ptr<bluetooth::hci::EventPacketBuilder>)>
      send_event_;
  std::function<void(std::shared_ptr<std::vector<uint8_t>>)> send_sco_;
  std::function<void(std::shared_ptr<std::vector<uint8_t>>)> send_iso_;

  // Callback to send packets to remote devices.
  std::function<void(std::shared_ptr<model::packets::LinkLayerPacketBuilder>,
                     Phy::Type phy_type)>
      send_to_remote_;

  // LE state
  std::vector<uint8_t> le_event_mask_;

  std::vector<std::tuple<Address, uint8_t>> le_white_list_;
  std::vector<std::tuple<Address, uint8_t, std::array<uint8_t, kIrk_size>,
                         std::array<uint8_t, kIrk_size>>>
      le_resolving_list_;

  uint8_t le_advertising_enable_{false};
  std::chrono::steady_clock::time_point last_le_advertisement_;

  bluetooth::hci::OpCode le_scan_enable_{bluetooth::hci::OpCode::NONE};
  uint8_t le_scan_type_;
  uint16_t le_scan_interval_;
  uint16_t le_scan_window_;
  uint8_t le_scan_filter_policy_;
  uint8_t le_scan_filter_duplicates_;
  uint8_t le_address_type_;

  bool le_connect_{false};
  uint16_t le_connection_interval_min_;
  uint16_t le_connection_interval_max_;
  uint16_t le_connection_latency_;
  uint16_t le_connection_supervision_timeout_;
  uint16_t le_connection_minimum_ce_length_;
  uint16_t le_connection_maximum_ce_length_;
  uint8_t le_initiator_filter_policy_;

  Address le_peer_address_;
  uint8_t le_peer_address_type_;

  // Classic state

  SecurityManager security_manager_{10};
  std::chrono::steady_clock::time_point last_inquiry_;
  model::packets::InquiryType inquiry_mode_;
  Inquiry::InquiryState inquiry_state_;
  uint64_t inquiry_lap_;
  uint8_t inquiry_max_responses_;

  bool page_scans_enabled_{false};
  bool inquiry_scans_enabled_{false};

  bool simple_pairing_mode_enabled_{false};
};

}  // namespace test_vendor_lib
