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

#define LOG_TAG "Cts-NdkBinderTest"

#include "utilities.h"

#include <android/log.h>

static size_t sNumInstances = 0;
size_t ThisShouldBeDestroyed::numInstances() { return sNumInstances; }
ThisShouldBeDestroyed::ThisShouldBeDestroyed() { sNumInstances++; }
ThisShouldBeDestroyed::~ThisShouldBeDestroyed() { sNumInstances--; }

void* SampleClassOnCreate(void* args) {
  return args;  // SampleData
}

void SampleClassOnDestroy(void* userData) {
  SampleData* data = static_cast<SampleData*>(userData);
  if (data->onDestroy != nullptr) {
    data->onDestroy(data);
  }
  delete data;
}

binder_status_t SampleClassOnTransact(AIBinder* binder, transaction_code_t code,
                                      const AParcel* in, AParcel* out) {
  SampleData* data = static_cast<SampleData*>(AIBinder_getUserData(binder));
  if (data == nullptr) {
    __android_log_write(ANDROID_LOG_FATAL, LOG_TAG, "null user data");
  }
  data->numberTransactions++;
  if (data->onTransact == nullptr) {
    ADD_FAILURE() << "onTransact not specified, but transactions called";
    return STATUS_FAILED_TRANSACTION;
  }
  return data->onTransact(code, in, out);
}

const char* SampleData::kDescriptor = "this-is-arbitrary";
const AIBinder_Class* SampleData::kClass =
    AIBinder_Class_define(SampleData::kDescriptor, SampleClassOnCreate,
                          SampleClassOnDestroy, SampleClassOnTransact);

const AIBinder_Class* SampleData::kAnotherClassWithSameDescriptor = AIBinder_Class_define(
    SampleData::kDescriptor, SampleClassOnCreate, SampleClassOnDestroy, SampleClassOnTransact);

const char* SampleData::kAnotherDescriptor = "this-is-another-arbitrary-thing";
const AIBinder_Class* SampleData::kAnotherClass =
    AIBinder_Class_define(SampleData::kAnotherDescriptor, SampleClassOnCreate,
                          SampleClassOnDestroy, SampleClassOnTransact);

JNIEnv* GetEnv() {
  JavaVM* vm = GetJavaVM();
  if (vm == nullptr) return nullptr;

  JNIEnv* result = nullptr;
  jint attach = vm->AttachCurrentThread(&result, nullptr);

  EXPECT_EQ(JNI_OK, attach);
  EXPECT_NE(nullptr, result);
  return result;
}

jobject callStaticJavaMethodForObject(JNIEnv* env, const std::string& clazz,
                                      const std::string& method, const std::string& type) {
  jclass cl = env->FindClass(clazz.c_str());
  if (cl == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "No class %s", clazz.c_str());
    return nullptr;
  }

  jmethodID mid = env->GetStaticMethodID(cl, method.c_str(), type.c_str());
  if (mid == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "No method id %s", method.c_str());
    return nullptr;
  }

  jobject object = env->CallStaticObjectMethod(cl, mid);
  if (object == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Got null object from Java");
    return nullptr;
  }

  return object;
}
