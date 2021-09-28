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

#ifndef __VTS_RESOURCE_VTSHIDLMEMORYDRIVER_H
#define __VTS_RESOURCE_VTSHIDLMEMORYDRIVER_H

#include <mutex>
#include <unordered_map>

#include <android-base/logging.h>
#include <android/hidl/memory/1.0/IMemory.h>
#include <hidl/HidlSupport.h>

using android::sp;
using android::hardware::hidl_memory;
using android::hidl::memory::V1_0::IMemory;

using namespace std;
using MemoryId = int;

namespace android {
namespace vts {

// Need to store both hidl_memory pointer and IMemory pointer.
// Conversion from hidl_memory to IMemory is slow, and we can send hidl_memory
// pointer in hidl, and operate on the memory object using IMemory pointer.
struct MemoryInfo {
  // Pointer to hidl_memory, which can be passed around in hidl.
  // Need to manually manage hidl_memory as a pointer, because the returned
  // hidl_memory reference from hidl API goes out of scope.
  unique_ptr<hidl_memory> hidl_mem_ptr;
  // Pointer to IMemory that allows actual memory operation.
  sp<IMemory> memory;
};

// A hidl_memory driver that manages all hidl_memory objects created
// on the target side. Reader and writer use their id to read from and write
// into the memory.
// Example:
//   VtsHidlMemoryDriver mem_driver;
//   // Allcate memory.
//   int mem_id = mem_driver.Allocate(100);
//
//   string write_data = "abcdef";
//   // Write into the memory.
//   mem_driver.Update(mem_id);
//   mem_driver.UpdateBytes(mem_id, write_data.c_str(),
//                          write_data.length());
//   mem_driver.Commit(mem_id);
//   // Read from the memory.
//   char read_data[write_data.length()];
//   mem_driver.Read(mem_id);
//   mem_driver.ReadBytes(mem_id, read_data, write_data.length());
//   mem_driver.Commit(mem_id);
class VtsHidlMemoryDriver {
 public:
  // Constructor to initialize a hidl_memory manager.
  VtsHidlMemoryDriver();

  // Destructor to clean up the class.
  ~VtsHidlMemoryDriver();

  // Allocate a memory region with size mem_size.
  //
  // @param mem_size size of the memory.
  //
  // @return int, id to be used to reference the memory object later.
  //              -1 if allocation fails.
  MemoryId Allocate(size_t mem_size);

  // Registers a memory object in the driver.
  //
  // @param hidl_mem_address address of hidl_memory pointer.
  //
  // @return id to be used to reference the memory region later.
  //         -1 if registration fails.
  MemoryId RegisterHidlMemory(size_t hidl_mem_address);

  // Notify that caller will possibly write to all memory region with id mem_id.
  //
  // @param mem_id identifies the memory object.
  //
  // @return true if memory object is found, false otherwise.
  bool Update(MemoryId mem_id);

  // Notify that caller will possibly write to memory region with mem_id
  // starting at start and ending at start + length.
  // start + length must be < size.
  //
  // @param mem_id identifies the memory object.
  // @param start  offset from the start of memory region to be modified.
  // @param length number of bytes to be modified.
  //
  // @return true if memory object is found, false otherwise.
  bool UpdateRange(MemoryId mem_id, uint64_t start, uint64_t length);

  // Notify that caller will read the entire memory.
  //
  // @param mem_id identifies the memory object.
  //
  // @return true if memory object is found, false otherwise.
  bool Read(MemoryId mem_id);

  // Notify that caller will read memory region with mem_id starting at start
  // and end at start + length.
  //
  // @param mem_id identifies the memory object.
  // @param start  offset from the start of memory region to be modified.
  // @param length number of bytes to be modified.
  //
  // @return true if memory object is found, false otherwise.
  bool ReadRange(MemoryId mem_id, uint64_t start, uint64_t length);

  // Add this method to perform actual write operation in memory,
  // because host side won't be able to cast c++ pointers.
  //
  // @param mem_id     identifies the memory object.
  // @param write_data pointer to the start of buffer to be written into memory.
  // @param length     number of bytes to write.
  // @param start      offset from the start of memory region to be modified.
  //
  // @return true if memory object is found, false otherwise.
  bool UpdateBytes(MemoryId mem_id, const char* write_data, uint64_t length,
                   uint64_t start = 0);

  // Add this method to perform actual read operation in memory,
  // because host side won't be able to cast c++ pointers.
  //
  // @param mem_id    identifies the memory object.
  // @param read_data pointer to the start of buffer to be filled with
  //                  memory content.
  // @param length    number of bytes to be read.
  // @param start     offset from the start of the memory region to be modified.
  //
  // @return true if memory object is found, false otherwise.
  bool ReadBytes(MemoryId mem_id, char* read_data, uint64_t length,
                 uint64_t start = 0);

  // Caller signals done with reading from or writing to memory.
  //
  // @param mem_id identifies the memory object.
  //
  // @return true if memory object is found, false otherwise.
  bool Commit(MemoryId mem_id);

  // Get the size of the memory.
  //
  // @param mem_id identifies the memory object.
  //
  // @return true if memory object is found, false otherwise.
  bool GetSize(MemoryId mem_id, size_t* result);

  // Get hidl_memory pointer address of memory object with mem_id.
  //
  // @param mem_id identifies the memory object.
  // @param result stores the hidl_memory pointer address.
  //
  // @return true if memory object is found, false otherwise.
  bool GetHidlMemoryAddress(MemoryId mem_id, size_t* result);

 private:
  // Finds the memory object with ID mem_id.
  // Logs error if mem_id is not found.
  //
  // @param mem_id identifies the memory object.
  //
  // @return MemoryInfo pointer, which contains both hidl_memory pointer and
  //         IMemory pointer.
  MemoryInfo* FindMemory(MemoryId mem_id);

  // A map to keep track of each hidl_memory information.
  // Store MemoryInfo smart pointer, which contains both hidl_memory,
  // and actual memory pointer.
  unordered_map<MemoryId, unique_ptr<MemoryInfo>> hidl_memory_map_;

  // A mutex to ensure insert and lookup on hidl_memory_map_ are thread-safe.
  mutex map_mutex_;
};

}  // namespace vts
}  // namespace android
#endif  //__VTS_RESOURCE_VTSHIDLMEMORYDRIVER_H
