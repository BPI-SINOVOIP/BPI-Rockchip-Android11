/*
 * Copyright (C) 2020 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "NativeThermalTest"

#include <condition_variable>
#include <jni.h>
#include <mutex>
#include <optional>
#include <thread>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include <android/thermal.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <android-base/thread_annotations.h>
#include <log/log.h>
#include <sys/stat.h>
#include <utils/Errors.h>

using namespace android;
using namespace std::chrono_literals;
using android::base::StringPrintf;

struct AThermalTestContext {
    AThermalManager *mThermalMgr;
    std::mutex mMutex;
    std::condition_variable mCv;
    std::vector<AThermalStatus> mListenerStatus GUARDED_BY(mMutex);
};

static jclass    gNativeThermalTest_class;
static jmethodID gNativeThermalTest_thermalOverrideMethodID;

void onStatusChange(void *data, AThermalStatus status) {
    AThermalTestContext *ctx = static_cast<AThermalTestContext *>(data);
    if (ctx == nullptr) {
        return;
    } else {
        std::lock_guard<std::mutex> guard(ctx->mMutex);
        ctx->mListenerStatus.push_back(status);
        ctx->mCv.notify_all();
    }
}

static inline void setThermalStatusOverride(JNIEnv* env, jobject obj, int32_t level) {
    env->CallVoidMethod(obj, gNativeThermalTest_thermalOverrideMethodID, level);
}

static inline jstring returnJString(JNIEnv *env, std::optional<std::string> result) {
    if (result.has_value()) {
        return env->NewStringUTF(result.value().c_str());
    } else {
        return env->NewStringUTF("");
    }
}

static std::optional<std::string> testGetCurrentThermalStatus(
                                        JNIEnv *env, jobject obj, int32_t level) {
    AThermalTestContext ctx;

    ctx.mThermalMgr = AThermal_acquireManager();
    if (ctx.mThermalMgr == nullptr) {
        return "AThermal_acquireManager failed";
    }

    setThermalStatusOverride(env, obj, level);
    AThermalStatus thermalStatus = AThermal_getCurrentThermalStatus(ctx.mThermalMgr);
    if (thermalStatus == ATHERMAL_STATUS_ERROR) {
        return "getCurrentThermalStatus returns ATHERMAL_STATUS_ERROR";
    }
    // Verify the current thermal status is same as override
    if (thermalStatus != static_cast<AThermalStatus>(level)) {
        return StringPrintf("getCurrentThermalStatus %" PRId32 " != override %" PRId32 ".",
                            thermalStatus, level);
    }

    AThermal_releaseManager(ctx.mThermalMgr);
    return std::nullopt;
}

static jstring nativeTestGetCurrentThermalStatus(JNIEnv *env, jobject obj, jint level) {
    return returnJString(env, testGetCurrentThermalStatus(env, obj, static_cast<int32_t>(level)));
}

static std::optional<std::string> testRegisterThermalStatusListener(JNIEnv *env, jobject obj) {
    AThermalTestContext ctx;
    std::unique_lock<std::mutex> lock(ctx.mMutex);

    ctx.mThermalMgr = AThermal_acquireManager();
    if (ctx.mThermalMgr == nullptr) {
        return "AThermal_acquireManager failed";
    }

    // Register a listener with valid callback
    int ret = AThermal_registerThermalStatusListener(ctx.mThermalMgr, onStatusChange, &ctx);
    if (ret != 0) {
        return StringPrintf("AThermal_registerThermalStatusListener failed: %s",
                    strerror(ret));
    }

    // Expect the callback after registration
    if (ctx.mCv.wait_for(lock, 1s) == std::cv_status::timeout) {
        return "Listener callback should be called after registration";
    }

    // Verify the current thermal status is same as listener callback
    auto thermalStatus = AThermal_getCurrentThermalStatus(ctx.mThermalMgr);
    auto listenerStatus = ctx.mListenerStatus.back();
    if (thermalStatus != listenerStatus) {
        return StringPrintf("thermalStatus %" PRId32 " != Listener status %" PRId32 ".",
                            thermalStatus, listenerStatus);
    }

    // Change override level and verify the listener callback
    for (int32_t level = ATHERMAL_STATUS_LIGHT; level <= ATHERMAL_STATUS_SHUTDOWN; level++) {
        setThermalStatusOverride(env, obj, level);
        if (ctx.mCv.wait_for(lock, 1s) == std::cv_status::timeout) {
            return StringPrintf("Listener callback timeout at level %" PRId32, level);
        }
        auto overrideStatus = static_cast<AThermalStatus>(level);
        auto listenerStatus = ctx.mListenerStatus.back();
        if (listenerStatus != overrideStatus) {
            return StringPrintf("Listener thermalStatus%" PRId32 " != override %" PRId32 ".",
                            listenerStatus, overrideStatus);
        }
    }

    // Unregister listener
    ret = AThermal_unregisterThermalStatusListener(ctx.mThermalMgr, onStatusChange, &ctx);
    if (ret != 0) {
        return StringPrintf("AThermal_unregisterThermalStatusListener failed: %s",
                    strerror(ret));
    }

    AThermal_releaseManager(ctx.mThermalMgr);
    return std::nullopt;
}

static jstring nativeTestRegisterThermalStatusListener(JNIEnv *env, jobject obj) {
    return returnJString(env, testRegisterThermalStatusListener(env, obj));
}

static std::optional<std::string> testThermalStatusRegisterNullListener() {
    AThermalTestContext ctx;

    ctx.mThermalMgr = AThermal_acquireManager();
    if (ctx.mThermalMgr == nullptr) {
        return StringPrintf("AThermal_acquireManager failed");
    }

    // Register a listener with null callback
    int ret = AThermal_registerThermalStatusListener(ctx.mThermalMgr, nullptr, &ctx);
    if (ret != EINVAL) {
        return "AThermal_registerThermalStatusListener should fail with null callback";
    }

    // Register a listener with null data
    ret = AThermal_registerThermalStatusListener(ctx.mThermalMgr, onStatusChange, &ctx);
    if (ret != 0) {
        return StringPrintf("AThermal_registerThermalStatusListener failed: %s",
                    strerror(ret));
    }

    // Unregister listener with null callback and null data
    ret = AThermal_unregisterThermalStatusListener(ctx.mThermalMgr, nullptr, nullptr);
    if (ret != EINVAL) {
        return "AThermal_unregisterThermalStatusListener should fail with null listener";
    }

    AThermal_releaseManager(ctx.mThermalMgr);
    return std::nullopt;
}

static jstring nativeTestThermalStatusRegisterNullListener(JNIEnv *env, jobject) {
    return returnJString(env, testThermalStatusRegisterNullListener());
}

static std::optional<std::string> testThermalStatusListenerDoubleRegistration
                                                        (JNIEnv *env, jobject obj) {
    AThermalTestContext ctx;
    std::unique_lock<std::mutex> lock(ctx.mMutex);

    ctx.mThermalMgr = AThermal_acquireManager();
    if (ctx.mThermalMgr == nullptr) {
        return "AThermal_acquireManager failed";
    }

    // Register a listener with valid callback
    int ret = AThermal_registerThermalStatusListener(ctx.mThermalMgr, onStatusChange, &ctx);
    if (ret != 0) {
        return StringPrintf("AThermal_registerThermalStatusListener failed: %s",
                    strerror(ret));
    }

    // Register the listener again with same callback and data
    ret = AThermal_registerThermalStatusListener(ctx.mThermalMgr, onStatusChange, &ctx);
    if (ret != EINVAL) {
        return "Register should fail as listener already registered";
    }

    // Register a listener with same callback but null data
    ret = AThermal_registerThermalStatusListener(ctx.mThermalMgr, onStatusChange, nullptr);
    if (ret != 0) {
        return StringPrintf("Register listener with null data failed: %s", strerror(ret));
    }

    // Expect listener callback
    if (ctx.mCv.wait_for(lock, 1s) == std::cv_status::timeout) {
        return "Thermal listener callback timeout";
    }

    // Unregister listener
    ret = AThermal_unregisterThermalStatusListener(ctx.mThermalMgr, onStatusChange, &ctx);
    if (ret != 0) {
        return StringPrintf("AThermal_unregisterThermalStatusListener failed: %s",
                    strerror(ret));
    }

    for (int32_t level = ATHERMAL_STATUS_LIGHT; level <= ATHERMAL_STATUS_SHUTDOWN; level++) {
        setThermalStatusOverride(env, obj, level);
        // Expect no listener callback
        if (ctx.mCv.wait_for(lock, 1s) != std::cv_status::timeout) {
            return "Thermal listener got callback after unregister.";
        }
    }

    // Unregister listener already unregistered
    ret = AThermal_unregisterThermalStatusListener(ctx.mThermalMgr, onStatusChange, &ctx);
    if (ret != EINVAL) {
        return "Unregister should fail with listener already unregistered";
    }

    AThermal_releaseManager(ctx.mThermalMgr);
    return std::nullopt;
}

static jstring nativeTestThermalStatusListenerDoubleRegistration(JNIEnv *env, jobject obj) {
    return returnJString(env, testThermalStatusListenerDoubleRegistration(env, obj));
}

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
    JNIEnv* env;
    const JNINativeMethod methodTable[] = {
        {"nativeTestGetCurrentThermalStatus", "(I)Ljava/lang/String;",
         (void*)nativeTestGetCurrentThermalStatus},
        {"nativeTestRegisterThermalStatusListener", "()Ljava/lang/String;",
         (void*)nativeTestRegisterThermalStatusListener},
        {"nativeTestThermalStatusRegisterNullListener", "()Ljava/lang/String;",
         (void*)nativeTestThermalStatusRegisterNullListener},
        {"nativeTestThermalStatusListenerDoubleRegistration", "()Ljava/lang/String;",
         (void*)nativeTestThermalStatusListenerDoubleRegistration},
    };
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    gNativeThermalTest_class = env->FindClass("android/thermal/cts/NativeThermalTest");
    gNativeThermalTest_thermalOverrideMethodID =
                env->GetMethodID(gNativeThermalTest_class, "setOverrideStatus", "(I)V");
    if (gNativeThermalTest_thermalOverrideMethodID == nullptr) {
        return JNI_ERR;
    }
    if (env->RegisterNatives(gNativeThermalTest_class, methodTable,
            sizeof(methodTable) / sizeof(JNINativeMethod)) != JNI_OK) {
        return JNI_ERR;
    }
    return JNI_VERSION_1_6;
}
