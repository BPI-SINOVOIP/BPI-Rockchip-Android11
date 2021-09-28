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
#include "hci/device_database.h"

#include <memory>
#include <utility>

#include "hci/classic_device.h"
#include "hci/dual_device.h"
#include "hci/le_device.h"
#include "os/log.h"

using namespace bluetooth::hci;

std::shared_ptr<ClassicDevice> DeviceDatabase::CreateClassicDevice(Address address) {
  ClassicDevice device(address);
  const std::string uuid = device.GetUuid();
  AddDeviceToMap(std::move(device));
  return GetClassicDevice(uuid);
}

std::shared_ptr<LeDevice> DeviceDatabase::CreateLeDevice(Address address) {
  LeDevice device(address);
  const std::string uuid = device.GetUuid();
  AddDeviceToMap(std::move(device));
  return GetLeDevice(uuid);
}

std::shared_ptr<DualDevice> DeviceDatabase::CreateDualDevice(Address address) {
  auto classic = CreateClassicDevice(address);
  auto le = CreateLeDevice(address);
  if (classic && le) {
    DualDevice device(address, classic, le);
    std::string uuid = device.GetUuid();
    AddDeviceToMap(std::move(device));
    return GetDualDevice(uuid);
  }
  LOG_WARN("Attempting to instert a DUAL device that already exists!");
  return std::shared_ptr<DualDevice>();
}

bool DeviceDatabase::RemoveDevice(const std::shared_ptr<Device>& device) {
  const DeviceType type = device->GetDeviceType();
  bool success;
  switch (type) {
    case CLASSIC:
      success = false;
      {
        std::lock_guard<std::mutex> lock(device_map_mutex_);
        auto classic_it = classic_device_map_.find(device->GetUuid());
        // If we have a record with the same key
        if (classic_it != classic_device_map_.end()) {
          classic_device_map_.erase(device->GetUuid());
          success = true;
        }
      }
      if (success) {
        ASSERT_LOG(WriteToDisk(), "Failed to write data to disk!");
      } else {
        LOG_WARN("Device not in database!");
      }
      return success;
    case LE:
      success = false;
      {
        std::lock_guard<std::mutex> lock(device_map_mutex_);
        auto le_it = le_device_map_.find(device->GetUuid());
        // If we have a record with the same key
        if (le_it != le_device_map_.end()) {
          le_device_map_.erase(device->GetUuid());
          success = true;
        }
      }
      if (success) {
        ASSERT_LOG(WriteToDisk(), "Failed to write data to disk!");
      } else {
        LOG_WARN("Device not in database!");
      }
      return success;
    case DUAL:
      std::shared_ptr<DualDevice> dual_device = nullptr;
      {
        std::lock_guard<std::mutex> lock(device_map_mutex_);
        auto dual_it = dual_device_map_.find(device->GetUuid());
        if (dual_it != dual_device_map_.end()) {
          dual_device = GetDualDevice(device->GetUuid());
        }
      }
      success = false;
      if (dual_device != nullptr) {
        if (RemoveDevice(dual_device->GetClassicDevice()) && RemoveDevice(dual_device->GetLeDevice())) {
          dual_device_map_.erase(device->GetUuid());
          success = true;
        }
      }
      if (success) {
        ASSERT_LOG(WriteToDisk(), "Failed to write data to disk!");
      } else {
        LOG_WARN("Device not in database!");
      }
      return success;
  }
}

std::shared_ptr<ClassicDevice> DeviceDatabase::GetClassicDevice(const std::string& uuid) {
  std::lock_guard<std::mutex> lock(device_map_mutex_);
  auto it = classic_device_map_.find(uuid);
  if (it != classic_device_map_.end()) {
    return it->second;
  }
  LOG_WARN("Device '%s' not found!", uuid.c_str());
  return std::shared_ptr<ClassicDevice>();
}

std::shared_ptr<LeDevice> DeviceDatabase::GetLeDevice(const std::string& uuid) {
  std::lock_guard<std::mutex> lock(device_map_mutex_);
  auto it = le_device_map_.find(uuid);
  if (it != le_device_map_.end()) {
    return it->second;
  }
  LOG_WARN("Device '%s' not found!", uuid.c_str());
  return std::shared_ptr<LeDevice>();
}

std::shared_ptr<DualDevice> DeviceDatabase::GetDualDevice(const std::string& uuid) {
  std::lock_guard<std::mutex> lock(device_map_mutex_);
  auto it = dual_device_map_.find(uuid);
  if (it != dual_device_map_.end()) {
    return it->second;
  }
  LOG_WARN("Device '%s' not found!", uuid.c_str());
  return std::shared_ptr<DualDevice>();
}

bool DeviceDatabase::UpdateDeviceAddress(const std::shared_ptr<Device>& device, Address new_address) {
  // Hold onto device
  const DeviceType type = device->GetDeviceType();
  if (type == CLASSIC) {
    auto classic_device = GetClassicDevice(device->GetUuid());
    // This gets rid of the shared_ptr in the map
    ASSERT_LOG(RemoveDevice(device), "Failed to remove the device!");
    classic_device->SetAddress(new_address);
    // Move the value located at the pointer
    return AddDeviceToMap(std::move(*(classic_device.get())));
  } else if (type == LE) {
    auto le_device = GetLeDevice(device->GetUuid());
    // This gets rid of the shared_ptr in the map
    ASSERT_LOG(RemoveDevice(device), "Failed to remove the device!");
    le_device->SetAddress(new_address);
    // Move the value located at the pointer
    return AddDeviceToMap(std::move(*(le_device.get())));
  } else if (type == DUAL) {
    auto dual_device = GetDualDevice(device->GetUuid());
    // This gets rid of the shared_ptr in the map
    ASSERT_LOG(RemoveDevice(device), "Failed to remove the device!");
    dual_device->SetAddress(new_address);
    // Move the value located at the pointer
    return AddDeviceToMap(std::move(*(dual_device.get())));
  }
  LOG_ALWAYS_FATAL("Someone added a device type but didn't account for it here.");
  return false;
}

bool DeviceDatabase::AddDeviceToMap(ClassicDevice&& device) {
  const std::string uuid = device.GetUuid();
  bool success = false;
  {
    std::lock_guard<std::mutex> lock(device_map_mutex_);
    auto it = classic_device_map_.find(device.GetUuid());
    // If we have a record with the same key
    if (it != classic_device_map_.end()) {
      // We don't want to insert and overwrite
      return false;
    }
    std::shared_ptr<ClassicDevice> device_ptr = std::make_shared<ClassicDevice>(std::move(device));
    // returning the boolean value of insert success
    if (classic_device_map_
            .insert(std::pair<std::string, std::shared_ptr<ClassicDevice>>(device_ptr->GetUuid(), device_ptr))
            .second) {
      success = true;
    }
  }
  if (success) {
    ASSERT_LOG(WriteToDisk(), "Failed to write data to disk!");
  } else {
    LOG_WARN("Failed to add device '%s' to map.", uuid.c_str());
  }
  return success;
}

bool DeviceDatabase::AddDeviceToMap(LeDevice&& device) {
  const std::string uuid = device.GetUuid();
  bool success = false;
  {
    std::lock_guard<std::mutex> lock(device_map_mutex_);
    auto it = le_device_map_.find(device.GetUuid());
    // If we have a record with the same key
    if (it != le_device_map_.end()) {
      // We don't want to insert and overwrite
      return false;
    }
    std::shared_ptr<LeDevice> device_ptr = std::make_shared<LeDevice>(std::move(device));
    // returning the boolean value of insert success
    if (le_device_map_.insert(std::pair<std::string, std::shared_ptr<LeDevice>>(device_ptr->GetUuid(), device_ptr))
            .second) {
      success = true;
    }
  }
  if (success) {
    ASSERT_LOG(WriteToDisk(), "Failed to write data to disk!");
  } else {
    LOG_WARN("Failed to add device '%s' to map.", uuid.c_str());
  }
  return success;
}

bool DeviceDatabase::AddDeviceToMap(DualDevice&& device) {
  const std::string uuid = device.GetUuid();
  bool success = false;
  {
    std::lock_guard<std::mutex> lock(device_map_mutex_);
    auto it = dual_device_map_.find(device.GetUuid());
    // If we have a record with the same key
    if (it != dual_device_map_.end()) {
      // We don't want to insert and overwrite
      return false;
    }
    std::shared_ptr<DualDevice> device_ptr = std::make_shared<DualDevice>(std::move(device));
    // returning the boolean value of insert success
    if (dual_device_map_.insert(std::pair<std::string, std::shared_ptr<DualDevice>>(device_ptr->GetUuid(), device_ptr))
            .second) {
      success = true;
    }
  }
  if (success) {
    ASSERT_LOG(WriteToDisk(), "Failed to write data to disk!");
  } else {
    LOG_WARN("Failed to add device '%s' to map.", uuid.c_str());
  }
  return success;
}

bool DeviceDatabase::WriteToDisk() {
  // TODO(optedoblivion): Implement
  // TODO(optedoblivion): FIX ME!
  // If synchronous stack dies before async write, we can miss adding device
  // post(WriteToDisk());
  // Current Solution: Synchronous disk I/O...
  std::lock_guard<std::mutex> lock(device_map_mutex_);
  // Collect information to sync to database
  // Create SQL query for insert/update
  // submit SQL
  return true;
}

bool DeviceDatabase::ReadFromDisk() {
  // TODO(optedoblivion): Implement
  // Current Solution: Synchronous disk I/O...
  std::lock_guard<std::mutex> lock(device_map_mutex_);
  return true;
}
