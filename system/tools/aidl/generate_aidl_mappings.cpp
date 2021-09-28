/*
 * Copyright (C) 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "generate_aidl_mappings.h"
#include "aidl_to_java.h"

#include <sstream>

namespace android {
namespace aidl {
namespace mappings {

std::string dump_location(const AidlNode& method) {
  return method.PrintLine();
}

SignatureMap generate_mappings(const AidlDefinedType* defined_type,
                               const AidlTypenames& typenames) {
  const AidlInterface* interface = defined_type->AsInterface();
  SignatureMap mappings;
  if (interface == nullptr) {
    return mappings;
  }
  for (const auto& method : interface->GetMethods()) {
    if (method->IsUserDefined()) {
      std::stringstream signature;
      signature << interface->GetCanonicalName() << "|";
      signature << method->GetName() << "|";
      for (const auto& arg : method->GetArguments()) {
        signature << java::JavaSignatureOf(arg->GetType(), typenames) << ",";
      }
      signature << "|";
      signature << java::JavaSignatureOf(method->GetType(), typenames);
      mappings[signature.str()] = dump_location(*method);
    }
  }
  return mappings;
}

}  // namespace mappings
}  // namespace aidl
}  // namespace android
