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
 *
 */

#include <jni.h>
#include <chrono>
#include <cstdlib>
#include <android/choreographer.h>
#include <android/looper.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <vector>
#include <cmath>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <android/log.h>
#define LOG(...) __android_log_print(ANDROID_LOG_ERROR, "CHOREO-TEST", __VA_ARGS__)

using namespace std;

static ANativeWindow *theWindow = nullptr;

static EGLConfig eglConf;
static EGLSurface eglSurface;
static EGLContext eglCtx;
static EGLDisplay eglDisp;

static bool setupEGL(int w, int h) {
    const EGLint confAttr[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE,   8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE,  8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 16,
            EGL_NONE
    };

    const EGLint ctxAttr[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
    };

    EGLint eglMajVers, eglMinVers;
    EGLint numConfigs;
    EGLint format;

    eglDisp = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisp == EGL_NO_DISPLAY) {
        return false;
    }

    if (!eglInitialize(eglDisp, &eglMajVers, &eglMinVers)) {
        return false;
    }

    if (!eglChooseConfig(eglDisp, confAttr, &eglConf, 1, &numConfigs)) {
        return false;
    }

    if (!eglGetConfigAttrib(eglDisp, eglConf, EGL_NATIVE_VISUAL_ID, &format)) {
        return false;
    }

    ANativeWindow_setBuffersGeometry(theWindow, 0, 0, format);


    eglSurface = eglCreateWindowSurface(eglDisp, eglConf, theWindow, 0);

    if (eglSurface == EGL_NO_SURFACE) {
        return false;
    }

    eglCtx = eglCreateContext(eglDisp, eglConf, EGL_NO_CONTEXT, ctxAttr);

    if (eglCtx == EGL_NO_CONTEXT) {
        return false;
    }

    if (!eglMakeCurrent(eglDisp, eglSurface, eglSurface, eglCtx)) {
        return false;
    }

    glViewport(0, 0, w, h);

    return true;
}

static void shutdownEGL() {
    eglMakeCurrent(eglDisp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(eglDisp, eglCtx);
    eglDestroySurface(eglDisp, eglSurface);
    eglTerminate(eglDisp);

    eglDisp = EGL_NO_DISPLAY;
    eglSurface = EGL_NO_SURFACE;
    eglCtx = EGL_NO_CONTEXT;

    if (theWindow) {
        ANativeWindow_release(theWindow);
        theWindow = nullptr;
    }
}

static void renderFrames() {
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(eglDisp, eglSurface);
}

static volatile bool doRender = false;
static std::vector<unsigned long> frameTimes;
static AChoreographer* choreographer;
static ALooper* looper;

static void frameCallback(long frameTimeNanos, void*) {
    // On 32-bit machine, some frametimes will come out as negative longs
    frameTimes.push_back((unsigned long)frameTimeNanos);
    AChoreographer_postFrameCallback(choreographer, frameCallback, nullptr);
    renderFrames();
    ALooper_wake(looper);
}

extern "C"
JNIEXPORT bool JNICALL
Java_com_android_game_qualification_tests_ChoreoTestActivity_runTheTest(JNIEnv* env, jobject, jobject surface) {
    theWindow = ANativeWindow_fromSurface(env, surface);
    if (!theWindow) {
        return false;
    }
    if (!setupEGL(500,500)) {
        return false;
    }

    looper = ALooper_prepare(0);
    choreographer = reinterpret_cast<AChoreographer*>(AChoreographer_getInstance());

    if (choreographer == nullptr) {
        return false;
    }

    AChoreographer_postFrameCallback(choreographer, frameCallback, nullptr);

    while (doRender && ALooper_pollAll(-1, nullptr, nullptr, nullptr));

    shutdownEGL();

    return true;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_android_game_qualification_tests_ChoreoTestActivity_stopTheTest(JNIEnv*, jobject) {
    doRender = false;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_android_game_qualification_tests_ChoreoTestActivity_startTheTest(JNIEnv*, jobject) {
    doRender = true;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_android_game_qualification_tests_ChoreoTestActivity_getFrameIntervals(JNIEnv* env, jobject) {
    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID arrayListConstructor = env->GetMethodID(arrayListClass, "<init>", "()V");
    jmethodID addMethod = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");

    jclass longClass = env->FindClass("java/lang/Long");
    jmethodID longValueOf = env->GetStaticMethodID(longClass, "valueOf", "(J)Ljava/lang/Long;");

    jobject list = env->NewObject(arrayListClass, arrayListConstructor);

    // Some early frames might have misleading data.
    for (int i = 5; i < frameTimes.size(); ++i) {
        jlong interval = frameTimes[i] - frameTimes[i-1];

        jobject javaInterval = env->CallStaticObjectMethod(longClass, longValueOf, interval);
        env->CallBooleanMethod(list, addMethod, javaInterval);
    }

    return list;
}