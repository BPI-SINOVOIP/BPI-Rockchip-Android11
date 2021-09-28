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

#include <android/binder_ibinder.h>
#include <android/binder_ibinder_jni.h>
#include <gtest/gtest.h>
#include <nativetesthelper_jni/utils.h>

#include "utilities.h"

void* NothingClass_onCreate(void* args) { return args; }
void NothingClass_onDestroy(void* /*userData*/) {}
binder_status_t NothingClass_onTransact(AIBinder*, transaction_code_t,
                                        const AParcel*, AParcel*) {
  return STATUS_UNKNOWN_ERROR;
}

static AIBinder_Class* kNothingClass =
    AIBinder_Class_define("nothing", NothingClass_onCreate,
                          NothingClass_onDestroy, NothingClass_onTransact);

class NdkBinderTest_AIBinder_Jni : public NdkBinderTest {};

TEST_F(NdkBinderTest_AIBinder_Jni, ConvertJni) {
  JNIEnv* env = GetEnv();
  ASSERT_NE(nullptr, env);

  AIBinder* binder = AIBinder_new(kNothingClass, nullptr);
  EXPECT_NE(nullptr, binder);

  jobject object = AIBinder_toJavaBinder(env, binder);
  EXPECT_NE(nullptr, object);

  AIBinder* fromJavaBinder = AIBinder_fromJavaBinder(env, object);
  EXPECT_EQ(binder, fromJavaBinder);

  AIBinder_decStrong(binder);
  AIBinder_decStrong(fromJavaBinder);
}
