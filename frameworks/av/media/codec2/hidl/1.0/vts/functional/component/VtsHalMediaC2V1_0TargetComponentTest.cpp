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

//#define LOG_NDEBUG 0
#define LOG_TAG "codec2_hidl_hal_component_test"

#include <android-base/logging.h>
#include <gtest/gtest.h>
#include <hidl/GtestPrinter.h>

#include <C2Config.h>
#include <codec2/hidl/client.h>

#include "media_c2_hidl_test_common.h"

/* Time_Out for start(), stop(), reset(), release(), flush(), queue() are
 * defined in hardware/interfaces/media/c2/1.0/IComponent.hal. Adding 50ms
 * extra in case of timeout is 500ms, 1ms extra in case timeout is 1ms/5ms. All
 * timeout is calculated in us.
 */
#define START_TIME_OUT 550000
#define STOP_TIME_OUT 550000
#define RESET_TIME_OUT 550000
#define RELEASE_TIME_OUT 550000
#define FLUSH_TIME_OUT 6000
#define QUEUE_TIME_OUT 2000

// Time_Out for config(), query(), querySupportedParams() are defined in
// hardware/interfaces/media/c2/1.0/IConfigurable.hal.
#define CONFIG_TIME_OUT 6000
#define QUERY_TIME_OUT 6000
#define QUERYSUPPORTEDPARAMS_TIME_OUT 2000

#define CHECK_TIMEOUT(timeConsumed, TIME_OUT, FuncName) \
    if (timeConsumed > TIME_OUT) {                      \
        ALOGW("TIMED_OUT %s  timeConsumed=%" PRId64     \
              " us is "                                 \
              "greater than threshold %d us",           \
              FuncName, timeConsumed, TIME_OUT);        \
    }

namespace {

static std::vector<std::tuple<std::string, std::string, std::string, std::string>>
        kInputTestParameters;

// google.codec2 Component test setup
class Codec2ComponentHidlTestBase : public ::testing::Test {
  public:
    virtual void SetUp() override {
        getParams();
        mEos = false;
        mClient = android::Codec2Client::CreateFromService(mInstanceName.c_str());
        ASSERT_NE(mClient, nullptr);
        mListener.reset(new CodecListener([this](std::list<std::unique_ptr<C2Work>>& workItems) {
            handleWorkDone(workItems);
        }));
        ASSERT_NE(mListener, nullptr);
        mClient->createComponent(mComponentName.c_str(), mListener, &mComponent);
        ASSERT_NE(mComponent, nullptr);
        for (int i = 0; i < MAX_INPUT_BUFFERS; ++i) {
            mWorkQueue.emplace_back(new C2Work);
        }
    }

    virtual void TearDown() override {
        if (mComponent != nullptr) {
            // If you have encountered a fatal failure, it is possible that
            // freeNode() will not go through. Instead of hanging the app.
            // let it pass through and report errors
            if (::testing::Test::HasFatalFailure()) return;
            mComponent->release();
            mComponent = nullptr;
        }
    }

    // Get the test parameters from GetParam call.
    virtual void getParams() {}

    // callback function to process onWorkDone received by Listener
    void handleWorkDone(std::list<std::unique_ptr<C2Work>>& workItems) {
        for (std::unique_ptr<C2Work>& work : workItems) {
            if (!work->worklets.empty()) {
                bool mCsd = false;
                uint32_t mFramesReceived = 0;
                std::list<uint64_t> mFlushedIndices;
                workDone(mComponent, work, mFlushedIndices, mQueueLock, mQueueCondition, mWorkQueue,
                         mEos, mCsd, mFramesReceived);
            }
        }
    }

    std::string mInstanceName;
    std::string mComponentName;
    bool mEos;
    std::mutex mQueueLock;
    std::condition_variable mQueueCondition;
    std::list<std::unique_ptr<C2Work>> mWorkQueue;

    std::shared_ptr<android::Codec2Client> mClient;
    std::shared_ptr<android::Codec2Client::Listener> mListener;
    std::shared_ptr<android::Codec2Client::Component> mComponent;

  protected:
    static void description(const std::string& description) {
        RecordProperty("description", description);
    }
};

class Codec2ComponentHidlTest
    : public Codec2ComponentHidlTestBase,
      public ::testing::WithParamInterface<std::tuple<std::string, std::string>> {
    void getParams() {
        mInstanceName = std::get<0>(GetParam());
        mComponentName = std::get<1>(GetParam());
    }
};

// Test Empty Flush
TEST_P(Codec2ComponentHidlTest, EmptyFlush) {
    ALOGV("Empty Flush Test");
    c2_status_t err = mComponent->start();
    ASSERT_EQ(err, C2_OK);

    std::list<std::unique_ptr<C2Work>> flushedWork;
    err = mComponent->flush(C2Component::FLUSH_COMPONENT, &flushedWork);
    ASSERT_EQ(err, C2_OK);

    err = mComponent->stop();
    ASSERT_EQ(err, C2_OK);
    // Empty Flush should not return any work
    ASSERT_EQ(flushedWork.size(), 0u);
}

// Test Queue Empty Work
TEST_P(Codec2ComponentHidlTest, QueueEmptyWork) {
    ALOGV("Queue Empty Work Test");
    c2_status_t err = mComponent->start();
    ASSERT_EQ(err, C2_OK);

    // Queueing an empty WorkBundle
    std::list<std::unique_ptr<C2Work>> workList;
    mComponent->queue(&workList);

    err = mComponent->reset();
    ASSERT_EQ(err, C2_OK);
}

// Test Component Configuration
TEST_P(Codec2ComponentHidlTest, Config) {
    ALOGV("Configuration Test");

    C2String name = mComponent->getName();
    EXPECT_NE(name.empty(), true) << "Invalid Component Name";

    c2_status_t err = C2_OK;
    std::vector<std::unique_ptr<C2Param>> queried;
    std::vector<std::unique_ptr<C2SettingResult>> failures;

    // Query supported params by the component
    std::vector<std::shared_ptr<C2ParamDescriptor>> params;
    err = mComponent->querySupportedParams(&params);
    ASSERT_EQ(err, C2_OK);
    ALOGV("Number of total params - %zu", params.size());

    // Query and config all the supported params
    for (std::shared_ptr<C2ParamDescriptor> p : params) {
        ALOGD("Querying index %d", (int)p->index());
        err = mComponent->query({}, {p->index()}, C2_DONT_BLOCK, &queried);
        EXPECT_NE(queried.size(), 0u);
        EXPECT_EQ(err, C2_OK);
        err = mComponent->config({queried[0].get()}, C2_DONT_BLOCK, &failures);
        ASSERT_EQ(err, C2_OK);
        ASSERT_EQ(failures.size(), 0u);
    }
}

// Test Multiple Start Stop Reset Test
TEST_P(Codec2ComponentHidlTest, MultipleStartStopReset) {
    ALOGV("Multiple Start Stop and Reset Test");

    for (size_t i = 0; i < MAX_RETRY; i++) {
        mComponent->start();
        mComponent->stop();
    }

    ASSERT_EQ(mComponent->start(), C2_OK);

    for (size_t i = 0; i < MAX_RETRY; i++) {
        mComponent->reset();
    }

    ASSERT_EQ(mComponent->start(), C2_OK);
    ASSERT_EQ(mComponent->stop(), C2_OK);

    // Second stop should return error
    ASSERT_NE(mComponent->stop(), C2_OK);
}

// Test Component Release API
TEST_P(Codec2ComponentHidlTest, MultipleRelease) {
    ALOGV("Multiple Release Test");
    c2_status_t err = mComponent->start();
    ASSERT_EQ(err, C2_OK);

    // Query Component Domain Type
    std::vector<std::unique_ptr<C2Param>> queried;
    err = mComponent->query({}, {C2PortMediaTypeSetting::input::PARAM_TYPE}, C2_DONT_BLOCK,
                            &queried);
    EXPECT_NE(queried.size(), 0u);

    // Configure Component Domain
    std::vector<std::unique_ptr<C2SettingResult>> failures;
    C2PortMediaTypeSetting::input* portMediaType =
            C2PortMediaTypeSetting::input::From(queried[0].get());
    err = mComponent->config({portMediaType}, C2_DONT_BLOCK, &failures);
    ASSERT_EQ(err, C2_OK);
    ASSERT_EQ(failures.size(), 0u);

    for (size_t i = 0; i < MAX_RETRY; i++) {
        mComponent->release();
    }
}

// Test API's Timeout
TEST_P(Codec2ComponentHidlTest, Timeout) {
    ALOGV("Timeout Test");
    c2_status_t err = C2_OK;

    int64_t startTime = getNowUs();
    err = mComponent->start();
    int64_t timeConsumed = getNowUs() - startTime;
    CHECK_TIMEOUT(timeConsumed, START_TIME_OUT, "start()");
    ALOGV("mComponent->start() timeConsumed=%" PRId64 " us", timeConsumed);
    ASSERT_EQ(err, C2_OK);

    startTime = getNowUs();
    err = mComponent->reset();
    timeConsumed = getNowUs() - startTime;
    CHECK_TIMEOUT(timeConsumed, RESET_TIME_OUT, "reset()");
    ALOGV("mComponent->reset() timeConsumed=%" PRId64 " us", timeConsumed);
    ASSERT_EQ(err, C2_OK);

    err = mComponent->start();
    ASSERT_EQ(err, C2_OK);

    // Query supported params by the component
    std::vector<std::shared_ptr<C2ParamDescriptor>> params;
    startTime = getNowUs();
    err = mComponent->querySupportedParams(&params);
    timeConsumed = getNowUs() - startTime;
    CHECK_TIMEOUT(timeConsumed, QUERYSUPPORTEDPARAMS_TIME_OUT, "querySupportedParams()");
    ALOGV("mComponent->querySupportedParams() timeConsumed=%" PRId64 " us", timeConsumed);
    ASSERT_EQ(err, C2_OK);

    std::vector<std::unique_ptr<C2Param>> queried;
    std::vector<std::unique_ptr<C2SettingResult>> failures;
    // Query and config all the supported params
    for (std::shared_ptr<C2ParamDescriptor> p : params) {
        startTime = getNowUs();
        err = mComponent->query({}, {p->index()}, C2_DONT_BLOCK, &queried);
        timeConsumed = getNowUs() - startTime;
        CHECK_TIMEOUT(timeConsumed, QUERY_TIME_OUT, "query()");
        EXPECT_NE(queried.size(), 0u);
        EXPECT_EQ(err, C2_OK);
        ALOGV("mComponent->query() for %s timeConsumed=%" PRId64 " us", p->name().c_str(),
              timeConsumed);

        startTime = getNowUs();
        err = mComponent->config({queried[0].get()}, C2_DONT_BLOCK, &failures);
        timeConsumed = getNowUs() - startTime;
        CHECK_TIMEOUT(timeConsumed, CONFIG_TIME_OUT, "config()");
        ASSERT_EQ(err, C2_OK);
        ASSERT_EQ(failures.size(), 0u);
        ALOGV("mComponent->config() for %s timeConsumed=%" PRId64 " us", p->name().c_str(),
              timeConsumed);
    }

    std::list<std::unique_ptr<C2Work>> workList;
    startTime = getNowUs();
    err = mComponent->queue(&workList);
    timeConsumed = getNowUs() - startTime;
    ALOGV("mComponent->queue() timeConsumed=%" PRId64 " us", timeConsumed);
    CHECK_TIMEOUT(timeConsumed, QUEUE_TIME_OUT, "queue()");

    startTime = getNowUs();
    err = mComponent->flush(C2Component::FLUSH_COMPONENT, &workList);
    timeConsumed = getNowUs() - startTime;
    ALOGV("mComponent->flush() timeConsumed=%" PRId64 " us", timeConsumed);
    CHECK_TIMEOUT(timeConsumed, FLUSH_TIME_OUT, "flush()");

    startTime = getNowUs();
    err = mComponent->stop();
    timeConsumed = getNowUs() - startTime;
    ALOGV("mComponent->stop() timeConsumed=%" PRId64 " us", timeConsumed);
    CHECK_TIMEOUT(timeConsumed, STOP_TIME_OUT, "stop()");
    ASSERT_EQ(err, C2_OK);

    startTime = getNowUs();
    err = mComponent->release();
    timeConsumed = getNowUs() - startTime;
    ALOGV("mComponent->release() timeConsumed=%" PRId64 " us", timeConsumed);
    CHECK_TIMEOUT(timeConsumed, RELEASE_TIME_OUT, "release()");
    ASSERT_EQ(err, C2_OK);
}

class Codec2ComponentInputTests
    : public Codec2ComponentHidlTestBase,
      public ::testing::WithParamInterface<
              std::tuple<std::string, std::string, std::string, std::string>> {
    void getParams() {
        mInstanceName = std::get<0>(GetParam());
        mComponentName = std::get<1>(GetParam());
    }
};

TEST_P(Codec2ComponentInputTests, InputBufferTest) {
    description("Tests for different inputs");

    uint32_t flags = std::stoul(std::get<2>(GetParam()));
    bool isNullBuffer = !std::get<3>(GetParam()).compare("true");
    if (isNullBuffer)
        ALOGD("Testing for null input buffer with flag : %u", flags);
    else
        ALOGD("Testing for empty input buffer with flag : %u", flags);
    mEos = false;
    ASSERT_EQ(mComponent->start(), C2_OK);
    ASSERT_NO_FATAL_FAILURE(
            testInputBuffer(mComponent, mQueueLock, mWorkQueue, flags, isNullBuffer));

    ALOGD("Waiting for input consumption");
    ASSERT_NO_FATAL_FAILURE(waitOnInputConsumption(mQueueLock, mQueueCondition, mWorkQueue));

    if (flags == C2FrameData::FLAG_END_OF_STREAM) ASSERT_EQ(mEos, true);
    ASSERT_EQ(mComponent->stop(), C2_OK);
    ASSERT_EQ(mComponent->reset(), C2_OK);
}

INSTANTIATE_TEST_SUITE_P(PerInstance, Codec2ComponentHidlTest, testing::ValuesIn(kTestParameters),
                         android::hardware::PrintInstanceTupleNameToString<>);

INSTANTIATE_TEST_CASE_P(NonStdInputs, Codec2ComponentInputTests,
                        testing::ValuesIn(kInputTestParameters),
                        android::hardware::PrintInstanceTupleNameToString<>);
}  // anonymous namespace

// TODO: Add test for Invalid work,
// TODO: Add test for Invalid states
int main(int argc, char** argv) {
    kTestParameters = getTestParameters();
    for (auto params : kTestParameters) {
        kInputTestParameters.push_back(
                std::make_tuple(std::get<0>(params), std::get<1>(params), "0", "true"));
        kInputTestParameters.push_back(
                std::make_tuple(std::get<0>(params), std::get<1>(params),
                                std::to_string(C2FrameData::FLAG_END_OF_STREAM), "true"));
        kInputTestParameters.push_back(
                std::make_tuple(std::get<0>(params), std::get<1>(params), "0", "false"));
        kInputTestParameters.push_back(
                std::make_tuple(std::get<0>(params), std::get<1>(params),
                                std::to_string(C2FrameData::FLAG_CODEC_CONFIG), "false"));
        kInputTestParameters.push_back(
                std::make_tuple(std::get<0>(params), std::get<1>(params),
                                std::to_string(C2FrameData::FLAG_END_OF_STREAM), "false"));
    }

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
