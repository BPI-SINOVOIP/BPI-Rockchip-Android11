/*
 * Copyright 2020 The Android Open Source Project
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

#define LOG_TAG "VhalHandlerTests"

#include <android/hardware/automotive/vehicle/2.0/IVehicle.h>
#include "VhalHandler.h"

#include <gtest/gtest.h>
#include <time.h>

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {
namespace {

using vehicle::V2_0::VehicleArea;
using vehicle::V2_0::VehicleProperty;

void SetSamplePropertiesToRead(VhalHandler* vhalHandler) {
    std::vector<vehicle::V2_0::VehiclePropValue> propertiesToRead;
    vehicle::V2_0::VehiclePropValue propertyRead;
    propertyRead.prop = static_cast<int32_t>(VehicleProperty::INFO_MAKE);
    propertiesToRead.push_back(propertyRead);
    ASSERT_TRUE(vhalHandler->setPropertiesToRead(propertiesToRead));
}

void SetSamplePropertiesToReadInt64(VhalHandler* vhalHandler) {
    std::vector<uint64_t> propertiesToRead;
    uint64_t propertyInt64 = (static_cast<uint64_t>(VehicleProperty::INFO_MAKE) << 32) |
            static_cast<uint64_t>(VehicleArea::GLOBAL);
    propertiesToRead.push_back(propertyInt64);
    ASSERT_TRUE(vhalHandler->setPropertiesToRead(propertiesToRead));
}

TEST(VhalhandlerTests, UninitializedStartFail) {
    VhalHandler vhalHandler;
    ASSERT_FALSE(vhalHandler.startPropertiesUpdate());
}

TEST(VhalhandlerTests, StartStopSuccess) {
    VhalHandler vhalHandler;
    ASSERT_TRUE(vhalHandler.initialize(VhalHandler::UpdateMethod::GET, 10));
    SetSamplePropertiesToRead(&vhalHandler);
    ASSERT_TRUE(vhalHandler.startPropertiesUpdate());
    ASSERT_TRUE(vhalHandler.stopPropertiesUpdate());
}

TEST(VhalhandlerTests, StopTwiceFail) {
    VhalHandler vhalHandler;
    ASSERT_TRUE(vhalHandler.initialize(VhalHandler::UpdateMethod::GET, 10));
    SetSamplePropertiesToRead(&vhalHandler);
    ASSERT_TRUE(vhalHandler.startPropertiesUpdate());
    ASSERT_TRUE(vhalHandler.stopPropertiesUpdate());
    ASSERT_FALSE(vhalHandler.stopPropertiesUpdate());
}

TEST(VhalhandlerTests, NoStartFail) {
    VhalHandler vhalHandler;
    ASSERT_TRUE(vhalHandler.initialize(VhalHandler::UpdateMethod::GET, 10));
    SetSamplePropertiesToRead(&vhalHandler);
    ASSERT_FALSE(vhalHandler.stopPropertiesUpdate());
}

TEST(VhalhandlerTests, StartAgainSuccess) {
    VhalHandler vhalHandler;
    ASSERT_TRUE(vhalHandler.initialize(VhalHandler::UpdateMethod::GET, 10));
    SetSamplePropertiesToRead(&vhalHandler);
    ASSERT_TRUE(vhalHandler.startPropertiesUpdate());
    ASSERT_TRUE(vhalHandler.stopPropertiesUpdate());
    ASSERT_TRUE(vhalHandler.startPropertiesUpdate());
    ASSERT_TRUE(vhalHandler.stopPropertiesUpdate());
}

TEST(VhalhandlerTests, GetMethodSuccess) {
    VhalHandler vhalHandler;
    ASSERT_TRUE(vhalHandler.initialize(VhalHandler::UpdateMethod::GET, 10));

    SetSamplePropertiesToRead(&vhalHandler);

    ASSERT_TRUE(vhalHandler.startPropertiesUpdate());
    sleep(1);
    std::vector<vehicle::V2_0::VehiclePropValue> propertyValues;
    EXPECT_TRUE(vhalHandler.getPropertyValues(&propertyValues));
    EXPECT_EQ(propertyValues.size(), 1);

    EXPECT_TRUE(vhalHandler.stopPropertiesUpdate());
}

TEST(VhalhandlerTests, GetMethodInt64Success) {
    VhalHandler vhalHandler;
    ASSERT_TRUE(vhalHandler.initialize(VhalHandler::UpdateMethod::GET, 10));

    SetSamplePropertiesToReadInt64(&vhalHandler);

    ASSERT_TRUE(vhalHandler.startPropertiesUpdate());
    sleep(1);
    std::vector<vehicle::V2_0::VehiclePropValue> propertyValues;
    EXPECT_TRUE(vhalHandler.getPropertyValues(&propertyValues));
    EXPECT_EQ(propertyValues.size(), 1);

    EXPECT_TRUE(vhalHandler.stopPropertiesUpdate());
}

}  // namespace
}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
