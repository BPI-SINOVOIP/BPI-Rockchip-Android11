/*
 * Copyright (C) 2010 The Android Open Source Project
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

#define LOG_TAG "libcore" // We'll be next to "dalvikvm" in the log; make the distinction clear.

#include <stdlib.h>

#include <log/log.h>
#include <nativehelper/JNIHelp.h>
#include <nativehelper/ScopedLocalFrame.h>

#include "JniConstants.h"

// DalvikVM calls this on startup, so we can statically register all our native methods.
jint JNI_OnLoad(JavaVM* vm, void*) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        ALOGE("JavaVM::GetEnv() failed");
        abort();
    }

    ScopedLocalFrame localFrame(env);

#define REGISTER(FN) extern void FN(JNIEnv*); FN(env)
    REGISTER(register_android_system_OsConstants);
    //    REGISTER(register_java_lang_StringToReal);
    REGISTER(register_java_lang_invoke_MethodHandle);
    REGISTER(register_java_lang_invoke_VarHandle);
    REGISTER(register_java_math_NativeBN);
    REGISTER(register_libcore_icu_ICU);
    REGISTER(register_libcore_icu_TimeZoneNames);
    REGISTER(register_libcore_io_AsynchronousCloseMonitor);
    REGISTER(register_libcore_io_Linux);
    REGISTER(register_libcore_io_Memory);
    REGISTER(register_libcore_util_NativeAllocationRegistry);
    REGISTER(register_org_apache_harmony_dalvik_NativeTestTarget);
    REGISTER(register_org_apache_harmony_xml_ExpatParser);
    REGISTER(register_sun_misc_Unsafe);
#undef REGISTER

    JniConstants::Initialize(env);
    return JNI_VERSION_1_6;
}

// ART calls this on shutdown, do any global cleanup here.
// -- Very important if we restart multiple ART runtimes in the same process to reset the state.
void JNI_OnUnload(JavaVM*, void*) {
    // Don't use the JavaVM in this method. ART only calls this once all threads are
    // unregistered.
    ALOGV("libjavacore JNI_OnUnload");
#define UNREGISTER(FN) extern void FN(); FN()
    UNREGISTER(unregister_libcore_icu_ICU);
#undef UNREGISTER
    JniConstants::Invalidate();
    jniUninitializeConstants();
}
