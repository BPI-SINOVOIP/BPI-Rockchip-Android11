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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_ANNOTATOR_JNI_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_ANNOTATOR_JNI_H_

#include <jni.h>
#include <string>
#include "annotator/annotator_jni_common.h"
#include "annotator/types.h"
#include "utils/java/jni-base.h"

#ifdef __cplusplus
extern "C" {
#endif

// SmartSelection.
TC3_JNI_METHOD(jlong, TC3_ANNOTATOR_CLASS_NAME, nativeNewAnnotator)
(JNIEnv* env, jobject thiz, jint fd);

TC3_JNI_METHOD(jlong, TC3_ANNOTATOR_CLASS_NAME, nativeNewAnnotatorFromPath)
(JNIEnv* env, jobject thiz, jstring path);

TC3_JNI_METHOD(jlong, TC3_ANNOTATOR_CLASS_NAME, nativeNewAnnotatorWithOffset)
(JNIEnv* env, jobject thiz, jint fd, jlong offset, jlong size);

TC3_JNI_METHOD(jboolean, TC3_ANNOTATOR_CLASS_NAME,
               nativeInitializeKnowledgeEngine)
(JNIEnv* env, jobject thiz, jlong ptr, jbyteArray serialized_config);

TC3_JNI_METHOD(jboolean, TC3_ANNOTATOR_CLASS_NAME,
               nativeInitializeContactEngine)
(JNIEnv* env, jobject thiz, jlong ptr, jbyteArray serialized_config);

TC3_JNI_METHOD(jboolean, TC3_ANNOTATOR_CLASS_NAME,
               nativeInitializeInstalledAppEngine)
(JNIEnv* env, jobject thiz, jlong ptr, jbyteArray serialized_config);

TC3_JNI_METHOD(jboolean, TC3_ANNOTATOR_CLASS_NAME,
               nativeInitializePersonNameEngine)
(JNIEnv* env, jobject thiz, jlong ptr, jint fd, jlong offset, jlong size);

TC3_JNI_METHOD(void, TC3_ANNOTATOR_CLASS_NAME, nativeSetLangId)
(JNIEnv* env, jobject thiz, jlong annotator_ptr, jlong lang_id_ptr);

TC3_JNI_METHOD(jlong, TC3_ANNOTATOR_CLASS_NAME, nativeGetNativeModelPtr)
(JNIEnv* env, jobject thiz, jlong ptr);

TC3_JNI_METHOD(jintArray, TC3_ANNOTATOR_CLASS_NAME, nativeSuggestSelection)
(JNIEnv* env, jobject thiz, jlong ptr, jstring context, jint selection_begin,
 jint selection_end, jobject options);

TC3_JNI_METHOD(jobjectArray, TC3_ANNOTATOR_CLASS_NAME, nativeClassifyText)
(JNIEnv* env, jobject thiz, jlong ptr, jstring context, jint selection_begin,
 jint selection_end, jobject options, jobject app_context,
 jstring device_locales);

TC3_JNI_METHOD(jobjectArray, TC3_ANNOTATOR_CLASS_NAME,
               nativeAnnotateStructuredInput)
(JNIEnv* env, jobject thiz, jlong ptr, jobjectArray jinput_fragments,
 jobject options);

TC3_JNI_METHOD(jobjectArray, TC3_ANNOTATOR_CLASS_NAME, nativeAnnotate)
(JNIEnv* env, jobject thiz, jlong ptr, jstring context, jobject options);

TC3_JNI_METHOD(jbyteArray, TC3_ANNOTATOR_CLASS_NAME,
               nativeLookUpKnowledgeEntity)
(JNIEnv* env, jobject thiz, jlong ptr, jstring id);

TC3_JNI_METHOD(void, TC3_ANNOTATOR_CLASS_NAME, nativeCloseAnnotator)
(JNIEnv* env, jobject thiz, jlong ptr);

// DEPRECATED. Use nativeGetLocales instead.
TC3_JNI_METHOD(jstring, TC3_ANNOTATOR_CLASS_NAME, nativeGetLanguage)
(JNIEnv* env, jobject clazz, jint fd);

TC3_JNI_METHOD(jstring, TC3_ANNOTATOR_CLASS_NAME, nativeGetLocales)
(JNIEnv* env, jobject clazz, jint fd);

TC3_JNI_METHOD(jstring, TC3_ANNOTATOR_CLASS_NAME, nativeGetLocalesWithOffset)
(JNIEnv* env, jobject thiz, jint fd, jlong offset, jlong size);

TC3_JNI_METHOD(jint, TC3_ANNOTATOR_CLASS_NAME, nativeGetVersion)
(JNIEnv* env, jobject clazz, jint fd);

TC3_JNI_METHOD(jint, TC3_ANNOTATOR_CLASS_NAME, nativeGetVersionWithOffset)
(JNIEnv* env, jobject thiz, jint fd, jlong offset, jlong size);

TC3_JNI_METHOD(jstring, TC3_ANNOTATOR_CLASS_NAME, nativeGetName)
(JNIEnv* env, jobject clazz, jint fd);

TC3_JNI_METHOD(jstring, TC3_ANNOTATOR_CLASS_NAME, nativeGetNameWithOffset)
(JNIEnv* env, jobject thiz, jint fd, jlong offset, jlong size);

#ifdef __cplusplus
}
#endif

namespace libtextclassifier3 {

// Given a utf8 string and a span expressed in Java BMP (basic multilingual
// plane) codepoints, converts it to a span expressed in utf8 codepoints.
libtextclassifier3::CodepointSpan ConvertIndicesBMPToUTF8(
    const std::string& utf8_str, libtextclassifier3::CodepointSpan bmp_indices);

// Given a utf8 string and a span expressed in utf8 codepoints, converts it to a
// span expressed in Java BMP (basic multilingual plane) codepoints.
libtextclassifier3::CodepointSpan ConvertIndicesUTF8ToBMP(
    const std::string& utf8_str,
    libtextclassifier3::CodepointSpan utf8_indices);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_ANNOTATOR_JNI_H_
