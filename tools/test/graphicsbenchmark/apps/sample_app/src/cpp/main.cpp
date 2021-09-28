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

#include "agq.h"
#include "circle.h"
#include "common.h"
#include "renderer.h"
#include "vecmath.h"

#include <cassert>
#include <chrono>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <initializer_list>
#include <jni.h>
#include <memory>
#include <sys/time.h>
#include <vector>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>

using namespace std::chrono_literals;
using namespace android::gamecore;

namespace {

int animating = 0;

/**
 * Process the next input event.
 */
int32_t engine_handle_input(struct android_app*, AInputEvent* event) {
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        animating = 1;
        return 1;
    }
    return 0;
}

/**
 * Process the next main command.
 */
void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    //struct engine* engine = (struct engine*)app->userData;
    Renderer* renderer = reinterpret_cast<Renderer*>(app->userData);

    switch (cmd) {
        case APP_CMD_SAVE_STATE:
            // We are not saving the state.
            break;
        case APP_CMD_INIT_WINDOW:
            // The window is being shown, get it ready.
            if (app->window != NULL) {
                renderer->initDisplay(app->window);
                renderer->draw();
                animating = 1;
            }
            break;
        case APP_CMD_TERM_WINDOW:
            // The window is being hidden or closed, clean it up.
            //engine_term_display(engine);
            renderer->terminateDisplay();
            animating = 0;
            break;
        case APP_CMD_LOST_FOCUS:
            // Also stop animating.
            animating = 0;
            renderer->draw();
            break;
        default:
            break;
    }
}

} // end of anonymous namespace

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app* state) {
    std::srand(0);

    LOGI("Running with SDK %d", state->activity->sdkVersion);

    std::unique_ptr<Renderer> renderer(new Renderer(1));
    state->userData = renderer.get();
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;

    // loop waiting for stuff to do.
    while (1) {
        // Read all pending events.
        int events;
        struct android_poll_source* source;

        // If not animating, we will block forever waiting for events.
        // If animating, we loop until all events are read, then continue
        // to draw the next frame of animation.
        while (ALooper_pollAll(animating ? 0 : -1, NULL, &events, (void**)&source) >= 0) {

            // Process this event.
            if (source != NULL) {
                source->process(state, source);
            }

            // Check if we are exiting.
            if (state->destroyRequested != 0) {
                renderer->terminateDisplay();
                return;
            }
        }

        if (animating) {
            renderer->update();

            // Drawing is throttled to the screen update rate, so there
            // is no need to do timing here.
            renderer->draw();

            // Broadcast intent every 5 seconds.
            static auto last_timestamp = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (now - last_timestamp >= std::chrono::seconds(5)) {
                last_timestamp = now;
                android::GameQualification qualification;
                qualification.startLoop(state->activity);
            }
        }
    }
}
