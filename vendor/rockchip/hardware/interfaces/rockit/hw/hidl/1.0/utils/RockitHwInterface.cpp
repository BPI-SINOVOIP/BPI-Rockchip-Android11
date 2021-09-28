/*
 * Copyright (C) 2019 Rockchip Electronics Co. LTD
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
 //#define LOG_NDEBUG 0
#define LOG_TAG "RockitHwInterface"

#include <dlfcn.h>
#include "RTUtils.h"
#include <utils/Log.h>
#include "include/RockitHwInterface.h"


using ::rockchip::hardware::rockit::hw::V1_0::RockitHWParamPair;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWParamPairs;

namespace rockchip {
namespace hardware {
namespace rockit {
namespace hw {
namespace V1_0 {
namespace utils {

RockitHwInterface::RockitHwInterface() {
}

RockitHwInterface::~RockitHwInterface() {
}

uint64_t RockitHwInterface::getValue(const RockitHWParamPairs& param, uint32_t key) {
    uint64_t  value = (uint64_t)KEY_NO_VALUES;
    for (int i = 0; i < param.counter; i ++) {
        if (param.pairs[i].key == key) {
            value = param.pairs[i].value;
            break;
        }
    }

    return value;
}

void RockitHwInterface::setValue(RockitHWParamPairs& param, uint32_t key, uint64_t value) {
    int size = param.counter;
    param.counter ++;
    param.pairs[size].key = key;
    param.pairs[size].value = value;
}

}  // namespace utils
}  // namespace V1_0
}  // namespace hw
}  // namespace rockit
}  // namespace hardware
}  // namespace rockchip