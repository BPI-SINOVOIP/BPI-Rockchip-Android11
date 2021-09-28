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
#define LOG_TAG "VtsResourceManager"

#include "resource_manager/VtsResourceManager.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <regex>

#include "test/vts/proto/ComponentSpecificationMessage.pb.h"
#include "test/vts/proto/VtsResourceControllerMessage.pb.h"

using android::hardware::kSynchronizedReadWrite;
using android::hardware::kUnsynchronizedWrite;

using namespace std;

namespace android {
namespace vts {

VtsResourceManager::VtsResourceManager() {}

VtsResourceManager::~VtsResourceManager() {}

void VtsResourceManager::ProcessHidlHandleCommand(
    const HidlHandleRequestMessage& hidl_handle_request,
    HidlHandleResponseMessage* hidl_handle_response) {
  HidlHandleOp operation = hidl_handle_request.operation();
  HandleId handle_id = hidl_handle_request.handle_id();
  HandleDataValueMessage handle_info = hidl_handle_request.handle_info();
  size_t read_data_size = hidl_handle_request.read_data_size();
  const void* write_data =
      static_cast<const void*>(hidl_handle_request.write_data().c_str());
  size_t write_data_size = hidl_handle_request.write_data().length();
  bool success = false;

  switch (operation) {
    case HANDLE_PROTO_CREATE_FILE: {
      if (handle_info.fd_val().size() == 0) {
        LOG(ERROR) << "No files to open.";
        break;
      }

      // TODO: currently only support opening a single file.
      // Support any file descriptor type in the future.
      FdMessage file_desc_info = handle_info.fd_val(0);
      if (file_desc_info.type() != FILE_TYPE) {
        LOG(ERROR) << "Currently only support file type.";
        break;
      }

      string filepath = file_desc_info.file_name();
      int flag;
      int mode = 0;
      // Translate the mode into C++ flags and modes.
      if (file_desc_info.file_mode_str() == "r" ||
          file_desc_info.file_mode_str() == "rb") {
        flag = O_RDONLY;
      } else if (file_desc_info.file_mode_str() == "w" ||
                 file_desc_info.file_mode_str() == "wb") {
        flag = O_WRONLY | O_CREAT | O_TRUNC;
        mode = S_IRWXU;  // User has the right to read/write/execute.
      } else if (file_desc_info.file_mode_str() == "a" ||
                 file_desc_info.file_mode_str() == "ab") {
        flag = O_WRONLY | O_CREAT | O_APPEND;
        mode = S_IRWXU;
      } else if (file_desc_info.file_mode_str() == "r+" ||
                 file_desc_info.file_mode_str() == "rb+" ||
                 file_desc_info.file_mode_str() == "r+b") {
        flag = O_RDWR;
      } else if (file_desc_info.file_mode_str() == "w+" ||
                 file_desc_info.file_mode_str() == "wb+" ||
                 file_desc_info.file_mode_str() == "w+b") {
        flag = O_RDWR | O_CREAT | O_TRUNC;
        mode = S_IRWXU;
      } else if (file_desc_info.file_mode_str() == "a+" ||
                 file_desc_info.file_mode_str() == "ab+" ||
                 file_desc_info.file_mode_str() == "a+b") {
        flag = O_RDWR | O_CREAT | O_APPEND;
        mode = S_IRWXU;
      } else if (file_desc_info.file_mode_str() == "x" ||
                 file_desc_info.file_mode_str() == "x+") {
        struct stat buffer;
        if (stat(filepath.c_str(), &buffer) == 0) {
          LOG(ERROR) << "Host side creates a file with mode x, "
                     << "but file already exists";
          break;
        }
        flag = O_CREAT;
        mode = S_IRWXU;
        if (file_desc_info.file_mode_str() == "x+") {
          flag |= O_RDWR;
        } else {
          flag |= O_WRONLY;
        }
      } else {
        LOG(ERROR) << "Unknown file mode.";
        break;
      }

      // Convert repeated int field into vector.
      vector<int> int_data(write_data_size);
      transform(handle_info.int_val().cbegin(), handle_info.int_val().cend(),
                int_data.begin(), [](const int& item) { return item; });
      // Call API on hidl_handle driver to create a file handle.
      int new_handle_id =
          hidl_handle_driver_.CreateFileHandle(filepath, flag, mode, int_data);
      hidl_handle_response->set_new_handle_id(new_handle_id);
      success = new_handle_id != -1;
      break;
    }
    case HANDLE_PROTO_READ_FILE: {
      char read_data[read_data_size];
      // Call API on hidl_handle driver to read the file.
      ssize_t read_success_bytes = hidl_handle_driver_.ReadFile(
          handle_id, static_cast<void*>(read_data), read_data_size);
      success = read_success_bytes != -1;
      hidl_handle_response->set_read_data(
          string(read_data, read_success_bytes));
      break;
    }
    case HANDLE_PROTO_WRITE_FILE: {
      // Call API on hidl_handle driver to write to the file.
      ssize_t write_success_bytes =
          hidl_handle_driver_.WriteFile(handle_id, write_data, write_data_size);
      success = write_success_bytes != -1;
      hidl_handle_response->set_write_data_size(write_success_bytes);
      break;
    }
    case HANDLE_PROTO_DELETE: {
      // Call API on hidl_handle driver to unregister the handle object.
      success = hidl_handle_driver_.UnregisterHidlHandle(handle_id);
      break;
    }
    default:
      LOG(ERROR) << "Unknown operation.";
      break;
  }
  hidl_handle_response->set_success(success);
}

int VtsResourceManager::RegisterHidlHandle(
    const VariableSpecificationMessage& hidl_handle_msg) {
  size_t hidl_handle_address =
      hidl_handle_msg.handle_value().hidl_handle_address();
  if (hidl_handle_address == 0) {
    LOG(ERROR) << "Invalid hidl_handle address."
               << "HAL driver either didn't set the address or "
               << "set a null pointer.";
    return -1;  // check for null pointer
  }
  return hidl_handle_driver_.RegisterHidlHandle(hidl_handle_address);
}

bool VtsResourceManager::GetHidlHandleAddress(
    const VariableSpecificationMessage& hidl_handle_msg, size_t* result) {
  int handle_id = hidl_handle_msg.handle_value().handle_id();
  bool success = hidl_handle_driver_.GetHidlHandleAddress(handle_id, result);
  return success;
}

void VtsResourceManager::ProcessHidlMemoryCommand(
    const HidlMemoryRequestMessage& hidl_memory_request,
    HidlMemoryResponseMessage* hidl_memory_response) {
  size_t mem_size = hidl_memory_request.mem_size();
  int mem_id = hidl_memory_request.mem_id();
  uint64_t start = hidl_memory_request.start();
  uint64_t length = hidl_memory_request.length();
  const string& write_data = hidl_memory_request.write_data();
  bool success = false;

  switch (hidl_memory_request.operation()) {
    case MEM_PROTO_ALLOCATE: {
      int new_mem_id = hidl_memory_driver_.Allocate(mem_size);
      hidl_memory_response->set_new_mem_id(new_mem_id);
      success = new_mem_id != -1;
      break;
    }
    case MEM_PROTO_START_READ: {
      success = hidl_memory_driver_.Read(mem_id);
      break;
    }
    case MEM_PROTO_START_READ_RANGE: {
      success = hidl_memory_driver_.ReadRange(mem_id, start, length);
      break;
    }
    case MEM_PROTO_START_UPDATE: {
      success = hidl_memory_driver_.Update(mem_id);
      break;
    }
    case MEM_PROTO_START_UPDATE_RANGE: {
      success = hidl_memory_driver_.UpdateRange(mem_id, start, length);
      break;
    }
    case MEM_PROTO_UPDATE_BYTES: {
      success = hidl_memory_driver_.UpdateBytes(mem_id, write_data.c_str(),
                                                length, start);
      break;
    }
    case MEM_PROTO_READ_BYTES: {
      char read_data[length];
      success = hidl_memory_driver_.ReadBytes(mem_id, read_data, length, start);
      hidl_memory_response->set_read_data(string(read_data, length));
      break;
    }
    case MEM_PROTO_COMMIT: {
      success = hidl_memory_driver_.Commit(mem_id);
      break;
    }
    case MEM_PROTO_GET_SIZE: {
      size_t result_mem_size;
      success = hidl_memory_driver_.GetSize(mem_id, &result_mem_size);
      hidl_memory_response->set_mem_size(result_mem_size);
      break;
    }
    default:
      LOG(ERROR) << "unknown operation in hidl_memory_driver.";
      break;
  }
  hidl_memory_response->set_success(success);
}

int VtsResourceManager::RegisterHidlMemory(
    const VariableSpecificationMessage& hidl_memory_msg) {
  size_t hidl_mem_address =
      hidl_memory_msg.hidl_memory_value().hidl_mem_address();
  if (hidl_mem_address == 0) {
    LOG(ERROR) << "Resource manager: invalid hidl_memory address."
               << "HAL driver either didn't set the address or "
               << "set a null pointer.";
    return -1;  // check for null pointer
  }
  return hidl_memory_driver_.RegisterHidlMemory(hidl_mem_address);
}

bool VtsResourceManager::GetHidlMemoryAddress(
    const VariableSpecificationMessage& hidl_memory_msg, size_t* result) {
  int mem_id = hidl_memory_msg.hidl_memory_value().mem_id();
  bool success = hidl_memory_driver_.GetHidlMemoryAddress(mem_id, result);
  return success;
}

void VtsResourceManager::ProcessFmqCommand(const FmqRequestMessage& fmq_request,
                                           FmqResponseMessage* fmq_response) {
  const string& data_type = fmq_request.data_type();
  // Find the correct function with template to process FMQ operation.
  auto iterator = func_map_.find(data_type);
  if (iterator == func_map_.end()) {  // queue not found
    LOG(ERROR) << "Resource manager: current FMQ driver doesn't support type "
               << data_type;
    fmq_response->set_success(false);
  } else {
    (this->*(iterator->second))(fmq_request, fmq_response);
  }
}

int VtsResourceManager::RegisterFmq(
    const VariableSpecificationMessage& queue_msg) {
  size_t queue_desc_addr = queue_msg.fmq_value(0).fmq_desc_address();
  if (queue_desc_addr == 0) {
    LOG(ERROR)
        << "Resource manager: invalid queue descriptor address."
        << "HAL driver either didn't set the address or set a null pointer.";
    return -1;  // check for null pointer
  }

  FmqRequestMessage fmq_request;
  FmqResponseMessage fmq_response;
  fmq_request.set_operation(FMQ_CREATE);
  fmq_request.set_sync(queue_msg.type() == TYPE_FMQ_SYNC);
  // TODO: support user-defined types in the future, only support scalar types
  // for now.
  fmq_request.set_data_type(queue_msg.fmq_value(0).scalar_type());
  fmq_request.set_queue_desc_addr(queue_desc_addr);
  ProcessFmqCommand(fmq_request, &fmq_response);
  return fmq_response.queue_id();
}

bool VtsResourceManager::GetQueueDescAddress(
    const VariableSpecificationMessage& queue_msg, size_t* result) {
  FmqRequestMessage fmq_request;
  FmqResponseMessage fmq_response;
  fmq_request.set_operation(FMQ_GET_DESC_ADDR);
  fmq_request.set_sync(queue_msg.type() == TYPE_FMQ_SYNC);
  // TODO: support user-defined types in the future, only support scalar types
  // for now.
  fmq_request.set_data_type(queue_msg.fmq_value(0).scalar_type());
  fmq_request.set_queue_id(queue_msg.fmq_value(0).fmq_id());
  ProcessFmqCommand(fmq_request, &fmq_response);
  bool success = fmq_response.success();
  *result = fmq_response.sizet_return_val();
  return success;
}

template <typename T>
void VtsResourceManager::ProcessFmqCommandWithType(
    const FmqRequestMessage& fmq_request, FmqResponseMessage* fmq_response) {
  // Infers queue flavor and calls the internal method.
  if (fmq_request.sync()) {
    ProcessFmqCommandInternal<T, kSynchronizedReadWrite>(fmq_request,
                                                         fmq_response);
  } else {
    ProcessFmqCommandInternal<T, kUnsynchronizedWrite>(fmq_request,
                                                       fmq_response);
  }
}

template <typename T, hardware::MQFlavor flavor>
void VtsResourceManager::ProcessFmqCommandInternal(
    const FmqRequestMessage& fmq_request, FmqResponseMessage* fmq_response) {
  const string& data_type = fmq_request.data_type();
  int queue_id = fmq_request.queue_id();
  size_t queue_size = fmq_request.queue_size();
  bool blocking = fmq_request.blocking();
  bool reset_pointers = fmq_request.reset_pointers();
  size_t write_data_size = fmq_request.write_data_size();
  T write_data[write_data_size];
  size_t read_data_size = fmq_request.read_data_size();
  T read_data[read_data_size];
  size_t queue_desc_addr = fmq_request.queue_desc_addr();
  int64_t time_out_nanos = fmq_request.time_out_nanos();
  // TODO: The three variables below are manually created.
  // In the future, get these from host side when we support long-form
  // blocking from host side.
  uint32_t read_notification = 0;
  uint32_t write_notification = 0;
  atomic<uint32_t> event_flag_word;
  bool success = false;
  size_t sizet_result;

  switch (fmq_request.operation()) {
    case FMQ_CREATE: {
      int new_queue_id = -1;
      if (queue_id == -1) {
        if (queue_desc_addr == 0) {
          new_queue_id =
              fmq_driver_.CreateFmq<T, flavor>(data_type, queue_size, blocking);
        } else {
          new_queue_id =
              fmq_driver_.CreateFmq<T, flavor>(data_type, queue_desc_addr);
        }
      } else {
        new_queue_id = fmq_driver_.CreateFmq<T, flavor>(data_type, queue_id,
                                                        reset_pointers);
      }
      fmq_response->set_queue_id(new_queue_id);
      success = new_queue_id != -1;
      break;
    }
    case FMQ_READ: {
      success = fmq_driver_.ReadFmq<T, flavor>(data_type, queue_id, read_data,
                                               read_data_size);
      if (!FmqCpp2Proto<T>(fmq_response, data_type, read_data,
                           read_data_size)) {
        LOG(ERROR) << "Resource manager: failed to convert C++ type into "
                   << "protobuf message for type " << data_type;
        break;
      }
      break;
    }
    case FMQ_READ_BLOCKING: {
      success = fmq_driver_.ReadFmqBlocking<T, flavor>(
          data_type, queue_id, read_data, read_data_size, time_out_nanos);
      if (!FmqCpp2Proto<T>(fmq_response, data_type, read_data,
                           read_data_size)) {
        LOG(ERROR) << "Resource manager: failed to convert C++ type into "
                   << "protobuf message for type " << data_type;
        break;
      }
      break;
    }
    case FMQ_READ_BLOCKING_LONG: {
      // TODO: implement a meaningful long-form blocking mechanism
      // Currently passing a dummy event flag word.
      success = fmq_driver_.ReadFmqBlocking<T, flavor>(
          data_type, queue_id, read_data, read_data_size, read_notification,
          write_notification, time_out_nanos, &event_flag_word);
      if (!FmqCpp2Proto<T>(fmq_response, data_type, read_data,
                           read_data_size)) {
        LOG(ERROR) << "Resource manager: failed to convert C++ type into "
                   << "protobuf message for type " << data_type;
        break;
      }
      break;
    }
    case FMQ_WRITE: {
      if (!FmqProto2Cpp<T>(fmq_request, write_data, write_data_size)) {
        LOG(ERROR) << "Resource manager: failed to convert protobuf message "
                   << "into C++ types for type " << data_type;
        break;
      }
      success = fmq_driver_.WriteFmq<T, flavor>(data_type, queue_id, write_data,
                                                write_data_size);
      break;
    }
    case FMQ_WRITE_BLOCKING: {
      if (!FmqProto2Cpp<T>(fmq_request, write_data, write_data_size)) {
        LOG(ERROR) << "Resource manager: failed to convert protobuf message "
                   << "into C++ types for type " << data_type;
        break;
      }
      success = fmq_driver_.WriteFmqBlocking<T, flavor>(
          data_type, queue_id, write_data, write_data_size, time_out_nanos);
      break;
    }
    case FMQ_WRITE_BLOCKING_LONG: {
      // TODO: implement a meaningful long-form blocking mechanism
      // Currently passing a dummy event flag word.
      if (!FmqProto2Cpp<T>(fmq_request, write_data, write_data_size)) {
        LOG(ERROR) << "Resource manager: failed to convert protobuf message "
                   << "into C++ types for type " << data_type;
        break;
      }
      success = fmq_driver_.WriteFmqBlocking<T, flavor>(
          data_type, queue_id, write_data, write_data_size, read_notification,
          write_notification, time_out_nanos, &event_flag_word);
      break;
    }
    case FMQ_AVAILABLE_WRITE: {
      success = fmq_driver_.AvailableToWrite<T, flavor>(data_type, queue_id,
                                                        &sizet_result);
      fmq_response->set_sizet_return_val(sizet_result);
      break;
    }
    case FMQ_AVAILABLE_READ: {
      success = fmq_driver_.AvailableToRead<T, flavor>(data_type, queue_id,
                                                       &sizet_result);
      fmq_response->set_sizet_return_val(sizet_result);
      break;
    }
    case FMQ_GET_QUANTUM_SIZE: {
      success = fmq_driver_.GetQuantumSize<T, flavor>(data_type, queue_id,
                                                      &sizet_result);
      fmq_response->set_sizet_return_val(sizet_result);
      break;
    }
    case FMQ_GET_QUANTUM_COUNT: {
      success = fmq_driver_.GetQuantumCount<T, flavor>(data_type, queue_id,
                                                       &sizet_result);
      fmq_response->set_sizet_return_val(sizet_result);
      break;
    }
    case FMQ_IS_VALID: {
      success = fmq_driver_.IsValid<T, flavor>(data_type, queue_id);
      break;
    }
    case FMQ_GET_DESC_ADDR: {
      success = fmq_driver_.GetQueueDescAddress<T, flavor>(data_type, queue_id,
                                                           &sizet_result);
      fmq_response->set_sizet_return_val(sizet_result);
      break;
    }
    default:
      LOG(ERROR) << "Resource manager: Unsupported FMQ operation.";
  }
  fmq_response->set_success(success);
}

template <typename T>
bool VtsResourceManager::FmqProto2Cpp(const FmqRequestMessage& fmq_request,
                                      T* write_data, size_t write_data_size) {
  const string& data_type = fmq_request.data_type();
  // Read from different proto fields based on type.
  if (data_type == "int8_t") {
    int8_t* convert_data = reinterpret_cast<int8_t*>(write_data);
    for (int i = 0; i < write_data_size; i++) {
      convert_data[i] = (fmq_request.write_data(i).scalar_value().int8_t());
    }
  } else if (data_type == "uint8_t") {
    uint8_t* convert_data = reinterpret_cast<uint8_t*>(write_data);
    for (int i = 0; i < write_data_size; i++) {
      convert_data[i] = fmq_request.write_data(i).scalar_value().uint8_t();
    }
  } else if (data_type == "int16_t") {
    int16_t* convert_data = reinterpret_cast<int16_t*>(write_data);
    for (int i = 0; i < write_data_size; i++) {
      convert_data[i] = fmq_request.write_data(i).scalar_value().int16_t();
    }
  } else if (data_type == "uint16_t") {
    uint16_t* convert_data = reinterpret_cast<uint16_t*>(write_data);
    for (int i = 0; i < write_data_size; i++) {
      convert_data[i] = fmq_request.write_data(i).scalar_value().uint16_t();
    }
  } else if (data_type == "int32_t") {
    int32_t* convert_data = reinterpret_cast<int32_t*>(write_data);
    for (int i = 0; i < write_data_size; i++) {
      convert_data[i] = fmq_request.write_data(i).scalar_value().int32_t();
    }
  } else if (data_type == "uint32_t") {
    uint32_t* convert_data = reinterpret_cast<uint32_t*>(write_data);
    for (int i = 0; i < write_data_size; i++) {
      convert_data[i] = fmq_request.write_data(i).scalar_value().uint32_t();
    }
  } else if (data_type == "int64_t") {
    int64_t* convert_data = reinterpret_cast<int64_t*>(write_data);
    for (int i = 0; i < write_data_size; i++) {
      convert_data[i] = fmq_request.write_data(i).scalar_value().int64_t();
    }
  } else if (data_type == "uint64_t") {
    uint64_t* convert_data = reinterpret_cast<uint64_t*>(write_data);
    for (int i = 0; i < write_data_size; i++) {
      convert_data[i] = fmq_request.write_data(i).scalar_value().uint64_t();
    }
  } else if (data_type == "float_t") {
    float* convert_data = reinterpret_cast<float*>(write_data);
    for (int i = 0; i < write_data_size; i++) {
      convert_data[i] = fmq_request.write_data(i).scalar_value().float_t();
    }
  } else if (data_type == "double_t") {
    double* convert_data = reinterpret_cast<double*>(write_data);
    for (int i = 0; i < write_data_size; i++) {
      convert_data[i] = fmq_request.write_data(i).scalar_value().double_t();
    }
  } else if (data_type == "bool_t") {
    bool* convert_data = reinterpret_cast<bool*>(write_data);
    for (int i = 0; i < write_data_size; i++) {
      convert_data[i] = fmq_request.write_data(i).scalar_value().bool_t();
    }
  } else {
    // Encounter a predefined type in HAL service.
    LOG(INFO) << "Resource manager: detected host side specifies a "
              << "predefined type.";
    void* shared_lib_obj = LoadSharedLibFromTypeName(data_type);

    // Locate the symbol for the translation function.
    typedef void (*parse_fn)(const VariableSpecificationMessage&, T*,
                             const string&);
    parse_fn parser =
        (parse_fn)(GetTranslationFuncPtr(shared_lib_obj, data_type, true));
    if (!parser) return false;  // Error logged in helper function.

    // Parse the data from protobuf to C++.
    for (int i = 0; i < write_data_size; i++) {
      (*parser)(fmq_request.write_data(i), &write_data[i], "");
    }
    dlclose(shared_lib_obj);
  }
  return true;
}

// TODO: support user-defined types, only support primitive types now.
template <typename T>
bool VtsResourceManager::FmqCpp2Proto(FmqResponseMessage* fmq_response,
                                      const string& data_type, T* read_data,
                                      size_t read_data_size) {
  fmq_response->clear_read_data();
  // Write to different proto fields based on type.
  if (data_type == "int8_t") {
    int8_t* convert_data = reinterpret_cast<int8_t*>(read_data);
    for (size_t i = 0; i < read_data_size; i++) {
      VariableSpecificationMessage* item = fmq_response->add_read_data();
      item->set_type(TYPE_SCALAR);
      item->set_scalar_type(data_type);
      (item->mutable_scalar_value())->set_int8_t(convert_data[i]);
    }
  } else if (data_type == "uint8_t") {
    uint8_t* convert_data = reinterpret_cast<uint8_t*>(read_data);
    for (size_t i = 0; i < read_data_size; i++) {
      VariableSpecificationMessage* item = fmq_response->add_read_data();
      item->set_type(TYPE_SCALAR);
      item->set_scalar_type(data_type);
      (item->mutable_scalar_value())->set_uint8_t(convert_data[i]);
    }
  } else if (data_type == "int16_t") {
    int16_t* convert_data = reinterpret_cast<int16_t*>(read_data);
    for (size_t i = 0; i < read_data_size; i++) {
      VariableSpecificationMessage* item = fmq_response->add_read_data();
      item->set_type(TYPE_SCALAR);
      item->set_scalar_type(data_type);
      (item->mutable_scalar_value())->set_int16_t(convert_data[i]);
    }
  } else if (data_type == "uint16_t") {
    uint16_t* convert_data = reinterpret_cast<uint16_t*>(read_data);
    for (size_t i = 0; i < read_data_size; i++) {
      VariableSpecificationMessage* item = fmq_response->add_read_data();
      item->set_type(TYPE_SCALAR);
      item->set_scalar_type(data_type);
      (item->mutable_scalar_value())->set_uint16_t(convert_data[i]);
    }
  } else if (data_type == "int32_t") {
    int32_t* convert_data = reinterpret_cast<int32_t*>(read_data);
    for (size_t i = 0; i < read_data_size; i++) {
      VariableSpecificationMessage* item = fmq_response->add_read_data();
      item->set_type(TYPE_SCALAR);
      item->set_scalar_type(data_type);
      (item->mutable_scalar_value())->set_int32_t(convert_data[i]);
    }
  } else if (data_type == "uint32_t") {
    uint32_t* convert_data = reinterpret_cast<uint32_t*>(read_data);
    for (size_t i = 0; i < read_data_size; i++) {
      VariableSpecificationMessage* item = fmq_response->add_read_data();
      item->set_type(TYPE_SCALAR);
      item->set_scalar_type(data_type);
      (item->mutable_scalar_value())->set_uint32_t(convert_data[i]);
    }
  } else if (data_type == "int64_t") {
    int64_t* convert_data = reinterpret_cast<int64_t*>(read_data);
    for (size_t i = 0; i < read_data_size; i++) {
      VariableSpecificationMessage* item = fmq_response->add_read_data();
      item->set_type(TYPE_SCALAR);
      item->set_scalar_type(data_type);
      (item->mutable_scalar_value())->set_int64_t(convert_data[i]);
    }
  } else if (data_type == "uint64_t") {
    uint64_t* convert_data = reinterpret_cast<uint64_t*>(read_data);
    for (size_t i = 0; i < read_data_size; i++) {
      VariableSpecificationMessage* item = fmq_response->add_read_data();
      item->set_type(TYPE_SCALAR);
      item->set_scalar_type(data_type);
      (item->mutable_scalar_value())->set_uint64_t(convert_data[i]);
    }
  } else if (data_type == "float_t") {
    float* convert_data = reinterpret_cast<float*>(read_data);
    for (size_t i = 0; i < read_data_size; i++) {
      VariableSpecificationMessage* item = fmq_response->add_read_data();
      item->set_type(TYPE_SCALAR);
      item->set_scalar_type(data_type);
      (item->mutable_scalar_value())->set_float_t(convert_data[i]);
    }
  } else if (data_type == "double_t") {
    double* convert_data = reinterpret_cast<double*>(read_data);
    for (size_t i = 0; i < read_data_size; i++) {
      VariableSpecificationMessage* item = fmq_response->add_read_data();
      item->set_type(TYPE_SCALAR);
      item->set_scalar_type(data_type);
      (item->mutable_scalar_value())->set_double_t(convert_data[i]);
    }
  } else if (data_type == "bool_t") {
    bool* convert_data = reinterpret_cast<bool*>(read_data);
    for (size_t i = 0; i < read_data_size; i++) {
      VariableSpecificationMessage* item = fmq_response->add_read_data();
      item->set_type(TYPE_SCALAR);
      item->set_scalar_type(data_type);
      (item->mutable_scalar_value())->set_bool_t(convert_data[i]);
    }
  } else {
    // Encounter a predefined type in HAL service.
    LOG(INFO) << "Resource manager: detected host side specifies a "
              << "predefined type.";
    void* shared_lib_obj = LoadSharedLibFromTypeName(data_type);
    if (!shared_lib_obj) return false;

    // Locate the symbol for the translation function.
    typedef void (*set_result_fn)(VariableSpecificationMessage*, T);
    set_result_fn parser =
        (set_result_fn)(GetTranslationFuncPtr(shared_lib_obj, data_type, 0));
    if (!parser) return false;  // Error logged in helper function.

    // Parse the data from C++ to protobuf.
    for (int i = 0; i < read_data_size; i++) {
      VariableSpecificationMessage* item = fmq_response->add_read_data();
      (*parser)(item, read_data[i]);
    }
    dlclose(shared_lib_obj);
  }
  return true;
}

void* VtsResourceManager::LoadSharedLibFromTypeName(const string& data_type) {
  // Base path.
  // TODO: Consider determining the path and bitness by passing a field
  // in the protobuf message.
  string shared_lib_path = "/data/local/tmp/64/";
  // Start searching after the first ::
  size_t curr_index = 0;
  size_t next_index;
  bool success = false;
  regex version_regex("V[0-9]+_[0-9]+");
  const string split_str = "::";

  while ((next_index = data_type.find(split_str, curr_index)) != string::npos) {
    if (curr_index == next_index) {
      // No character between the current :: and the next ::
      // Likely it is the first :: in the type name.
      curr_index = next_index + split_str.length();
      continue;
    }
    string curr_string = data_type.substr(curr_index, next_index - curr_index);
    // Check if it is a version, e.g. V4_0.
    if (regex_match(curr_string, version_regex)) {
      size_t length = shared_lib_path.length();
      // Change _ into ., e.g. V4_0 to V4.0.
      size_t separator = curr_string.find("_");
      curr_string.replace(separator, 1, ".");
      // Use @ before version.
      shared_lib_path.replace(length - 1, 1, "@");
      // Exclude V in the front.
      shared_lib_path += curr_string.substr(1);
      success = true;
      break;
    } else {
      shared_lib_path += curr_string + ".";
    }
    // Start searching from the next ::
    curr_index = next_index + split_str.length();
  }
  // Failed to parse the shared library name from type name.
  if (!success) return nullptr;

  shared_lib_path += "-vts.driver.so";
  // Load the shared library that contains translation functions.
  void* shared_lib_obj = dlopen(shared_lib_path.c_str(), RTLD_LAZY);
  if (!shared_lib_obj) {
    LOG(ERROR) << "Resource manager: failed to load shared lib "
               << shared_lib_path << " for type " << data_type;
    return nullptr;
  }
  LOG(INFO) << "Resource manager: successfully loaded shared library "
            << shared_lib_path;
  return shared_lib_obj;
}

void* VtsResourceManager::GetTranslationFuncPtr(void* shared_lib_obj,
                                                const string& data_type,
                                                bool is_proto_to_cpp) {
  string translation_func_name = data_type;
  // Replace all :: with __.
  // TODO: there might be a special case where part of the type name involves
  // a single :. Consider fixing this in the future.
  replace(translation_func_name.begin(), translation_func_name.end(), ':', '_');

  void* func_ptr = nullptr;
  if (is_proto_to_cpp) {
    func_ptr =
        dlsym(shared_lib_obj, ("MessageTo" + translation_func_name).c_str());
    if (!func_ptr) {
      LOG(ERROR) << "Resource manager: failed to load function name "
                 << "MessageTo" << translation_func_name << " or "
                 << "EnumValue" << translation_func_name
                 << " to parse protobuf message into C++ type.";
    }
  } else {
    func_ptr =
        dlsym(shared_lib_obj, ("SetResult" + translation_func_name).c_str());
    if (!func_ptr) {
      LOG(ERROR) << "Resource manager: failed to load the function name "
                 << "SetResult" << translation_func_name
                 << " to parse C++ type into protobuf message.";
    }
  }
  return func_ptr;
}

}  // namespace vts
}  // namespace android
