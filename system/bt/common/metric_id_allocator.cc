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

#include <base/logging.h>
#include <functional>
#include <mutex>
#include <thread>

#include "metric_id_allocator.h"

namespace bluetooth {

namespace common {

const std::string MetricIdAllocator::LOGGING_TAG = "BluetoothMetricIdAllocator";
const size_t MetricIdAllocator::kMaxNumUnpairedDevicesInMemory = 200;
const size_t MetricIdAllocator::kMaxNumPairedDevicesInMemory = 65000;
const int MetricIdAllocator::kMinId = 1;
const int MetricIdAllocator::kMaxId = 65534;  // 2^16 - 2

// id space should always be larger than kMaxNumPairedDevicesInMemory +
// kMaxNumUnpairedDevicesInMemory
static_assert((MetricIdAllocator::kMaxNumUnpairedDevicesInMemory +
               MetricIdAllocator::kMaxNumPairedDevicesInMemory) <
                  (MetricIdAllocator::kMaxId - MetricIdAllocator::kMinId),
              "id space should always be larger than "
              "kMaxNumPairedDevicesInMemory + MaxNumUnpairedDevicesInMemory");

MetricIdAllocator::MetricIdAllocator()
    : paired_device_cache_(kMaxNumPairedDevicesInMemory, LOGGING_TAG),
      temporary_device_cache_(kMaxNumUnpairedDevicesInMemory, LOGGING_TAG) {}

bool MetricIdAllocator::Init(
    const std::unordered_map<RawAddress, int>& paired_device_map,
    Callback save_id_callback, Callback forget_device_callback) {
  std::lock_guard<std::mutex> lock(id_allocator_mutex_);
  if (initialized_) {
    return false;
  }

  // init paired_devices_map
  if (paired_device_map.size() > kMaxNumPairedDevicesInMemory) {
    LOG(FATAL)
        << LOGGING_TAG
        << "Paired device map is bigger than kMaxNumPairedDevicesInMemory";
    // fail loudly to let caller know
    return false;
  }

  next_id_ = kMinId;
  for (const auto& p : paired_device_map) {
    if (p.second < kMinId || p.second > kMaxId) {
      LOG(FATAL) << LOGGING_TAG << "Invalid Bluetooth Metric Id in config";
    }
    auto evicted = paired_device_cache_.Put(p.first, p.second);
    if (evicted) {
      ForgetDevicePostprocess(evicted->first, evicted->second);
    }
    id_set_.insert(p.second);
    next_id_ = std::max(next_id_, p.second + 1);
  }
  if (next_id_ > kMaxId) {
    next_id_ = kMinId;
  }

  // init callbacks
  save_id_callback_ = save_id_callback;
  forget_device_callback_ = forget_device_callback;

  return initialized_ = true;
}

MetricIdAllocator::~MetricIdAllocator() { Close(); }

bool MetricIdAllocator::Close() {
  std::lock_guard<std::mutex> lock(id_allocator_mutex_);
  if (!initialized_) {
    return false;
  }
  paired_device_cache_.Clear();
  temporary_device_cache_.Clear();
  id_set_.clear();
  initialized_ = false;
  return true;
}

MetricIdAllocator& MetricIdAllocator::GetInstance() {
  static MetricIdAllocator metric_id_allocator;
  return metric_id_allocator;
}

bool MetricIdAllocator::IsEmpty() const {
  std::lock_guard<std::mutex> lock(id_allocator_mutex_);
  return paired_device_cache_.Size() == 0 &&
         temporary_device_cache_.Size() == 0;
}

// call this function when a new device is scanned
int MetricIdAllocator::AllocateId(const RawAddress& mac_address) {
  std::lock_guard<std::mutex> lock(id_allocator_mutex_);
  int id = 0;
  // if already have an id, return it
  if (paired_device_cache_.Get(mac_address, &id)) {
    return id;
  }
  if (temporary_device_cache_.Get(mac_address, &id)) {
    return id;
  }

  // find next available id
  while (id_set_.count(next_id_) > 0) {
    next_id_++;
    if (next_id_ > kMaxId) {
      next_id_ = kMinId;
      LOG(WARNING) << LOGGING_TAG << "Bluetooth metric id overflow.";
    }
  }
  id = next_id_++;
  id_set_.insert(id);
  auto evicted = temporary_device_cache_.Put(mac_address, id);
  if (evicted) {
    this->id_set_.erase(evicted->second);
  }

  if (next_id_ > kMaxId) {
    next_id_ = kMinId;
  }
  return id;
}

// call this function when a device is paired
bool MetricIdAllocator::SaveDevice(const RawAddress& mac_address) {
  std::lock_guard<std::mutex> lock(id_allocator_mutex_);
  int id = 0;
  if (paired_device_cache_.Get(mac_address, &id)) {
    return true;
  }
  if (!temporary_device_cache_.Get(mac_address, &id)) {
    LOG(ERROR) << LOGGING_TAG
               << "Failed to save device because device is not in "
               << "temporary_device_cache_";
    return false;
  }
  if (!temporary_device_cache_.Remove(mac_address)) {
    LOG(ERROR) << LOGGING_TAG
               << "Failed to remove device from temporary_device_cache_";
    return false;
  }
  auto evicted = paired_device_cache_.Put(mac_address, id);
  if (evicted) {
    ForgetDevicePostprocess(evicted->first, evicted->second);
  }
  if (!save_id_callback_(mac_address, id)) {
    LOG(ERROR) << LOGGING_TAG
               << "Callback returned false after saving the device";
    return false;
  }
  return true;
}

// call this function when a device is forgotten
void MetricIdAllocator::ForgetDevice(const RawAddress& mac_address) {
  std::lock_guard<std::mutex> lock(id_allocator_mutex_);
  int id = 0;
  if (!paired_device_cache_.Get(mac_address, &id)) {
    LOG(ERROR) << LOGGING_TAG
               << "Failed to forget device because device is not in "
               << "paired_device_cache_";
    return;
  }
  if (!paired_device_cache_.Remove(mac_address)) {
    LOG(ERROR) << LOGGING_TAG
               << "Failed to remove device from paired_device_cache_";
    return;
  }
  ForgetDevicePostprocess(mac_address, id);
}

bool MetricIdAllocator::IsValidId(const int id) {
  return id >= kMinId && id <= kMaxId;
}

void MetricIdAllocator::ForgetDevicePostprocess(const RawAddress& mac_address,
                                                const int id) {
  id_set_.erase(id);
  forget_device_callback_(mac_address, id);
}

}  // namespace common
}  // namespace bluetooth
