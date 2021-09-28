//
// Copyright 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#define LOG_TAG "VtsHidlMemoryDriver"

#include "hidl_memory_driver/VtsHidlMemoryDriver.h"

#include <android-base/logging.h>
#include <android/hidl/allocator/1.0/IAllocator.h>
#include <hidlmemory/mapping.h>

using android::sp;
using android::hardware::hidl_memory;
using android::hidl::allocator::V1_0::IAllocator;
using android::hidl::memory::V1_0::IMemory;

using namespace std;

namespace android {
namespace vts {

VtsHidlMemoryDriver::VtsHidlMemoryDriver() {}

VtsHidlMemoryDriver::~VtsHidlMemoryDriver() {
  // clears objects in the map.
  hidl_memory_map_.clear();
}

MemoryId VtsHidlMemoryDriver::Allocate(size_t mem_size) {
  sp<IAllocator> ashmem_allocator = IAllocator::getService("ashmem");
  unique_ptr<MemoryInfo> mem_info = nullptr;
  ashmem_allocator->allocate(
      mem_size, [&](bool success, const hidl_memory& mem) {
        if (!success) {  // error
          LOG(ERROR) << "Allocate memory failure.";
        } else {
          unique_ptr<hidl_memory> hidl_mem_ptr(new hidl_memory(mem));
          sp<IMemory> mem_ptr = mapMemory(mem);
          mem_info.reset(new MemoryInfo{move(hidl_mem_ptr), mem_ptr});
        }
      });
  if (mem_info == nullptr) return -1;
  map_mutex_.lock();
  size_t new_mem_id = hidl_memory_map_.size();
  hidl_memory_map_.emplace(new_mem_id, move(mem_info));
  map_mutex_.unlock();
  return new_mem_id;
}

MemoryId VtsHidlMemoryDriver::RegisterHidlMemory(size_t hidl_mem_address) {
  unique_ptr<hidl_memory> hidl_mem_ptr(
      reinterpret_cast<hidl_memory*>(hidl_mem_address));
  sp<IMemory> mem_ptr = mapMemory(*hidl_mem_ptr.get());
  if (mem_ptr == nullptr) {
    LOG(ERROR) << "Register memory failure. "
               << "Unable to map hidl_memory to IMemory object.";
    return -1;
  }
  unique_ptr<MemoryInfo> mem_info(new MemoryInfo{move(hidl_mem_ptr), mem_ptr});
  map_mutex_.lock();
  size_t new_mem_id = hidl_memory_map_.size();
  hidl_memory_map_.emplace(new_mem_id, move(mem_info));
  map_mutex_.unlock();
  return new_mem_id;
}

bool VtsHidlMemoryDriver::Update(MemoryId mem_id) {
  MemoryInfo* mem_info = FindMemory(mem_id);
  if (mem_info == nullptr) return false;
  (mem_info->memory)->update();
  return true;
}

bool VtsHidlMemoryDriver::UpdateRange(MemoryId mem_id, uint64_t start,
                                      uint64_t length) {
  MemoryInfo* mem_info = FindMemory(mem_id);
  if (mem_info == nullptr) return false;
  (mem_info->memory)->updateRange(start, length);
  return true;
}

bool VtsHidlMemoryDriver::Read(MemoryId mem_id) {
  MemoryInfo* mem_info = FindMemory(mem_id);
  if (mem_info == nullptr) return false;
  (mem_info->memory)->read();
  return true;
}

bool VtsHidlMemoryDriver::ReadRange(MemoryId mem_id, uint64_t start,
                                    uint64_t length) {
  MemoryInfo* mem_info = FindMemory(mem_id);
  if (mem_info == nullptr) return false;
  (mem_info->memory)->readRange(start, length);
  return true;
}

bool VtsHidlMemoryDriver::UpdateBytes(MemoryId mem_id, const char* write_data,
                                      uint64_t length, uint64_t start) {
  MemoryInfo* mem_info = FindMemory(mem_id);
  if (mem_info == nullptr) return false;
  void* memory_ptr = (mem_info->memory)->getPointer();
  char* memory_char_ptr = static_cast<char*>(memory_ptr);
  memcpy(memory_char_ptr + start, write_data, length);
  return true;
}

bool VtsHidlMemoryDriver::ReadBytes(MemoryId mem_id, char* read_data,
                                    uint64_t length, uint64_t start) {
  MemoryInfo* mem_info = FindMemory(mem_id);
  if (mem_info == nullptr) return false;
  void* memory_ptr = (mem_info->memory)->getPointer();
  char* memory_char_ptr = static_cast<char*>(memory_ptr);
  memcpy(read_data, memory_char_ptr + start, length);
  return true;
}

bool VtsHidlMemoryDriver::Commit(MemoryId mem_id) {
  MemoryInfo* mem_info = FindMemory(mem_id);
  if (mem_info == nullptr) return false;
  (mem_info->memory)->commit();
  return true;
}

bool VtsHidlMemoryDriver::GetSize(MemoryId mem_id, size_t* result) {
  MemoryInfo* mem_info = FindMemory(mem_id);
  if (mem_info == nullptr) return false;
  *result = (mem_info->memory)->getSize();
  return true;
}

bool VtsHidlMemoryDriver::GetHidlMemoryAddress(MemoryId mem_id,
                                               size_t* result) {
  MemoryInfo* mem_info = FindMemory(mem_id);
  if (mem_info == nullptr) return false;  // unable to find memory object.
  hidl_memory* hidl_mem_ptr = (mem_info->hidl_mem_ptr).get();
  *result = reinterpret_cast<size_t>(hidl_mem_ptr);
  return true;
}

MemoryInfo* VtsHidlMemoryDriver::FindMemory(MemoryId mem_id) {
  MemoryInfo* mem_info;
  map_mutex_.lock();
  auto iterator = hidl_memory_map_.find(mem_id);
  if (iterator == hidl_memory_map_.end()) {
    LOG(ERROR) << "Unable to find memory region associated with mem_id "
               << mem_id;
    map_mutex_.unlock();
    return nullptr;
  }
  mem_info = (iterator->second).get();
  map_mutex_.unlock();
  return mem_info;
}

}  // namespace vts
}  // namespace android
