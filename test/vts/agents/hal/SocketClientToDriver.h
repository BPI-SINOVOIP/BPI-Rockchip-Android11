/*
 * Copyright 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __VTS_FUZZER_TCP_CLIENT_H_
#define __VTS_FUZZER_TCP_CLIENT_H_

#include <string>
#include <vector>

#include <VtsDriverCommUtil.h>
#include "test/vts/proto/VtsDriverControlMessage.pb.h"
#include "test/vts/proto/VtsResourceControllerMessage.pb.h"

using namespace std;

namespace android {
namespace vts {

// Socket client instance for an agent to control a driver.
class VtsDriverSocketClient : public VtsDriverCommUtil {
 public:
  explicit VtsDriverSocketClient() : VtsDriverCommUtil() {}

  // closes the socket.
  void Close();

  // Sends a EXIT request;
  bool Exit();

  // Sends a LOAD_HAL request.
  // Args:
  //   target_version_major: int, hal major version
  //   target_version_minor: int, hal minor version
  int32_t LoadHal(const string& file_path, int target_class, int target_type,
                  int target_version_major, int target_version_minor,
                  const string& target_package,
                  const string& target_component_name,
                  const string& hw_binder_service_name,
                  const string& module_name);

  // Sends a LIST_FUNCTIONS request.
  string GetFunctions();

  // Sends a VTS_DRIVER_COMMAND_READ_SPECIFICATION request.
  // Args:
  //   target_version_major: int, hal major version
  //   target_version_minor: int, hal minor version
  string ReadSpecification(const string& component_name, int target_class,
                           int target_type, int target_version_major,
                           int target_version_minor,
                           const string& target_package);

  // Sends a CALL_FUNCTION request.
  string Call(const string& arg, const string& uid);

  // Sends a GET_ATTRIBUTE request.
  string GetAttribute(const string& arg);

  // Sends a GET_STATUS request.
  int32_t Status(int32_t type);

  // Sends a EXECUTE request.
  unique_ptr<VtsDriverControlResponseMessage> ExecuteShellCommand(
      const ::google::protobuf::RepeatedPtrField<::std::string> shell_command);

  // Processes the command for a FMQ request, stores the result in fmq_response.
  //
  // @param fmq_request  contains arguments in a request message for FMQ driver.
  // @param fmq_response pointer to the message that will be sent back to host.
  //
  // @return true if api is called successfully and data have been transferred
  //              without error,
  //         false otherwise.
  bool ProcessFmqCommand(const FmqRequestMessage& fmq_request,
                         FmqResponseMessage* fmq_response);

  // Processes the command for a hidl_memory request, stores the result in
  // hidl_memory_response.
  //
  // @param hidl_memory_request  contains arguments in a request message for
  //                             hidl_memory driver.
  // @param hidl_memory_response pointer to message sent back to host.
  //
  // @return true if api is called successfully and data have been transferred
  //              without error,
  //         false otherwise.
  bool ProcessHidlMemoryCommand(
      const HidlMemoryRequestMessage& hidl_memory_request,
      HidlMemoryResponseMessage* hidl_memory_response);

  // Processes the command for a hidl_handle request, stores the result in
  // hidl_handle_response.
  //
  // @param hidl_handle_request  contains arguments in a request message for
  //                             hidl_handle driver.
  // @param hidl_handle_response pointer to message sent back to host.
  //
  // @return true if api is called successfully and data have been transferred
  //              without error,
  //         false otherwise.
  bool ProcessHidlHandleCommand(
      const HidlHandleRequestMessage& hidl_handle_request,
      HidlHandleResponseMessage* hidl_handle_response);
};

// returns the socket port file's path for the given service_name.
extern string GetSocketPortFilePath(const string& service_name);

// returns true if the specified driver is running.
bool IsDriverRunning(const string& service_name, int retry_count);

// creates and returns VtsDriverSocketClient of the given service_name.
extern VtsDriverSocketClient* GetDriverSocketClient(const string& service_name);

}  // namespace vts
}  // namespace android

#endif  // __VTS_FUZZER_TCP_CLIENT_H_
