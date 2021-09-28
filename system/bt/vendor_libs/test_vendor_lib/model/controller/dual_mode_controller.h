/*
 * Copyright 2015 The Android Open Source Project
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

#include <unistd.h>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/time/time.h"
#include "hci/address.h"
#include "hci/hci_packets.h"
#include "link_layer_controller.h"
#include "model/devices/device.h"
#include "model/setup/async_manager.h"
#include "security_manager.h"

namespace test_vendor_lib {

using ::bluetooth::hci::Address;
using ::bluetooth::hci::CommandPacketView;

// Emulates a dual mode BR/EDR + LE controller by maintaining the link layer
// state machine detailed in the Bluetooth Core Specification Version 4.2,
// Volume 6, Part B, Section 1.1 (page 30). Provides methods corresponding to
// commands sent by the HCI. These methods will be registered as callbacks from
// a controller instance with the HciHandler. To implement a new Bluetooth
// command, simply add the method declaration below, with return type void and a
// single const std::vector<uint8_t>& argument. After implementing the
// method, simply register it with the HciHandler using the SET_HANDLER macro in
// the controller's default constructor. Be sure to name your method after the
// corresponding Bluetooth command in the Core Specification with the prefix
// "Hci" to distinguish it as a controller command.
class DualModeController : public Device {
  // The location of the config file loaded to populate controller attributes.
  static constexpr char kControllerPropertiesFile[] = "/etc/bluetooth/controller_properties.json";
  static constexpr uint16_t kSecurityManagerNumKeys = 15;

 public:
  // Sets all of the methods to be used as callbacks in the HciHandler.
  DualModeController(const std::string& properties_filename = std::string(kControllerPropertiesFile),
                     uint16_t num_keys = kSecurityManagerNumKeys);

  ~DualModeController() = default;

  // Device methods.
  virtual void Initialize(const std::vector<std::string>& args) override;

  virtual std::string GetTypeString() const override;

  virtual void IncomingPacket(
      model::packets::LinkLayerPacketView incoming) override;

  virtual void TimerTick() override;

  // Route commands and data from the stack.
  void HandleAcl(std::shared_ptr<std::vector<uint8_t>> acl_packet);
  void HandleCommand(std::shared_ptr<std::vector<uint8_t>> command_packet);
  void HandleSco(std::shared_ptr<std::vector<uint8_t>> sco_packet);
  void HandleIso(std::shared_ptr<std::vector<uint8_t>> iso_packet);

  // Set the callbacks for scheduling tasks.
  void RegisterTaskScheduler(std::function<AsyncTaskId(std::chrono::milliseconds, const TaskCallback&)> evtScheduler);

  void RegisterPeriodicTaskScheduler(
      std::function<AsyncTaskId(std::chrono::milliseconds, std::chrono::milliseconds, const TaskCallback&)>
          periodicEvtScheduler);

  void RegisterTaskCancel(std::function<void(AsyncTaskId)> cancel);

  // Set the callbacks for sending packets to the HCI.
  void RegisterEventChannel(
      const std::function<void(std::shared_ptr<std::vector<uint8_t>>)>&
          send_event);

  void RegisterAclChannel(const std::function<void(std::shared_ptr<std::vector<uint8_t>>)>& send_acl);

  void RegisterScoChannel(const std::function<void(std::shared_ptr<std::vector<uint8_t>>)>& send_sco);

  void RegisterIsoChannel(
      const std::function<void(std::shared_ptr<std::vector<uint8_t>>)>&
          send_iso);

  // Set the device's address.
  void SetAddress(Address address) override;

  // Controller commands. For error codes, see the Bluetooth Core Specification,
  // Version 4.2, Volume 2, Part D (page 370).

  // Link Control Commands
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.1

  // 7.1.1
  void Inquiry(CommandPacketView args);

  // 7.1.2
  void InquiryCancel(CommandPacketView args);

  // 7.1.5
  void CreateConnection(CommandPacketView args);

  // 7.1.6
  void Disconnect(CommandPacketView args);

  // 7.1.8
  void AcceptConnectionRequest(CommandPacketView args);

  // 7.1.9
  void RejectConnectionRequest(CommandPacketView args);

  // 7.1.10
  void LinkKeyRequestReply(CommandPacketView args);

  // 7.1.11
  void LinkKeyRequestNegativeReply(CommandPacketView args);

  // 7.1.14
  void ChangeConnectionPacketType(CommandPacketView args);

  // 7.1.15
  void AuthenticationRequested(CommandPacketView args);

  // 7.1.16
  void SetConnectionEncryption(CommandPacketView args);

  // 7.1.17
  void ChangeConnectionLinkKey(CommandPacketView args);

  // 7.1.18
  void MasterLinkKey(CommandPacketView args);

  // 7.1.19
  void RemoteNameRequest(CommandPacketView args);

  // 7.2.8
  void SwitchRole(CommandPacketView args);

  // 7.1.21
  void ReadRemoteSupportedFeatures(CommandPacketView args);

  // 7.1.22
  void ReadRemoteExtendedFeatures(CommandPacketView args);

  // 7.1.23
  void ReadRemoteVersionInformation(CommandPacketView args);

  // 7.1.24
  void ReadClockOffset(CommandPacketView args);

  // 7.1.29
  void IoCapabilityRequestReply(CommandPacketView args);

  // 7.1.30
  void UserConfirmationRequestReply(CommandPacketView args);

  // 7.1.31
  void UserConfirmationRequestNegativeReply(CommandPacketView args);

  // 7.1.32
  void UserPasskeyRequestReply(CommandPacketView args);

  // 7.1.33
  void UserPasskeyRequestNegativeReply(CommandPacketView args);

  // 7.1.34
  void RemoteOobDataRequestReply(CommandPacketView args);

  // 7.1.35
  void RemoteOobDataRequestNegativeReply(CommandPacketView args);

  // 7.1.36
  void IoCapabilityRequestNegativeReply(CommandPacketView args);

  // Link Policy Commands
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.2

  // 7.2.1
  void HoldMode(CommandPacketView args);

  // 7.2.2
  void SniffMode(CommandPacketView args);

  // 7.2.3
  void ExitSniffMode(CommandPacketView args);

  // 7.2.6
  void QosSetup(CommandPacketView args);

  // 7.2.10
  void WriteLinkPolicySettings(CommandPacketView args);

  // 7.2.12
  void WriteDefaultLinkPolicySettings(CommandPacketView args);

  // 7.2.13
  void FlowSpecification(CommandPacketView args);

  // 7.2.14
  void SniffSubrating(CommandPacketView args);

  // Link Controller Commands
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3

  // 7.3.1
  void SetEventMask(CommandPacketView args);

  // 7.3.2
  void Reset(CommandPacketView args);

  // 7.3.3
  void SetEventFilter(CommandPacketView args);

  // 7.3.10
  void DeleteStoredLinkKey(CommandPacketView args);

  // 7.3.11
  void WriteLocalName(CommandPacketView args);

  // 7.3.12
  void ReadLocalName(CommandPacketView args);

  // 7.3.15
  void ReadPageTimeout(CommandPacketView args);

  // 7.3.16
  void WritePageTimeout(CommandPacketView args);

  // 7.3.17
  void ReadScanEnable(CommandPacketView args);

  // 7.3.18
  void WriteScanEnable(CommandPacketView args);

  // 7.3.19
  void ReadPageScanActivity(CommandPacketView args);

  // 7.3.20
  void WritePageScanActivity(CommandPacketView args);

  // 7.3.21
  void ReadInquiryScanActivity(CommandPacketView args);

  // 7.3.22
  void WriteInquiryScanActivity(CommandPacketView args);

  // 7.3.23
  void ReadAuthenticationEnable(CommandPacketView args);

  // 7.3.24
  void WriteAuthenticationEnable(CommandPacketView args);

  // 7.3.26
  void WriteClassOfDevice(CommandPacketView args);

  // 7.3.28
  void WriteVoiceSetting(CommandPacketView args);

  // 7.3.39
  void HostBufferSize(CommandPacketView args);

  // 7.3.42
  void WriteLinkSupervisionTimeout(CommandPacketView args);

  // 7.3.43
  void ReadNumberOfSupportedIac(CommandPacketView args);

  // 7.3.44
  void ReadCurrentIacLap(CommandPacketView args);

  // 7.3.45
  void WriteCurrentIacLap(CommandPacketView args);

  // 7.3.47
  void ReadInquiryScanType(CommandPacketView args);

  // 7.3.48
  void WriteInquiryScanType(CommandPacketView args);

  // 7.3.49
  void ReadInquiryMode(CommandPacketView args);

  // 7.3.50
  void WriteInquiryMode(CommandPacketView args);

  // 7.3.52
  void ReadPageScanType(CommandPacketView args);

  // 7.3.52
  void WritePageScanType(CommandPacketView args);

  // 7.3.56
  void WriteExtendedInquiryResponse(CommandPacketView args);

  // 7.3.57
  void RefreshEncryptionKey(CommandPacketView args);

  // 7.3.59
  void WriteSimplePairingMode(CommandPacketView args);

  // 7.3.61
  void ReadInquiryResponseTransmitPowerLevel(CommandPacketView args);

  // 7.3.79
  void WriteLeHostSupport(CommandPacketView args);

  // 7.3.92
  void WriteSecureConnectionsHostSupport(CommandPacketView args);

  // Informational Parameters Commands
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.4

  // 7.4.5
  void ReadBufferSize(CommandPacketView args);

  // 7.4.1
  void ReadLocalVersionInformation(CommandPacketView args);

  // 7.4.6
  void ReadBdAddr(CommandPacketView args);

  // 7.4.2
  void ReadLocalSupportedCommands(CommandPacketView args);

  // 7.4.3
  void ReadLocalSupportedFeatures(CommandPacketView args);

  // 7.4.4
  void ReadLocalExtendedFeatures(CommandPacketView args);

  // 7.4.8
  void ReadLocalSupportedCodecs(CommandPacketView args);

  // Status Parameters Commands
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.5

  // 7.5.7
  void ReadEncryptionKeySize(CommandPacketView args);

  // Test Commands
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.7

  // 7.7.1
  void ReadLoopbackMode(CommandPacketView args);

  // 7.7.2
  void WriteLoopbackMode(CommandPacketView args);

  // LE Controller Commands
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.8

  // 7.8.1
  void LeSetEventMask(CommandPacketView args);

  // 7.8.2
  void LeReadBufferSize(CommandPacketView args);

  // 7.8.3
  void LeReadLocalSupportedFeatures(CommandPacketView args);

  // 7.8.4
  void LeSetRandomAddress(CommandPacketView args);

  // 7.8.5
  void LeSetAdvertisingParameters(CommandPacketView args);

  // 7.8.7
  void LeSetAdvertisingData(CommandPacketView args);

  // 7.8.8
  void LeSetScanResponseData(CommandPacketView args);

  // 7.8.9
  void LeSetAdvertisingEnable(CommandPacketView args);

  // 7.8.10
  void LeSetScanParameters(CommandPacketView args);

  // 7.8.11
  void LeSetScanEnable(CommandPacketView args);

  // 7.8.12
  void LeCreateConnection(CommandPacketView args);

  // 7.8.18
  void LeConnectionUpdate(CommandPacketView args);

  // 7.8.13
  void LeConnectionCancel(CommandPacketView args);

  // 7.8.14
  void LeReadWhiteListSize(CommandPacketView args);

  // 7.8.15
  void LeClearWhiteList(CommandPacketView args);

  // 7.8.16
  void LeAddDeviceToWhiteList(CommandPacketView args);

  // 7.8.17
  void LeRemoveDeviceFromWhiteList(CommandPacketView args);

  // 7.8.21
  void LeReadRemoteFeatures(CommandPacketView args);

  // 7.8.23
  void LeRand(CommandPacketView args);

  // 7.8.24
  void LeStartEncryption(CommandPacketView args);

  // 7.8.27
  void LeReadSupportedStates(CommandPacketView args);

  // 7.8.38
  void LeAddDeviceToResolvingList(CommandPacketView args);

  // 7.8.39
  void LeRemoveDeviceFromResolvingList(CommandPacketView args);

  // 7.8.40
  void LeClearResolvingList(CommandPacketView args);

  // 7.8.52
  void LeSetExtendedAdvertisingRandomAddress(CommandPacketView args);

  // 7.8.53
  void LeSetExtendedAdvertisingParameters(CommandPacketView args);

  // 7.8.54
  void LeSetExtendedAdvertisingData(CommandPacketView args);

  // 7.8.55
  void LeSetExtendedAdvertisingScanResponse(CommandPacketView args);

  // 7.8.56
  void LeSetExtendedAdvertisingEnable(CommandPacketView args);

  // 7.8.64
  void LeSetExtendedScanParameters(CommandPacketView args);

  // 7.8.65
  void LeSetExtendedScanEnable(CommandPacketView args);

  // 7.8.66
  void LeExtendedCreateConnection(CommandPacketView args);

  // 7.8.77
  void LeSetPrivacyMode(CommandPacketView args);

  // Vendor-specific Commands

  void LeVendorSleepMode(CommandPacketView args);
  void LeVendorCap(CommandPacketView args);
  void LeVendorMultiAdv(CommandPacketView args);
  void LeVendor155(CommandPacketView args);
  void LeVendor157(CommandPacketView args);
  void LeEnergyInfo(CommandPacketView args);
  void LeAdvertisingFilter(CommandPacketView args);
  void LeExtendedScanParams(CommandPacketView args);

  void SetTimerPeriod(std::chrono::milliseconds new_period);
  void StartTimer();
  void StopTimer();

 protected:
  LinkLayerController link_layer_controller_{properties_};

 private:
  // Set a timer for a future action
  void AddControllerEvent(std::chrono::milliseconds, const TaskCallback& callback);

  void AddConnectionAction(const TaskCallback& callback, uint16_t handle);

  void SendCommandCompleteUnknownOpCodeEvent(uint16_t command_opcode) const;

  // Callbacks to send packets back to the HCI.
  std::function<void(std::shared_ptr<bluetooth::hci::AclPacketBuilder>)>
      send_acl_;
  std::function<void(std::shared_ptr<bluetooth::hci::EventPacketBuilder>)>
      send_event_;
  std::function<void(std::shared_ptr<std::vector<uint8_t>>)> send_sco_;
  std::function<void(std::shared_ptr<std::vector<uint8_t>>)> send_iso_;

  // Maintains the commands to be registered and used in the HciHandler object.
  // Keys are command opcodes and values are the callbacks to handle each
  // command.
  std::unordered_map<bluetooth::hci::OpCode,
                     std::function<void(bluetooth::hci::CommandPacketView)>>
      active_hci_commands_;

  bluetooth::hci::LoopbackMode loopback_mode_;

  SecurityManager security_manager_;

  DualModeController(const DualModeController& cmdPckt) = delete;
  DualModeController& operator=(const DualModeController& cmdPckt) = delete;
};

}  // namespace test_vendor_lib
