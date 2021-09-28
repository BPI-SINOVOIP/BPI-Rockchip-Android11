/*
 * Copyright 2019 The Android Open Source Project
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdio.h>

#include <list>
#include <memory>
#include <string>
#include <utility>

#include "FakeRunner.h"
#include "PipeRegistration.h"
#include "PipeRunner.h"

using namespace android::automotive::computepipe::router;
using namespace android::automotive::computepipe::router::V1_0::implementation;
using namespace android::automotive::computepipe::tests;
using namespace aidl::android::automotive::computepipe::runner;
using namespace aidl::android::automotive::computepipe::registry;
/**
 * Test fixture that manages the underlying registry creation and tear down
 */
class PipeRegistrationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        mRegistry = std::make_shared<PipeRegistry<PipeRunner>>();
        ASSERT_THAT(mRegistry, testing::NotNull());
    }

    void TearDown() override {
        mRegistry = nullptr;
    }
    std::shared_ptr<PipeRegistry<PipeRunner>> mRegistry;
};

// Valid registration succeeds
TEST_F(PipeRegistrationTest, RegisterFakeRunner) {
    std::shared_ptr<IPipeRunner> fake = ndk::SharedRefBase::make<FakeRunner>();
    std::shared_ptr<IPipeRegistration> rIface =
        ndk::SharedRefBase::make<PipeRegistration>(this->mRegistry);
    EXPECT_TRUE(rIface->registerPipeRunner("fake", fake).isOk());
}

// Duplicate registration fails
TEST_F(PipeRegistrationTest, RegisterDuplicateRunner) {
    std::shared_ptr<IPipeRunner> fake = ndk::SharedRefBase::make<FakeRunner>();
    std::shared_ptr<IPipeRegistration> rIface =
        ndk::SharedRefBase::make<PipeRegistration>(this->mRegistry);
    ASSERT_TRUE(rIface->registerPipeRunner("fake", fake).isOk());
    EXPECT_FALSE(rIface->registerPipeRunner("fake", fake).isOk());
}
