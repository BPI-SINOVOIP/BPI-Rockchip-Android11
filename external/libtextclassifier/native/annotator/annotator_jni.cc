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

// JNI wrapper for the Annotator.

#include "annotator/annotator_jni.h"

#include <jni.h>

#include <type_traits>
#include <vector>

#include "annotator/annotator.h"
#include "annotator/annotator_jni_common.h"
#include "annotator/types.h"
#include "utils/base/integral_types.h"
#include "utils/base/status_macros.h"
#include "utils/base/statusor.h"
#include "utils/calendar/calendar.h"
#include "utils/intents/intent-generator.h"
#include "utils/intents/jni.h"
#include "utils/intents/remote-action-template.h"
#include "utils/java/jni-cache.h"
#include "utils/java/jni-helper.h"
#include "utils/java/string_utils.h"
#include "utils/memory/mmap.h"
#include "utils/strings/stringpiece.h"
#include "utils/utf8/unilib.h"

#ifdef TC3_UNILIB_JAVAICU
#ifndef TC3_CALENDAR_JAVAICU
#error Inconsistent usage of Java ICU components
#else
#define TC3_USE_JAVAICU
#endif
#endif

using libtextclassifier3::AnnotatedSpan;
using libtextclassifier3::Annotator;
using libtextclassifier3::ClassificationResult;
using libtextclassifier3::CodepointSpan;
using libtextclassifier3::JniHelper;
using libtextclassifier3::Model;
using libtextclassifier3::ScopedLocalRef;
using libtextclassifier3::StatusOr;
// When using the Java's ICU, CalendarLib and UniLib need to be instantiated
// with a JavaVM pointer from JNI. When using a standard ICU the pointer is
// not needed and the objects are instantiated implicitly.
#ifdef TC3_USE_JAVAICU
using libtextclassifier3::CalendarLib;
using libtextclassifier3::UniLib;
#endif

namespace libtextclassifier3 {

using libtextclassifier3::CodepointSpan;

namespace {
class AnnotatorJniContext {
 public:
  static AnnotatorJniContext* Create(
      const std::shared_ptr<libtextclassifier3::JniCache>& jni_cache,
      std::unique_ptr<Annotator> model) {
    if (jni_cache == nullptr || model == nullptr) {
      return nullptr;
    }
    // Intent generator will be null if the options are not specified.
    std::unique_ptr<IntentGenerator> intent_generator =
        IntentGenerator::Create(model->model()->intent_options(),
                                model->model()->resources(), jni_cache);
    std::unique_ptr<RemoteActionTemplatesHandler> template_handler =
        libtextclassifier3::RemoteActionTemplatesHandler::Create(jni_cache);
    if (template_handler == nullptr) {
      return nullptr;
    }

    return new AnnotatorJniContext(jni_cache, std::move(model),
                                   std::move(intent_generator),
                                   std::move(template_handler));
  }

  std::shared_ptr<libtextclassifier3::JniCache> jni_cache() const {
    return jni_cache_;
  }

  Annotator* model() const { return model_.get(); }

  // NOTE: Intent generator will be null if the options are not specified in
  // the model.
  IntentGenerator* intent_generator() const { return intent_generator_.get(); }

  RemoteActionTemplatesHandler* template_handler() const {
    return template_handler_.get();
  }

 private:
  AnnotatorJniContext(
      const std::shared_ptr<libtextclassifier3::JniCache>& jni_cache,
      std::unique_ptr<Annotator> model,
      std::unique_ptr<IntentGenerator> intent_generator,
      std::unique_ptr<RemoteActionTemplatesHandler> template_handler)
      : jni_cache_(jni_cache),
        model_(std::move(model)),
        intent_generator_(std::move(intent_generator)),
        template_handler_(std::move(template_handler)) {}

  std::shared_ptr<libtextclassifier3::JniCache> jni_cache_;
  std::unique_ptr<Annotator> model_;
  std::unique_ptr<IntentGenerator> intent_generator_;
  std::unique_ptr<RemoteActionTemplatesHandler> template_handler_;
};

StatusOr<ScopedLocalRef<jobject>> ClassificationResultWithIntentsToJObject(
    JNIEnv* env, const AnnotatorJniContext* model_context, jobject app_context,
    jclass result_class, jmethodID result_class_constructor,
    jclass datetime_parse_class, jmethodID datetime_parse_class_constructor,
    const jstring device_locales, const ClassificationOptions* options,
    const std::string& context, const CodepointSpan& selection_indices,
    const ClassificationResult& classification_result, bool generate_intents) {
  TC3_ASSIGN_OR_RETURN(
      ScopedLocalRef<jstring> row_string,
      JniHelper::NewStringUTF(env, classification_result.collection.c_str()));

  ScopedLocalRef<jobject> row_datetime_parse;
  if (classification_result.datetime_parse_result.IsSet()) {
    TC3_ASSIGN_OR_RETURN(
        row_datetime_parse,
        JniHelper::NewObject(
            env, datetime_parse_class, datetime_parse_class_constructor,
            classification_result.datetime_parse_result.time_ms_utc,
            classification_result.datetime_parse_result.granularity));
  }

  ScopedLocalRef<jbyteArray> serialized_knowledge_result;
  const std::string& serialized_knowledge_result_string =
      classification_result.serialized_knowledge_result;
  if (!serialized_knowledge_result_string.empty()) {
    TC3_ASSIGN_OR_RETURN(serialized_knowledge_result,
                         JniHelper::NewByteArray(
                             env, serialized_knowledge_result_string.size()));
    env->SetByteArrayRegion(serialized_knowledge_result.get(), 0,
                            serialized_knowledge_result_string.size(),
                            reinterpret_cast<const jbyte*>(
                                serialized_knowledge_result_string.data()));
  }

  ScopedLocalRef<jstring> contact_name;
  if (!classification_result.contact_name.empty()) {
    TC3_ASSIGN_OR_RETURN(contact_name,
                         JniHelper::NewStringUTF(
                             env, classification_result.contact_name.c_str()));
  }

  ScopedLocalRef<jstring> contact_given_name;
  if (!classification_result.contact_given_name.empty()) {
    TC3_ASSIGN_OR_RETURN(
        contact_given_name,
        JniHelper::NewStringUTF(
            env, classification_result.contact_given_name.c_str()));
  }

  ScopedLocalRef<jstring> contact_family_name;
  if (!classification_result.contact_family_name.empty()) {
    TC3_ASSIGN_OR_RETURN(
        contact_family_name,
        JniHelper::NewStringUTF(
            env, classification_result.contact_family_name.c_str()));
  }

  ScopedLocalRef<jstring> contact_nickname;
  if (!classification_result.contact_nickname.empty()) {
    TC3_ASSIGN_OR_RETURN(
        contact_nickname,
        JniHelper::NewStringUTF(
            env, classification_result.contact_nickname.c_str()));
  }

  ScopedLocalRef<jstring> contact_email_address;
  if (!classification_result.contact_email_address.empty()) {
    TC3_ASSIGN_OR_RETURN(
        contact_email_address,
        JniHelper::NewStringUTF(
            env, classification_result.contact_email_address.c_str()));
  }

  ScopedLocalRef<jstring> contact_phone_number;
  if (!classification_result.contact_phone_number.empty()) {
    TC3_ASSIGN_OR_RETURN(
        contact_phone_number,
        JniHelper::NewStringUTF(
            env, classification_result.contact_phone_number.c_str()));
  }

  ScopedLocalRef<jstring> contact_id;
  if (!classification_result.contact_id.empty()) {
    TC3_ASSIGN_OR_RETURN(
        contact_id,
        JniHelper::NewStringUTF(env, classification_result.contact_id.c_str()));
  }

  ScopedLocalRef<jstring> app_name;
  if (!classification_result.app_name.empty()) {
    TC3_ASSIGN_OR_RETURN(
        app_name,
        JniHelper::NewStringUTF(env, classification_result.app_name.c_str()));
  }

  ScopedLocalRef<jstring> app_package_name;
  if (!classification_result.app_package_name.empty()) {
    TC3_ASSIGN_OR_RETURN(
        app_package_name,
        JniHelper::NewStringUTF(
            env, classification_result.app_package_name.c_str()));
  }

  ScopedLocalRef<jobjectArray> extras;
  if (model_context->model()->entity_data_schema() != nullptr &&
      !classification_result.serialized_entity_data.empty()) {
    TC3_ASSIGN_OR_RETURN(
        extras,
        model_context->template_handler()->EntityDataAsNamedVariantArray(
            model_context->model()->entity_data_schema(),
            classification_result.serialized_entity_data));
  }

  ScopedLocalRef<jbyteArray> serialized_entity_data;
  if (!classification_result.serialized_entity_data.empty()) {
    TC3_ASSIGN_OR_RETURN(
        serialized_entity_data,
        JniHelper::NewByteArray(
            env, classification_result.serialized_entity_data.size()));
    env->SetByteArrayRegion(
        serialized_entity_data.get(), 0,
        classification_result.serialized_entity_data.size(),
        reinterpret_cast<const jbyte*>(
            classification_result.serialized_entity_data.data()));
  }

  ScopedLocalRef<jobjectArray> remote_action_templates_result;
  // Only generate RemoteActionTemplate for the top classification result
  // as classifyText does not need RemoteAction from other results anyway.
  if (generate_intents && model_context->intent_generator() != nullptr) {
    std::vector<RemoteActionTemplate> remote_action_templates;
    if (!model_context->intent_generator()->GenerateIntents(
            device_locales, classification_result,
            options->reference_time_ms_utc, context, selection_indices,
            app_context, model_context->model()->entity_data_schema(),
            &remote_action_templates)) {
      return {Status::UNKNOWN};
    }

    TC3_ASSIGN_OR_RETURN(
        remote_action_templates_result,
        model_context->template_handler()->RemoteActionTemplatesToJObjectArray(
            remote_action_templates));
  }

  return JniHelper::NewObject(
      env, result_class, result_class_constructor, row_string.get(),
      static_cast<jfloat>(classification_result.score),
      row_datetime_parse.get(), serialized_knowledge_result.get(),
      contact_name.get(), contact_given_name.get(), contact_family_name.get(),
      contact_nickname.get(), contact_email_address.get(),
      contact_phone_number.get(), contact_id.get(), app_name.get(),
      app_package_name.get(), extras.get(), serialized_entity_data.get(),
      remote_action_templates_result.get(), classification_result.duration_ms,
      classification_result.numeric_value,
      classification_result.numeric_double_value);
}

StatusOr<ScopedLocalRef<jobjectArray>>
ClassificationResultsWithIntentsToJObjectArray(
    JNIEnv* env, const AnnotatorJniContext* model_context, jobject app_context,
    const jstring device_locales, const ClassificationOptions* options,
    const std::string& context, const CodepointSpan& selection_indices,
    const std::vector<ClassificationResult>& classification_result,
    bool generate_intents) {
  TC3_ASSIGN_OR_RETURN(
      ScopedLocalRef<jclass> result_class,
      JniHelper::FindClass(env, TC3_PACKAGE_PATH TC3_ANNOTATOR_CLASS_NAME_STR
                           "$ClassificationResult"));

  TC3_ASSIGN_OR_RETURN(
      ScopedLocalRef<jclass> datetime_parse_class,
      JniHelper::FindClass(env, TC3_PACKAGE_PATH TC3_ANNOTATOR_CLASS_NAME_STR
                           "$DatetimeResult"));

  TC3_ASSIGN_OR_RETURN(
      const jmethodID result_class_constructor,
      JniHelper::GetMethodID(
          env, result_class.get(), "<init>",
          "(Ljava/lang/String;FL" TC3_PACKAGE_PATH TC3_ANNOTATOR_CLASS_NAME_STR
          "$DatetimeResult;[BLjava/lang/String;Ljava/lang/String;Ljava/lang/"
          "String;"
          "Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/"
          "String;"
          "Ljava/lang/String;Ljava/lang/String;[L" TC3_PACKAGE_PATH
          "" TC3_NAMED_VARIANT_CLASS_NAME_STR ";[B[L" TC3_PACKAGE_PATH
          "" TC3_REMOTE_ACTION_TEMPLATE_CLASS_NAME_STR ";JJD)V"));
  TC3_ASSIGN_OR_RETURN(const jmethodID datetime_parse_class_constructor,
                       JniHelper::GetMethodID(env, datetime_parse_class.get(),
                                              "<init>", "(JI)V"));

  TC3_ASSIGN_OR_RETURN(
      ScopedLocalRef<jobjectArray> results,
      JniHelper::NewObjectArray(env, classification_result.size(),
                                result_class.get()));

  for (int i = 0; i < classification_result.size(); i++) {
    TC3_ASSIGN_OR_RETURN(
        ScopedLocalRef<jobject> result,
        ClassificationResultWithIntentsToJObject(
            env, model_context, app_context, result_class.get(),
            result_class_constructor, datetime_parse_class.get(),
            datetime_parse_class_constructor, device_locales, options, context,
            selection_indices, classification_result[i],
            generate_intents && (i == 0)));
    TC3_RETURN_IF_ERROR(
        JniHelper::SetObjectArrayElement(env, results.get(), i, result.get()));
  }
  return results;
}

StatusOr<ScopedLocalRef<jobjectArray>> ClassificationResultsToJObjectArray(
    JNIEnv* env, const AnnotatorJniContext* model_context,
    const std::vector<ClassificationResult>& classification_result) {
  return ClassificationResultsWithIntentsToJObjectArray(
      env, model_context,
      /*(unused) app_context=*/nullptr,
      /*(unused) devide_locale=*/nullptr,
      /*(unusued) options=*/nullptr,
      /*(unused) selection_text=*/"",
      /*(unused) selection_indices=*/{kInvalidIndex, kInvalidIndex},
      classification_result,
      /*generate_intents=*/false);
}

CodepointSpan ConvertIndicesBMPUTF8(const std::string& utf8_str,
                                    CodepointSpan orig_indices,
                                    bool from_utf8) {
  const libtextclassifier3::UnicodeText unicode_str =
      libtextclassifier3::UTF8ToUnicodeText(utf8_str, /*do_copy=*/false);

  int unicode_index = 0;
  int bmp_index = 0;

  const int* source_index;
  const int* target_index;
  if (from_utf8) {
    source_index = &unicode_index;
    target_index = &bmp_index;
  } else {
    source_index = &bmp_index;
    target_index = &unicode_index;
  }

  CodepointSpan result{-1, -1};
  std::function<void()> assign_indices_fn = [&result, &orig_indices,
                                             &source_index, &target_index]() {
    if (orig_indices.first == *source_index) {
      result.first = *target_index;
    }

    if (orig_indices.second == *source_index) {
      result.second = *target_index;
    }
  };

  for (auto it = unicode_str.begin(); it != unicode_str.end();
       ++it, ++unicode_index, ++bmp_index) {
    assign_indices_fn();

    // There is 1 extra character in the input for each UTF8 character > 0xFFFF.
    if (*it > 0xFFFF) {
      ++bmp_index;
    }
  }
  assign_indices_fn();

  return result;
}

}  // namespace

CodepointSpan ConvertIndicesBMPToUTF8(const std::string& utf8_str,
                                      CodepointSpan bmp_indices) {
  return ConvertIndicesBMPUTF8(utf8_str, bmp_indices, /*from_utf8=*/false);
}

CodepointSpan ConvertIndicesUTF8ToBMP(const std::string& utf8_str,
                                      CodepointSpan utf8_indices) {
  return ConvertIndicesBMPUTF8(utf8_str, utf8_indices, /*from_utf8=*/true);
}

StatusOr<ScopedLocalRef<jstring>> GetLocalesFromMmap(
    JNIEnv* env, libtextclassifier3::ScopedMmap* mmap) {
  if (!mmap->handle().ok()) {
    return JniHelper::NewStringUTF(env, "");
  }
  const Model* model = libtextclassifier3::ViewModel(
      mmap->handle().start(), mmap->handle().num_bytes());
  if (!model || !model->locales()) {
    return JniHelper::NewStringUTF(env, "");
  }

  return JniHelper::NewStringUTF(env, model->locales()->c_str());
}

jint GetVersionFromMmap(JNIEnv* env, libtextclassifier3::ScopedMmap* mmap) {
  if (!mmap->handle().ok()) {
    return 0;
  }
  const Model* model = libtextclassifier3::ViewModel(
      mmap->handle().start(), mmap->handle().num_bytes());
  if (!model) {
    return 0;
  }
  return model->version();
}

StatusOr<ScopedLocalRef<jstring>> GetNameFromMmap(
    JNIEnv* env, libtextclassifier3::ScopedMmap* mmap) {
  if (!mmap->handle().ok()) {
    return JniHelper::NewStringUTF(env, "");
  }
  const Model* model = libtextclassifier3::ViewModel(
      mmap->handle().start(), mmap->handle().num_bytes());
  if (!model || !model->name()) {
    return JniHelper::NewStringUTF(env, "");
  }
  return JniHelper::NewStringUTF(env, model->name()->c_str());
}

}  // namespace libtextclassifier3

using libtextclassifier3::AnnotatorJniContext;
using libtextclassifier3::ClassificationResultsToJObjectArray;
using libtextclassifier3::ClassificationResultsWithIntentsToJObjectArray;
using libtextclassifier3::ConvertIndicesBMPToUTF8;
using libtextclassifier3::ConvertIndicesUTF8ToBMP;
using libtextclassifier3::FromJavaAnnotationOptions;
using libtextclassifier3::FromJavaClassificationOptions;
using libtextclassifier3::FromJavaInputFragment;
using libtextclassifier3::FromJavaSelectionOptions;
using libtextclassifier3::InputFragment;
using libtextclassifier3::ToStlString;

TC3_JNI_METHOD(jlong, TC3_ANNOTATOR_CLASS_NAME, nativeNewAnnotator)
(JNIEnv* env, jobject thiz, jint fd) {
  std::shared_ptr<libtextclassifier3::JniCache> jni_cache(
      libtextclassifier3::JniCache::Create(env));
#ifdef TC3_USE_JAVAICU
  return reinterpret_cast<jlong>(AnnotatorJniContext::Create(
      jni_cache,
      Annotator::FromFileDescriptor(
          fd, std::unique_ptr<UniLib>(new UniLib(jni_cache)),
          std::unique_ptr<CalendarLib>(new CalendarLib(jni_cache)))));
#else
  return reinterpret_cast<jlong>(AnnotatorJniContext::Create(
      jni_cache, Annotator::FromFileDescriptor(fd)));
#endif
}

TC3_JNI_METHOD(jlong, TC3_ANNOTATOR_CLASS_NAME, nativeNewAnnotatorFromPath)
(JNIEnv* env, jobject thiz, jstring path) {
  TC3_ASSIGN_OR_RETURN_0(const std::string path_str, ToStlString(env, path));
  std::shared_ptr<libtextclassifier3::JniCache> jni_cache(
      libtextclassifier3::JniCache::Create(env));
#ifdef TC3_USE_JAVAICU
  return reinterpret_cast<jlong>(AnnotatorJniContext::Create(
      jni_cache,
      Annotator::FromPath(
          path_str, std::unique_ptr<UniLib>(new UniLib(jni_cache)),
          std::unique_ptr<CalendarLib>(new CalendarLib(jni_cache)))));
#else
  return reinterpret_cast<jlong>(
      AnnotatorJniContext::Create(jni_cache, Annotator::FromPath(path_str)));
#endif
}

TC3_JNI_METHOD(jlong, TC3_ANNOTATOR_CLASS_NAME, nativeNewAnnotatorWithOffset)
(JNIEnv* env, jobject thiz, jint fd, jlong offset, jlong size) {
  std::shared_ptr<libtextclassifier3::JniCache> jni_cache(
      libtextclassifier3::JniCache::Create(env));
#ifdef TC3_USE_JAVAICU
  return reinterpret_cast<jlong>(AnnotatorJniContext::Create(
      jni_cache,
      Annotator::FromFileDescriptor(
          fd, offset, size, std::unique_ptr<UniLib>(new UniLib(jni_cache)),
          std::unique_ptr<CalendarLib>(new CalendarLib(jni_cache)))));
#else
  return reinterpret_cast<jlong>(AnnotatorJniContext::Create(
      jni_cache, Annotator::FromFileDescriptor(fd, offset, size)));
#endif
}

TC3_JNI_METHOD(jboolean, TC3_ANNOTATOR_CLASS_NAME,
               nativeInitializeKnowledgeEngine)
(JNIEnv* env, jobject thiz, jlong ptr, jbyteArray serialized_config) {
  if (!ptr) {
    return false;
  }

  Annotator* model = reinterpret_cast<AnnotatorJniContext*>(ptr)->model();

  std::string serialized_config_string;
  TC3_ASSIGN_OR_RETURN_FALSE(jsize length,
                             JniHelper::GetArrayLength(env, serialized_config));
  serialized_config_string.resize(length);
  env->GetByteArrayRegion(serialized_config, 0, length,
                          reinterpret_cast<jbyte*>(const_cast<char*>(
                              serialized_config_string.data())));

  return model->InitializeKnowledgeEngine(serialized_config_string);
}

TC3_JNI_METHOD(jboolean, TC3_ANNOTATOR_CLASS_NAME,
               nativeInitializeContactEngine)
(JNIEnv* env, jobject thiz, jlong ptr, jbyteArray serialized_config) {
  if (!ptr) {
    return false;
  }

  Annotator* model = reinterpret_cast<AnnotatorJniContext*>(ptr)->model();

  std::string serialized_config_string;
  TC3_ASSIGN_OR_RETURN_FALSE(jsize length,
                             JniHelper::GetArrayLength(env, serialized_config));
  serialized_config_string.resize(length);
  env->GetByteArrayRegion(serialized_config, 0, length,
                          reinterpret_cast<jbyte*>(const_cast<char*>(
                              serialized_config_string.data())));

  return model->InitializeContactEngine(serialized_config_string);
}

TC3_JNI_METHOD(jboolean, TC3_ANNOTATOR_CLASS_NAME,
               nativeInitializeInstalledAppEngine)
(JNIEnv* env, jobject thiz, jlong ptr, jbyteArray serialized_config) {
  if (!ptr) {
    return false;
  }

  Annotator* model = reinterpret_cast<AnnotatorJniContext*>(ptr)->model();

  std::string serialized_config_string;
  TC3_ASSIGN_OR_RETURN_FALSE(jsize length,
                             JniHelper::GetArrayLength(env, serialized_config));
  serialized_config_string.resize(length);
  env->GetByteArrayRegion(serialized_config, 0, length,
                          reinterpret_cast<jbyte*>(const_cast<char*>(
                              serialized_config_string.data())));

  return model->InitializeInstalledAppEngine(serialized_config_string);
}

TC3_JNI_METHOD(jboolean, TC3_ANNOTATOR_CLASS_NAME,
               nativeInitializePersonNameEngine)
(JNIEnv* env, jobject thiz, jlong ptr, jint fd, jlong offset, jlong size) {
  if (!ptr) {
    return false;
  }

  Annotator* model = reinterpret_cast<AnnotatorJniContext*>(ptr)->model();

  return model->InitializePersonNameEngineFromFileDescriptor(fd, offset, size);
}

TC3_JNI_METHOD(void, TC3_ANNOTATOR_CLASS_NAME, nativeSetLangId)
(JNIEnv* env, jobject thiz, jlong annotator_ptr, jlong lang_id_ptr) {
  if (!annotator_ptr) {
    return;
  }
  Annotator* model =
      reinterpret_cast<AnnotatorJniContext*>(annotator_ptr)->model();
  libtextclassifier3::mobile::lang_id::LangId* lang_id_model =
      reinterpret_cast<libtextclassifier3::mobile::lang_id::LangId*>(lang_id_ptr);
  model->SetLangId(lang_id_model);
}

TC3_JNI_METHOD(jlong, TC3_ANNOTATOR_CLASS_NAME, nativeGetNativeModelPtr)
(JNIEnv* env, jobject thiz, jlong ptr) {
  if (!ptr) {
    return 0L;
  }
  return reinterpret_cast<jlong>(
      reinterpret_cast<AnnotatorJniContext*>(ptr)->model());
}

TC3_JNI_METHOD(jintArray, TC3_ANNOTATOR_CLASS_NAME, nativeSuggestSelection)
(JNIEnv* env, jobject thiz, jlong ptr, jstring context, jint selection_begin,
 jint selection_end, jobject options) {
  if (!ptr) {
    return nullptr;
  }
  const Annotator* model = reinterpret_cast<AnnotatorJniContext*>(ptr)->model();
  TC3_ASSIGN_OR_RETURN_NULL(const std::string context_utf8,
                            ToStlString(env, context));
  CodepointSpan input_indices =
      ConvertIndicesBMPToUTF8(context_utf8, {selection_begin, selection_end});
  TC3_ASSIGN_OR_RETURN_NULL(
      libtextclassifier3::SelectionOptions selection_options,
      FromJavaSelectionOptions(env, options));
  CodepointSpan selection =
      model->SuggestSelection(context_utf8, input_indices, selection_options);
  selection = ConvertIndicesUTF8ToBMP(context_utf8, selection);

  TC3_ASSIGN_OR_RETURN_NULL(ScopedLocalRef<jintArray> result,
                            JniHelper::NewIntArray(env, 2));
  env->SetIntArrayRegion(result.get(), 0, 1, &(std::get<0>(selection)));
  env->SetIntArrayRegion(result.get(), 1, 1, &(std::get<1>(selection)));
  return result.release();
}

TC3_JNI_METHOD(jobjectArray, TC3_ANNOTATOR_CLASS_NAME, nativeClassifyText)
(JNIEnv* env, jobject thiz, jlong ptr, jstring context, jint selection_begin,
 jint selection_end, jobject options, jobject app_context,
 jstring device_locales) {
  if (!ptr) {
    return nullptr;
  }
  const AnnotatorJniContext* model_context =
      reinterpret_cast<AnnotatorJniContext*>(ptr);

  TC3_ASSIGN_OR_RETURN_NULL(const std::string context_utf8,
                            ToStlString(env, context));
  const CodepointSpan input_indices =
      ConvertIndicesBMPToUTF8(context_utf8, {selection_begin, selection_end});
  TC3_ASSIGN_OR_RETURN_NULL(
      const libtextclassifier3::ClassificationOptions classification_options,
      FromJavaClassificationOptions(env, options));
  const std::vector<ClassificationResult> classification_result =
      model_context->model()->ClassifyText(context_utf8, input_indices,
                                           classification_options);

  ScopedLocalRef<jobjectArray> result;
  if (app_context != nullptr) {
    TC3_ASSIGN_OR_RETURN_NULL(
        result, ClassificationResultsWithIntentsToJObjectArray(
                    env, model_context, app_context, device_locales,
                    &classification_options, context_utf8, input_indices,
                    classification_result,
                    /*generate_intents=*/true));

  } else {
    TC3_ASSIGN_OR_RETURN_NULL(
        result, ClassificationResultsToJObjectArray(env, model_context,
                                                    classification_result));
  }

  return result.release();
}

TC3_JNI_METHOD(jobjectArray, TC3_ANNOTATOR_CLASS_NAME, nativeAnnotate)
(JNIEnv* env, jobject thiz, jlong ptr, jstring context, jobject options) {
  if (!ptr) {
    return nullptr;
  }
  const AnnotatorJniContext* model_context =
      reinterpret_cast<AnnotatorJniContext*>(ptr);
  TC3_ASSIGN_OR_RETURN_NULL(const std::string context_utf8,
                            ToStlString(env, context));
  TC3_ASSIGN_OR_RETURN_NULL(
      libtextclassifier3::AnnotationOptions annotation_options,
      FromJavaAnnotationOptions(env, options));
  const std::vector<AnnotatedSpan> annotations =
      model_context->model()->Annotate(context_utf8, annotation_options);

  TC3_ASSIGN_OR_RETURN_NULL(
      ScopedLocalRef<jclass> result_class,
      JniHelper::FindClass(
          env, TC3_PACKAGE_PATH TC3_ANNOTATOR_CLASS_NAME_STR "$AnnotatedSpan"));

  TC3_ASSIGN_OR_RETURN_NULL(
      jmethodID result_class_constructor,
      JniHelper::GetMethodID(
          env, result_class.get(), "<init>",
          "(II[L" TC3_PACKAGE_PATH TC3_ANNOTATOR_CLASS_NAME_STR
          "$ClassificationResult;)V"));

  TC3_ASSIGN_OR_RETURN_NULL(
      ScopedLocalRef<jobjectArray> results,
      JniHelper::NewObjectArray(env, annotations.size(), result_class.get()));

  for (int i = 0; i < annotations.size(); ++i) {
    CodepointSpan span_bmp =
        ConvertIndicesUTF8ToBMP(context_utf8, annotations[i].span);

    TC3_ASSIGN_OR_RETURN_NULL(
        ScopedLocalRef<jobjectArray> classification_results,
        ClassificationResultsToJObjectArray(env, model_context,
                                            annotations[i].classification));

    TC3_ASSIGN_OR_RETURN_NULL(
        ScopedLocalRef<jobject> result,
        JniHelper::NewObject(env, result_class.get(), result_class_constructor,
                             static_cast<jint>(span_bmp.first),
                             static_cast<jint>(span_bmp.second),
                             classification_results.get()));
    if (!JniHelper::SetObjectArrayElement(env, results.get(), i, result.get())
             .ok()) {
      return nullptr;
    }
  }
  return results.release();
}

TC3_JNI_METHOD(jobjectArray, TC3_ANNOTATOR_CLASS_NAME,
               nativeAnnotateStructuredInput)
(JNIEnv* env, jobject thiz, jlong ptr, jobjectArray jinput_fragments,
 jobject options) {
  if (!ptr) {
    return nullptr;
  }
  const AnnotatorJniContext* model_context =
      reinterpret_cast<AnnotatorJniContext*>(ptr);

  std::vector<InputFragment> string_fragments;
  TC3_ASSIGN_OR_RETURN_NULL(jsize input_size,
                            JniHelper::GetArrayLength(env, jinput_fragments));
  for (int i = 0; i < input_size; ++i) {
    TC3_ASSIGN_OR_RETURN_NULL(
        ScopedLocalRef<jobject> jfragment,
        JniHelper::GetObjectArrayElement<jobject>(env, jinput_fragments, i));
    TC3_ASSIGN_OR_RETURN_NULL(InputFragment fragment,
                              FromJavaInputFragment(env, jfragment.get()));
    string_fragments.push_back(std::move(fragment));
  }

  TC3_ASSIGN_OR_RETURN_NULL(
      libtextclassifier3::AnnotationOptions annotation_options,
      FromJavaAnnotationOptions(env, options));
  const StatusOr<std::vector<std::vector<AnnotatedSpan>>> annotations_or =
      model_context->model()->AnnotateStructuredInput(string_fragments,
                                                      annotation_options);
  if (!annotations_or.ok()) {
    TC3_LOG(ERROR) << "Annotation of structured input failed with error: "
                   << annotations_or.status().error_message();
    return nullptr;
  }

  std::vector<std::vector<AnnotatedSpan>> annotations =
      std::move(annotations_or.ValueOrDie());
  TC3_ASSIGN_OR_RETURN_NULL(
      ScopedLocalRef<jclass> span_class,
      JniHelper::FindClass(
          env, TC3_PACKAGE_PATH TC3_ANNOTATOR_CLASS_NAME_STR "$AnnotatedSpan"));

  TC3_ASSIGN_OR_RETURN_NULL(
      jmethodID span_class_constructor,
      JniHelper::GetMethodID(
          env, span_class.get(), "<init>",
          "(II[L" TC3_PACKAGE_PATH TC3_ANNOTATOR_CLASS_NAME_STR
          "$ClassificationResult;)V"));

  TC3_ASSIGN_OR_RETURN_NULL(
      ScopedLocalRef<jclass> span_class_array,
      JniHelper::FindClass(env,
                           "[L" TC3_PACKAGE_PATH TC3_ANNOTATOR_CLASS_NAME_STR
                           "$AnnotatedSpan;"));

  TC3_ASSIGN_OR_RETURN_NULL(
      ScopedLocalRef<jobjectArray> results,
      JniHelper::NewObjectArray(env, input_size, span_class_array.get()));

  for (int fragment_index = 0; fragment_index < annotations.size();
       ++fragment_index) {
    TC3_ASSIGN_OR_RETURN_NULL(
        ScopedLocalRef<jobjectArray> jfragmentAnnotations,
        JniHelper::NewObjectArray(env, annotations[fragment_index].size(),
                                  span_class.get()));
    for (int annotation_index = 0;
         annotation_index < annotations[fragment_index].size();
         ++annotation_index) {
      CodepointSpan span_bmp = ConvertIndicesUTF8ToBMP(
          string_fragments[fragment_index].text,
          annotations[fragment_index][annotation_index].span);
      TC3_ASSIGN_OR_RETURN_NULL(
          ScopedLocalRef<jobjectArray> classification_results,
          ClassificationResultsToJObjectArray(
              env, model_context,
              annotations[fragment_index][annotation_index].classification));
      TC3_ASSIGN_OR_RETURN_NULL(
          ScopedLocalRef<jobject> single_annotation,
          JniHelper::NewObject(env, span_class.get(), span_class_constructor,
                               static_cast<jint>(span_bmp.first),
                               static_cast<jint>(span_bmp.second),
                               classification_results.get()));

      if (!JniHelper::SetObjectArrayElement(env, jfragmentAnnotations.get(),
                                            annotation_index,
                                            single_annotation.get())
               .ok()) {
        return nullptr;
      }
    }

    if (!JniHelper::SetObjectArrayElement(env, results.get(), fragment_index,
                                          jfragmentAnnotations.get())
             .ok()) {
      return nullptr;
    }
  }

  return results.release();
}

TC3_JNI_METHOD(jbyteArray, TC3_ANNOTATOR_CLASS_NAME,
               nativeLookUpKnowledgeEntity)
(JNIEnv* env, jobject thiz, jlong ptr, jstring id) {
  if (!ptr) {
    return nullptr;
  }
  const Annotator* model = reinterpret_cast<AnnotatorJniContext*>(ptr)->model();
  TC3_ASSIGN_OR_RETURN_NULL(const std::string id_utf8, ToStlString(env, id));
  std::string serialized_knowledge_result;
  if (!model->LookUpKnowledgeEntity(id_utf8, &serialized_knowledge_result)) {
    return nullptr;
  }

  TC3_ASSIGN_OR_RETURN_NULL(
      ScopedLocalRef<jbyteArray> result,
      JniHelper::NewByteArray(env, serialized_knowledge_result.size()));
  env->SetByteArrayRegion(
      result.get(), 0, serialized_knowledge_result.size(),
      reinterpret_cast<const jbyte*>(serialized_knowledge_result.data()));
  return result.release();
}

TC3_JNI_METHOD(void, TC3_ANNOTATOR_CLASS_NAME, nativeCloseAnnotator)
(JNIEnv* env, jobject thiz, jlong ptr) {
  const AnnotatorJniContext* context =
      reinterpret_cast<AnnotatorJniContext*>(ptr);
  delete context;
}

TC3_JNI_METHOD(jstring, TC3_ANNOTATOR_CLASS_NAME, nativeGetLanguage)
(JNIEnv* env, jobject clazz, jint fd) {
  TC3_LOG(WARNING) << "Using deprecated getLanguage().";
  return TC3_JNI_METHOD_NAME(TC3_ANNOTATOR_CLASS_NAME, nativeGetLocales)(
      env, clazz, fd);
}

TC3_JNI_METHOD(jstring, TC3_ANNOTATOR_CLASS_NAME, nativeGetLocales)
(JNIEnv* env, jobject clazz, jint fd) {
  const std::unique_ptr<libtextclassifier3::ScopedMmap> mmap(
      new libtextclassifier3::ScopedMmap(fd));
  TC3_ASSIGN_OR_RETURN_NULL(ScopedLocalRef<jstring> value,
                            GetLocalesFromMmap(env, mmap.get()));
  return value.release();
}

TC3_JNI_METHOD(jstring, TC3_ANNOTATOR_CLASS_NAME, nativeGetLocalesWithOffset)
(JNIEnv* env, jobject thiz, jint fd, jlong offset, jlong size) {
  const std::unique_ptr<libtextclassifier3::ScopedMmap> mmap(
      new libtextclassifier3::ScopedMmap(fd, offset, size));
  TC3_ASSIGN_OR_RETURN_NULL(ScopedLocalRef<jstring> value,
                            GetLocalesFromMmap(env, mmap.get()));
  return value.release();
}

TC3_JNI_METHOD(jint, TC3_ANNOTATOR_CLASS_NAME, nativeGetVersion)
(JNIEnv* env, jobject clazz, jint fd) {
  const std::unique_ptr<libtextclassifier3::ScopedMmap> mmap(
      new libtextclassifier3::ScopedMmap(fd));
  return GetVersionFromMmap(env, mmap.get());
}

TC3_JNI_METHOD(jint, TC3_ANNOTATOR_CLASS_NAME, nativeGetVersionWithOffset)
(JNIEnv* env, jobject thiz, jint fd, jlong offset, jlong size) {
  const std::unique_ptr<libtextclassifier3::ScopedMmap> mmap(
      new libtextclassifier3::ScopedMmap(fd, offset, size));
  return GetVersionFromMmap(env, mmap.get());
}

TC3_JNI_METHOD(jstring, TC3_ANNOTATOR_CLASS_NAME, nativeGetName)
(JNIEnv* env, jobject clazz, jint fd) {
  const std::unique_ptr<libtextclassifier3::ScopedMmap> mmap(
      new libtextclassifier3::ScopedMmap(fd));
  TC3_ASSIGN_OR_RETURN_NULL(ScopedLocalRef<jstring> value,
                            GetNameFromMmap(env, mmap.get()));
  return value.release();
}

TC3_JNI_METHOD(jstring, TC3_ANNOTATOR_CLASS_NAME, nativeGetNameWithOffset)
(JNIEnv* env, jobject thiz, jint fd, jlong offset, jlong size) {
  const std::unique_ptr<libtextclassifier3::ScopedMmap> mmap(
      new libtextclassifier3::ScopedMmap(fd, offset, size));
  TC3_ASSIGN_OR_RETURN_NULL(ScopedLocalRef<jstring> value,
                            GetNameFromMmap(env, mmap.get()));
  return value.release();
}
