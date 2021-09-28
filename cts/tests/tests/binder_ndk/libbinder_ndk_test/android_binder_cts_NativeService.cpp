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

#include <android/binder_ibinder_jni.h>
#include <android/log.h>

#include "itest_impl.h"

using ::aidl::test_package::ITest;
using ::ndk::SharedRefBase;
using ::ndk::SpAIBinder;

extern "C" JNIEXPORT jobject JNICALL
Java_android_binder_cts_NativeService_getBinder_1native(JNIEnv* env) {
  // The ref owns the MyTest, and the binder owns the ref.
  SpAIBinder binder = SharedRefBase::make<MyTest>()->asBinder();

  // adding an arbitrary class as the extension
  std::shared_ptr<MyTest> ext = SharedRefBase::make<MyTest>();
  SpAIBinder extBinder = ext->asBinder();

  binder_status_t ret = AIBinder_setExtension(binder.get(), extBinder.get());
  if (ret != STATUS_OK) {
    __android_log_write(ANDROID_LOG_ERROR, LOG_TAG, "Could not set local extension");
  }

  // And the Java object owns the binder
  return AIBinder_toJavaBinder(env, binder.get());
}
