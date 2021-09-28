/*
 * Copyright (C) 2017 The Android Open Source Project
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
#define LOG_TAG "VtsHalDriverManager"

#include "driver_manager/VtsHalDriverManager.h"

#include <iostream>
#include <string>

#include <android-base/logging.h>
#include <google/protobuf/text_format.h>

#include "utils/InterfaceSpecUtil.h"
#include "utils/StringUtil.h"

static constexpr const char* kErrorString = "error";
static constexpr const char* kVoidString = "void";
static constexpr const int kInvalidDriverId = -1;

namespace android {
namespace vts {

VtsHalDriverManager::VtsHalDriverManager(const string& spec_dir,
                                         const int epoch_count,
                                         const string& callback_socket_name,
                                         VtsResourceManager* resource_manager)
    : callback_socket_name_(callback_socket_name),
      hal_driver_loader_(
          HalDriverLoader(spec_dir, epoch_count, callback_socket_name)),
      resource_manager_(resource_manager) {}

DriverId VtsHalDriverManager::LoadTargetComponent(
    const string& dll_file_name, const string& spec_lib_file_path,
    const int component_class, const int component_type,
    const int version_major, const int version_minor,
    const string& package_name, const string& component_name,
    const string& hw_binder_service_name) {
  LOG(DEBUG) << "dll_file_name = " << dll_file_name;
  ComponentSpecificationMessage spec_message;
  if (!hal_driver_loader_.FindComponentSpecification(
          component_class, package_name, version_major, version_minor,
          component_name, component_type, &spec_message)) {
    LOG(ERROR) << "Failed to load specification for component: "
               << GetComponentDebugMsg(
                      component_class, component_type,
                      GetVersionString(version_major, version_minor),
                      package_name, component_name);
    return kInvalidDriverId;
  }
  LOG(INFO) << "Loaded specification for component: "
            << GetComponentDebugMsg(
                   component_class, component_type,
                   GetVersionString(version_major, version_minor), package_name,
                   component_name);

  string driver_lib_path = "";
  if (component_class == HAL_HIDL) {
    driver_lib_path =
        GetHidlHalDriverLibName(package_name, version_major, version_minor);
  } else {
    driver_lib_path = spec_lib_file_path;
  }

  LOG(DEBUG) << "driver lib path " << driver_lib_path;

  std::unique_ptr<DriverBase> hal_driver = nullptr;
  hal_driver.reset(hal_driver_loader_.GetDriver(driver_lib_path, spec_message,
                                                hw_binder_service_name, 0,
                                                false, dll_file_name));
  if (!hal_driver) {
    LOG(ERROR) << "Can't load driver for component: "
               << GetComponentDebugMsg(
                      component_class, component_type,
                      GetVersionString(version_major, version_minor),
                      package_name, component_name);
    return kInvalidDriverId;
  } else {
    LOG(INFO) << "Loaded driver for component: "
              << GetComponentDebugMsg(
                     component_class, component_type,
                     GetVersionString(version_major, version_minor),
                     package_name, component_name);
  }
  // TODO (zhuoyao): get hidl_proxy_pointer for loaded hidl hal dirver.
  uint64_t interface_pt = 0;
  return RegisterDriver(std::move(hal_driver), spec_message, interface_pt);
}

string VtsHalDriverManager::CallFunction(FunctionCallMessage* call_msg) {
  string output = "";
  DriverBase* driver = GetDriverWithCallMsg(*call_msg);
  if (!driver) {
    LOG(ERROR) << "can't find driver for component: "
               << GetComponentDebugMsg(
                      call_msg->component_class(), call_msg->component_type(),
                      GetVersionString(
                          call_msg->component_type_version_major(),
                          call_msg->component_type_version_minor()),
                      call_msg->package_name(), call_msg->component_name());
    return kErrorString;
  }

  FunctionSpecificationMessage* api = call_msg->mutable_api();
  void* result;
  FunctionSpecificationMessage result_msg;
  driver->FunctionCallBegin();
  LOG(DEBUG) << "Call Function " << api->name();
  if (call_msg->component_class() == HAL_HIDL) {
    // Pre-processing if we want to call an API with an interface as argument.
    for (int index = 0; index < api->arg_size(); index++) {
      auto* arg = api->mutable_arg(index);
      bool process_success = PreprocessHidlHalFunctionCallArgs(arg);
      if (!process_success) {
        LOG(ERROR) << "Error in preprocess argument index " << index;
        return kErrorString;
      }
    }
    // For Hidl HAL, use CallFunction method.
    if (!driver->CallFunction(*api, callback_socket_name_, &result_msg)) {
      LOG(ERROR) << "Failed to call function: " << api->DebugString();
      return kErrorString;
    }
  } else {
    if (!driver->Fuzz(api, &result, callback_socket_name_)) {
      LOG(ERROR) << "Failed to call function: " << api->DebugString();
      return kErrorString;
    }
  }
  LOG(DEBUG) << "Called function " << api->name();

  // set coverage data.
  driver->FunctionCallEnd(api);

  if (call_msg->component_class() == HAL_HIDL) {
    for (int index = 0; index < result_msg.return_type_hidl_size(); index++) {
      auto* return_val = result_msg.mutable_return_type_hidl(index);
      bool set_success = SetHidlHalFunctionCallResults(return_val);
      if (!set_success) {
        LOG(ERROR) << "Error in setting return value index " << index;
        return kErrorString;
      }
    }
    google::protobuf::TextFormat::PrintToString(result_msg, &output);
    return output;
  } else if (call_msg->component_class() == LIB_SHARED) {
    return ProcessFuncResultsForLibrary(api, result);
  }
  return kVoidString;
}

bool VtsHalDriverManager::VerifyResults(
    DriverId id, const FunctionSpecificationMessage& expected_result,
    const FunctionSpecificationMessage& actual_result) {
  DriverBase* driver = GetDriverById(id);
  if (!driver) {
    LOG(ERROR) << "Can't find driver with id: " << id;
    return false;
  }
  return driver->VerifyResults(expected_result, actual_result);
}

string VtsHalDriverManager::GetAttribute(FunctionCallMessage* call_msg) {
  string output = "";
  DriverBase* driver = GetDriverWithCallMsg(*call_msg);
  if (!driver) {
    LOG(ERROR) << "Can't find driver for component: "
               << GetComponentDebugMsg(
                      call_msg->component_class(), call_msg->component_type(),
                      GetVersionString(
                          call_msg->component_type_version_major(),
                          call_msg->component_type_version_minor()),
                      call_msg->package_name(), call_msg->component_name());
    return kErrorString;
  }

  void* result;
  FunctionSpecificationMessage* api = call_msg->mutable_api();
  LOG(DEBUG) << "Get Atrribute " << api->name() << " parent_path("
             << api->parent_path() << ")";
  if (!driver->GetAttribute(api, &result)) {
    LOG(ERROR) << "attribute not found - todo handle more explicitly";
    return kErrorString;
  }

  if (call_msg->component_class() == HAL_HIDL) {
    api->mutable_return_type()->set_type(TYPE_STRING);
    api->mutable_return_type()->mutable_string_value()->set_message(
        *(string*)result);
    api->mutable_return_type()->mutable_string_value()->set_length(
        ((string*)result)->size());
    free(result);
    string* output = new string();
    google::protobuf::TextFormat::PrintToString(*api, output);
    return *output;
  } else if (call_msg->component_class() == LIB_SHARED) {
    return ProcessFuncResultsForLibrary(api, result);
  }
  return kVoidString;
}

DriverId VtsHalDriverManager::RegisterDriver(
    std::unique_ptr<DriverBase> driver,
    const ComponentSpecificationMessage& spec_msg,
    const uint64_t interface_pt) {
  DriverId driver_id = FindDriverIdInternal(spec_msg, interface_pt, true);
  if (driver_id == kInvalidDriverId) {
    driver_id = hal_driver_map_.size();
    hal_driver_map_.insert(make_pair(
        driver_id, HalDriverInfo(spec_msg, interface_pt, std::move(driver))));
  } else {
    LOG(WARNING) << "Driver already exists. ";
  }

  return driver_id;
}

DriverBase* VtsHalDriverManager::GetDriverById(const DriverId id) {
  auto res = hal_driver_map_.find(id);
  if (res == hal_driver_map_.end()) {
    LOG(ERROR) << "Failed to find driver info with id: " << id;
    return nullptr;
  }
  LOG(DEBUG) << "Found driver info with id: " << id;
  return res->second.driver.get();
}

uint64_t VtsHalDriverManager::GetDriverPointerById(const DriverId id) {
  auto res = hal_driver_map_.find(id);
  if (res == hal_driver_map_.end()) {
    LOG(ERROR) << "Failed to find driver info with id: " << id;
    return 0;
  }
  LOG(DEBUG) << "Found driver info with id: " << id;
  return res->second.hidl_hal_proxy_pt;
}

DriverId VtsHalDriverManager::GetDriverIdForHidlHalInterface(
    const string& package_name, const int version_major,
    const int version_minor, const string& interface_name,
    const string& hal_service_name) {
  ComponentSpecificationMessage spec_msg;
  spec_msg.set_component_class(HAL_HIDL);
  spec_msg.set_package(package_name);
  spec_msg.set_component_type_version_major(version_major);
  spec_msg.set_component_type_version_minor(version_minor);
  spec_msg.set_component_name(interface_name);
  DriverId driver_id = FindDriverIdInternal(spec_msg);
  if (driver_id == kInvalidDriverId) {
    string driver_lib_path =
        GetHidlHalDriverLibName(package_name, version_major, version_minor);
    driver_id = LoadTargetComponent("", driver_lib_path, HAL_HIDL, 0,
                                    version_major, version_minor, package_name,
                                    interface_name, hal_service_name);
  }
  return driver_id;
}

bool VtsHalDriverManager::FindComponentSpecification(
    const int component_class, const int component_type,
    const int version_major, const int version_minor,
    const string& package_name, const string& component_name,
    ComponentSpecificationMessage* spec_msg) {
  return hal_driver_loader_.FindComponentSpecification(
      component_class, package_name, version_major, version_minor,
      component_name, component_type, spec_msg);
}

ComponentSpecificationMessage*
VtsHalDriverManager::GetComponentSpecification() {
  if (hal_driver_map_.empty()) {
    return nullptr;
  } else {
    return &(hal_driver_map_.find(0)->second.spec_msg);
  }
}

DriverId VtsHalDriverManager::FindDriverIdInternal(
    const ComponentSpecificationMessage& spec_msg, const uint64_t interface_pt,
    bool with_interface_pointer) {
  if (!spec_msg.has_component_class()) {
    LOG(ERROR) << "Component class not specified. ";
    return kInvalidDriverId;
  }
  if (spec_msg.component_class() == HAL_HIDL) {
    if (!spec_msg.has_package() || spec_msg.package().empty()) {
      LOG(ERROR) << "Package name is required but not specified.";
      return kInvalidDriverId;
    }
    if (!spec_msg.has_component_type_version_major() ||
        !spec_msg.has_component_type_version_minor()) {
      LOG(ERROR) << "Package version is required but not specified.";
      return kInvalidDriverId;
    }
    if (!spec_msg.has_component_name() || spec_msg.component_name().empty()) {
      LOG(ERROR) << "Component name is required but not specified.";
      return kInvalidDriverId;
    }
  }
  for (auto it = hal_driver_map_.begin(); it != hal_driver_map_.end(); ++it) {
    ComponentSpecificationMessage cur_spec_msg = it->second.spec_msg;
    if (cur_spec_msg.component_class() != spec_msg.component_class()) {
      continue;
    }
    // If package name is specified, match package name.
    if (spec_msg.has_package()) {
      if (!cur_spec_msg.has_package() ||
          cur_spec_msg.package() != spec_msg.package()) {
        continue;
      }
    }
    // If version is specified, match version.
    if (spec_msg.has_component_type_version_major() &&
        spec_msg.has_component_type_version_minor()) {
      if (!cur_spec_msg.has_component_type_version_major() ||
          !cur_spec_msg.has_component_type_version_minor() ||
          cur_spec_msg.component_type_version_major() !=
              spec_msg.component_type_version_major() ||
          cur_spec_msg.component_type_version_minor() !=
              spec_msg.component_type_version_minor()) {
        continue;
      }
    }
    if (spec_msg.component_class() == HAL_HIDL) {
      if (cur_spec_msg.component_name() != spec_msg.component_name()) {
        continue;
      }
      if (with_interface_pointer &&
          it->second.hidl_hal_proxy_pt != interface_pt) {
        continue;
      }
      LOG(DEBUG) << "Found hidl hal driver with id: " << it->first;
      return it->first;
    } else if (spec_msg.component_class() == LIB_SHARED) {
      if (spec_msg.has_component_type() &&
          cur_spec_msg.component_type() == spec_msg.component_type()) {
        LOG(DEBUG) << "Found shared lib driver with id: " << it->first;
        return it->first;
      }
    }
  }
  return kInvalidDriverId;
}

DriverBase* VtsHalDriverManager::GetDriverWithCallMsg(
    const FunctionCallMessage& call_msg) {
  DriverId driver_id = kInvalidDriverId;
  // If call_mag contains driver_id, use that given driver id.
  if (call_msg.has_hal_driver_id() &&
      call_msg.hal_driver_id() != kInvalidDriverId) {
    driver_id = call_msg.hal_driver_id();
  } else {
    // Otherwise, try to find a registed driver matches the given info. e.g.,
    // package_name, version etc.
    ComponentSpecificationMessage spec_msg;
    spec_msg.set_component_class(call_msg.component_class());
    spec_msg.set_package(call_msg.package_name());
    spec_msg.set_component_type_version_major(
        call_msg.component_type_version_major());
    spec_msg.set_component_type_version_minor(
        call_msg.component_type_version_minor());
    spec_msg.set_component_name(call_msg.component_name());
    driver_id = FindDriverIdInternal(spec_msg);
  }

  if (driver_id == kInvalidDriverId) {
    LOG(ERROR) << "Can't find driver ID for package: "
               << call_msg.package_name() << " version: "
               << GetVersionString(call_msg.component_type_version_major(),
                                   call_msg.component_type_version_minor());
    return nullptr;
  } else {
    return GetDriverById(driver_id);
  }
}

string VtsHalDriverManager::ProcessFuncResultsForLibrary(
    FunctionSpecificationMessage* func_msg, void* result) {
  string output = "";
  if (func_msg->return_type().type() == TYPE_PREDEFINED) {
    // TODO: actually handle this case.
    if (result != NULL) {
      // loads that interface spec and enqueues all functions.
      LOG(DEBUG) << "Return type: " << func_msg->return_type().type();
    } else {
      LOG(ERROR) << "Return value = NULL";
    }
    LOG(ERROR) << "Todo: support aggregate";
    google::protobuf::TextFormat::PrintToString(*func_msg, &output);
    return output;
  } else if (func_msg->return_type().type() == TYPE_SCALAR) {
    // TODO handle when the size > 1.
    // todo handle more types;
    if (!strcmp(func_msg->return_type().scalar_type().c_str(), "int32_t")) {
      func_msg->mutable_return_type()->mutable_scalar_value()->set_int32_t(
          *((int*)(&result)));
      google::protobuf::TextFormat::PrintToString(*func_msg, &output);
      return output;
    } else if (!strcmp(func_msg->return_type().scalar_type().c_str(),
                       "uint32_t")) {
      func_msg->mutable_return_type()->mutable_scalar_value()->set_uint32_t(
          *((int*)(&result)));
      google::protobuf::TextFormat::PrintToString(*func_msg, &output);
      return output;
    } else if (!strcmp(func_msg->return_type().scalar_type().c_str(),
                       "int16_t")) {
      func_msg->mutable_return_type()->mutable_scalar_value()->set_int16_t(
          *((int*)(&result)));
      google::protobuf::TextFormat::PrintToString(*func_msg, &output);
      return output;
    } else if (!strcmp(func_msg->return_type().scalar_type().c_str(),
                       "uint16_t")) {
      google::protobuf::TextFormat::PrintToString(*func_msg, &output);
      return output;
    }
  }
  return kVoidString;
}

string VtsHalDriverManager::GetComponentDebugMsg(const int component_class,
                                                 const int component_type,
                                                 const string& version,
                                                 const string& package_name,
                                                 const string& component_name) {
  if (component_class == HAL_HIDL) {
    return "HIDL_HAL: " + package_name + "@" + version + "::" + component_name;
  } else {
    return "component_type: " + std::to_string(component_type) +
           " version: " + version + " component_name: " + component_name;
  }
}

bool VtsHalDriverManager::PreprocessHidlHalFunctionCallArgs(
    VariableSpecificationMessage* arg) {
  switch (arg->type()) {
    case TYPE_ARRAY:
    case TYPE_VECTOR: {
      // Recursively parse each element in the vector/array.
      for (int i = 0; i < arg->vector_size(); i++) {
        if (!PreprocessHidlHalFunctionCallArgs(arg->mutable_vector_value(i))) {
          // Bad argument, preprocess failure.
          LOG(ERROR) << "Failed to preprocess vector value " << i << ".";
          return false;
        }
      }
      break;
    }
    case TYPE_UNION: {
      // Recursively parse each union value.
      for (int i = 0; i < arg->union_value_size(); i++) {
        auto* union_field = arg->mutable_union_value(i);
        if (!PreprocessHidlHalFunctionCallArgs(union_field)) {
          // Bad argument, preprocess failure.
          LOG(ERROR) << "Failed to preprocess union field \""
                     << union_field->name() << "\" in union \"" << arg->name()
                     << "\".";
          return false;
        }
      }
      break;
    }
    case TYPE_STRUCT: {
      // Recursively parse each struct value.
      for (int i = 0; i < arg->struct_value_size(); i++) {
        auto* struct_field = arg->mutable_struct_value(i);
        if (!PreprocessHidlHalFunctionCallArgs(struct_field)) {
          // Bad argument, preprocess failure.
          LOG(ERROR) << "Failed to preprocess struct field \""
                     << struct_field->name() << "\" in struct \"" << arg->name()
                     << "\".";
          return false;
        }
      }
      break;
    }
    case TYPE_REF: {
      if (!PreprocessHidlHalFunctionCallArgs(arg->mutable_ref_value())) {
        // Bad argument, preprocess failure.
        LOG(ERROR) << "Failed to preprocess reference value with name \""
                   << arg->name() << "\".";
        return false;
      }
      break;
    }
    case TYPE_HIDL_INTERFACE: {
      string type_name = arg->predefined_type();
      ComponentSpecificationMessage spec_msg;
      string version_str = GetVersion(type_name);
      int version_major = GetVersionMajor(version_str, true);
      int version_minor = GetVersionMinor(version_str, true);
      spec_msg.set_package(GetPackageName(type_name));
      spec_msg.set_component_type_version_major(version_major);
      spec_msg.set_component_type_version_minor(version_minor);
      spec_msg.set_component_name(GetComponentName(type_name));
      DriverId driver_id = FindDriverIdInternal(spec_msg);
      // If found a registered driver for the interface, set the pointer in
      // the arg proto.
      if (driver_id != kInvalidDriverId) {
        uint64_t interface_pt = GetDriverPointerById(driver_id);
        arg->set_hidl_interface_pointer(interface_pt);
      }
      break;
    }
    case TYPE_FMQ_SYNC:
    case TYPE_FMQ_UNSYNC: {
      if (arg->fmq_value_size() == 0) {
        LOG(ERROR) << "Driver manager: host side didn't specify queue "
                   << "information in fmq_value field.";
        return false;
      }
      if (arg->fmq_value(0).fmq_id() != -1) {
        // Preprocess an argument that wants to use an existing FMQ.
        // resource_manager returns address of hidl_memory pointer and
        // driver_manager fills the address in the proto field,
        // which can be read by HAL driver.
        size_t descriptor_addr;
        bool success =
            resource_manager_->GetQueueDescAddress(*arg, &descriptor_addr);
        if (!success) {
          LOG(ERROR) << "Unable to find queue descriptor for queue with id "
                     << arg->fmq_value(0).fmq_id();
          return false;
        }
        arg->mutable_fmq_value(0)->set_fmq_desc_address(descriptor_addr);
      }
      break;
    }
    case TYPE_HIDL_MEMORY: {
      if (arg->hidl_memory_value().mem_id() != -1) {
        // Preprocess an argument that wants to use an existing hidl_memory.
        // resource_manager returns the address of the hidl_memory pointer,
        // and driver_manager fills the address in the proto field,
        // which can be read by vtsc.
        size_t hidl_mem_address;
        bool success =
            resource_manager_->GetHidlMemoryAddress(*arg, &hidl_mem_address);
        if (!success) {
          LOG(ERROR) << "Unable to find hidl_memory with id "
                     << arg->hidl_memory_value().mem_id();
          return false;
        }
        arg->mutable_hidl_memory_value()->set_hidl_mem_address(
            hidl_mem_address);
      }
      break;
    }
    case TYPE_HANDLE: {
      if (arg->handle_value().handle_id() != -1) {
        // Preprocess an argument that wants to use an existing hidl_handle.
        // resource_manager returns the address of the hidl_memory pointer,
        // and driver_manager fills the address in the proto field,
        // which can be read by vtsc.
        size_t hidl_handle_address;
        bool success =
            resource_manager_->GetHidlHandleAddress(*arg, &hidl_handle_address);
        if (!success) {
          LOG(ERROR) << "Unable to find hidl_handle with id "
                     << arg->handle_value().handle_id();
          return false;
        }
        arg->mutable_handle_value()->set_hidl_handle_address(
            hidl_handle_address);
      }
      break;
    }
    default:
      break;
  }
  return true;
}

bool VtsHalDriverManager::SetHidlHalFunctionCallResults(
    VariableSpecificationMessage* return_val) {
  switch (return_val->type()) {
    case TYPE_ARRAY:
    case TYPE_VECTOR: {
      // Recursively set each element in the vector/array.
      for (int i = 0; i < return_val->vector_size(); i++) {
        if (!SetHidlHalFunctionCallResults(
                return_val->mutable_vector_value(i))) {
          // Failed to set recursive return value.
          LOG(ERROR) << "Failed to set vector value " << i << ".";
          return false;
        }
      }
      break;
    }
    case TYPE_UNION: {
      // Recursively set each field.
      for (int i = 0; i < return_val->union_value_size(); i++) {
        auto* union_field = return_val->mutable_union_value(i);
        if (!SetHidlHalFunctionCallResults(union_field)) {
          // Failed to set recursive return value.
          LOG(ERROR) << "Failed to set union field \"" << union_field->name()
                     << "\" in union \"" << return_val->name() << "\".";
          return false;
        }
      }
      break;
    }
    case TYPE_STRUCT: {
      // Recursively set each field.
      for (int i = 0; i < return_val->struct_value_size(); i++) {
        auto* struct_field = return_val->mutable_struct_value(i);
        if (!SetHidlHalFunctionCallResults(struct_field)) {
          // Failed to set recursive return value.
          LOG(ERROR) << "Failed to set struct field \"" << struct_field->name()
                     << "\" in struct \"" << return_val->name() << "\".";
          return false;
        }
      }
      break;
    }
    case TYPE_REF: {
      if (!SetHidlHalFunctionCallResults(return_val->mutable_ref_value())) {
        // Failed to set recursive return value.
        LOG(ERROR) << "Failed to set reference value for \""
                   << return_val->name() << "\".";
        return false;
      }
      break;
    }
    case TYPE_HIDL_INTERFACE: {
      if (return_val->hidl_interface_pointer() != 0) {
        string type_name = return_val->predefined_type();
        uint64_t interface_pt = return_val->hidl_interface_pointer();
        std::unique_ptr<DriverBase> driver;
        ComponentSpecificationMessage spec_msg;
        string version_str = GetVersion(type_name);
        int version_major = GetVersionMajor(version_str, true);
        int version_minor = GetVersionMinor(version_str, true);
        string package_name = GetPackageName(type_name);
        string component_name = GetComponentName(type_name);
        if (!hal_driver_loader_.FindComponentSpecification(
                HAL_HIDL, package_name, version_major, version_minor,
                component_name, 0, &spec_msg)) {
          LOG(ERROR) << "Failed to load specification for generated interface :"
                     << type_name;
          return false;
        }
        string driver_lib_path =
            GetHidlHalDriverLibName(package_name, version_major, version_minor);
        // TODO(zhuoyao): figure out a way to get the service_name.
        string hw_binder_service_name = "default";
        driver.reset(hal_driver_loader_.GetDriver(driver_lib_path, spec_msg,
                                                  hw_binder_service_name,
                                                  interface_pt, true, ""));
        int32_t driver_id =
            RegisterDriver(std::move(driver), spec_msg, interface_pt);
        return_val->set_hidl_interface_id(driver_id);
      } else {
        // in case of generated nullptr, set the driver_id to -1.
        return_val->set_hidl_interface_id(-1);
      }
      break;
    }
    case TYPE_FMQ_SYNC:
    case TYPE_FMQ_UNSYNC: {
      // Tell resource_manager to register a new FMQ.
      int new_queue_id = resource_manager_->RegisterFmq(*return_val);
      return_val->mutable_fmq_value(0)->set_fmq_id(new_queue_id);
      break;
    }
    case TYPE_HIDL_MEMORY: {
      // Tell resource_manager to register the new memory object.
      int new_mem_id = resource_manager_->RegisterHidlMemory(*return_val);
      return_val->mutable_hidl_memory_value()->set_mem_id(new_mem_id);
      break;
    }
    case TYPE_HANDLE: {
      // Tell resource_manager to register the new handle object.
      int new_handle_id = resource_manager_->RegisterHidlHandle(*return_val);
      return_val->mutable_handle_value()->set_handle_id(new_handle_id);
      break;
    }
    default:
      break;
  }
  return true;
}

}  // namespace vts
}  // namespace android
