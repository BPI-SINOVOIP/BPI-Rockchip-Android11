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

#include "utils/java/jni-helper.h"

namespace libtextclassifier3 {

StatusOr<ScopedLocalRef<jclass>> JniHelper::FindClass(JNIEnv* env,
                                                      const char* class_name) {
  TC3_ENSURE_LOCAL_CAPACITY_OR_RETURN;
  ScopedLocalRef<jclass> result(env->FindClass(class_name), env);
  TC3_NO_EXCEPTION_OR_RETURN;
  TC3_NOT_NULL_OR_RETURN;
  return result;
}

StatusOr<jmethodID> JniHelper::GetMethodID(JNIEnv* env, jclass clazz,
                                           const char* method_name,
                                           const char* return_type) {
  jmethodID result = env->GetMethodID(clazz, method_name, return_type);
  TC3_NO_EXCEPTION_OR_RETURN;
  TC3_NOT_NULL_OR_RETURN;
  return result;
}

StatusOr<ScopedLocalRef<jobject>> JniHelper::GetStaticObjectField(
    JNIEnv* env, jclass class_name, jfieldID field_id) {
  TC3_ENSURE_LOCAL_CAPACITY_OR_RETURN;
  ScopedLocalRef<jobject> result(
      env->GetStaticObjectField(class_name, field_id), env);
  TC3_NO_EXCEPTION_OR_RETURN;
  TC3_NOT_NULL_OR_RETURN;
  return result;
}

StatusOr<ScopedLocalRef<jbyteArray>> JniHelper::NewByteArray(JNIEnv* env,
                                                             jsize length) {
  TC3_ENSURE_LOCAL_CAPACITY_OR_RETURN;
  ScopedLocalRef<jbyteArray> result(env->NewByteArray(length), env);
  TC3_NO_EXCEPTION_OR_RETURN;
  TC3_NOT_NULL_OR_RETURN;
  return result;
}

Status JniHelper::CallVoidMethod(JNIEnv* env, jobject object,
                                 jmethodID method_id, ...) {
  va_list args;
  va_start(args, method_id);
  env->CallVoidMethodV(object, method_id, args);
  va_end(args);

  TC3_NO_EXCEPTION_OR_RETURN;
  return Status::OK;
}

StatusOr<bool> JniHelper::CallBooleanMethod(JNIEnv* env, jobject object,
                                            jmethodID method_id, ...) {
  va_list args;
  va_start(args, method_id);
  bool result = env->CallBooleanMethodV(object, method_id, args);
  va_end(args);

  TC3_NO_EXCEPTION_OR_RETURN;
  return result;
}

StatusOr<int32> JniHelper::CallIntMethod(JNIEnv* env, jobject object,
                                         jmethodID method_id, ...) {
  va_list args;
  va_start(args, method_id);
  jint result = env->CallIntMethodV(object, method_id, args);
  va_end(args);

  TC3_NO_EXCEPTION_OR_RETURN;
  return result;
}

StatusOr<int64> JniHelper::CallLongMethod(JNIEnv* env, jobject object,
                                          jmethodID method_id, ...) {
  va_list args;
  va_start(args, method_id);
  jlong result = env->CallLongMethodV(object, method_id, args);
  va_end(args);

  TC3_NO_EXCEPTION_OR_RETURN;
  return result;
}

StatusOr<float> JniHelper::CallFloatMethod(JNIEnv* env, jobject object,
                                           jmethodID method_id, ...) {
  va_list args;
  va_start(args, method_id);
  jfloat result = env->CallFloatMethodV(object, method_id, args);
  va_end(args);

  TC3_NO_EXCEPTION_OR_RETURN;
  return result;
}

StatusOr<double> JniHelper::CallDoubleMethod(JNIEnv* env, jobject object,
                                             jmethodID method_id, ...) {
  va_list args;
  va_start(args, method_id);
  jdouble result = env->CallDoubleMethodV(object, method_id, args);
  va_end(args);

  TC3_NO_EXCEPTION_OR_RETURN;
  return result;
}

StatusOr<ScopedLocalRef<jintArray>> JniHelper::NewIntArray(JNIEnv* env,
                                                           jsize length) {
  TC3_ENSURE_LOCAL_CAPACITY_OR_RETURN;
  ScopedLocalRef<jintArray> result(env->NewIntArray(length), env);
  TC3_NO_EXCEPTION_OR_RETURN;
  TC3_NOT_NULL_OR_RETURN;
  return result;
}

StatusOr<ScopedLocalRef<jfloatArray>> JniHelper::NewFloatArray(JNIEnv* env,
                                                               jsize length) {
  TC3_ENSURE_LOCAL_CAPACITY_OR_RETURN;
  ScopedLocalRef<jfloatArray> result(env->NewFloatArray(length), env);
  TC3_NO_EXCEPTION_OR_RETURN;
  TC3_NOT_NULL_OR_RETURN;
  return result;
}

Status JniHelper::SetObjectArrayElement(JNIEnv* env, jobjectArray array,
                                        jsize index, jobject val) {
  TC3_ENSURE_LOCAL_CAPACITY_OR_RETURN;
  env->SetObjectArrayElement(array, index, val);
  TC3_NO_EXCEPTION_OR_RETURN;
  return Status::OK;
}

StatusOr<ScopedLocalRef<jobjectArray>> JniHelper::NewObjectArray(
    JNIEnv* env, jsize length, jclass element_class, jobject initial_element) {
  TC3_ENSURE_LOCAL_CAPACITY_OR_RETURN;
  ScopedLocalRef<jobjectArray> result(
      env->NewObjectArray(length, element_class, initial_element), env);
  TC3_NO_EXCEPTION_OR_RETURN;
  TC3_NOT_NULL_OR_RETURN;
  return result;
}

StatusOr<jsize> JniHelper::GetArrayLength(JNIEnv* env,
                                          jarray jinput_fragments) {
  TC3_ENSURE_LOCAL_CAPACITY_OR_RETURN;
  jsize result = env->GetArrayLength(jinput_fragments);
  TC3_NO_EXCEPTION_OR_RETURN;
  return result;
}

StatusOr<ScopedLocalRef<jstring>> JniHelper::NewStringUTF(JNIEnv* env,
                                                          const char* bytes) {
  TC3_ENSURE_LOCAL_CAPACITY_OR_RETURN;
  ScopedLocalRef<jstring> result(env->NewStringUTF(bytes), env);
  TC3_NO_EXCEPTION_OR_RETURN;
  TC3_NOT_NULL_OR_RETURN;
  return result;
}

}  // namespace libtextclassifier3
