/*
 * Copyright 2020, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <getopt.h>
#include <gtest/gtest.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <teeui/example/teeui.h>
#include <unistd.h>

#include <teeui/test/teeui_render_test.h>

#define TeeuiRenderTest_DO_LOG_DEBUG

namespace teeui {

namespace test {

class TeeuiRenderTest : public ::testing::Test {
  public:
    // Default device configuration set to Blueline
    DeviceInfo device_info = {
        1080,         // width in px
        2160,         // height om px
        2.62135,      // dp2px pixel per density independent pixel
        17.42075974,  // mm2px pixel per millimeter (px/mm) ratio
        20.26,        // distance from the top of the power button to the top of the screen in mm>
        30.26,        // distance from the bottom of the power button to the top of the screen in mm
        40.26,  // distance from the top of the UP volume button to the top of the screen in mm
        50.26,  // distance from the bottom of the UP power button to the top of the screen in mm
    };

    static TeeuiRenderTest* Instance() {
        static TeeuiRenderTest* instance = new TeeuiRenderTest;
        return instance;
    }

    void initFromOptions(int argc, char** argv);
    int runTest(const char* language, bool magnified);
    void TestBody() {}
    void createDevice(int widthPx, int heightPx, double dp2px, double mm2px,
                      double powerButtonTopMm, double powerButtonBottomMm, double volUpButtonTopMm,
                      double volUpButtonBottomMm);
};

}  // namespace test

}  // namespace teeui
