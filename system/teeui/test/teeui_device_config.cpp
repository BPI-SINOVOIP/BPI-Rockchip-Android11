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

#include <getopt.h>
#include <gtest/gtest.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <teeui/example/teeui.h>
#include <unistd.h>

#include "teeui_device_config.h"
#include <teeui/test/teeui_render_test.h>

#define TeeuiRenderTest_DO_LOG_DEBUG

namespace teeui {

namespace test {

void initRenderTest(int argc, char** argv) {
    ::teeui::test::TeeuiRenderTest::Instance()->initFromOptions(argc, argv);
}

int runRenderTest(const char* language, bool magnified) {
    DeviceInfo* device_info_ptr = &TeeuiRenderTest::Instance()->device_info;
    selectLanguage(language);
    setDeviceInfo(*device_info_ptr, magnified);
    uint32_t w = device_info_ptr->width_;
    uint32_t h = device_info_ptr->height_;
    uint32_t linestride = w;
    uint32_t buffer_size = h * linestride;
    std::vector<uint32_t> buffer(buffer_size);
    int error = renderUIIntoBuffer(0, 0, w, h, linestride, buffer.data(), buffer_size);
    return error;
}

/*
 * Configures device with test parameters
 * widthPx, heightPx : pixel dimension of devices
 * dp2px : density pixel to pixel
 * mm2px : millimeter to pixel
 * powerButtonTopMm : location of the top of the power button in mm
 * powerButtonBottomMm : location of the bottom of the power button in mm
 * volUpButtonTopMm : location of the top of the up volume button in mm
 * volUpButtonBottomMm : location of the bottom of the up power button in mm
 */
void TeeuiRenderTest::createDevice(int widthPx, int heightPx, double dp2px, double mm2px,
                                   double powerButtonTopMm, double powerButtonBottomMm,
                                   double volUpButtonTopMm, double volUpButtonBottomMm) {
    DeviceInfo* device_info_ptr = &TeeuiRenderTest::Instance()->device_info;
    device_info_ptr->width_ = widthPx;
    device_info_ptr->height_ = heightPx;
    device_info_ptr->dp2px_ = dp2px;
    device_info_ptr->mm2px_ = mm2px;
    device_info_ptr->powerButtonTopMm_ = powerButtonTopMm;
    device_info_ptr->powerButtonBottomMm_ = powerButtonBottomMm;
    device_info_ptr->volUpButtonTopMm_ = volUpButtonTopMm;
    device_info_ptr->volUpButtonBottomMm_ = volUpButtonBottomMm;
}

void TeeuiRenderTest::initFromOptions(int argc, char** argv) {

    uint width = 0, height = 0;
    double dp2px = 0, mm2px = 0;
    double powerBottonTopMm = 0, powerButtonBottomMm = 0;
    double volUpButtonTopMm = 0, volUpButtonBottomMm = 0;

    int option_index = 0;
    static struct option options[] = {{"width", required_argument, 0, 'w'},
                                      {"height", required_argument, 0, 'l'},
                                      {"dp2px", required_argument, 0, 'd'},
                                      {"mm2px", required_argument, 0, 'm'},
                                      {"powerButtonTop", required_argument, 0, 't'},
                                      {"powerButtonBottom", required_argument, 0, 'b'},
                                      {"volUpButtonTop", required_argument, 0, 'u'},
                                      {"volUpButtonBottom", required_argument, 0, 'v'},
                                      {"help", 0, 0, 'h'},
                                      {"?", 0, 0, '?'},
                                      {0, 0, 0, 0}};
    while (true) {
        int c = getopt_long(argc, argv, "w:l:d:m:t:b:u:v:h?", options, &option_index);
        if (c == -1) break;
        double numeric_value = 0;
        switch (c) {
        case 'w':
            width = atoi(optarg);
            break;
        case 'l':
            height = atoi(optarg);
            break;
        case 'd':
            numeric_value = strtod(optarg, NULL);
            dp2px = numeric_value;
            break;
        case 'm':
            numeric_value = strtod(optarg, NULL);
            mm2px = numeric_value;
            break;
        case 't':
            numeric_value = strtod(optarg, NULL);
            powerBottonTopMm = numeric_value;
            break;
        case 'b':
            numeric_value = strtod(optarg, NULL);
            powerButtonBottomMm = numeric_value;
            break;
        case 'u':
            numeric_value = strtod(optarg, NULL);
            volUpButtonTopMm = numeric_value;
            break;
        case 'v':
            numeric_value = strtod(optarg, NULL);
            volUpButtonBottomMm = numeric_value;
            break;
        case '?':
        case 'h':
            std::cout << "Options:" << std::endl;
            std::cout << "--width=<device width in pixels>" << std::endl;
            std::cout << "--height=<device height in pixels>" << std::endl;
            std::cout << "--dp2px=<pixel per density independent pixel (px/dp) ratio of the "
                         "device. Typically <width in pixels>/412 >"
                      << std::endl;
            std::cout << "--mm2px=<pixel per millimeter (px/mm) ratio>" << std::endl;
            std::cout << "--powerButtonTop=<distance from the top of the power button to the top "
                         "of the screen in mm>"
                      << std::endl;
            std::cout << "--powerButtonBottom=<distance from the bottom of the power button to the "
                         "top of the screen in mm>"
                      << std::endl;
            std::cout << "--volUpButtonTop=<distance from the top of the UP volume button to the "
                         "top of the screen in mm>"
                      << std::endl;
            std::cout << "--volUpButtonBottom=<distance from the bottom of the UP power button to "
                         "the top of the screen in mm>"
                      << std::endl;
            exit(0);
        }
    }
    createDevice(width, height, dp2px, mm2px, powerBottonTopMm, powerButtonBottomMm,
                 volUpButtonTopMm, volUpButtonBottomMm);
}

}  // namespace test

}  // namespace teeui
