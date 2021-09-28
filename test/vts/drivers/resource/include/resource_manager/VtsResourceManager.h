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

#ifndef __VTS_RESOURCE_VTSRESOURCEMANAGER_H
#define __VTS_RESOURCE_VTSRESOURCEMANAGER_H

#include <android-base/logging.h>
#include <android/hardware/audio/4.0/IStreamIn.h>
#include <android/hardware/audio/4.0/IStreamOut.h>
#include <android/hardware/audio/effect/2.0/types.h>
#include <android/hardware/audio/effect/4.0/types.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/text_format.h>

#include "fmq_driver/VtsFmqDriver.h"
#include "hidl_handle_driver/VtsHidlHandleDriver.h"
#include "hidl_memory_driver/VtsHidlMemoryDriver.h"
#include "test/vts/proto/ComponentSpecificationMessage.pb.h"
#include "test/vts/proto/VtsResourceControllerMessage.pb.h"

using namespace std;

namespace android {
namespace vts {

typedef ::android::hardware::audio::V4_0::IStreamIn::ReadParameters
    ReadParameters;
typedef ::android::hardware::audio::V4_0::IStreamIn::ReadStatus ReadStatus;
typedef ::android::hardware::audio::V4_0::IStreamOut::WriteCommand WriteCommand;
typedef ::android::hardware::audio::V4_0::IStreamOut::WriteStatus WriteStatus;
typedef ::android::hardware::audio::effect::V4_0::Result ResultV4_0;
typedef ::android::hardware::audio::effect::V2_0::Result ResultV2_0;

// A class that manages all resources allocated on the target side.
// Resources include fast message queue, hidl_memory, hidl_handle.
//
// Example (Process FMQ Command):
//   // Initialize a manager.
//   VtsResourceManager manager;
//
//   // Generate some FMQ request (e.g. creating a queue.).
//   FmqRequestMessage fmq_request;
//   fmq_request.set_operation(FMQ_CREATE);
//   fmq_request.set_data_type("uint16_t");
//   fmq_request.set_sync(true);
//   fmq_request.set_queue_size(2048);
//   fmq_request.set_blocking(false);
//
//   // receive response.
//   FmqRequestResponse fmq_response;
//   // This will ask FMQ driver to process request and send response.
//   ProcessFmqCommand(fmq_request, &fmq_response);
class VtsResourceManager {
 public:
  // Constructor to set up the resource manager.
  VtsResourceManager();

  // Destructor to clean up the resource manager.
  ~VtsResourceManager();

  // Processes command for operations on hidl_handle.
  //
  // @param hidl_handle_request  contains arguments for the operation.
  // @param hidl_handle_response to be filled by the function.
  void ProcessHidlHandleCommand(
      const HidlHandleRequestMessage& hidl_handle_request,
      HidlHandleResponseMessage* hidl_handle_response);

  // Registers the handle object in hidl_handle_driver_ given the hidl_handle
  // address provided in hidl_handle_msg.
  //
  // @param hidl_handle_msg stores hidl_handle address, used to find actual
  //                        handle object.
  //
  // @return handle_id assigned to the new handle object.
  int RegisterHidlHandle(const VariableSpecificationMessage& hidl_handle_msg);

  // Gets hidl_handle address in hidl_handle_driver_.
  // If caller wants to use a handle object in the driver, it specifies
  // handle_id in HandleDataValueMessage. This method calls hidl_handle_driver_
  // to locate the handle object with handle_id, and stores the address
  // in result pointer.
  //
  // @param hidl_handle_msg contains handle_id of the handle object.
  // @param result          stores hidl_handle address.
  //
  // @return true if the handle object with handle_id is found, and stores
  //              address in result,
  //         false otherwise.
  bool GetHidlHandleAddress(const VariableSpecificationMessage& hidl_handle_msg,
                            size_t* result);

  // Processes command for operations on hidl_memory.
  //
  // @param hidl_memory_request  contains arguments for the operation.
  // @param hidl_memory_response to be filled by the function.
  void ProcessHidlMemoryCommand(
      const HidlMemoryRequestMessage& hidl_memory_request,
      HidlMemoryResponseMessage* hidl_memory_response);

  // Registers the memory object in hidl_memory_driver_ given the hidl_memory
  // pointer address provided in hidl_memory_msg.
  //
  // @param hidl_memory_msg stores hidl_memory pointer, used to find actual
  //                        memory pointer.
  //
  // @return mem_id assigned to the new memory object.
  int RegisterHidlMemory(const VariableSpecificationMessage& hidl_memory_msg);

  // Gets hidl_memory pointer address in hidl_memory_driver_.
  // If caller wants to use a memory object in the driver, it specifies mem_id
  // in MemoryDataValueMessage. This method calls hidl_memory_driver to locate
  // the memory object with mem_id, and stores the address in result pointer.
  //
  // @param hidl_memory_msg contains memory object mem_id.
  // @param result          stores hidl_memory pointer.
  //
  // @return true if the memory object with mem_id is found, and stores pointer
  //              address in result,
  //         false otherwise.
  bool GetHidlMemoryAddress(const VariableSpecificationMessage& hidl_memory_msg,
                            size_t* result);

  // Processes command for operations on Fast Message Queue.
  // The arguments are specified in fmq_request, and this function stores result
  // in fmq_response.
  //
  // @param fmq_request  contains arguments for the operation.
  // @param fmq_response to be filled by the function.
  void ProcessFmqCommand(const FmqRequestMessage& fmq_request,
                         FmqResponseMessage* fmq_response);

  // Registers a fmq in fmq_driver_ given the information provided in
  // queue_msg.
  // This message stores queue data_type, sync option, and existing
  // descriptor address. This method recasts the address into a pointer
  // and passes it to fmq_driver_.
  //
  // @param queue_msg stores queue information, data_type, sync option,
  //                  and queue descriptor address.
  //
  // @return queue_id assigned to the new queue object.
  int RegisterFmq(const VariableSpecificationMessage& queue_msg);

  // Gets queue descriptor address specified in VariableSpecificationMessage.
  // The message contains type of data in the queue, queue flavor,
  // and queue id. The method calls fmq_driver to locate the address of the
  // descriptor using these information, then stores the address in
  // result pointer.
  //
  // @param queue_msg contains queue information.
  // @param result    to store queue descriptor pointer address.
  //
  // @return true if queue is found and type matches, and stores the descriptor
  //              address in result.
  //         false otherwise.
  bool GetQueueDescAddress(const VariableSpecificationMessage& queue_msg,
                           size_t* result);

 private:
  // Function template used in our map that maps type name to function
  // with template.
  typedef void (VtsResourceManager::*ProcessFmqCommandFn)(
      const FmqRequestMessage&, FmqResponseMessage*);

  // This method infers the queue flavor from the sync field in fmq_request
  // proto message, and calls ProcessFmqCommandInternal() with template T
  // and queue flavor.
  // Notice we create another method called
  // ProcessFmqCommandWithPredefinedType() with the same interface.
  // This prevents compiler error during conversion between protobuf message
  // and C++.
  //
  // @param fmq_request  contains arguments for FMQ operation.
  // @param fmq_response FMQ response to be filled by this function.
  template <typename T>
  void ProcessFmqCommandWithType(const FmqRequestMessage& fmq_request,
                                 FmqResponseMessage* fmq_response);

  // A helper method to call methods on fmq_driver.
  // This method already has the template type and flavor of FMQ.
  //
  // @param fmq_request  contains arguments for FMQ operation.
  // @param fmq_response FMQ response to be filled by this function.
  template <typename T, hardware::MQFlavor flavor>
  void ProcessFmqCommandInternal(const FmqRequestMessage& fmq_request,
                                 FmqResponseMessage* fmq_response);

  // Converts write_data field in fmq_request to a C++ buffer.
  // For user-defined type, dynamically load the HAL shared library
  // to parse protobuf message to C++ type.
  //
  // @param fmq_request    contains the write_data, represented as a repeated
  //                       proto field.
  // @param write_data     converted data that will be written into FMQ.
  // @param write_data_size number of items in write_data.
  //
  // @return true if parsing is successful, false otherwise.
  //         This function can fail if loading shared library or locating
  //         function symbols fails in user-defined type.
  template <typename T>
  bool FmqProto2Cpp(const FmqRequestMessage& fmq_request, T* write_data,
                    size_t write_data_size);

  // Converts a C++ buffer into read_data field in fmq_response.
  // For user-defined type, dynamically load the HAL shared library
  // to parse C++ type to protobuf message.
  //
  // @param fmq_response   to be filled by the function. The function fills the
  //                       read_data field, which is represented as a repeated
  //                       proto field.
  // @param data_type      type of data in FMQ, this information will be
  //                       written into protobuf message.
  // @param read_data      contains data read from FMQ read operation.
  // @param read_data_size number of items in read_data.
  //
  // @return true if parsing is successful, false otherwise.
  //         This function can fail if loading shared library or locating
  //         function symbols fails in user-defined type.
  template <typename T>
  bool FmqCpp2Proto(FmqResponseMessage* fmq_response, const string& data_type,
                    T* read_data, size_t read_data_size);

  // Loads the corresponding HAL driver shared library from the type name.
  // This function parses the shared library path from a type name, and
  // loads the shared library object from the path.
  //
  // Example:
  // For type ::android::hardware::audio::V4_0::IStreamIn::ReadParameters,
  // the path that is parsed from the type name is
  // /data/local/tmp/android.hardware.audio@4.0-vts.driver.so.
  // Then the function loads the shared library object from this path.
  //
  // TODO: Consider determining the path and bitness by passing a field
  // in the protobuf message.
  //
  // @param data_type type name.
  //
  // @return shared library object.
  void* LoadSharedLibFromTypeName(const string& data_type);

  // Load the translation function between C++ and protobuf.
  // This method parses the function name that can translate C++ to protobuf
  // or translate protobuf to C++ from data_type.
  // Then it loads the function symbol from shared_lib_obj, which is an opened
  // HAL shared library.
  //
  // Example: type name is
  // ::android::hardware::audio::V4_0::IStreamIn::ReadParameters,
  // and we have the shared library pointer shared_lib_obj.
  // To translate from protobuf to C++, we need to call
  // GetTranslationFuncPtr(shared_lib_obj, data_type, true);
  // To translate from C++ to protobuf, we need to call
  // GetTranslationFuncPtr(shared_lib_obj, data_type, false);
  //
  // @param shared_lib_obj  opened HAL shared library object.
  // @param data_type       type name.
  // @param is_proto_to_cpp whether the function is to convert proto to C++.
  //
  // @return name of the translation function.
  void* GetTranslationFuncPtr(void* shared_lib_obj, const string& data_type,
                              bool is_proto_to_cpp);

  // Manages Fast Message Queue (FMQ) driver.
  VtsFmqDriver fmq_driver_;
  // Manages hidl_memory driver.
  VtsHidlMemoryDriver hidl_memory_driver_;
  // Manages hidl_handle driver.
  VtsHidlHandleDriver hidl_handle_driver_;
  // A map that maps each FMQ user-defined type into a process
  // function with template.
  const unordered_map<string, ProcessFmqCommandFn> func_map_ = {
      {"int8_t", &VtsResourceManager::ProcessFmqCommandWithType<int8_t>},
      {"uint8_t", &VtsResourceManager::ProcessFmqCommandWithType<uint8_t>},
      {"int16_t", &VtsResourceManager::ProcessFmqCommandWithType<int16_t>},
      {"uint16_t", &VtsResourceManager::ProcessFmqCommandWithType<uint16_t>},
      {"int32_t", &VtsResourceManager::ProcessFmqCommandWithType<int32_t>},
      {"uint32_t", &VtsResourceManager::ProcessFmqCommandWithType<uint32_t>},
      {"int64_t", &VtsResourceManager::ProcessFmqCommandWithType<int64_t>},
      {"uint64_t", &VtsResourceManager::ProcessFmqCommandWithType<uint64_t>},
      {"float_t", &VtsResourceManager::ProcessFmqCommandWithType<float>},
      {"double_t", &VtsResourceManager::ProcessFmqCommandWithType<double>},
      {"bool_t", &VtsResourceManager::ProcessFmqCommandWithType<bool>},
      {"::android::hardware::audio::V4_0::IStreamIn::ReadParameters",
       &VtsResourceManager::ProcessFmqCommandWithType<ReadParameters>},
      {"::android::hardware::audio::V4_0::IStreamIn::ReadStatus",
       &VtsResourceManager::ProcessFmqCommandWithType<ReadStatus>},
      {"::android::hardware::audio::V4_0::IStreamOut::WriteCommand",
       &VtsResourceManager::ProcessFmqCommandWithType<WriteCommand>},
      {"::android::hardware::audio::effect::V4_0::Result",
       &VtsResourceManager::ProcessFmqCommandWithType<ResultV4_0>},
      {"::android::hardware::audio::effect::V2_0::Result",
       &VtsResourceManager::ProcessFmqCommandWithType<ResultV2_0>}};
};

}  // namespace vts
}  // namespace android
#endif  //__VTS_RESOURCE_VTSRESOURCEMANAGER_H
