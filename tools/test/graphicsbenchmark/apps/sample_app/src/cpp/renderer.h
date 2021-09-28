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

#ifndef ANDROID_GAMECORE_RENDERER_H
#define ANDROID_GAMECORE_RENDERER_H

#include "circle.h"

#include <EGL/egl.h>
#include <vector>

namespace android {
namespace gamecore {

class Renderer {
public:
    struct Egl {
        //struct android_app* app;
        NativeWindowType window;
        EGLDisplay display;
        EGLSurface surface;
        EGLContext context;
        int32_t width;
        int32_t height;
        float left;
        float right;
        float top;
        float bottom;
    };

    struct State {
        int numCircles;
        std::vector<Circle> circles;
        std::vector<Vec2> velocities;
    };

    struct Egl egl;
    struct State state;

    Renderer(int numCircles);
    int initDisplay(NativeWindowType window);
    void terminateDisplay();
    void update();
    void draw();
};

} // end of namespace gamecore
} // end of namespace android

#endif // ANDROID_GAMECORE_RENDERER_H
