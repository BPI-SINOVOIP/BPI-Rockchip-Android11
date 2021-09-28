/******************************************************************************
 *
 *  Copyright 2019 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#pragma once

#include <string>

#include "hci/address.h"
#include "hci/class_of_device.h"

namespace bluetooth::hci {

/**
 * Used to determine device functionality
 */
enum DeviceType { DUAL, CLASSIC, LE };

/**
 * Represents a physical HCI device.
 *
 * <p>Contains all of the metadata required to represent a phycial device.
 *
 * <p>Devices should only be created and modified by HCI.
 */
class Device {
 public:
  virtual ~Device() = default;

  Address GetAddress() const {
    return address_;
  }

  /**
   * Returns 1 of 3 enum values for device's type (DUAL, CLASSIC, LE)
   */
  DeviceType GetDeviceType() const {
    return device_type_;
  }

  /**
   * Unique identifier for bluetooth devices
   *
   * @return string representation of the uuid
   */
  std::string /** use UUID when ported */ GetUuid() {
    return uid_;
  }

  std::string GetName() {
    return name_;
  }

  ClassOfDevice GetClassOfDevice() {
    return class_of_device_;
  }

  bool IsBonded() {
    return is_bonded_;
  }

  bool operator==(const Device& rhs) const {
    return this->uid_ == rhs.uid_ && this->address_ == rhs.address_ && this->device_type_ == rhs.device_type_ &&
           this->is_bonded_ == rhs.is_bonded_;
  }

 protected:
  friend class DeviceDatabase;
  friend class DualDevice;

  /**
   * @param raw_address the address of the device
   * @param device_type specify the type of device to create
   */
  Device(Address address, DeviceType device_type)
      : address_(address), device_type_(device_type), uid_(generate_uid()), name_(""), class_of_device_() {}

  /**
   * Called only by friend class DeviceDatabase
   *
   * @param address
   */
  virtual void SetAddress(Address address) {
    address_ = address;
    uid_ = generate_uid();
  }

  /**
   * Set the type of the device.
   *
   * <p>Needed by dual mode to arbitrarily set the valure to DUAL for corresponding LE/Classic devices
   *
   * @param type of device
   */
  void SetDeviceType(DeviceType type) {
    device_type_ = type;
  }

  void SetName(std::string& name) {
    name_ = name;
  }

  void SetClassOfDevice(ClassOfDevice class_of_device) {
    class_of_device_ = class_of_device;
  }

  void SetIsBonded(bool is_bonded) {
    is_bonded_ = is_bonded;
  }

 private:
  Address address_{Address::kEmpty};
  DeviceType device_type_;
  std::string uid_;
  std::string name_;
  ClassOfDevice class_of_device_;
  bool is_bonded_ = false;

  /* Uses specific information about the device to calculate a UID */
  std::string generate_uid();
};

}  // namespace bluetooth::hci
