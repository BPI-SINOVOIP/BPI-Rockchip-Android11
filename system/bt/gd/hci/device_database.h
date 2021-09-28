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

#include <map>
#include <mutex>

#include "hci/classic_device.h"
#include "hci/device.h"
#include "hci/dual_device.h"
#include "hci/le_device.h"
#include "os/log.h"

namespace bluetooth::hci {

/**
 * Stores all of the paired or connected devices in the database.
 *
 * <p>If a device is stored here it is actively being used by the stack.
 *
 * <p>This database is not meant for scan results.
 */
class DeviceDatabase {
 public:
  DeviceDatabase() : classic_device_map_(), le_device_map_(), dual_device_map_() {
    if (!ReadFromDisk()) {
      LOG_WARN("First boot or missing data!");
    }
  }

  /**
   * Adds a device to the internal memory map and triggers a WriteToDisk.
   *
   * @param address private address for device
   * @return weak pointer to the device or empty pointer if device already exists
   */
  std::shared_ptr<ClassicDevice> CreateClassicDevice(Address address);

  /**
   * Adds a device to the internal memory map and triggers a WriteToDisk.
   *
   * @param address private address for device
   * @return weak pointer to the device or empty pointer if device already exists
   */
  std::shared_ptr<LeDevice> CreateLeDevice(Address address);

  /**
   * Adds a device to the internal memory map and triggers a WriteToDisk.
   *
   * @param address private address for device
   * @return weak pointer to the device or empty pointer if device already exists
   */
  std::shared_ptr<DualDevice> CreateDualDevice(Address address);

  /**
   * Fetches a Classic Device matching the given uuid.
   *
   * @param uuid generated uuid from a Device
   * @return a weak reference to the matching Device or empty shared_ptr (nullptr)
   */
  std::shared_ptr<ClassicDevice> GetClassicDevice(const std::string& uuid);

  /**
   * Fetches a Le Device matching the given uuid.
   *
   * @param uuid generated uuid from a Device
   * @return a weak reference to the matching Device or empty shared_ptr (nullptr)
   */
  std::shared_ptr<LeDevice> GetLeDevice(const std::string& uuid);

  /**
   * Fetches a Dual Device matching the given uuid.
   *
   * @param uuid generated uuid from a Device
   * @return a weak reference to the matching Device or empty shared_ptr (nullptr)
   */
  std::shared_ptr<DualDevice> GetDualDevice(const std::string& uuid);

  /**
   * Removes a device from the internal database.
   *
   * @param device weak pointer to device to remove from the database
   * @return <code>true</code> if the device is removed
   */
  bool RemoveDevice(const std::shared_ptr<Device>& device);

  /**
   * Changes an address for a device.
   *
   * Also fixes the key mapping for the device.
   *
   * @param new_address this will replace the existing address
   * @return <code>true</code> if updated
   */
  bool UpdateDeviceAddress(const std::shared_ptr<Device>& device, Address new_address);

  // TODO(optedoblivion): Make interfaces for device modification
  // We want to keep the device modification encapsulated to the DeviceDatabase.
  // Pass around shared_ptr to device, device metadata only accessible via Getters.
  // Choices:
  //  a) Have Getters/Setters on device object
  //  b) Have Getters/Setters on device database accepting a device object
  //  c) Have Getters on device object and Setters on device database accepting a device object
  // I chose to go with option c for now as I think it is the best option.

  /**
   * Fetches a list of classic devices.
   *
   * @return vector of weak pointers to classic devices
   */
  std::vector<std::shared_ptr<Device>> GetClassicDevices();

  /**
   * Fetches a list of le devices
   *
   * @return vector of weak pointers to le devices
   */
  std::vector<std::shared_ptr<Device>> GetLeDevices();

 private:
  std::mutex device_map_mutex_;
  std::map<std::string, std::shared_ptr<ClassicDevice>> classic_device_map_;
  std::map<std::string, std::shared_ptr<LeDevice>> le_device_map_;
  std::map<std::string, std::shared_ptr<DualDevice>> dual_device_map_;

  bool AddDeviceToMap(ClassicDevice&& device);
  bool AddDeviceToMap(LeDevice&& device);
  bool AddDeviceToMap(DualDevice&& device);

  bool WriteToDisk();
  bool ReadFromDisk();
};

}  // namespace bluetooth::hci
