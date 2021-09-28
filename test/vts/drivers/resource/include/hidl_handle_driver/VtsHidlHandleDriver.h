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

#ifndef __VTS_RESOURCE_VTSHIDLHANDLEDRIVER_H
#define __VTS_RESOURCE_VTSHIDLHANDLEDRIVER_H

#include <mutex>
#include <unordered_map>

#include <android-base/logging.h>
#include <cutils/native_handle.h>
#include <hidl/HidlSupport.h>

using android::hardware::hidl_handle;

using namespace std;
using HandleId = int;

namespace android {
namespace vts {

// A hidl_handle driver that manages all hidl_handle objects created
// on the target side. Users can create handle objects to manage their
// File I/O.
// TODO: Currently, this class only supports opening a single file in
//       one handle object. In the future, we need to support opening
//       a list of various file types, such as socket, pipe.
//
// Example:
//   VtsHidlHandleDriver handle_driver;
//
//   // Create a new file handler.
//   int writer_id = handle_driver.CreateFileHandle(
//        "test.txt", O_RDWR | O_CREAT | O_TRUNC, S_IRWXG, vector<int>());
//
//   string write_data = "Hello World!";
//   // Writer writes to test.txt.
//   handle_driver.WriteFile(writer_id,
//                           static_cast<const void*>(write_data.c_str())),
//                           write_data.length());
class VtsHidlHandleDriver {
 public:
  // Constructor to initialize a hidl_handle manager.
  VtsHidlHandleDriver();

  // Destructor to clean up the class.
  ~VtsHidlHandleDriver();

  // Creates a hidl_handle object by providing a single file path to the file
  // to be opened with the flag and mode, and a list of integers needed in
  // native_handle_t struct.
  //
  // @param filepath path to the file to be opened.
  // @param flag     file status flag, details in Linux man page.
  // @param mode     file access mode, details in Linux man page.
  // @param data     vector of integers useful in native_handle_t struct.
  //
  // @return new handle ID registered on the target side.
  HandleId CreateFileHandle(string filepath, int flag, int mode,
                            vector<int> data);

  // Closes all file descriptors in the handle object associated with input ID.
  //
  // @param handle_id identifies the handle object.
  //
  // @return true if the handle object is found, false otherwise.
  bool UnregisterHidlHandle(HandleId handle_id);

  // Reads a file in the handle object.
  // Caller specifies the handle_id and number of bytes to read.
  // This function assumes caller only wants to read from a single file,
  // so it will access the first file descriptor in the native_handle_t struct,
  // and the first descriptor must be a file.
  //
  // @param handle_id identifies the handle object.
  // @param read_data data read back from file.
  // @param num_bytes number of bytes to read.
  //
  // @return number of bytes read, -1 to signal failure.
  ssize_t ReadFile(HandleId handle_id, void* read_data, size_t num_bytes);

  // Writes to a file in the handle object.
  // Caller specifies the handle_id and number of bytes to write.
  // This function assumes caller only wants to write to a single file,
  // so it will access the first file descriptor in the native_handle_t struct,
  // and the first descriptor must be a file.
  //
  // @param handle_id  identifies the handle object.
  // @param write_data data to be written into to file.
  // @param num_bytes  number of bytes to write.
  //
  // @return number of bytes written, -1 to signal failure.
  ssize_t WriteFile(HandleId handle_id, const void* write_data,
                    size_t num_bytes);

  // Registers a handle object in the driver using an existing
  // hidl_handle address created by vtsc.
  //
  // @param handle_address address of hidl_handle pointer.
  //
  // @return id to be used to hidl_handle later.
  //         -1 if registration fails.
  HandleId RegisterHidlHandle(size_t hidl_handle_address);

  // Get hidl_handle address of handle object with handle_id.
  //
  // @param handle_id identifies the handle object.
  // @param result    stores the hidl_handle address.
  //
  // @return true if handle object is found, false otherwise.
  bool GetHidlHandleAddress(HandleId handle_id, size_t* result);

 private:
  // Finds the handle object with ID handle_id.
  // Logs error if handle_id is not found.
  //
  // @param handle_id identifies the handle object.
  // @param release   whether to release the handle object managed unique_ptr.
  //        This parameter is only true if UnregisterHidlHandle() is called.
  //
  // @return hidl_handle pointer.
  hidl_handle* FindHandle(HandleId handle_id, bool release = false);

  // A map to keep track of each hidl_handle information.
  // Store hidl_handle smart pointers.
  unordered_map<HandleId, unique_ptr<hidl_handle>> hidl_handle_map_;

  // A mutex to ensure insert and lookup on hidl_handle_map_ are thread-safe.
  mutex map_mutex_;
};

}  // namespace vts
}  // namespace android
#endif  //__VTS_RESOURCE_VTSHIDLHANDLEDRIVER_H