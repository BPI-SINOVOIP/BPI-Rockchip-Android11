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

#include <aidl/android/automotive/computepipe/registry/BnClientInfo.h>
#include <binder/IInterface.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <list>
#include <memory>
#include <string>
#include <utility>

#include "FakeRunner.h"
#include "PipeClient.h"
#include "PipeQuery.h"
#include "PipeRunner.h"

using namespace android::automotive::computepipe::router;
using namespace android::automotive::computepipe::router::V1_0::implementation;
using namespace android::automotive::computepipe::tests;
using namespace aidl::android::automotive::computepipe::runner;
using namespace aidl::android::automotive::computepipe::registry;
using namespace ::testing;

/**
 * Fakeclass to instantiate client info for query purposes
 */
class FakeClientInfo : public BnClientInfo {
  public:
    ndk::ScopedAStatus getClientName(std::string* name) override {
        *name = "FakeClient";
        return ndk::ScopedAStatus::ok();
    }
};

/**
 * Class that exposes protected interfaces of PipeRegistry
 * a) Used to retrieve entries without client ref counts
 * b) Used to remove entries
 */
class FakeRegistry : public PipeRegistry<PipeRunner> {
  public:
    std ::unique_ptr<PipeHandle<PipeRunner>> getDebuggerPipeHandle(const std::string& name) {
        return getPipeHandle(name, nullptr);
    }
    Error RemoveEntry(const std::string& name) {
        return DeletePipeHandle(name);
    }
};

/**
 * Test Fixture class that is responsible for maintaining a registry.
 * The registry object is used to test the query interfaces
 */
class PipeQueryTest : public ::testing::Test {
  protected:
    /**
     * Setup for the test fixture to initialize a registry to be used in all
     * tests
     */
    void SetUp() override {
        mRegistry = std::make_shared<FakeRegistry>();
    }
    /**
     * Utility to generate fake runners
     */
    void addFakeRunner(const std::string& name, const std::shared_ptr<IPipeRunner>& runnerIface) {
        std::unique_ptr<PipeHandle<PipeRunner>> handle = std::make_unique<RunnerHandle>(runnerIface);
        Error status = mRegistry->RegisterPipe(std::move(handle), name);
        ASSERT_THAT(status, testing::Eq(Error::OK));
    }
    /**
     * Utility to remove runners from the registry
     */
    void removeRunner(const std::string& name) {
        ASSERT_THAT(mRegistry->RemoveEntry(name), testing::Eq(Error::OK));
    }
    /**
     * Tear down to cleanup registry resources
     */
    void TearDown() override {
        mRegistry = nullptr;
    }
    std::shared_ptr<FakeRegistry> mRegistry;
};

// Check retrieval of inserted entries
TEST_F(PipeQueryTest, GetGraphListTest) {
    std::shared_ptr<IPipeRunner> stub1 = ndk::SharedRefBase::make<FakeRunner>();
    addFakeRunner("stub1", stub1);
    std::shared_ptr<IPipeRunner> stub2 = ndk::SharedRefBase::make<FakeRunner>();
    addFakeRunner("stub2", stub2);

    std::vector<std::string>* outNames = new std::vector<std::string>();
    std::shared_ptr<PipeQuery> qIface = ndk::SharedRefBase::make<PipeQuery>(mRegistry);
    ASSERT_TRUE(qIface->getGraphList(outNames).isOk());

    ASSERT_NE(outNames->size(), 0);
    EXPECT_THAT(std::find(outNames->begin(), outNames->end(), "stub1"),
                testing::Ne(outNames->end()));
    EXPECT_THAT(std::find(outNames->begin(), outNames->end(), "stub2"),
                testing::Ne(outNames->end()));
}

// Check successful retrieval of runner
TEST_F(PipeQueryTest, GetRunnerTest) {
    std::shared_ptr<IPipeRunner> stub1 = ndk::SharedRefBase::make<FakeRunner>();
    addFakeRunner("stub1", stub1);

    std::shared_ptr<PipeQuery> qIface = ndk::SharedRefBase::make<PipeQuery>(mRegistry);
    std::shared_ptr<IClientInfo> info = ndk::SharedRefBase::make<FakeClientInfo>();
    std::shared_ptr<IPipeRunner> runner;
    ASSERT_TRUE(qIface->getPipeRunner("stub1", info, &runner).isOk());
    EXPECT_THAT(runner, testing::NotNull());
}
