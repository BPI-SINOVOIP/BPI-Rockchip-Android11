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

#include "utils/intents/jni.h"

#include <memory>

#include "utils/base/statusor.h"
#include "utils/java/jni-base.h"
#include "utils/java/jni-helper.h"

namespace libtextclassifier3 {

// The macros below are intended to reduce the boilerplate and avoid
// easily introduced copy/paste errors.
#define TC3_CHECK_JNI_PTR(PTR) TC3_CHECK((PTR) != nullptr)
#define TC3_GET_CLASS(FIELD, NAME)                                         \
  {                                                                        \
    StatusOr<ScopedLocalRef<jclass>> status_or_clazz =                     \
        JniHelper::FindClass(env, NAME);                                   \
    handler->FIELD = MakeGlobalRef(status_or_clazz.ValueOrDie().release(), \
                                   env, jni_cache->jvm);                   \
    TC3_CHECK_JNI_PTR(handler->FIELD) << "Error finding class: " << NAME;  \
  }
#define TC3_GET_METHOD(CLASS, FIELD, NAME, SIGNATURE)                       \
  handler->FIELD = env->GetMethodID(handler->CLASS.get(), NAME, SIGNATURE); \
  TC3_CHECK(handler->FIELD) << "Error finding method: " << NAME;

std::unique_ptr<RemoteActionTemplatesHandler>
RemoteActionTemplatesHandler::Create(
    const std::shared_ptr<JniCache>& jni_cache) {
  JNIEnv* env = jni_cache->GetEnv();
  if (env == nullptr) {
    return nullptr;
  }

  std::unique_ptr<RemoteActionTemplatesHandler> handler(
      new RemoteActionTemplatesHandler(jni_cache));

  TC3_GET_CLASS(integer_class_, "java/lang/Integer");
  TC3_GET_METHOD(integer_class_, integer_init_, "<init>", "(I)V");

  TC3_GET_CLASS(remote_action_template_class_,
                TC3_PACKAGE_PATH TC3_REMOTE_ACTION_TEMPLATE_CLASS_NAME_STR);
  TC3_GET_METHOD(
      remote_action_template_class_, remote_action_template_init_, "<init>",
      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/"
      "String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/"
      "Integer;[Ljava/lang/String;Ljava/lang/String;[L" TC3_PACKAGE_PATH
          TC3_NAMED_VARIANT_CLASS_NAME_STR ";Ljava/lang/Integer;)V");

  TC3_GET_CLASS(named_variant_class_,
                TC3_PACKAGE_PATH TC3_NAMED_VARIANT_CLASS_NAME_STR);

  TC3_GET_METHOD(named_variant_class_, named_variant_from_int_, "<init>",
                 "(Ljava/lang/String;I)V");
  TC3_GET_METHOD(named_variant_class_, named_variant_from_long_, "<init>",
                 "(Ljava/lang/String;J)V");
  TC3_GET_METHOD(named_variant_class_, named_variant_from_float_, "<init>",
                 "(Ljava/lang/String;F)V");
  TC3_GET_METHOD(named_variant_class_, named_variant_from_double_, "<init>",
                 "(Ljava/lang/String;D)V");
  TC3_GET_METHOD(named_variant_class_, named_variant_from_bool_, "<init>",
                 "(Ljava/lang/String;Z)V");
  TC3_GET_METHOD(named_variant_class_, named_variant_from_string_, "<init>",
                 "(Ljava/lang/String;Ljava/lang/String;)V");
  TC3_GET_METHOD(named_variant_class_, named_variant_from_string_array_,
                 "<init>", "(Ljava/lang/String;[Ljava/lang/String;)V");
  TC3_GET_METHOD(named_variant_class_, named_variant_from_float_array_,
                 "<init>", "(Ljava/lang/String;[F)V");
  TC3_GET_METHOD(named_variant_class_, named_variant_from_int_array_, "<init>",
                 "(Ljava/lang/String;[I)V");
  TC3_GET_METHOD(
      named_variant_class_, named_variant_from_named_variant_array_, "<init>",
      "(Ljava/lang/String;[L" TC3_PACKAGE_PATH TC3_NAMED_VARIANT_CLASS_NAME_STR
      ";)V");
  return handler;
}

StatusOr<ScopedLocalRef<jstring>> RemoteActionTemplatesHandler::AsUTF8String(
    const Optional<std::string>& optional) const {
  if (!optional.has_value()) {
    return {{nullptr, jni_cache_->GetEnv()}};
  }
  return jni_cache_->ConvertToJavaString(optional.value());
}

StatusOr<ScopedLocalRef<jobject>> RemoteActionTemplatesHandler::AsInteger(
    const Optional<int>& optional) const {
  if (!optional.has_value()) {
    return {{nullptr, jni_cache_->GetEnv()}};
  }

  TC3_ASSIGN_OR_RETURN(
      ScopedLocalRef<jobject> result,
      JniHelper::NewObject(jni_cache_->GetEnv(), integer_class_.get(),
                           integer_init_, optional.value()));

  return result;
}

StatusOr<ScopedLocalRef<jobjectArray>>
RemoteActionTemplatesHandler::AsStringArray(
    const std::vector<std::string>& values) const {
  if (values.empty()) {
    return {{nullptr, jni_cache_->GetEnv()}};
  }

  TC3_ASSIGN_OR_RETURN(
      ScopedLocalRef<jobjectArray> result,
      JniHelper::NewObjectArray(jni_cache_->GetEnv(), values.size(),
                                jni_cache_->string_class.get(), nullptr));

  for (int k = 0; k < values.size(); k++) {
    TC3_ASSIGN_OR_RETURN(ScopedLocalRef<jstring> value_str,
                         jni_cache_->ConvertToJavaString(values[k]));
    jni_cache_->GetEnv()->SetObjectArrayElement(result.get(), k,
                                                value_str.get());
  }
  return result;
}

StatusOr<ScopedLocalRef<jfloatArray>>
RemoteActionTemplatesHandler::AsFloatArray(
    const std::vector<float>& values) const {
  if (values.empty()) {
    return {{nullptr, jni_cache_->GetEnv()}};
  }

  TC3_ASSIGN_OR_RETURN(
      ScopedLocalRef<jfloatArray> result,
      JniHelper::NewFloatArray(jni_cache_->GetEnv(), values.size()));

  jni_cache_->GetEnv()->SetFloatArrayRegion(result.get(), /*start=*/0,
                                            /*len=*/values.size(),
                                            &(values[0]));
  return result;
}

StatusOr<ScopedLocalRef<jintArray>> RemoteActionTemplatesHandler::AsIntArray(
    const std::vector<int>& values) const {
  if (values.empty()) {
    return {{nullptr, jni_cache_->GetEnv()}};
  }

  TC3_ASSIGN_OR_RETURN(
      ScopedLocalRef<jintArray> result,
      JniHelper::NewIntArray(jni_cache_->GetEnv(), values.size()));

  jni_cache_->GetEnv()->SetIntArrayRegion(result.get(), /*start=*/0,
                                          /*len=*/values.size(), &(values[0]));
  return result;
}

StatusOr<ScopedLocalRef<jobject>> RemoteActionTemplatesHandler::AsNamedVariant(
    const std::string& name_str, const Variant& value) const {
  TC3_ASSIGN_OR_RETURN(ScopedLocalRef<jstring> name,
                       jni_cache_->ConvertToJavaString(name_str));

  JNIEnv* env = jni_cache_->GetEnv();
  switch (value.GetType()) {
    case Variant::TYPE_INT_VALUE:
      return JniHelper::NewObject(env, named_variant_class_.get(),
                                  named_variant_from_int_, name.get(),
                                  value.Value<int>());

    case Variant::TYPE_INT64_VALUE:
      return JniHelper::NewObject(env, named_variant_class_.get(),
                                  named_variant_from_long_, name.get(),
                                  value.Value<int64>());

    case Variant::TYPE_FLOAT_VALUE:
      return JniHelper::NewObject(env, named_variant_class_.get(),
                                  named_variant_from_float_, name.get(),
                                  value.Value<float>());

    case Variant::TYPE_DOUBLE_VALUE:
      return JniHelper::NewObject(env, named_variant_class_.get(),
                                  named_variant_from_double_, name.get(),
                                  value.Value<double>());

    case Variant::TYPE_BOOL_VALUE:
      return JniHelper::NewObject(env, named_variant_class_.get(),
                                  named_variant_from_bool_, name.get(),
                                  value.Value<bool>());

    case Variant::TYPE_STRING_VALUE: {
      TC3_ASSIGN_OR_RETURN(
          ScopedLocalRef<jstring> value_jstring,
          jni_cache_->ConvertToJavaString(value.ConstRefValue<std::string>()));
      return JniHelper::NewObject(env, named_variant_class_.get(),
                                  named_variant_from_string_, name.get(),
                                  value_jstring.get());
    }

    case Variant::TYPE_STRING_VECTOR_VALUE: {
      TC3_ASSIGN_OR_RETURN(
          ScopedLocalRef<jobjectArray> value_jstring_array,
          AsStringArray(value.ConstRefValue<std::vector<std::string>>()));

      return JniHelper::NewObject(env, named_variant_class_.get(),
                                  named_variant_from_string_array_, name.get(),
                                  value_jstring_array.get());
    }

    case Variant::TYPE_FLOAT_VECTOR_VALUE: {
      TC3_ASSIGN_OR_RETURN(
          ScopedLocalRef<jfloatArray> value_jfloat_array,
          AsFloatArray(value.ConstRefValue<std::vector<float>>()));

      return JniHelper::NewObject(env, named_variant_class_.get(),
                                  named_variant_from_float_array_, name.get(),
                                  value_jfloat_array.get());
    }

    case Variant::TYPE_INT_VECTOR_VALUE: {
      TC3_ASSIGN_OR_RETURN(ScopedLocalRef<jintArray> value_jint_array,
                           AsIntArray(value.ConstRefValue<std::vector<int>>()));

      return JniHelper::NewObject(env, named_variant_class_.get(),
                                  named_variant_from_int_array_, name.get(),
                                  value_jint_array.get());
    }

    case Variant::TYPE_STRING_VARIANT_MAP_VALUE: {
      TC3_ASSIGN_OR_RETURN(
          ScopedLocalRef<jobjectArray> value_jobect_array,
          AsNamedVariantArray(
              value.ConstRefValue<std::map<std::string, Variant>>()));
      return JniHelper::NewObject(env, named_variant_class_.get(),
                                  named_variant_from_named_variant_array_,
                                  name.get(), value_jobect_array.get());
    }

    case Variant::TYPE_EMPTY:
      return {Status::UNKNOWN};

    default:
      TC3_LOG(ERROR) << "Unsupported NamedVariant type: " << value.GetType();
      return {Status::UNKNOWN};
  }
}

StatusOr<ScopedLocalRef<jobjectArray>>
RemoteActionTemplatesHandler::AsNamedVariantArray(
    const std::map<std::string, Variant>& values) const {
  JNIEnv* env = jni_cache_->GetEnv();
  if (values.empty()) {
    return {{nullptr, env}};
  }

  TC3_ASSIGN_OR_RETURN(
      ScopedLocalRef<jobjectArray> result,
      JniHelper::NewObjectArray(jni_cache_->GetEnv(), values.size(),
                                named_variant_class_.get(), nullptr));
  int element_index = 0;
  for (const auto& key_value_pair : values) {
    if (!key_value_pair.second.HasValue()) {
      element_index++;
      continue;
    }
    TC3_ASSIGN_OR_RETURN(
        StatusOr<ScopedLocalRef<jobject>> named_extra,
        AsNamedVariant(key_value_pair.first, key_value_pair.second));
    env->SetObjectArrayElement(result.get(), element_index,
                               named_extra.ValueOrDie().get());
    element_index++;
  }
  return result;
}

StatusOr<ScopedLocalRef<jobjectArray>>
RemoteActionTemplatesHandler::RemoteActionTemplatesToJObjectArray(
    const std::vector<RemoteActionTemplate>& remote_actions) const {
  JNIEnv* env = jni_cache_->GetEnv();
  TC3_ASSIGN_OR_RETURN(
      ScopedLocalRef<jobjectArray> results,
      JniHelper::NewObjectArray(env, remote_actions.size(),
                                remote_action_template_class_.get(), nullptr));

  for (int i = 0; i < remote_actions.size(); i++) {
    const RemoteActionTemplate& remote_action = remote_actions[i];

    TC3_ASSIGN_OR_RETURN(
        const StatusOr<ScopedLocalRef<jstring>> title_without_entity,
        AsUTF8String(remote_action.title_without_entity));
    TC3_ASSIGN_OR_RETURN(
        const StatusOr<ScopedLocalRef<jstring>> title_with_entity,
        AsUTF8String(remote_action.title_with_entity));
    TC3_ASSIGN_OR_RETURN(const StatusOr<ScopedLocalRef<jstring>> description,
                         AsUTF8String(remote_action.description));
    TC3_ASSIGN_OR_RETURN(
        const StatusOr<ScopedLocalRef<jstring>> description_with_app_name,
        AsUTF8String(remote_action.description_with_app_name));
    TC3_ASSIGN_OR_RETURN(const StatusOr<ScopedLocalRef<jstring>> action,
                         AsUTF8String(remote_action.action));
    TC3_ASSIGN_OR_RETURN(const StatusOr<ScopedLocalRef<jstring>> data,
                         AsUTF8String(remote_action.data));
    TC3_ASSIGN_OR_RETURN(const StatusOr<ScopedLocalRef<jstring>> type,
                         AsUTF8String(remote_action.type));
    TC3_ASSIGN_OR_RETURN(const StatusOr<ScopedLocalRef<jobject>> flags,
                         AsInteger(remote_action.flags));
    TC3_ASSIGN_OR_RETURN(const StatusOr<ScopedLocalRef<jobjectArray>> category,
                         AsStringArray(remote_action.category));
    TC3_ASSIGN_OR_RETURN(const StatusOr<ScopedLocalRef<jstring>> package,
                         AsUTF8String(remote_action.package_name));
    TC3_ASSIGN_OR_RETURN(const StatusOr<ScopedLocalRef<jobjectArray>> extra,
                         AsNamedVariantArray(remote_action.extra));
    TC3_ASSIGN_OR_RETURN(const StatusOr<ScopedLocalRef<jobject>> request_code,
                         AsInteger(remote_action.request_code));

    TC3_ASSIGN_OR_RETURN(
        const ScopedLocalRef<jobject> result,
        JniHelper::NewObject(
            env, remote_action_template_class_.get(),
            remote_action_template_init_,
            title_without_entity.ValueOrDie().get(),
            title_with_entity.ValueOrDie().get(),
            description.ValueOrDie().get(),
            description_with_app_name.ValueOrDie().get(),
            action.ValueOrDie().get(), data.ValueOrDie().get(),
            type.ValueOrDie().get(), flags.ValueOrDie().get(),
            category.ValueOrDie().get(), package.ValueOrDie().get(),
            extra.ValueOrDie().get(), request_code.ValueOrDie().get()));
    env->SetObjectArrayElement(results.get(), i, result.get());
  }
  return results;
}

StatusOr<ScopedLocalRef<jobjectArray>>
RemoteActionTemplatesHandler::EntityDataAsNamedVariantArray(
    const reflection::Schema* entity_data_schema,
    const std::string& serialized_entity_data) const {
  ReflectiveFlatbufferBuilder entity_data_builder(entity_data_schema);
  std::unique_ptr<ReflectiveFlatbuffer> buffer = entity_data_builder.NewRoot();
  buffer->MergeFromSerializedFlatbuffer(serialized_entity_data);
  std::map<std::string, Variant> entity_data_map = buffer->AsFlatMap();
  return AsNamedVariantArray(entity_data_map);
}

}  // namespace libtextclassifier3
