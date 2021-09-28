/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <jni.h>
#include <jvmti.h>

#include "android-base/logging.h"
#include "jni_binder.h"
#include "jvmti_helper.h"
#include "scoped_local_ref.h"
#include "test_env.h"

namespace art {

extern void register_art_Main(jvmtiEnv*, JNIEnv*);
extern void register_android_jvmti_cts_JvmtiRedefineClassesTest(jvmtiEnv*, JNIEnv*);
extern void register_android_jvmti_cts_JvmtiTaggingTest(jvmtiEnv*, JNIEnv*);
extern void register_android_jvmti_cts_JvmtiTrackingTest(jvmtiEnv*, JNIEnv*);
extern void register_android_jvmti_cts_JvmtiRunTestBasedTest(jvmtiEnv*, JNIEnv*);

static void InformMainAttach(jvmtiEnv* jenv,
                             JNIEnv* env,
                             const char* class_name,
                             const char* method_name) {
  // Register native methods from available classes
  // The agent is used with each test class, but we don't know which class is currently available.
  // For that reason, we try to register the native methods in each one. Each function returns
  // without throwing an error if the specified class can't be found.
  register_art_Main(jenv, env);
  register_android_jvmti_cts_JvmtiRedefineClassesTest(jenv, env);
  register_android_jvmti_cts_JvmtiTaggingTest(jenv, env);
  register_android_jvmti_cts_JvmtiTrackingTest(jenv, env);
  register_android_jvmti_cts_JvmtiRunTestBasedTest(jenv, env);

  // Use JNI to load the class.
  ScopedLocalRef<jclass> klass(env, GetClass(jenv, env, class_name, nullptr));
  CHECK(klass.get() != nullptr) << class_name;

  jmethodID method = env->GetStaticMethodID(klass.get(), method_name, "()V");
  CHECK(method != nullptr);

  env->CallStaticVoidMethod(klass.get(), method);
}

static constexpr const char* kMainClass = "art/CtsMain";
static constexpr const char* kMainClassStartup = "startup";

extern "C" JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* vm,
                                               char* options ATTRIBUTE_UNUSED,
                                               void* reserved ATTRIBUTE_UNUSED) {
  if (vm->GetEnv(reinterpret_cast<void**>(&jvmti_env), JVMTI_VERSION_1_0) != 0) {
    LOG(FATAL) << "Could not get shared jvmtiEnv";
  }

  SetStandardCapabilities(jvmti_env);
  return 0;
}

extern "C" JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM* vm,
                                                 char* options ATTRIBUTE_UNUSED,
                                                 void* reserved ATTRIBUTE_UNUSED) {
  JNIEnv* env;
  CHECK_EQ(0, vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6))
      << "Could not get JNIEnv";

  if (vm->GetEnv(reinterpret_cast<void**>(&jvmti_env), JVMTI_VERSION_1_0) != 0) {
    LOG(FATAL) << "Could not get shared jvmtiEnv";
  }

  SetStandardCapabilities(jvmti_env);
  InformMainAttach(jvmti_env, env, kMainClass, kMainClassStartup);
  return 0;
}

}  // namespace art
