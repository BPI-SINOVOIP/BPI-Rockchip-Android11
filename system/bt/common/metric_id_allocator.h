/******************************************************************************
 *
 *  Copyright 2020 Google, Inc.
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

#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include "raw_address.h"

#include "lru.h"

namespace bluetooth {

namespace common {

class MetricIdAllocator {
 public:
  using Callback = std::function<bool(const RawAddress& address, const int id)>;

  static const size_t kMaxNumUnpairedDevicesInMemory;
  static const size_t kMaxNumPairedDevicesInMemory;

  static const int kMinId;
  static const int kMaxId;

  ~MetricIdAllocator();

  /**
   * Get the instance of singleton
   *
   * @return MetricIdAllocator&
   */
  static MetricIdAllocator& GetInstance();

  /**
   * Initialize the allocator
   *
   * @param paired_device_map map from mac_address to id already saved
   * in the disk before init
   * @param save_id_callback a callback that will be called after successfully
   * saving id for a paired device
   * @param forget_device_callback a callback that will be called after
   * successful id deletion for forgotten device,
   * @return true if successfully initialized
   */
  bool Init(const std::unordered_map<RawAddress, int>& paired_device_map,
            Callback save_id_callback, Callback forget_device_callback);

  /**
   * Close the allocator. should be called when Bluetooth process is killed
   *
   * @return true if successfully close
   */
  bool Close();

  /**
   * Check if no id saved in memory
   *
   * @return true if no id is saved
   */
  bool IsEmpty() const;

  /**
   * Allocate an id for a scanned device, or return the id if there is already
   * one
   *
   * @param mac_address mac address of Bluetooth device
   * @return the id of device
   */
  int AllocateId(const RawAddress& mac_address);

  /**
   * Save the id for a paired device
   *
   * @param mac_address mac address of Bluetooth device
   * @return true if save successfully
   */
  bool SaveDevice(const RawAddress& mac_address);

  /**
   * Delete the id for a device to be forgotten
   *
   * @param mac_address mac address of Bluetooth device
   */
  void ForgetDevice(const RawAddress& mac_address);

  /**
   * Check if an id is valid.
   * The id should be less than or equal to kMaxId and bigger than or equal to
   * kMinId
   *
   * @param mac_address mac address of Bluetooth device
   * @return true if delete successfully
   */
  static bool IsValidId(const int id);

 protected:
  // Singleton
  MetricIdAllocator();

 private:
  static const std::string LOGGING_TAG;
  mutable std::mutex id_allocator_mutex_;

  LruCache<RawAddress, int> paired_device_cache_;
  LruCache<RawAddress, int> temporary_device_cache_;
  std::unordered_set<int> id_set_;

  int next_id_{kMinId};
  bool initialized_{false};
  Callback save_id_callback_;
  Callback forget_device_callback_;

  void ForgetDevicePostprocess(const RawAddress& mac_address, const int id);

  // delete copy constructor for singleton
  MetricIdAllocator(MetricIdAllocator const&) = delete;
  MetricIdAllocator& operator=(MetricIdAllocator const&) = delete;
};

}  // namespace common
}  // namespace bluetooth
