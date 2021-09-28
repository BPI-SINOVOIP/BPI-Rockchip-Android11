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

#include "common/callback.h"
#include "hci/address.h"
#include "hci/hci_packets.h"
#include "module.h"
#include "os/handler.h"

namespace bluetooth {
namespace hci {

class Controller : public Module {
 public:
  Controller();
  virtual ~Controller();
  DISALLOW_COPY_AND_ASSIGN(Controller);

  virtual void RegisterCompletedAclPacketsCallback(
      common::Callback<void(uint16_t /* handle */, uint16_t /* num_packets */)> cb, os::Handler* handler);

  virtual std::string GetControllerLocalName() const;

  virtual LocalVersionInformation GetControllerLocalVersionInformation() const;

  virtual std::array<uint8_t, 64> GetControllerLocalSupportedCommands() const;

  virtual uint64_t GetControllerLocalSupportedFeatures() const;

  virtual uint8_t GetControllerLocalExtendedFeaturesMaxPageNumber() const;

  virtual uint64_t GetControllerLocalExtendedFeatures(uint8_t page_number) const;

  virtual uint16_t GetControllerAclPacketLength() const;

  virtual uint16_t GetControllerNumAclPacketBuffers() const;

  virtual uint8_t GetControllerScoPacketLength() const;

  virtual uint16_t GetControllerNumScoPacketBuffers() const;

  virtual Address GetControllerMacAddress() const;

  virtual void SetEventMask(uint64_t event_mask);

  virtual void Reset();

  virtual void SetEventFilterClearAll();

  virtual void SetEventFilterInquiryResultAllDevices();

  virtual void SetEventFilterInquiryResultClassOfDevice(ClassOfDevice class_of_device,
                                                        ClassOfDevice class_of_device_mask);

  virtual void SetEventFilterInquiryResultAddress(Address address);

  virtual void SetEventFilterConnectionSetupAllDevices(AutoAcceptFlag auto_accept_flag);

  virtual void SetEventFilterConnectionSetupClassOfDevice(ClassOfDevice class_of_device,
                                                          ClassOfDevice class_of_device_mask,
                                                          AutoAcceptFlag auto_accept_flag);

  virtual void SetEventFilterConnectionSetupAddress(Address address, AutoAcceptFlag auto_accept_flag);

  virtual void WriteLocalName(std::string local_name);

  virtual void HostBufferSize(uint16_t host_acl_data_packet_length, uint8_t host_synchronous_data_packet_length,
                              uint16_t host_total_num_acl_data_packets,
                              uint16_t host_total_num_synchronous_data_packets);

  // LE controller commands
  virtual void LeSetEventMask(uint64_t le_event_mask);

  virtual LeBufferSize GetControllerLeBufferSize() const;

  virtual uint64_t GetControllerLeLocalSupportedFeatures() const;

  virtual uint64_t GetControllerLeSupportedStates() const;

  virtual LeMaximumDataLength GetControllerLeMaximumDataLength() const;

  virtual uint16_t GetControllerLeMaximumAdvertisingDataLength() const;

  virtual uint8_t GetControllerLeNumberOfSupportedAdverisingSets() const;

  virtual VendorCapabilities GetControllerVendorCapabilities() const;

  virtual bool IsSupported(OpCode op_code) const;

  static const ModuleFactory Factory;

  static constexpr uint64_t kDefaultEventMask = 0x3dbfffffffffffff;

 protected:
  void ListDependencies(ModuleList* list) override;

  void Start() override;

  void Stop() override;

  std::string ToString() const override;

 private:
  struct impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace hci
}  // namespace bluetooth
