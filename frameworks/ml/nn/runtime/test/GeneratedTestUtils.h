/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef ANDROID_FRAMEWORKS_ML_NN_RUNTIME_TEST_GENERATED_TEST_UTILS_H
#define ANDROID_FRAMEWORKS_ML_NN_RUNTIME_TEST_GENERATED_TEST_UTILS_H

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "TestHarness.h"
#include "TestNeuralNetworksWrapper.h"

namespace android::nn::generated_tests {

class GeneratedTestBase
    : public ::testing::TestWithParam<test_helper::TestModelManager::TestParam> {
   protected:
    const std::string& kTestName = GetParam().first;
    const test_helper::TestModel& testModel = *GetParam().second;
};

#define INSTANTIATE_GENERATED_TEST(TestSuite, filter)                                          \
    INSTANTIATE_TEST_SUITE_P(                                                                  \
            TestGenerated, TestSuite,                                                          \
            ::testing::ValuesIn(::test_helper::TestModelManager::get().getTestModels(filter)), \
            [](const auto& info) { return info.param.first; })

// A generated NDK model.
class GeneratedModel : public test_wrapper::Model {
   public:
    // A helper method to simplify referenced model lifetime management.
    //
    // Usage:
    //     GeneratedModel model;
    //     std::vector<Model> refModels;
    //     createModel(&model, &refModels);
    //     model.setRefModels(std::move(refModels));
    //
    // This makes sure referenced models live as long as the main model.
    //
    void setRefModels(std::vector<test_wrapper::Model> refModels) {
        mRefModels = std::move(refModels);
    }

    // A helper method to simplify CONSTANT_REFERENCE memory lifetime management.
    void setConstantReferenceMemory(std::unique_ptr<test_wrapper::Memory> memory) {
        mConstantReferenceMemory = std::move(memory);
    }

   private:
    std::vector<test_wrapper::Model> mRefModels;
    std::unique_ptr<test_wrapper::Memory> mConstantReferenceMemory;
};

// Convert TestModel to NDK model.
void createModel(const test_helper::TestModel& testModel, bool testDynamicOutputShape,
                 GeneratedModel* model);
inline void createModel(const test_helper::TestModel& testModel, GeneratedModel* model) {
    createModel(testModel, /*testDynamicOutputShape=*/false, model);
}

void createRequest(const test_helper::TestModel& testModel, test_wrapper::Execution* execution,
                   std::vector<test_helper::TestBuffer>* outputs);

}  // namespace android::nn::generated_tests

#endif  // ANDROID_FRAMEWORKS_ML_NN_RUNTIME_TEST_GENERATED_TEST_UTILS_H
