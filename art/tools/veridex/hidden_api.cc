/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "hidden_api.h"

#include <fstream>
#include <sstream>

#include "android-base/strings.h"
#include "dex/dex_file-inl.h"

namespace art {

HiddenApi::HiddenApi(const char* filename, const ApiListFilter& api_list_filter)
    : api_list_filter_(api_list_filter) {
  CHECK(filename != nullptr);

  std::ifstream in(filename);
  for (std::string str; std::getline(in, str);) {
    std::vector<std::string> values = android::base::Split(str, ",");
    const std::string& signature = values[0];

    hiddenapi::ApiList membership;
    bool success = hiddenapi::ApiList::FromNames(values.begin() + 1, values.end(), &membership);
    CHECK(success) << "Unknown ApiList flag: " << str;
    CHECK(membership.IsValid()) << "Invalid ApiList: " << membership;

    AddSignatureToApiList(signature, membership);
    size_t pos = signature.find("->");
    if (pos != std::string::npos) {
      // Add the class name.
      AddSignatureToApiList(signature.substr(0, pos), membership);
      pos = signature.find('(');
      if (pos != std::string::npos) {
        // Add the class->method name (so stripping the signature).
        AddSignatureToApiList(signature.substr(0, pos), membership);
      }
      pos = signature.find(':');
      if (pos != std::string::npos) {
        // Add the class->field name (so stripping the type).
        AddSignatureToApiList(signature.substr(0, pos), membership);
      }
    }
  }
}

void HiddenApi::AddSignatureToApiList(const std::string& signature, hiddenapi::ApiList membership) {
  auto it = api_list_.find(signature);
  if (it == api_list_.end()) {
    // Does not exist yet. Add it to list.
    api_list_.emplace(signature, membership);
  } else if (membership.GetMaxAllowedSdkVersion() < it->second.GetMaxAllowedSdkVersion()) {
    // Already exist but `membership` is more restrictive.
    it->second = membership;
  } else {
    // Already exists and `membership` is equally or less restrictive.
  }
}

std::string HiddenApi::GetApiMethodName(const DexFile& dex_file, uint32_t method_index) {
  std::stringstream ss;
  const dex::MethodId& method_id = dex_file.GetMethodId(method_index);
  ss << dex_file.StringByTypeIdx(method_id.class_idx_)
     << "->"
     << dex_file.GetMethodName(method_id)
     << dex_file.GetMethodSignature(method_id).ToString();
  return ss.str();
}

std::string HiddenApi::GetApiFieldName(const DexFile& dex_file, uint32_t field_index) {
  std::stringstream ss;
  const dex::FieldId& field_id = dex_file.GetFieldId(field_index);
  ss << dex_file.StringByTypeIdx(field_id.class_idx_)
     << "->"
     << dex_file.GetFieldName(field_id)
     << ":"
     << dex_file.GetFieldTypeDescriptor(field_id);
  return ss.str();
}

}  // namespace art
