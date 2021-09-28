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

extern "C" JNIEXPORT void JNICALL
Java_android_jvmti_cts_JvmtiRunTestBasedTest_setupExtraLogging(JNIEnv* env, jclass) {
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetVerboseFlag(JVMTI_VERBOSE_OTHER, true));
}

static JNINativeMethod gMethods[] = {
  { "setupExtraLogging", "()V",
          (void*)Java_android_jvmti_cts_JvmtiRunTestBasedTest_setupExtraLogging },
};
void register_android_jvmti_cts_JvmtiRunTestBasedTest(jvmtiEnv* jenv, JNIEnv* env) {
  ScopedLocalRef<jclass> klass(env, GetClass(jenv, env,
          "android/jvmti/cts/JvmtiRunTestBasedTest", nullptr));
  if (klass.get() == nullptr) {
    env->ExceptionClear();
    return;
  }

  env->RegisterNatives(klass.get(), gMethods, sizeof(gMethods) / sizeof(JNINativeMethod));
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    LOG(ERROR) << "Could not register natives for JvmtiRedefineClassesTest class";
  }
}

}  // namespace art
