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
 */

#ifndef ANDROID_HARDWARE_NEURALNETWORKS_V1_1_VTS_HAL_NEURALNETWORKS_H
#define ANDROID_HARDWARE_NEURALNETWORKS_V1_1_VTS_HAL_NEURALNETWORKS_H

#include <android/hardware/neuralnetworks/1.0/IPreparedModel.h>
#include <android/hardware/neuralnetworks/1.1/IDevice.h>
#include <android/hardware/neuralnetworks/1.1/types.h>
#include <gtest/gtest.h>
#include <vector>
#include "1.0/Utils.h"

namespace android::hardware::neuralnetworks::V1_1::vts::functional {

using NamedDevice = Named<sp<IDevice>>;
using NeuralnetworksHidlTestParam = NamedDevice;

class NeuralnetworksHidlTest : public testing::TestWithParam<NeuralnetworksHidlTestParam> {
  protected:
    void SetUp() override;
    const sp<IDevice> kDevice = getData(GetParam());
};

const std::vector<NamedDevice>& getNamedDevices();

std::string printNeuralnetworksHidlTest(
        const testing::TestParamInfo<NeuralnetworksHidlTestParam>& info);

#define INSTANTIATE_DEVICE_TEST(TestSuite)                                                 \
    INSTANTIATE_TEST_SUITE_P(PerInstance, TestSuite, testing::ValuesIn(getNamedDevices()), \
                             printNeuralnetworksHidlTest)

// Create an IPreparedModel object. If the model cannot be prepared,
// "preparedModel" will be nullptr instead.
void createPreparedModel(const sp<IDevice>& device, const Model& model,
                         sp<V1_0::IPreparedModel>* preparedModel);

}  // namespace android::hardware::neuralnetworks::V1_1::vts::functional

#endif  // ANDROID_HARDWARE_NEURALNETWORKS_V1_1_VTS_HAL_NEURALNETWORKS_H
