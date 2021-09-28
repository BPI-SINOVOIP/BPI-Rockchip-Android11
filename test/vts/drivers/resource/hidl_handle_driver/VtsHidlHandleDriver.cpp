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
#define LOG_TAG "VtsHidlHandleDriver"

#include "hidl_handle_driver/VtsHidlHandleDriver.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include <android-base/logging.h>

using android::hardware::hidl_handle;

using namespace std;

namespace android {
namespace vts {

VtsHidlHandleDriver::VtsHidlHandleDriver() {}

VtsHidlHandleDriver::~VtsHidlHandleDriver() {
  // clears objects in the map.
  hidl_handle_map_.clear();
}

HandleId VtsHidlHandleDriver::CreateFileHandle(string filepath, int flag,
                                               int mode, vector<int> data) {
  int num_fds = 1;
  int num_ints = data.size();
  native_handle_t* native_handle = native_handle_create(num_fds, num_ints);
  if (native_handle == nullptr) {
    LOG(ERROR) << "native_handle create failure.";
    return -1;
  }

  for (int i = 0; i < num_fds + num_ints; i++) {
    if (i < num_fds) {
      int curr_fd = open(filepath.c_str(), flag, mode);
      if (curr_fd == -1) {
        LOG(ERROR) << "Failed to create file descriptor for file with path "
                   << filepath;
        // Need to close already opened files because creating handle fails.
        for (int j = 0; j < i; j++) {
          close(native_handle->data[j]);
        }
        native_handle_delete(native_handle);
        return -1;
      }
      native_handle->data[i] = curr_fd;
    } else {
      native_handle->data[i] = data[i - num_fds];
    }
  }

  // This class owns native_handle object, responsible for deleting it
  // in the destructor.
  unique_ptr<hidl_handle> hidl_handle_ptr(new hidl_handle());
  hidl_handle_ptr->setTo(native_handle, true);
  // Insert the handle object into the map.
  map_mutex_.lock();
  size_t new_handle_id = hidl_handle_map_.size();
  hidl_handle_map_.emplace(new_handle_id, move(hidl_handle_ptr));
  map_mutex_.unlock();
  return new_handle_id;
}

bool VtsHidlHandleDriver::UnregisterHidlHandle(HandleId handle_id) {
  // true flag to indicate we want smart pointer to release ownership.
  hidl_handle* handle_obj = FindHandle(handle_id, true);
  if (handle_obj == nullptr) return false;  // unable to find handle object.
  delete handle_obj;  // This closes open file descriptors in the handle object.
  return true;
}

ssize_t VtsHidlHandleDriver::ReadFile(HandleId handle_id, void* read_data,
                                      size_t num_bytes) {
  hidl_handle* handle_obj = FindHandle(handle_id);
  if (handle_obj == nullptr) return -1;

  const native_handle_t* native_handle = handle_obj->getNativeHandle();
  // Check if a file descriptor exists.
  if (native_handle->numFds == 0) {
    LOG(ERROR) << "Read from file failure: handle object with id " << handle_id
               << " has no file descriptor.";
    return -1;
  }
  int fd = native_handle->data[0];
  ssize_t read_result = read(fd, read_data, num_bytes);
  if (read_result == -1) {
    LOG(ERROR) << "Read from file failure: read from file with descriptor "
               << fd << " failure: " << strerror(errno);
  }
  return read_result;
}

ssize_t VtsHidlHandleDriver::WriteFile(HandleId handle_id,
                                       const void* write_data,
                                       size_t num_bytes) {
  hidl_handle* handle_obj = FindHandle(handle_id);
  if (handle_obj == nullptr) return -1;

  const native_handle_t* native_handle = handle_obj->getNativeHandle();
  // Check if a file descriptor exists.
  if (native_handle->numFds == 0) {
    LOG(ERROR) << "Write to file failure: handle object with id " << handle_id
               << " has no file descriptor.";
    return -1;
  }
  int fd = native_handle->data[0];
  ssize_t write_result = write(fd, write_data, num_bytes);
  if (write_result == -1) {
    LOG(ERROR) << "Write to file failure: write to file with descriptor " << fd
               << " failure: " << strerror(errno);
  }
  return write_result;
}

HandleId VtsHidlHandleDriver::RegisterHidlHandle(size_t hidl_handle_address) {
  unique_ptr<hidl_handle> hidl_handle_ptr(
      reinterpret_cast<hidl_handle*>(hidl_handle_address));

  map_mutex_.lock();
  size_t new_handle_id = hidl_handle_map_.size();
  hidl_handle_map_.emplace(new_handle_id, move(hidl_handle_ptr));
  map_mutex_.unlock();
  return new_handle_id;
}

bool VtsHidlHandleDriver::GetHidlHandleAddress(HandleId handle_id,
                                               size_t* result) {
  hidl_handle* handle = FindHandle(handle_id);
  if (handle == nullptr) return false;  // unable to find handle object.
  *result = reinterpret_cast<size_t>(handle);
  return true;
}

hidl_handle* VtsHidlHandleDriver::FindHandle(HandleId handle_id, bool release) {
  hidl_handle* handle;
  map_mutex_.lock();
  auto iterator = hidl_handle_map_.find(handle_id);
  if (iterator == hidl_handle_map_.end()) {
    LOG(ERROR) << "Unable to find hidl_handle associated with handle_id "
               << handle_id;
    map_mutex_.unlock();
    return nullptr;
  }
  // During unregistering a handle, unique_ptr releases ownership.
  if (release)
    handle = (iterator->second).release();
  else
    handle = (iterator->second).get();
  map_mutex_.unlock();
  return handle;
}

}  // namespace vts
}  // namespace android
