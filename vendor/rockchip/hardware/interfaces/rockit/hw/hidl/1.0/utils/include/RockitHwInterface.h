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
#ifndef ANDROID_ROCKIT_HW_INTERFACE_H
#define ANDROID_ROCKIT_HW_INTERFACE_H


#include <pthread.h>

using ::rockchip::hardware::rockit::hw::V1_0::RockitHWType;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWBuffer;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWBufferList;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWCtrCmd;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWParamPair;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWParamPairs;

namespace rockchip {
namespace hardware {
namespace rockit {
namespace hw {
namespace V1_0 {
namespace utils {

#define KEY_NO_VALUES (0)

enum HWQueryKey {
    KEY_HW_QUERY_UNKNOWN = 0,
    KEY_HW_QUERY_WIDTH_STRIDE,
    KEY_HW_QUERY_HEIGHT_STRIDE,

    KEY_HW_QUERY_MAX,
};

enum HWStatus {
    HW_STATUS_IDLE = 0,
    HW_STATUS_INIT,
    HW_STATUS_START,
    HW_STATUS_PAUSE,
    HW_STATUS_STOP,
    HW_STATUS_EXIT,
};

class RockitHwInterface {
public:
    RockitHwInterface();
    virtual ~RockitHwInterface();

    virtual int init(const RockitHWParamPairs& pairs) = 0;
    virtual int commitBuffer(const RockitHWBuffer& list) = 0;
    virtual int giveBackBuffer(const RockitHWBuffer& list) = 0;
    virtual int process(const RockitHWBufferList& list) = 0;
    virtual int enqueue(const RockitHWBuffer& buffer) = 0;
    virtual int dequeue(RockitHWBuffer& buffer) = 0;
    virtual int control(int cmd, const RockitHWParamPairs& param) = 0;
    virtual int query(int cmd, RockitHWParamPairs& out) = 0;
    virtual int flush() = 0;
    virtual int reset() = 0;

    virtual uint64_t getValue(const RockitHWParamPairs& pairs, uint32_t key);
    virtual void setValue(RockitHWParamPairs& pairs, uint32_t key, uint64_t value);
};

}  // namespace utils
}  // namespace V1_0
}  // namespace hw
}  // namespace rockit
}  // namespace hardware
}  // namespace rockchip

#endif  //  ANDROID_ROCKIT_HW_INTERFACE_H
