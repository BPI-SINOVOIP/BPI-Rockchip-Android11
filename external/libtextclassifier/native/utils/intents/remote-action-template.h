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

#ifndef LIBTEXTCLASSIFIER_UTILS_INTENTS_REMOTE_ACTION_TEMPLATE_H_
#define LIBTEXTCLASSIFIER_UTILS_INTENTS_REMOTE_ACTION_TEMPLATE_H_

#include <map>
#include <string>
#include <vector>

#include "utils/optional.h"
#include "utils/variant.h"

namespace libtextclassifier3 {

// A template with parameters for an Android remote action.
struct RemoteActionTemplate {
  // Title shown for the action (see: RemoteAction.getTitle).
  Optional<std::string> title_without_entity;

  // Title with entity for the action. It is not guaranteed that the client
  // will use this, so title should be always given and general enough.
  Optional<std::string> title_with_entity;

  // Description shown for the action (see: RemoteAction.getContentDescription).
  Optional<std::string> description;

  // Description shown for the action (see: RemoteAction.getContentDescription)
  // when app name is available. Caller is expected to replace the placeholder
  // by the name of the app that is going to handle the action.
  Optional<std::string> description_with_app_name;

  // The action to set on the Intent (see: Intent.setAction).
  Optional<std::string> action;

  // The data to set on the Intent (see: Intent.setData).
  Optional<std::string> data;

  // The type to set on the Intent (see: Intent.setType).
  Optional<std::string> type;

  // Flags for launching the Intent (see: Intent.setFlags).
  Optional<int> flags;

  // Categories to set on the Intent (see: Intent.addCategory).
  std::vector<std::string> category;

  // Explicit application package to set on the Intent (see: Intent.setPackage).
  Optional<std::string> package_name;

  // The list of all the extras to add to the Intent.
  std::map<std::string, Variant> extra;

  // Private request code to use for the Intent.
  Optional<int> request_code;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_INTENTS_REMOTE_ACTION_TEMPLATE_H_
