/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <stdio.h>
#include <utils/SystemClock.h>
#include <string>
#include <log/log.h>

#include <DisplayUseCase.h>
#include <AnalyzeUseCase.h>
#include <Utils.h>

using namespace ::android::automotive::evs::support;

class SimpleRenderCallback : public BaseRenderCallback {
    void render(const Frame& inputFrame, const Frame& outputFrame) {
        ALOGI("SimpleRenderCallback::render");

        if (inputFrame.data == nullptr || outputFrame.data == nullptr) {
            ALOGE("Invalid frame data was passed to render callback");
            return;
        }

        // TODO(b/130246434): Use OpenCV to implement a more meaningful
        // callback.
        // Swap the RGB channels.
        int stride = inputFrame.stride;
        uint8_t* inDataPtr = inputFrame.data;
        uint8_t* outDataPtr = outputFrame.data;
        for (int i = 0; i < inputFrame.width; i++)
            for (int j = 0; j < inputFrame.height; j++) {
                outDataPtr[(i + j * stride) * 4 + 0] =
                    inDataPtr[(i + j * stride) * 4 + 1];
                outDataPtr[(i + j * stride) * 4 + 1] =
                    inDataPtr[(i + j * stride) * 4 + 2];
                outDataPtr[(i + j * stride) * 4 + 2] =
                    inDataPtr[(i + j * stride) * 4 + 0];
                outDataPtr[(i + j * stride) * 4 + 3] =
                    inDataPtr[(i + j * stride) * 4 + 3];
            }
    }
};

class SimpleAnalyzeCallback : public BaseAnalyzeCallback {
    void analyze(const Frame &frame) {
        ALOGD("SimpleAnalyzeCallback::analyze");
        if (frame.data == nullptr) {
            ALOGE("Invalid frame data was passed to analyze callback");
            return;
        }

        // TODO(b/130246434): Now we just put a one second delay as a place
        // holder. Replace it with an actual complicated enough algorithm.

        ALOGD("SimpleAnalyzerCallback: sleep for one second");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    };
};

// Main entry point
int main() {
    ALOGI("EVS app starting\n");

    // Get the default rear view camera from evs support lib
    std::string cameraId = Utils::getDefaultRearViewCameraId();
    if (cameraId.empty()) {
        ALOGE("Cannot find a valid camera");
        return EXIT_FAILURE;
    }

    DisplayUseCase displayUseCase =
        DisplayUseCase::createDefaultUseCase(cameraId, new SimpleRenderCallback());

    AnalyzeUseCase analyzeUseCase =
        AnalyzeUseCase::createDefaultUseCase(cameraId, new SimpleAnalyzeCallback());

    // Run both DisplayUseCase and AnalyzeUseCase together for 10 seconds.
    if (displayUseCase.startVideoStream()
        && analyzeUseCase.startVideoStream()) {

        std::this_thread::sleep_for(std::chrono::seconds(10));

        displayUseCase.stopVideoStream();
        analyzeUseCase.stopVideoStream();
    }

    // Run only AnalyzeUseCase for 10 seconds. The display control is back to
    // Android framework but the camera is still occupied by AnalyzeUseCase in
    // the background.
    if (analyzeUseCase.startVideoStream()) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        analyzeUseCase.stopVideoStream();
    }

    return 0;
}
