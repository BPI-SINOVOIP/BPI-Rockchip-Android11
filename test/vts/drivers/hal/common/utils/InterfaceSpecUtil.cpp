/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "utils/InterfaceSpecUtil.h"

#include <fstream>
#include <sstream>
#include <string>

#include <android-base/logging.h>
#include <assert.h>
#include <google/protobuf/message.h>
#include <google/protobuf/text_format.h>

#include "utils/StringUtil.h"
#include "test/vts/proto/ComponentSpecificationMessage.pb.h"

using namespace std;

namespace android {
namespace vts {

bool ParseInterfaceSpec(const char* file_path,
                        ComponentSpecificationMessage* message) {
  ifstream in_file(file_path);
  stringstream str_stream;
  if (!in_file.is_open()) {
    LOG(ERROR) << "Unable to open file. " << file_path;
    return false;
  }
  str_stream << in_file.rdbuf();
  in_file.close();
  const string data = str_stream.str();

  message->Clear();
  if (!google::protobuf::TextFormat::MergeFromString(data, message)) {
    LOG(ERROR) << "Can't parse a given proto file " << file_path;
    return false;
  }
  return true;
}

string GetFunctionNamePrefix(const ComponentSpecificationMessage& message) {
  stringstream prefix_ss;
  if (message.component_class() != HAL_HIDL) {
    prefix_ss << VTS_INTERFACE_SPECIFICATION_FUNCTION_NAME_PREFIX
              << message.component_class() << "_" << message.component_type()
              << "_"
              << GetVersionString(message.component_type_version_major(),
                                  message.component_type_version_minor(),
                                  true)  // set flag to true, use macro version,
                                         // e.g. V1_10.
              << "_";
  } else {
    string package_as_function_name(message.package());
    ReplaceSubString(package_as_function_name, ".", "_");
    prefix_ss << VTS_INTERFACE_SPECIFICATION_FUNCTION_NAME_PREFIX
              << message.component_class() << "_" << package_as_function_name
              << "_"
              << GetVersionString(message.component_type_version_major(),
                                  message.component_type_version_minor(),
                                  true)  // set flag to true, use macro version,
                                         // e.g. V1_10.
              << "_" << message.component_name() << "_";
  }
  return prefix_ss.str();
}

#define DEFAULT_FACTOR 10000

// deprecated
string GetVersionString(float version, bool for_macro) {
  std::ostringstream out;
  if (for_macro) {
    out << "V";
  }
  long version_long = version * DEFAULT_FACTOR;
  out << (version_long / DEFAULT_FACTOR);
  if (!for_macro) {
    out << ".";
  } else {
    out << "_";
  }
  version_long -= (version_long / DEFAULT_FACTOR) * DEFAULT_FACTOR;
  bool first = true;
  long factor = DEFAULT_FACTOR / 10;
  while (first || (version_long > 0 && factor > 1)) {
    out << (version_long / factor);
    version_long -= (version_long / factor) * factor;
    factor /= 10;
    first = false;
  }
  return out.str();
}

string GetVersionString(int version_major, int version_minor, bool for_macro) {
  std::ostringstream out;

  if (for_macro) out << "V";

  out << version_major;
  out << (for_macro ? "_" : ".");
  out << version_minor;
  return out.str();
}

string GetHidlHalDriverLibName(const string& package_name,
                               const int version_major,
                               const int version_minor) {
  return package_name + "@" + GetVersionString(version_major, version_minor) +
         "-vts.driver.so";
}

string GetInterfaceFQName(const string& package_name, const int version_major,
                          const int version_minor,
                          const string& interface_name) {
  return package_name + "@" + GetVersionString(version_major, version_minor) +
         "::" + interface_name;
}

string GetPackageName(const string& type_name) {
  string str = type_name.substr(0, type_name.find('V') - strlen("::"));
  if (str.find("::") == 0) {
    str = str.substr(strlen("::"));
  }
  ReplaceSubString(str, "::", ".");
  return str;
}

string GetVersion(const string& type_name) {
  string str = type_name.substr(type_name.find('V') + 1);
  string version_str = str.substr(0, str.find("::"));
  return version_str;
}

int GetVersionMajor(const string& version, bool for_macro) {
  if (for_macro) return std::stoi(version.substr(0, version.find("_")));
  return std::stoi(version.substr(0, version.find(".")));
}

int GetVersionMinor(const string& version, bool for_macro) {
  if (for_macro) return std::stoi(version.substr(version.find("_") + 1));
  return std::stoi(version.substr(version.find(".") + 1));
}

string GetComponentName(const string& type_name) {
  string str = type_name.substr(type_name.find('V'));
  return str.substr(str.find("::") + strlen("::"));
}

}  // namespace vts
}  // namespace android
