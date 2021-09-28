/*
 *
 * Copyright 2019, The Android Open Source Project
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

#ifndef TEEUI_LIBTEEUI_INCLUDE_TEEUI_H_
#define TEEUI_LIBTEEUI_INCLUDE_TEEUI_H_

#include <stddef.h>
#include <stdint.h>

struct DeviceInfo {
    uint32_t width_;
    uint32_t height_;
    double dp2px_;
    double mm2px_;
    double powerButtonTopMm_;
    double powerButtonBottomMm_;
    double volUpButtonTopMm_;
    double volUpButtonBottomMm_;
};

uint32_t setDeviceInfo(DeviceInfo device_info, bool magnified, bool inverted = false);
uint32_t renderUIIntoBuffer(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t lineStride,
                            uint32_t* buffer, size_t buffer_size_in_elements_not_bytes);

void selectLanguage(const char* language_id);

void setConfirmationMessage(const char* confirmationMessage);

#endif  // TEEUI_LIBTEEUI_INCLUDE_TEEUI_H_
