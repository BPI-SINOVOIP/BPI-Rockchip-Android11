/*
 * Copyright 2020 The Android Open Source Project
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
 *
 */

#define LOG_TAG "FrameRateCtsActivity"

#include <android/hardware_buffer.h>
#include <android/hardware_buffer_jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/rect.h>
#include <android/surface_control.h>
#include <jni.h>
#include <utils/Errors.h>

#include <array>
#include <string>

#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace android;

namespace {

class Buffer {
public:
    Buffer(int width, int height, int rgbaColor) {
        AHardwareBuffer_Desc desc;
        memset(&desc, 0, sizeof(desc));
        desc.width = width;
        desc.height = height;
        desc.layers = 1;
        desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
        desc.usage =
                AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
        int rc = AHardwareBuffer_allocate(&desc, &mBuffer);
        if (rc < 0 || mBuffer == nullptr) {
            ALOGE("AHardwareBuffer_allocate failed: %s (%d)", strerror(-rc), -rc);
            return;
        }
        int8_t* buf = nullptr;
        int32_t bytesPerPixel = 0;
        int32_t bytesPerStride = 0;
        std::string lockFunctionName = "AHardwareBuffer_lockAndGetInfo";
        rc = AHardwareBuffer_lockAndGetInfo(mBuffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY,
                                            /*fence=*/-1,
                                            /*rect=*/nullptr, reinterpret_cast<void**>(&buf),
                                            &bytesPerPixel, &bytesPerStride);
        if (rc == INVALID_OPERATION) {
            // Older versions of gralloc don't implement AHardwareBuffer_lockAndGetInfo(). Fall back
            // to AHardwareBuffer_lock().
            lockFunctionName = "AHardwareBuffer_lock";
            bytesPerPixel = 4;
            bytesPerStride = width * bytesPerPixel;
            rc = AHardwareBuffer_lock(mBuffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY,
                                      /*fence=*/-1,
                                      /*rect=*/nullptr, reinterpret_cast<void**>(&buf));
        }
        if (rc < 0 || buf == nullptr) {
            ALOGE("%s failed: %s (%d)", lockFunctionName.c_str(), strerror(-rc), -rc);
            AHardwareBuffer_release(mBuffer);
            mBuffer = nullptr;
            return;
        }

        // There's a bug where Qualcomm returns pixels per stride instead of bytes per stride. See
        // b/149601846.
        if (bytesPerStride < width * bytesPerPixel) {
            bytesPerStride *= bytesPerPixel;
        }

        int8_t* rgbaBytes = reinterpret_cast<int8_t*>(&rgbaColor);
        for (int row = 0; row < height; row++) {
            int8_t* ptr = buf + row * bytesPerStride;
            for (int col = 0; col < width; col++, ptr += bytesPerPixel) {
                ptr[0] = rgbaBytes[0];
                ptr[1] = rgbaBytes[1];
                ptr[2] = rgbaBytes[2];
                ptr[3] = rgbaBytes[3];
            }
        }

        rc = AHardwareBuffer_unlock(mBuffer, /*fence=*/nullptr);
        if (rc < 0) {
            ALOGE("AHardwareBuffer_unlock failed: %s (%d)", strerror(-rc), -rc);
            AHardwareBuffer_release(mBuffer);
            mBuffer = nullptr;
            return;
        }
    }

    ~Buffer() {
        if (mBuffer) {
            AHardwareBuffer_release(mBuffer);
        }
    }

    bool isValid() const { return mBuffer != nullptr; }
    AHardwareBuffer* getBuffer() const { return mBuffer; }

private:
    AHardwareBuffer* mBuffer = nullptr;
};

class Surface {
public:
    Surface(ANativeWindow* parentWindow, const std::string& name, int left, int top, int right,
            int bottom) {
        mSurface = ASurfaceControl_createFromWindow(parentWindow, name.c_str());
        if (mSurface == nullptr) {
            return;
        }

        mWidth = right - left;
        mHeight = bottom - top;
        ARect source{0, 0, mWidth, mHeight};
        ARect dest{left, top, right, bottom};
        ASurfaceTransaction* transaction = ASurfaceTransaction_create();
        ASurfaceTransaction_setGeometry(transaction, mSurface, source, dest,
                                        ANATIVEWINDOW_TRANSFORM_IDENTITY);
        ASurfaceTransaction_apply(transaction);
        ASurfaceTransaction_delete(transaction);
    }

    ~Surface() {
        ASurfaceTransaction* transaction = ASurfaceTransaction_create();
        ASurfaceTransaction_reparent(transaction, mSurface, nullptr);
        ASurfaceTransaction_apply(transaction);
        ASurfaceTransaction_delete(transaction);
        ASurfaceControl_release(mSurface);
    }

    bool isValid() const { return mSurface != nullptr; }
    ASurfaceControl* getSurfaceControl() const { return mSurface; }
    int getWidth() const { return mWidth; }
    int getHeight() const { return mHeight; }

private:
    ASurfaceControl* mSurface = nullptr;
    int mWidth = 0;
    int mHeight = 0;
};

jint nativeWindowSetFrameRate(JNIEnv* env, jclass, jobject jSurface, jfloat frameRate,
                              jint compatibility) {
    ANativeWindow* window = nullptr;
    if (jSurface) {
        window = ANativeWindow_fromSurface(env, jSurface);
    }
    return ANativeWindow_setFrameRate(window, frameRate, compatibility);
}

jlong surfaceControlCreate(JNIEnv* env, jclass, jobject jParentSurface, jstring jName, jint left,
                           jint top, jint right, jint bottom) {
    if (!jParentSurface || !jName) {
        return 0;
    }
    ANativeWindow* parentWindow = ANativeWindow_fromSurface(env, jParentSurface);
    if (!parentWindow) {
        return 0;
    }

    const char* name = env->GetStringUTFChars(jName, nullptr);
    std::string strName = name;
    env->ReleaseStringUTFChars(jName, name);

    Surface* surface = new Surface(parentWindow, strName, left, top, right, bottom);
    if (!surface->isValid()) {
        delete surface;
        return 0;
    }

    return reinterpret_cast<jlong>(surface);
}

void surfaceControlDestroy(JNIEnv*, jclass, jlong surfaceControlLong) {
    if (surfaceControlLong == 0) {
        return;
    }
    delete reinterpret_cast<Surface*>(surfaceControlLong);
}

void surfaceControlSetFrameRate(JNIEnv*, jclass, jlong surfaceControlLong, jfloat frameRate,
                                int compatibility) {
    ASurfaceControl* surfaceControl =
            reinterpret_cast<Surface*>(surfaceControlLong)->getSurfaceControl();
    ASurfaceTransaction* transaction = ASurfaceTransaction_create();
    ASurfaceTransaction_setFrameRate(transaction, surfaceControl, frameRate, compatibility);
    ASurfaceTransaction_apply(transaction);
    ASurfaceTransaction_delete(transaction);
}

void surfaceControlSetVisibility(JNIEnv*, jclass, jlong surfaceControlLong, jboolean visible) {
    ASurfaceControl* surfaceControl =
            reinterpret_cast<Surface*>(surfaceControlLong)->getSurfaceControl();
    ASurfaceTransaction* transaction = ASurfaceTransaction_create();
    ASurfaceTransaction_setVisibility(transaction, surfaceControl,
                                      visible == JNI_TRUE ? ASURFACE_TRANSACTION_VISIBILITY_SHOW
                                                          : ASURFACE_TRANSACTION_VISIBILITY_HIDE);
    ASurfaceTransaction_apply(transaction);
    ASurfaceTransaction_delete(transaction);
}

jboolean surfaceControlPostBuffer(JNIEnv*, jclass, jlong surfaceControlLong, jint argbColor) {
    Surface* surface = reinterpret_cast<Surface*>(surfaceControlLong);
    ASurfaceControl* surfaceControl = surface->getSurfaceControl();
    // Android's Color.* values are represented as ARGB. Convert to RGBA.
    int32_t rgbaColor = 0;
    int8_t* rgbaColorBytes = reinterpret_cast<int8_t*>(&rgbaColor);
    rgbaColorBytes[0] = (argbColor >> 16) & 0xff;
    rgbaColorBytes[1] = (argbColor >> 8) & 0xff;
    rgbaColorBytes[2] = (argbColor >> 0) & 0xff;
    rgbaColorBytes[3] = (argbColor >> 24) & 0xff;

    Buffer buffer(surface->getWidth(), surface->getHeight(), rgbaColor);
    if (!buffer.isValid()) {
        return JNI_FALSE;
    }

    ASurfaceTransaction* transaction = ASurfaceTransaction_create();
    ASurfaceTransaction_setBuffer(transaction, surfaceControl, buffer.getBuffer());
    ASurfaceTransaction_apply(transaction);
    ASurfaceTransaction_delete(transaction);
    return JNI_TRUE;
}

const std::array<JNINativeMethod, 6> JNI_METHODS = {{
        {"nativeWindowSetFrameRate", "(Landroid/view/Surface;FI)I",
         (void*)nativeWindowSetFrameRate},
        {"nativeSurfaceControlCreate", "(Landroid/view/Surface;Ljava/lang/String;IIII)J",
         (void*)surfaceControlCreate},
        {"nativeSurfaceControlDestroy", "(J)V", (void*)surfaceControlDestroy},
        {"nativeSurfaceControlSetFrameRate", "(JFI)V", (void*)surfaceControlSetFrameRate},
        {"nativeSurfaceControlSetVisibility", "(JZ)V", (void*)surfaceControlSetVisibility},
        {"nativeSurfaceControlPostBuffer", "(JI)Z", (void*)surfaceControlPostBuffer},
}};

} // namespace

int register_android_graphics_cts_FrameRateCtsActivity(JNIEnv* env) {
    jclass clazz = env->FindClass("android/graphics/cts/FrameRateCtsActivity");
    return env->RegisterNatives(clazz, JNI_METHODS.data(), JNI_METHODS.size());
}
