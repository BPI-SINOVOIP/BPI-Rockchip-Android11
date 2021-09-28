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
 *
 */

#include <renderer.h>
#include <common.h>
#include <jni.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <android/native_window_jni.h>

#include <vector>
#include <deque>

namespace {

using eglGetNextFrameIdANDROID_type = EGLBoolean (*)(EGLDisplay, EGLSurface, EGLuint64KHR *);
using eglGetFrameTimestampsANDROID_type = EGLBoolean (*)(EGLDisplay, EGLSurface, EGLuint64KHR, EGLint, const EGLint *, EGLnsecsANDROID *);

eglGetNextFrameIdANDROID_type eglGetNextFrameIdANDROID = nullptr;
eglGetFrameTimestampsANDROID_type eglGetFrameTimestampsANDROID = nullptr;

const int MAX_FRAMES = 240;
android::gamecore::Renderer* renderer = nullptr;
std::deque<EGLnsecsANDROID> frameReadyTime;
std::deque<EGLnsecsANDROID> latchTime;
std::deque<EGLuint64KHR> frameIds;

} // end of anonymous namespace

extern "C"
JNIEXPORT void JNICALL
Java_com_android_game_qualification_tests_SurfaceFlingerTestActivity_initDisplay(JNIEnv* env, jobject, jobject surface) {
    eglGetNextFrameIdANDROID =
            reinterpret_cast<eglGetNextFrameIdANDROID_type>(
                    eglGetProcAddress("eglGetNextFrameIdANDROID"));
    if (eglGetNextFrameIdANDROID == nullptr) {
        LOGE("Failed to load eglGetNextFrameIdANDROID");
        return;
    }
    eglGetFrameTimestampsANDROID =
            reinterpret_cast<eglGetFrameTimestampsANDROID_type>(
                    eglGetProcAddress("eglGetFrameTimestampsANDROID"));
    if (eglGetNextFrameIdANDROID == nullptr) {
        LOGE("Failed to load eglGetFrameTimestampsANDROID");
        return;
    }

    if (renderer == nullptr) {
        // Create a renderer that draws many circles to overload the GPU.
        renderer = new android::gamecore::Renderer(1500);
    }
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    renderer->initDisplay(window);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_android_game_qualification_tests_SurfaceFlingerTestActivity_drawFrame(JNIEnv*, jobject) {
    if (eglGetNextFrameIdANDROID == nullptr || eglGetFrameTimestampsANDROID == nullptr) {
        return;
    }
    EGLuint64KHR frameId;
    EGLBoolean rc = eglGetNextFrameIdANDROID(renderer->egl.display, renderer->egl.surface, &frameId);
    if (rc == EGL_TRUE) {
        frameIds.push_back(frameId);
    }
    renderer->update();
    renderer->draw();

    static const EGLint timestamps[] = {
            EGL_RENDERING_COMPLETE_TIME_ANDROID,
            EGL_COMPOSITION_LATCH_TIME_ANDROID};

    while(!frameIds.empty()) {
        EGLuint64KHR fid = *frameIds.begin();
        EGLnsecsANDROID values[2];
        rc = eglGetFrameTimestampsANDROID(
                renderer->egl.display, renderer->egl.surface, fid, 2, timestamps, values);

        // Timestamps pending, will try again next frame.
        if (values[0] == EGL_TIMESTAMP_PENDING_ANDROID
                || values[1] == EGL_TIMESTAMP_PENDING_ANDROID) {
            break;
        }
        frameIds.pop_front();
        if (rc == EGL_TRUE) {
            if (frameReadyTime.size() == MAX_FRAMES) {
                frameReadyTime.pop_front();
                latchTime.pop_front();
            }
            frameReadyTime.push_back(values[0]);
            latchTime.push_back(values[1]);
        } else {
            LOGE("Unable to retrieve frame data for frame %" PRIu64, fid);
        }
    }
}


/**
 * Return the oldest available frame data.
 */
extern "C"
JNIEXPORT jlongArray JNICALL
Java_com_android_game_qualification_tests_SurfaceFlingerTestActivity_getFrameData(JNIEnv* env, jobject) {
    if (frameReadyTime.empty()) {
        return nullptr;
    }
    jlong buffer[] = { *frameReadyTime.begin(), *latchTime.begin() };
    jlongArray result = env->NewLongArray(2);
    env->SetLongArrayRegion(result, 0, 2, buffer);
    frameReadyTime.pop_front();
    latchTime.pop_front();
    return result;
}


