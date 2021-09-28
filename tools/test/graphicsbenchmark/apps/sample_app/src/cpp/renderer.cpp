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

#include "renderer.h"

#include <vector>
#include <memory>

#include <EGL/eglext.h>
#include <GLES2/gl2.h>


namespace android {
namespace gamecore {

namespace {

const float RADIUS = 0.1f;

void printGLString(const char *name, GLenum s) {
    const char *v = (const char *) glGetString(s);
    LOGI("GL %s = %s\n", name, v);
}

} // end of anonymous namespace

Renderer::Renderer(int numCircles) {
    memset(&egl, 0, sizeof(egl));
    state.numCircles = numCircles;
}

int Renderer::initDisplay(NativeWindowType window) {
    egl.window = window;

    // initialize OpenGL ES and EGL

    /*
     * Here specify the attributes of the desired configuration.
     * Below, we select an EGLConfig with at least 8 bits per color
     * component compatible with on-screen windows
     */
    const EGLint attribs[] = {
            EGL_RENDERABLE_TYPE,
            EGL_OPENGL_ES2_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_NONE
    };
    EGLint w;
    EGLint h;
    EGLint format;
    EGLint numConfigs;
    EGLConfig config;
    EGLSurface surface;
    EGLContext context;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    eglInitialize(display, 0, 0);

    /* Here, the application chooses the configuration it desires.
     * find the best match if possible, otherwise use the very first one
     */
    eglChooseConfig(display, attribs, nullptr,0, &numConfigs);
    std::unique_ptr<EGLConfig[]> supportedConfigs(new EGLConfig[numConfigs]);
    assert(supportedConfigs);
    eglChooseConfig(display, attribs, supportedConfigs.get(), numConfigs, &numConfigs);
    assert(numConfigs);
    auto i = 0;
    for (; i < numConfigs; i++) {
        auto& cfg = supportedConfigs[i];
        EGLint r, g, b, d;
        if (eglGetConfigAttrib(display, cfg, EGL_RED_SIZE, &r)   &&
            eglGetConfigAttrib(display, cfg, EGL_GREEN_SIZE, &g) &&
            eglGetConfigAttrib(display, cfg, EGL_BLUE_SIZE, &b)  &&
            eglGetConfigAttrib(display, cfg, EGL_DEPTH_SIZE, &d) &&
            r == 8 && g == 8 && b == 8 && d == 0 ) {

            config = supportedConfigs[i];
            break;
        }
    }
    if (i == numConfigs) {
        config = supportedConfigs[0];
    }

    EGLint attrib_list[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
    };

    /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
     * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
     * As soon as we picked a EGLConfig, we can safely reconfigure the
     * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    surface = eglCreateWindowSurface(display, config, window, NULL);
    context = eglCreateContext(display, config, NULL, attrib_list);

    // Enable Android timing.
    eglSurfaceAttrib(display, surface, EGL_TIMESTAMPS_ANDROID, EGL_TRUE);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        LOGW("Unable to eglMakeCurrent");
        return -1;
    }

    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);

    egl.display = display;
    egl.context = context;
    egl.surface = surface;
    egl.width = w;
    egl.height = h;
    float ratio = float(w) / h;
    egl.left = -ratio;
    egl.right = ratio;
    egl.top = 1.0f;
    egl.bottom = -1.0f;

    // Check openGL on the system
    auto opengl_info = {GL_VENDOR, GL_RENDERER, GL_VERSION, GL_EXTENSIONS};
    for (auto name : opengl_info) {
        auto info = glGetString(name);
        LOGI("OpenGL Info: %s", info);
    }
    printGLString("Version", GL_VERSION);
    printGLString("Vendor", GL_VENDOR);
    printGLString("Renderer", GL_RENDERER);
    printGLString("Extensions", GL_EXTENSIONS);

    // Initialize GL state.
    glEnable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    // Initialize world state.
    state.circles.resize(state.numCircles, Circle(RADIUS));
    state.velocities.resize(state.numCircles);
    for (auto& v : state.velocities) {
        v = Vec2(
                0.05f * (float(rand()) / RAND_MAX - 0.5f),
                0.05f * (float(rand()) / RAND_MAX - 0.5f));
    }

    return 0;
}

void Renderer::terminateDisplay() {
    if (egl.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (egl.context != EGL_NO_CONTEXT) {
            eglDestroyContext(egl.display, egl.context);
        }
        if (egl.surface != EGL_NO_SURFACE) {
            eglDestroySurface(egl.display, egl.surface);
        }
        eglTerminate(egl.display);
    }
    egl.display = EGL_NO_DISPLAY;
    egl.context = EGL_NO_CONTEXT;
    egl.surface = EGL_NO_SURFACE;
}

void Renderer::update() {
    // Done with events; draw next animation frame.
    for (int i = 0; i < state.circles.size(); ++i) {
        auto& circle = state.circles[i];
        Vec2& v = state.velocities[i];
        circle.setPosition(circle.getPosition() + Vec3(v, 0.0f));
        Vec3 position = circle.getPosition();

        float x;
        float y;
        float z;
        position.Value(x, y, z);
        float vx;
        float vy;
        v.Value(vx, vy);
        if (x + RADIUS >= egl.right || x - RADIUS <= egl.left) {
            vx = -vx;
        }
        if (y + RADIUS >= egl.top || y - RADIUS <= egl.bottom) {
            vy = -vy;
        }
        v = Vec2(vx, vy);
    }
}

void Renderer::draw() {
    if (egl.display == NULL) {
        // No display.
        return;
    }

    // Just fill the screen with a color.
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    float ratio = float(egl.width) / egl.height;
    Mat4 projectionMatrix = Mat4::Ortho2D(-ratio, 1.0f, ratio, -1.0f);
    Mat4 viewMatrix =
            Mat4::LookAt(
                Vec3(0.0f, 0.0f, -1.0f),
                Vec3(0.0f, 0.0f, 1.0f),
                Vec3(0.0f, 1.0f, 0.0f));

    for (int i = 0; i < state.circles.size(); ++i) {
        auto& circle = state.circles[i];
        circle.updateViewProjection(projectionMatrix * viewMatrix);
        circle.draw();
    }
    eglSwapBuffers(egl.display, egl.surface);
}

} // end of namespace gamecore
} // end of namespace android


