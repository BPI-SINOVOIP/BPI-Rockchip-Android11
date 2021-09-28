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

#ifndef LIBTEXTCLASSIFIER_UTILS_INTENTS_JNI_H_
#define LIBTEXTCLASSIFIER_UTILS_INTENTS_JNI_H_

#include <jni.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "utils/base/statusor.h"
#include "utils/flatbuffers.h"
#include "utils/intents/remote-action-template.h"
#include "utils/java/jni-base.h"
#include "utils/java/jni-cache.h"
#include "utils/optional.h"
#include "utils/variant.h"

#ifndef TC3_REMOTE_ACTION_TEMPLATE_CLASS_NAME
#define TC3_REMOTE_ACTION_TEMPLATE_CLASS_NAME RemoteActionTemplate
#endif

#define TC3_REMOTE_ACTION_TEMPLATE_CLASS_NAME_STR \
  TC3_ADD_QUOTES(TC3_REMOTE_ACTION_TEMPLATE_CLASS_NAME)

#ifndef TC3_NAMED_VARIANT_CLASS_NAME
#define TC3_NAMED_VARIANT_CLASS_NAME NamedVariant
#endif

#define TC3_NAMED_VARIANT_CLASS_NAME_STR \
  TC3_ADD_QUOTES(TC3_NAMED_VARIANT_CLASS_NAME)

namespace libtextclassifier3 {

// A helper class to create RemoteActionTemplate object from model results.
class RemoteActionTemplatesHandler {
 public:
  static std::unique_ptr<RemoteActionTemplatesHandler> Create(
      const std::shared_ptr<JniCache>& jni_cache);

  StatusOr<ScopedLocalRef<jstring>> AsUTF8String(
      const Optional<std::string>& optional) const;
  StatusOr<ScopedLocalRef<jobject>> AsInteger(
      const Optional<int>& optional) const;
  StatusOr<ScopedLocalRef<jobjectArray>> AsStringArray(
      const std::vector<std::string>& values) const;
  StatusOr<ScopedLocalRef<jfloatArray>> AsFloatArray(
      const std::vector<float>& values) const;
  StatusOr<ScopedLocalRef<jintArray>> AsIntArray(
      const std::vector<int>& values) const;
  StatusOr<ScopedLocalRef<jobject>> AsNamedVariant(const std::string& name,
                                                   const Variant& value) const;
  StatusOr<ScopedLocalRef<jobjectArray>> AsNamedVariantArray(
      const std::map<std::string, Variant>& values) const;

  StatusOr<ScopedLocalRef<jobjectArray>> RemoteActionTemplatesToJObjectArray(
      const std::vector<RemoteActionTemplate>& remote_actions) const;

  StatusOr<ScopedLocalRef<jobjectArray>> EntityDataAsNamedVariantArray(
      const reflection::Schema* entity_data_schema,
      const std::string& serialized_entity_data) const;

 private:
  explicit RemoteActionTemplatesHandler(
      const std::shared_ptr<JniCache>& jni_cache)
      : jni_cache_(jni_cache),
        integer_class_(nullptr, jni_cache->jvm),
        remote_action_template_class_(nullptr, jni_cache->jvm),
        named_variant_class_(nullptr, jni_cache->jvm) {}

  std::shared_ptr<JniCache> jni_cache_;

  // java.lang.Integer
  ScopedGlobalRef<jclass> integer_class_;
  jmethodID integer_init_ = nullptr;

  // RemoteActionTemplate
  ScopedGlobalRef<jclass> remote_action_template_class_;
  jmethodID remote_action_template_init_ = nullptr;

  // NamedVariant
  ScopedGlobalRef<jclass> named_variant_class_;
  jmethodID named_variant_from_int_ = nullptr;
  jmethodID named_variant_from_long_ = nullptr;
  jmethodID named_variant_from_float_ = nullptr;
  jmethodID named_variant_from_double_ = nullptr;
  jmethodID named_variant_from_bool_ = nullptr;
  jmethodID named_variant_from_string_ = nullptr;
  jmethodID named_variant_from_string_array_ = nullptr;
  jmethodID named_variant_from_float_array_ = nullptr;
  jmethodID named_variant_from_int_array_ = nullptr;
  jmethodID named_variant_from_named_variant_array_ = nullptr;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_INTENTS_JNI_H_
