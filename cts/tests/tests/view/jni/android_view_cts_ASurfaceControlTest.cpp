/*
 * Copyright 2018 The Android Open Source Project
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

#define LOG_TAG "ASurfaceControlTest"

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include <array>
#include <cinttypes>
#include <string>

#include <android/data_space.h>
#include <android/hardware_buffer.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <android/surface_control.h>
#include <android/sync.h>

#include <errno.h>
#include <jni.h>
#include <time.h>

namespace {

// Raises a java exception
static void fail(JNIEnv* env, const char* format, ...) {
    va_list args;

    va_start(args, format);
    char* msg;
    vasprintf(&msg, format, args);
    va_end(args);

    jclass exClass;
    const char* className = "java/lang/AssertionError";
    exClass = env->FindClass(className);
    env->ThrowNew(exClass, msg);
    free(msg);
}

#define ASSERT(condition, format, args...) \
    if (!(condition)) {                    \
        fail(env, format, ##args);         \
        return;                            \
    }

#define NANOS_PER_SECOND 1000000000LL
int64_t systemTime() {
    struct timespec time;
    int result = clock_gettime(CLOCK_MONOTONIC, &time);
    if (result < 0) {
        return -errno;
    }
    return (time.tv_sec * NANOS_PER_SECOND) + time.tv_nsec;
}

static AHardwareBuffer* allocateBuffer(int32_t width, int32_t height) {
    AHardwareBuffer* buffer = nullptr;
    AHardwareBuffer_Desc desc = {};
    desc.width = width;
    desc.height = height;
    desc.layers = 1;
    desc.usage = AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN;
    desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;

    AHardwareBuffer_allocate(&desc, &buffer);

    return buffer;
}

static void fillRegion(void* data, int32_t left, int32_t top, int32_t right,
                       int32_t bottom, uint32_t color, uint32_t stride) {
    uint32_t* ptr = static_cast<uint32_t*>(data);

    ptr += stride * top;

    for (uint32_t y = top; y < bottom; y++) {
        for (uint32_t x = left; x < right; x++) {
            ptr[x] = color;
        }
        ptr += stride;
    }
}

static bool getSolidBuffer(int32_t width, int32_t height, uint32_t color,
                           AHardwareBuffer** outHardwareBuffer,
                           int* outFence) {
    AHardwareBuffer* buffer = allocateBuffer(width, height);
    if (!buffer) {
        return true;
    }

    AHardwareBuffer_Desc desc = {};
    AHardwareBuffer_describe(buffer, &desc);

    void* data = nullptr;
    const ARect rect{0, 0, width, height};
    AHardwareBuffer_lock(buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1, &rect,
                                             &data);
    if (!data) {
        return true;
    }

    fillRegion(data, 0, 0, width, height, color, desc.stride);

    AHardwareBuffer_unlock(buffer, outFence);

    *outHardwareBuffer = buffer;
    return false;
}

static bool getQuadrantBuffer(int32_t width, int32_t height, jint colorTopLeft,
                              jint colorTopRight, jint colorBottomRight,
                              jint colorBottomLeft,
                              AHardwareBuffer** outHardwareBuffer,
                              int* outFence) {
    AHardwareBuffer* buffer = allocateBuffer(width, height);
    if (!buffer) {
        return true;
    }

    AHardwareBuffer_Desc desc = {};
    AHardwareBuffer_describe(buffer, &desc);

    void* data = nullptr;
    const ARect rect{0, 0, width, height};
    AHardwareBuffer_lock(buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1, &rect,
                                             &data);
    if (!data) {
        return true;
    }

    fillRegion(data, 0, 0, width / 2, height / 2, colorTopLeft, desc.stride);
    fillRegion(data, width / 2, 0, width, height / 2, colorTopRight, desc.stride);
    fillRegion(data, 0, height / 2, width / 2, height, colorBottomLeft,
                         desc.stride);
    fillRegion(data, width / 2, height / 2, width, height, colorBottomRight,
                         desc.stride);

    AHardwareBuffer_unlock(buffer, outFence);

    *outHardwareBuffer = buffer;
    return false;
}

jlong SurfaceTransaction_create(JNIEnv* /*env*/, jclass) {
    return reinterpret_cast<jlong>(ASurfaceTransaction_create());
}

void SurfaceTransaction_delete(JNIEnv* /*env*/, jclass, jlong surfaceTransaction) {
    ASurfaceTransaction_delete(
            reinterpret_cast<ASurfaceTransaction*>(surfaceTransaction));
}

void SurfaceTransaction_apply(JNIEnv* /*env*/, jclass, jlong surfaceTransaction) {
    ASurfaceTransaction_apply(
            reinterpret_cast<ASurfaceTransaction*>(surfaceTransaction));
}

long SurfaceControl_createFromWindow(JNIEnv* env, jclass, jobject jSurface) {
    if (!jSurface) {
        return 0;
    }

    ANativeWindow* window = ANativeWindow_fromSurface(env, jSurface);
    if (!window) {
        return 0;
    }

    const std::string debugName = "SurfaceControl_createFromWindowLayer";
    ASurfaceControl* surfaceControl =
            ASurfaceControl_createFromWindow(window, debugName.c_str());
    if (!surfaceControl) {
        return 0;
    }

    ANativeWindow_release(window);

    return reinterpret_cast<jlong>(surfaceControl);
}

jlong SurfaceControl_create(JNIEnv* /*env*/, jclass, jlong parentSurfaceControlId) {
    ASurfaceControl* surfaceControl = nullptr;
    const std::string debugName = "SurfaceControl_create";

    surfaceControl = ASurfaceControl_create(
            reinterpret_cast<ASurfaceControl*>(parentSurfaceControlId),
            debugName.c_str());

    return reinterpret_cast<jlong>(surfaceControl);
}

void SurfaceControl_release(JNIEnv* /*env*/, jclass, jlong surfaceControl) {
    ASurfaceControl_release(reinterpret_cast<ASurfaceControl*>(surfaceControl));
}

jlong SurfaceTransaction_setSolidBuffer(JNIEnv* /*env*/, jclass,
                                        jlong surfaceControl,
                                        jlong surfaceTransaction, jint width,
                                        jint height, jint color) {
    AHardwareBuffer* buffer = nullptr;
    int fence = -1;

    bool err = getSolidBuffer(width, height, color, &buffer, &fence);
    if (err) {
        return 0;
    }

    ASurfaceTransaction_setBuffer(
            reinterpret_cast<ASurfaceTransaction*>(surfaceTransaction),
            reinterpret_cast<ASurfaceControl*>(surfaceControl), buffer, fence);

    ASurfaceTransaction_setBufferDataSpace(
            reinterpret_cast<ASurfaceTransaction*>(surfaceTransaction),
            reinterpret_cast<ASurfaceControl*>(surfaceControl), ADATASPACE_UNKNOWN);

    return reinterpret_cast<jlong>(buffer);
}

jlong SurfaceTransaction_setQuadrantBuffer(
        JNIEnv* /*env*/, jclass, jlong surfaceControl, jlong surfaceTransaction,
        jint width, jint height, jint colorTopLeft, jint colorTopRight,
        jint colorBottomRight, jint colorBottomLeft) {
    AHardwareBuffer* buffer = nullptr;
    int fence = -1;

    bool err =
            getQuadrantBuffer(width, height, colorTopLeft, colorTopRight,
                              colorBottomRight, colorBottomLeft, &buffer, &fence);
    if (err) {
        return 0;
    }

    ASurfaceTransaction_setBuffer(
            reinterpret_cast<ASurfaceTransaction*>(surfaceTransaction),
            reinterpret_cast<ASurfaceControl*>(surfaceControl), buffer, fence);

    ASurfaceTransaction_setBufferDataSpace(
            reinterpret_cast<ASurfaceTransaction*>(surfaceTransaction),
            reinterpret_cast<ASurfaceControl*>(surfaceControl), ADATASPACE_UNKNOWN);

    return reinterpret_cast<jlong>(buffer);
}

void SurfaceTransaction_releaseBuffer(JNIEnv* /*env*/, jclass, jlong buffer) {
    AHardwareBuffer_release(reinterpret_cast<AHardwareBuffer*>(buffer));
}

void SurfaceTransaction_setVisibility(JNIEnv* /*env*/, jclass,
                                      jlong surfaceControl,
                                      jlong surfaceTransaction, jboolean show) {
    int8_t visibility = (show) ? ASURFACE_TRANSACTION_VISIBILITY_SHOW :
                                 ASURFACE_TRANSACTION_VISIBILITY_HIDE;
    ASurfaceTransaction_setVisibility(
            reinterpret_cast<ASurfaceTransaction*>(surfaceTransaction),
            reinterpret_cast<ASurfaceControl*>(surfaceControl), visibility);
}

void SurfaceTransaction_setBufferOpaque(JNIEnv* /*env*/, jclass,
                                        jlong surfaceControl,
                                        jlong surfaceTransaction,
                                        jboolean opaque) {
    int8_t transparency = (opaque) ? ASURFACE_TRANSACTION_TRANSPARENCY_OPAQUE :
                                   ASURFACE_TRANSACTION_TRANSPARENCY_TRANSPARENT;
    ASurfaceTransaction_setBufferTransparency(
            reinterpret_cast<ASurfaceTransaction*>(surfaceTransaction),
            reinterpret_cast<ASurfaceControl*>(surfaceControl), transparency);
}

void SurfaceTransaction_setGeometry(JNIEnv* /*env*/, jclass,
                                    jlong surfaceControl,
                                    jlong surfaceTransaction,
                                    jint srcLeft, jint srcTop, jint srcRight, jint srcBottom,
                                    jint dstLeft, jint dstTop, jint dstRight, jint dstBottom,
                                    jint transform) {
    const ARect src{srcLeft, srcTop, srcRight, srcBottom};
    const ARect dst{dstLeft, dstTop, dstRight, dstBottom};
    ASurfaceTransaction_setGeometry(
            reinterpret_cast<ASurfaceTransaction*>(surfaceTransaction),
            reinterpret_cast<ASurfaceControl*>(surfaceControl), src, dst, transform);
}

void SurfaceTransaction_setDamageRegion(JNIEnv* /*env*/, jclass,
                                        jlong surfaceControl,
                                        jlong surfaceTransaction, jint left,
                                        jint top, jint right, jint bottom) {
    const ARect rect[] = {{left, top, right, bottom}};
    ASurfaceTransaction_setDamageRegion(
            reinterpret_cast<ASurfaceTransaction*>(surfaceTransaction),
            reinterpret_cast<ASurfaceControl*>(surfaceControl), rect, 1);
}

void SurfaceTransaction_setZOrder(JNIEnv* /*env*/, jclass, jlong surfaceControl,
                                  jlong surfaceTransaction, jint z) {
    ASurfaceTransaction_setZOrder(
            reinterpret_cast<ASurfaceTransaction*>(surfaceTransaction),
            reinterpret_cast<ASurfaceControl*>(surfaceControl), z);
}

static void onComplete(void* context, ASurfaceTransactionStats* stats) {
    if (!stats) {
        return;
    }

    int64_t latchTime = ASurfaceTransactionStats_getLatchTime(stats);
    if (latchTime < 0) {
        return;
    }

    ASurfaceControl** surfaceControls = nullptr;
    size_t surfaceControlsSize = 0;
    ASurfaceTransactionStats_getASurfaceControls(stats, &surfaceControls, &surfaceControlsSize);

    for (int i = 0; i < surfaceControlsSize; i++) {
        ASurfaceControl* surfaceControl = surfaceControls[i];

        int64_t acquireTime = ASurfaceTransactionStats_getAcquireTime(stats, surfaceControl);
        if (acquireTime < -1) {
            return;
        }

        int previousReleaseFence = ASurfaceTransactionStats_getPreviousReleaseFenceFd(
                    stats, surfaceControl);
        close(previousReleaseFence);
    }

    int presentFence = ASurfaceTransactionStats_getPresentFenceFd(stats);

    if (!context) {
        close(presentFence);
        return;
    }

    int* contextIntPtr = reinterpret_cast<int*>(context);
    contextIntPtr[0]++;
    contextIntPtr[1] = presentFence;
    int64_t* systemTimeLongPtr = reinterpret_cast<int64_t*>(contextIntPtr + 2);
    *systemTimeLongPtr = systemTime();
}

jlong SurfaceTransaction_setOnComplete(JNIEnv* /*env*/, jclass, jlong surfaceTransaction) {
    int* context = new int[4];
    context[0] = 0;
    context[1] = -1;
    context[2] = -1;
    context[3] = -1;

    ASurfaceTransaction_setOnComplete(
            reinterpret_cast<ASurfaceTransaction*>(surfaceTransaction),
            reinterpret_cast<void*>(context), onComplete);
    return reinterpret_cast<jlong>(context);
}

void SurfaceTransaction_checkOnComplete(JNIEnv* env, jclass, jlong context,
                                        jlong desiredPresentTime) {
    ASSERT(context != 0, "invalid context")

    int* contextPtr = reinterpret_cast<int*>(context);
    int data = contextPtr[0];
    int presentFence = contextPtr[1];

    int64_t* callbackTimePtr = reinterpret_cast<int64_t*>(contextPtr + 2);
    int64_t callbackTime = *callbackTimePtr;

    delete[] contextPtr;

    if (desiredPresentTime < 0) {
        close(presentFence);
        ASSERT(data >= 1, "did not receive a callback")
        ASSERT(data <= 1, "received too many callbacks")
        return;
    }

    if (presentFence >= 0) {
        struct sync_file_info* syncFileInfo = sync_file_info(presentFence);
        ASSERT(syncFileInfo, "invalid fence");

        if (syncFileInfo->status != 1) {
            sync_file_info_free(syncFileInfo);
            ASSERT(syncFileInfo->status == 1, "fence did not signal")
        }

        uint64_t presentTime = 0;
        struct sync_fence_info* syncFenceInfo = sync_get_fence_info(syncFileInfo);
        for (size_t i = 0; i < syncFileInfo->num_fences; i++) {
            if (syncFenceInfo[i].timestamp_ns > presentTime) {
                presentTime = syncFenceInfo[i].timestamp_ns;
            }
        }

        sync_file_info_free(syncFileInfo);
        close(presentFence);

        // In the worst case the worst present time should be no more than three frames off from the
        // desired present time. Since the test case is using a virtual display and there are no
        // requirements for virtual display refresh rate timing, lets assume a refresh rate of 16fps.
        ASSERT(presentTime < desiredPresentTime + 0.188 * 1e9, "transaction was presented too late");
        ASSERT(presentTime >= desiredPresentTime, "transaction was presented too early");
    } else {
        ASSERT(presentFence == -1, "invalid fences should be -1");
        // The device doesn't support present fences. We will use the callback time to roughly
        // verify the result. Since the callback could take up to half a frame, do the normal bound
        // check plus an additional half frame.
        ASSERT(callbackTime < desiredPresentTime + (0.188 + 0.031) * 1e9,
                                                  "transaction was presented too late");
        ASSERT(callbackTime >= desiredPresentTime, "transaction was presented too early");
    }

    ASSERT(data >= 1, "did not receive a callback")
    ASSERT(data <= 1, "received too many callbacks")
}

jlong SurfaceTransaction_setDesiredPresentTime(JNIEnv* /*env*/, jclass, jlong surfaceTransaction,
                                              jlong desiredPresentTimeOffset) {
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    int64_t currentTime = ((int64_t) t.tv_sec)*1000000000LL + t.tv_nsec;

    int64_t desiredPresentTime = currentTime + desiredPresentTimeOffset;

    ASurfaceTransaction_setDesiredPresentTime(
            reinterpret_cast<ASurfaceTransaction*>(surfaceTransaction), desiredPresentTime);

    return desiredPresentTime;
}

void SurfaceTransaction_setBufferAlpha(JNIEnv* /*env*/, jclass,
                                      jlong surfaceControl,
                                      jlong surfaceTransaction, jdouble alpha) {
    ASurfaceTransaction_setBufferAlpha(
            reinterpret_cast<ASurfaceTransaction*>(surfaceTransaction),
            reinterpret_cast<ASurfaceControl*>(surfaceControl), alpha);
}

void SurfaceTransaction_reparent(JNIEnv* /*env*/, jclass, jlong surfaceControl,
                                 jlong newParentSurfaceControl, jlong surfaceTransaction) {
    ASurfaceTransaction_reparent(
            reinterpret_cast<ASurfaceTransaction*>(surfaceTransaction),
            reinterpret_cast<ASurfaceControl*>(surfaceControl),
            reinterpret_cast<ASurfaceControl*>(newParentSurfaceControl));
}

void SurfaceTransaction_setColor(JNIEnv* /*env*/, jclass, jlong surfaceControl,
                                 jlong surfaceTransaction, jfloat r,
                                 jfloat g, jfloat b, jfloat alpha) {
    ASurfaceTransaction_setColor(
            reinterpret_cast<ASurfaceTransaction*>(surfaceTransaction),
            reinterpret_cast<ASurfaceControl*>(surfaceControl),
            r, g, b, alpha, ADATASPACE_UNKNOWN);
}

const std::array<JNINativeMethod, 20> JNI_METHODS = {{
    {"nSurfaceTransaction_create", "()J", (void*)SurfaceTransaction_create},
    {"nSurfaceTransaction_delete", "(J)V", (void*)SurfaceTransaction_delete},
    {"nSurfaceTransaction_apply", "(J)V", (void*)SurfaceTransaction_apply},
    {"nSurfaceControl_createFromWindow", "(Landroid/view/Surface;)J",
                                            (void*)SurfaceControl_createFromWindow},
    {"nSurfaceControl_create", "(J)J", (void*)SurfaceControl_create},
    {"nSurfaceControl_release", "(J)V", (void*)SurfaceControl_release},
    {"nSurfaceTransaction_setSolidBuffer", "(JJIII)J", (void*)SurfaceTransaction_setSolidBuffer},
    {"nSurfaceTransaction_setQuadrantBuffer", "(JJIIIIII)J",
                                            (void*)SurfaceTransaction_setQuadrantBuffer},
    {"nSurfaceTransaction_releaseBuffer", "(J)V", (void*)SurfaceTransaction_releaseBuffer},
    {"nSurfaceTransaction_setVisibility", "(JJZ)V", (void*)SurfaceTransaction_setVisibility},
    {"nSurfaceTransaction_setBufferOpaque", "(JJZ)V", (void*)SurfaceTransaction_setBufferOpaque},
    {"nSurfaceTransaction_setGeometry", "(JJIIIIIIIII)V", (void*)SurfaceTransaction_setGeometry},
    {"nSurfaceTransaction_setDamageRegion", "(JJIIII)V", (void*)SurfaceTransaction_setDamageRegion},
    {"nSurfaceTransaction_setZOrder", "(JJI)V", (void*)SurfaceTransaction_setZOrder},
    {"nSurfaceTransaction_setOnComplete", "(J)J", (void*)SurfaceTransaction_setOnComplete},
    {"nSurfaceTransaction_checkOnComplete", "(JJ)V", (void*)SurfaceTransaction_checkOnComplete},
    {"nSurfaceTransaction_setDesiredPresentTime", "(JJ)J",
                                            (void*)SurfaceTransaction_setDesiredPresentTime},
    {"nSurfaceTransaction_setBufferAlpha", "(JJD)V", (void*)SurfaceTransaction_setBufferAlpha},
    {"nSurfaceTransaction_reparent", "(JJJ)V", (void*)SurfaceTransaction_reparent},
    {"nSurfaceTransaction_setColor", "(JJFFFF)V", (void*)SurfaceTransaction_setColor},
}};

}  // anonymous namespace

jint register_android_view_cts_ASurfaceControlTest(JNIEnv* env) {
    jclass clazz = env->FindClass("android/view/cts/ASurfaceControlTest");
    return env->RegisterNatives(clazz, JNI_METHODS.data(), JNI_METHODS.size());
}
