/*
 * Copyright (C) 2019 The Android Open Source Project
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

// Unit Test for ECOSession.

//#define LOG_NDEBUG 0
#define LOG_TAG "ECOSessionTest"

#include <android-base/unique_fd.h>
#include <binder/Parcel.h>
#include <binder/Parcelable.h>
#include <cutils/ashmem.h>
#include <gtest/gtest.h>
#include <math.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <utils/Log.h>

#include "FakeECOServiceInfoListener.h"
#include "FakeECOServiceStatsProvider.h"
#include "eco/ECOSession.h"
#include "eco/ECOUtils.h"

namespace android {
namespace media {
namespace eco {

using android::sp;
using ::android::binder::Status;

static constexpr uint32_t kTestWidth = 1280;
static constexpr uint32_t kTestHeight = 720;
static constexpr bool kIsCameraRecording = true;
static constexpr int32_t kTargetBitrateBps = 22000000;
static constexpr int32_t kKeyFrameIntervalFrames = 30;
static constexpr float kFrameRate = 30.0f;

// A helpful class to help create ECOSession and manage ECOSession.
class EcoSessionTest : public ::testing::Test {
public:
    EcoSessionTest() { ALOGD("EcoSessionTest created"); }

    sp<ECOSession> createSession(int32_t width, int32_t height, bool isCameraRecording) {
        mSession = ECOSession::createECOSession(width, height, isCameraRecording);
        if (mSession == nullptr) return nullptr;
        return mSession;
    }

private:
    sp<ECOSession> mSession = nullptr;
};

TEST_F(EcoSessionTest, TestConstructorWithInvalidParameters) {
    // Expects failure as ECOService1.0 will only support up to 720P and camera recording case.
    EXPECT_TRUE(createSession(1920 /* width */, 1080 /* height */, true /* isCameraRecording */) ==
                nullptr);

    // Expects failure as ECOService1.0 will only support up to 720P and camera recording case.
    EXPECT_TRUE(createSession(1920 /* width */, 1080 /* height */, false /* isCameraRecording */) ==
                nullptr);

    EXPECT_TRUE(createSession(1920 /* width */, -1 /* height */, true /* isCameraRecording */) ==
                nullptr);

    EXPECT_TRUE(createSession(-1 /* width */, 1080 /* height */, true /* isCameraRecording */) ==
                nullptr);
}

TEST_F(EcoSessionTest, TestConstructorWithValidParameters) {
    // Expects success with <= 720P and is for camera recording.
    EXPECT_TRUE(createSession(1280 /* width */, 720 /* height */, true /* isCameraRecording */) !=
                nullptr);

    // Expects success with <= 720P and is for camera recording.
    EXPECT_TRUE(createSession(640 /* width */, 480 /* height */, true /* isCameraRecording */) !=
                nullptr);
}

TEST_F(EcoSessionTest, TestAddProviderWithoutSpecifyEcoDataType) {
    sp<ECOSession> ecoSession = createSession(kTestWidth, kTestHeight, kIsCameraRecording);
    EXPECT_TRUE(ecoSession);

    sp<FakeECOServiceStatsProvider> fakeProvider = new FakeECOServiceStatsProvider(
            kTestWidth, kTestHeight, kIsCameraRecording, kFrameRate, ecoSession);

    ECOData providerConfig;
    bool res;
    Status status = ecoSession->addStatsProvider(fakeProvider, providerConfig, &res);
    EXPECT_FALSE(status.isOk());
}

TEST_F(EcoSessionTest, TestAddProviderWithWrongEcoDataType) {
    sp<ECOSession> ecoSession = createSession(kTestWidth, kTestHeight, kIsCameraRecording);
    EXPECT_TRUE(ecoSession);

    sp<FakeECOServiceStatsProvider> fakeProvider = new FakeECOServiceStatsProvider(
            kTestWidth, kTestHeight, kIsCameraRecording, kFrameRate, ecoSession);

    ECOData providerConfig(ECOData::DATA_TYPE_INFO_LISTENER_CONFIG,
                           systemTime(SYSTEM_TIME_BOOTTIME));
    bool res;
    Status status = ecoSession->addStatsProvider(fakeProvider, providerConfig, &res);
    EXPECT_FALSE(status.isOk());
}

TEST_F(EcoSessionTest, TestAddNormalProvider) {
    sp<ECOSession> ecoSession = createSession(kTestWidth, kTestHeight, kIsCameraRecording);
    EXPECT_TRUE(ecoSession);

    sp<FakeECOServiceStatsProvider> fakeProvider = new FakeECOServiceStatsProvider(
            kTestWidth, kTestHeight, kIsCameraRecording, kFrameRate, ecoSession);

    ECOData providerConfig(ECOData::DATA_TYPE_STATS_PROVIDER_CONFIG,
                           systemTime(SYSTEM_TIME_BOOTTIME));
    bool res;
    Status status = ecoSession->addStatsProvider(fakeProvider, providerConfig, &res);
    EXPECT_TRUE(status.isOk());
}

// Add two providers and expect failure as ECOService1.0 only supports one provider and one
// listener.
TEST_F(EcoSessionTest, TestAddTwoProvider) {
    sp<ECOSession> ecoSession = createSession(kTestWidth, kTestHeight, kIsCameraRecording);
    EXPECT_TRUE(ecoSession);

    sp<FakeECOServiceStatsProvider> fakeProvider1 = new FakeECOServiceStatsProvider(
            kTestWidth, kTestHeight, kIsCameraRecording, kFrameRate, ecoSession);

    ECOData providerConfig(ECOData::DATA_TYPE_STATS_PROVIDER_CONFIG,
                           systemTime(SYSTEM_TIME_BOOTTIME));
    bool res;
    Status status = ecoSession->addStatsProvider(fakeProvider1, providerConfig, &res);
    EXPECT_TRUE(status.isOk());

    sp<FakeECOServiceStatsProvider> fakeProvider2 = new FakeECOServiceStatsProvider(
            kTestWidth, kTestHeight, kIsCameraRecording, kFrameRate, ecoSession);
    status = ecoSession->addStatsProvider(fakeProvider2, providerConfig, &res);
    EXPECT_FALSE(status.isOk());
}

TEST_F(EcoSessionTest, TestAddListenerWithDifferentHeight) {
    sp<ECOSession> ecoSession = createSession(kTestWidth, kTestHeight, kIsCameraRecording);
    EXPECT_TRUE(ecoSession);

    sp<FakeECOServiceInfoListener> fakeListener = new FakeECOServiceInfoListener(
            kTestWidth - 1, kTestHeight, kIsCameraRecording, ecoSession);

    ECOData ListenerConfig(ECOData::DATA_TYPE_INFO_LISTENER_CONFIG,
                           systemTime(SYSTEM_TIME_BOOTTIME));
    bool res;
    Status status = ecoSession->addInfoListener(fakeListener, ListenerConfig, &res);
    EXPECT_FALSE(status.isOk());
}

TEST_F(EcoSessionTest, TestAddListenerWithDifferentWidth) {
    sp<ECOSession> ecoSession = createSession(kTestWidth, kTestHeight, kIsCameraRecording);
    EXPECT_TRUE(ecoSession);

    sp<FakeECOServiceInfoListener> fakeListener = new FakeECOServiceInfoListener(
            kTestWidth, kTestHeight - 1, kIsCameraRecording, ecoSession);

    ECOData ListenerConfig(ECOData::DATA_TYPE_INFO_LISTENER_CONFIG,
                           systemTime(SYSTEM_TIME_BOOTTIME));
    bool res;
    Status status = ecoSession->addInfoListener(fakeListener, ListenerConfig, &res);
    EXPECT_FALSE(status.isOk());
}

TEST_F(EcoSessionTest, TestAddListenerWithCameraRecordingFalse) {
    sp<ECOSession> ecoSession = createSession(kTestWidth, kTestHeight, kIsCameraRecording);
    EXPECT_TRUE(ecoSession);

    sp<FakeECOServiceInfoListener> fakeListener = new FakeECOServiceInfoListener(
            kTestWidth, kTestHeight, !kIsCameraRecording, ecoSession);

    ECOData ListenerConfig(ECOData::DATA_TYPE_INFO_LISTENER_CONFIG,
                           systemTime(SYSTEM_TIME_BOOTTIME));
    bool res;
    Status status = ecoSession->addInfoListener(fakeListener, ListenerConfig, &res);
    EXPECT_FALSE(status.isOk());
}

// Test the ECOSession with FakeECOServiceStatsProvider and FakeECOServiceInfoListener. Push the
// stats to ECOSession through FakeECOServiceStatsProvider and check the info received in
// from FakeECOServiceInfoListener ECOSession.
TEST_F(EcoSessionTest, TestSessionWithProviderAndListenerSimpleTest) {
    // The time that listener needs to wait for the info from ECOService.
    static constexpr int kServiceWaitTimeMs = 10;

    // Create the session.
    sp<ECOSession> ecoSession = createSession(kTestWidth, kTestHeight, kIsCameraRecording);

    // Add provider.
    sp<FakeECOServiceStatsProvider> fakeProvider = new FakeECOServiceStatsProvider(
            kTestWidth, kTestHeight, kIsCameraRecording, kFrameRate, ecoSession);
    ECOData providerConfig(ECOData::DATA_TYPE_STATS_PROVIDER_CONFIG,
                           systemTime(SYSTEM_TIME_BOOTTIME));
    providerConfig.setString(KEY_PROVIDER_NAME, "FakeECOServiceStatsProvider");
    providerConfig.setInt32(KEY_PROVIDER_TYPE,
                            ECOServiceStatsProvider::STATS_PROVIDER_TYPE_VIDEO_ENCODER);
    bool res;
    Status status = ecoSession->addStatsProvider(fakeProvider, providerConfig, &res);

    // Create listener.
    sp<FakeECOServiceInfoListener> fakeListener =
            new FakeECOServiceInfoListener(kTestWidth, kTestHeight, kIsCameraRecording, ecoSession);

    // Create the listener config.
    ECOData listenerConfig(ECOData::DATA_TYPE_INFO_LISTENER_CONFIG,
                           systemTime(SYSTEM_TIME_BOOTTIME));
    listenerConfig.setString(KEY_LISTENER_NAME, "FakeECOServiceInfoListener");
    listenerConfig.setInt32(KEY_LISTENER_TYPE, ECOServiceInfoListener::INFO_LISTENER_TYPE_CAMERA);

    // Specify the qp thresholds for receiving notification.
    listenerConfig.setInt32(KEY_LISTENER_QP_BLOCKINESS_THRESHOLD, 40);
    listenerConfig.setInt32(KEY_LISTENER_QP_CHANGE_THRESHOLD, 5);

    status = ecoSession->addInfoListener(fakeListener, listenerConfig, &res);

    ECOData info;
    bool getInfo = false;

    // Set the getInfo flag to true and copy the info from fakeListener.
    fakeListener->setInfoAvailableCallback(
            [&info, &getInfo](const ::android::media::eco::ECOData& newInfo) {
                getInfo = true;
                info = newInfo;
            });

    // Inject the session stats into the ECOSession through fakeProvider.
    SimpleEncoderConfig sessionEncoderConfig("google-avc", CodecTypeAVC, AVCProfileHigh, AVCLevel52,
                                             kTargetBitrateBps, kKeyFrameIntervalFrames,
                                             kFrameRate);
    fakeProvider->injectSessionStats(sessionEncoderConfig.toEcoData(ECOData::DATA_TYPE_STATS));

    // Wait as ECOService may take some time to process.
    std::this_thread::sleep_for(std::chrono::milliseconds(kServiceWaitTimeMs));
    // Check the Session info matches with the session stats sent by provider.
    EXPECT_TRUE(getInfo);
    EXPECT_TRUE(info.getDataType() == ECOData::DATA_TYPE_INFO);

    std::string infoType;
    EXPECT_TRUE(info.findString(KEY_INFO_TYPE, &infoType) == ECODataStatus::OK);
    EXPECT_EQ(infoType, VALUE_INFO_TYPE_SESSION);

    // Check the session info matches the session stats provided by FakeECOServiceStatsProvider.
    int32_t codecType;
    EXPECT_TRUE(info.findInt32(ENCODER_TYPE, &codecType) == ECODataStatus::OK);
    EXPECT_EQ(codecType, CodecTypeAVC);

    int32_t profile;
    EXPECT_TRUE(info.findInt32(ENCODER_PROFILE, &profile) == ECODataStatus::OK);
    EXPECT_EQ(profile, AVCProfileHigh);

    int32_t level;
    EXPECT_TRUE(info.findInt32(ENCODER_LEVEL, &level) == ECODataStatus::OK);
    EXPECT_EQ(level, AVCLevel52);

    int32_t bitrate;
    EXPECT_TRUE(info.findInt32(ENCODER_TARGET_BITRATE_BPS, &bitrate) == ECODataStatus::OK);
    EXPECT_EQ(bitrate, kTargetBitrateBps);

    int32_t kfi;
    EXPECT_TRUE(info.findInt32(ENCODER_KFI_FRAMES, &kfi) == ECODataStatus::OK);
    EXPECT_EQ(kfi, kKeyFrameIntervalFrames);

    // =======================================================================================
    // Inject the frame stats with qp = 30. Expect receiving notification for the first frame.
    SimpleEncodedFrameData frameStats(1 /* seq number */, FrameTypeI, 0 /* framePtsUs */,
                                      30 /* avg-qp */, 56 /* frameSize */);

    getInfo = false;
    fakeProvider->injectFrameStats(frameStats.toEcoData(ECOData::DATA_TYPE_STATS));
    std::this_thread::sleep_for(std::chrono::milliseconds(kServiceWaitTimeMs));
    // Check the Session info matches with the session stats sent by provider.
    EXPECT_TRUE(getInfo);

    EXPECT_TRUE(info.findString(KEY_INFO_TYPE, &infoType) == ECODataStatus::OK);
    EXPECT_EQ(infoType, VALUE_INFO_TYPE_FRAME);

    int8_t frameType;
    EXPECT_TRUE(info.findInt8(FRAME_TYPE, &frameType) == ECODataStatus::OK);
    EXPECT_EQ(frameType, FrameTypeI);

    int32_t frameNum;
    EXPECT_TRUE(info.findInt32(FRAME_NUM, &frameNum) == ECODataStatus::OK);
    EXPECT_EQ(frameNum, 1);

    int64_t framePtsUs;
    EXPECT_TRUE(info.findInt64(FRAME_PTS_US, &framePtsUs) == ECODataStatus::OK);
    EXPECT_EQ(framePtsUs, 0);

    int32_t frameQp;
    EXPECT_TRUE(info.findInt32(FRAME_AVG_QP, &frameQp) == ECODataStatus::OK);
    EXPECT_EQ(frameQp, 30);

    int32_t frameSize;
    EXPECT_TRUE(info.findInt32(FRAME_SIZE_BYTES, &frameSize) == ECODataStatus::OK);
    EXPECT_EQ(frameSize, 56);

    // =======================================================================================
    // Inject the frame stats with qp = 35. Expect not receiving notification as 35 is below
    // threshold.
    frameStats = SimpleEncodedFrameData(2 /* seq number */, FrameTypeP, 333333 /* framePtsUs */,
                                        35 /* avg-qp */, 56 /* frameSize */);
    getInfo = false;
    fakeProvider->injectFrameStats(frameStats.toEcoData(ECOData::DATA_TYPE_STATS));
    // Wait as ECOService may take some time to process.
    std::this_thread::sleep_for(std::chrono::milliseconds(kServiceWaitTimeMs));
    EXPECT_FALSE(getInfo);

    // =======================================================================================
    // Inject the frame stats with qp = 41. Expect receiving notification as 41 goes beyond
    // threshold 40.
    frameStats = SimpleEncodedFrameData(3 /* seq number */, FrameTypeP, 666666 /* framePtsUs */,
                                        41 /* avg-qp */, 56 /* frameSize */);
    getInfo = false;
    fakeProvider->injectFrameStats(frameStats.toEcoData(ECOData::DATA_TYPE_STATS));
    // Wait as ECOService may take some time to process.
    std::this_thread::sleep_for(std::chrono::milliseconds(kServiceWaitTimeMs));

    // Check the frame info matches with the frame stats sent by provider.
    EXPECT_TRUE(getInfo);
    EXPECT_TRUE(info.findString(KEY_INFO_TYPE, &infoType) == ECODataStatus::OK);
    EXPECT_EQ(infoType, VALUE_INFO_TYPE_FRAME);
    EXPECT_TRUE(info.findInt8(FRAME_TYPE, &frameType) == ECODataStatus::OK);
    EXPECT_EQ(frameType, FrameTypeP);
    EXPECT_TRUE(info.findInt32(FRAME_NUM, &frameNum) == ECODataStatus::OK);
    EXPECT_EQ(frameNum, 3);
    EXPECT_TRUE(info.findInt64(FRAME_PTS_US, &framePtsUs) == ECODataStatus::OK);
    EXPECT_EQ(framePtsUs, 666666);
    EXPECT_TRUE(info.findInt32(FRAME_AVG_QP, &frameQp) == ECODataStatus::OK);
    EXPECT_EQ(frameQp, 41);
    EXPECT_TRUE(info.findInt32(FRAME_SIZE_BYTES, &frameSize) == ECODataStatus::OK);
    EXPECT_EQ(frameSize, 56);

    // =======================================================================================
    // Inject the frame stats with qp = 42. Expect not receiving notification as 42 goes beyond
    // threshold 40 but delta oes not go beyond the mQpChangeThreshold threshold.
    frameStats = SimpleEncodedFrameData(4 /* seq number */, FrameTypeP, 999999 /* framePtsUs */,
                                        42 /* avg-qp */, 56 /* frameSize */);
    getInfo = false;
    fakeProvider->injectFrameStats(frameStats.toEcoData(ECOData::DATA_TYPE_STATS));
    // Wait as ECOService may take some time to process.
    std::this_thread::sleep_for(std::chrono::milliseconds(kServiceWaitTimeMs));
    EXPECT_FALSE(getInfo);

    // =======================================================================================
    // Inject the frame stats with qp = 10. Expect receiving notification as the detal from
    // last reported QP is larger than threshold 4.
    frameStats = SimpleEncodedFrameData(5 /* seq number */, FrameTypeB, 1333332 /* framePtsUs */,
                                        49 /* avg-qp */, 56 /* frameSize */);
    getInfo = false;
    fakeProvider->injectFrameStats(frameStats.toEcoData(ECOData::DATA_TYPE_STATS));
    // Wait as ECOService may take some time to process.
    std::this_thread::sleep_for(std::chrono::milliseconds(kServiceWaitTimeMs));

    // Check the frame info matches with the frame stats sent by provider.
    EXPECT_TRUE(getInfo);
    EXPECT_TRUE(info.findString(KEY_INFO_TYPE, &infoType) == ECODataStatus::OK);
    EXPECT_EQ(infoType, VALUE_INFO_TYPE_FRAME);
    EXPECT_TRUE(info.findInt8(FRAME_TYPE, &frameType) == ECODataStatus::OK);
    EXPECT_EQ(frameType, FrameTypeB);
    EXPECT_TRUE(info.findInt32(FRAME_NUM, &frameNum) == ECODataStatus::OK);
    EXPECT_EQ(frameNum, 5);
    EXPECT_TRUE(info.findInt64(FRAME_PTS_US, &framePtsUs) == ECODataStatus::OK);
    EXPECT_EQ(framePtsUs, 1333332);
    EXPECT_TRUE(info.findInt32(FRAME_AVG_QP, &frameQp) == ECODataStatus::OK);
    EXPECT_EQ(frameQp, 49);
    EXPECT_TRUE(info.findInt32(FRAME_SIZE_BYTES, &frameSize) == ECODataStatus::OK);
    EXPECT_EQ(frameSize, 56);

    // =======================================================================================
    // Inject the frame stats with qp = 41. Expect receiving notification as the detal from
    // last reported QP is larger than threshold 4.
    frameStats = SimpleEncodedFrameData(6 /* seq number */, FrameTypeB, 1666665 /* framePtsUs */,
                                        41 /* avg-qp */, 56 /* frameSize */);
    getInfo = false;
    fakeProvider->injectFrameStats(frameStats.toEcoData(ECOData::DATA_TYPE_STATS));
    std::this_thread::sleep_for(std::chrono::milliseconds(kServiceWaitTimeMs));

    // Check the frame info matches with the frame stats sent by provider.
    EXPECT_TRUE(getInfo);
    EXPECT_TRUE(info.findString(KEY_INFO_TYPE, &infoType) == ECODataStatus::OK);
    EXPECT_EQ(infoType, VALUE_INFO_TYPE_FRAME);
    EXPECT_TRUE(info.findInt8(FRAME_TYPE, &frameType) == ECODataStatus::OK);
    EXPECT_EQ(frameType, FrameTypeB);
    EXPECT_TRUE(info.findInt32(FRAME_NUM, &frameNum) == ECODataStatus::OK);
    EXPECT_EQ(frameNum, 6);
    EXPECT_TRUE(info.findInt64(FRAME_PTS_US, &framePtsUs) == ECODataStatus::OK);
    EXPECT_EQ(framePtsUs, 1666665);
    EXPECT_TRUE(info.findInt32(FRAME_AVG_QP, &frameQp) == ECODataStatus::OK);
    EXPECT_EQ(frameQp, 41);
    EXPECT_TRUE(info.findInt32(FRAME_SIZE_BYTES, &frameSize) == ECODataStatus::OK);
    EXPECT_EQ(frameSize, 56);
}

TEST_F(EcoSessionTest, TestRemoveMatchProvider) {
    sp<ECOSession> ecoSession = createSession(kTestWidth, kTestHeight, kIsCameraRecording);
    EXPECT_TRUE(ecoSession);

    sp<FakeECOServiceStatsProvider> fakeProvider1 = new FakeECOServiceStatsProvider(
            kTestWidth, kTestHeight, kIsCameraRecording, kFrameRate, ecoSession);

    ECOData providerConfig(ECOData::DATA_TYPE_STATS_PROVIDER_CONFIG,
                           systemTime(SYSTEM_TIME_BOOTTIME));
    bool res;
    Status status = ecoSession->addStatsProvider(fakeProvider1, providerConfig, &res);
    EXPECT_TRUE(res);
    EXPECT_TRUE(status.isOk());

    status = ecoSession->removeStatsProvider(fakeProvider1, &res);
    EXPECT_TRUE(res);
    EXPECT_TRUE(status.isOk());
}

TEST_F(EcoSessionTest, TestRemoveMisMatchProvider) {
    sp<ECOSession> ecoSession = createSession(kTestWidth, kTestHeight, kIsCameraRecording);
    EXPECT_TRUE(ecoSession);

    sp<FakeECOServiceStatsProvider> fakeProvider1 = new FakeECOServiceStatsProvider(
            kTestWidth, kTestHeight, kIsCameraRecording, kFrameRate, ecoSession);

    ECOData providerConfig(ECOData::DATA_TYPE_STATS_PROVIDER_CONFIG,
                           systemTime(SYSTEM_TIME_BOOTTIME));
    bool res;
    Status status = ecoSession->addStatsProvider(fakeProvider1, providerConfig, &res);
    EXPECT_TRUE(res);
    EXPECT_TRUE(status.isOk());

    sp<FakeECOServiceStatsProvider> fakeProvider2 = new FakeECOServiceStatsProvider(
            kTestWidth, kTestHeight, kIsCameraRecording, kFrameRate, ecoSession);

    status = ecoSession->removeStatsProvider(fakeProvider2, &res);
    EXPECT_FALSE(res);
    EXPECT_FALSE(status.isOk());
}

TEST_F(EcoSessionTest, TestRemoveMatchListener) {
    sp<ECOSession> ecoSession = createSession(kTestWidth, kTestHeight, kIsCameraRecording);
    EXPECT_TRUE(ecoSession);

    // Create listener.
    sp<FakeECOServiceInfoListener> fakeListener =
            new FakeECOServiceInfoListener(kTestWidth, kTestHeight, kIsCameraRecording, ecoSession);

    // Create the listener config.
    ECOData listenerConfig(ECOData::DATA_TYPE_INFO_LISTENER_CONFIG,
                           systemTime(SYSTEM_TIME_BOOTTIME));
    listenerConfig.setString(KEY_LISTENER_NAME, "FakeECOServiceInfoListener");
    listenerConfig.setInt32(KEY_LISTENER_TYPE, ECOServiceInfoListener::INFO_LISTENER_TYPE_CAMERA);

    // Specify the qp thresholds for receiving notification.
    listenerConfig.setInt32(KEY_LISTENER_QP_BLOCKINESS_THRESHOLD, 40);
    listenerConfig.setInt32(KEY_LISTENER_QP_CHANGE_THRESHOLD, 5);

    bool res;
    Status status = ecoSession->addInfoListener(fakeListener, listenerConfig, &res);

    status = ecoSession->removeInfoListener(fakeListener, &res);
    EXPECT_TRUE(res);
    EXPECT_TRUE(status.isOk());
}

TEST_F(EcoSessionTest, TestRemoveMisMatchListener) {
    sp<ECOSession> ecoSession = createSession(kTestWidth, kTestHeight, kIsCameraRecording);
    EXPECT_TRUE(ecoSession);

    // Create listener.
    sp<FakeECOServiceInfoListener> fakeListener =
            new FakeECOServiceInfoListener(kTestWidth, kTestHeight, kIsCameraRecording, ecoSession);

    // Create the listener config.
    ECOData listenerConfig(ECOData::DATA_TYPE_INFO_LISTENER_CONFIG,
                           systemTime(SYSTEM_TIME_BOOTTIME));
    listenerConfig.setString(KEY_LISTENER_NAME, "FakeECOServiceInfoListener");
    listenerConfig.setInt32(KEY_LISTENER_TYPE, ECOServiceInfoListener::INFO_LISTENER_TYPE_CAMERA);

    // Specify the qp thresholds for receiving notification.
    listenerConfig.setInt32(KEY_LISTENER_QP_BLOCKINESS_THRESHOLD, 40);
    listenerConfig.setInt32(KEY_LISTENER_QP_CHANGE_THRESHOLD, 5);

    bool res;
    Status status = ecoSession->addInfoListener(fakeListener, listenerConfig, &res);

    // Create listener.
    sp<FakeECOServiceInfoListener> fakeListener2 =
            new FakeECOServiceInfoListener(kTestWidth, kTestHeight, kIsCameraRecording, ecoSession);

    status = ecoSession->removeInfoListener(fakeListener2, &res);
    EXPECT_FALSE(res);
    EXPECT_FALSE(status.isOk());
}

// Test the listener connects to the ECOSession after provider sends the session info. Listener
// should recieve the session info right after adding itself to the ECOSession.
TEST_F(EcoSessionTest, TestAddListenerAferProviderStarts) {
    // The time that listener needs to wait for the info from ECOService.
    static constexpr int kServiceWaitTimeMs = 10;

    // Create the session.
    sp<ECOSession> ecoSession = createSession(kTestWidth, kTestHeight, kIsCameraRecording);

    // Add provider.
    sp<FakeECOServiceStatsProvider> fakeProvider = new FakeECOServiceStatsProvider(
            kTestWidth, kTestHeight, kIsCameraRecording, kFrameRate, ecoSession);
    ECOData providerConfig(ECOData::DATA_TYPE_STATS_PROVIDER_CONFIG,
                           systemTime(SYSTEM_TIME_BOOTTIME));
    providerConfig.setString(KEY_PROVIDER_NAME, "FakeECOServiceStatsProvider");
    providerConfig.setInt32(KEY_PROVIDER_TYPE,
                            ECOServiceStatsProvider::STATS_PROVIDER_TYPE_VIDEO_ENCODER);
    bool res;
    Status status = ecoSession->addStatsProvider(fakeProvider, providerConfig, &res);

    // Inject the session stats into the ECOSession through fakeProvider.
    SimpleEncoderConfig sessionEncoderConfig("google-avc", CodecTypeAVC, AVCProfileHigh, AVCLevel52,
                                             kTargetBitrateBps, kKeyFrameIntervalFrames,
                                             kFrameRate);
    fakeProvider->injectSessionStats(sessionEncoderConfig.toEcoData(ECOData::DATA_TYPE_STATS));

    // Wait as ECOService may take some time to process.
    std::this_thread::sleep_for(std::chrono::milliseconds(kServiceWaitTimeMs));

    // =======================================================================================
    // Inject the frame stats with qp = 30. Expect receiving notification for the first frame.
    SimpleEncodedFrameData frameStats(1 /* seq number */, FrameTypeI, 0 /* framePtsUs */,
                                      30 /* avg-qp */, 56 /* frameSize */);

    fakeProvider->injectFrameStats(frameStats.toEcoData(ECOData::DATA_TYPE_STATS));
    std::this_thread::sleep_for(std::chrono::milliseconds(kServiceWaitTimeMs));

    // =======================================================================================
    // Create and add the listener to the ECOSession. Expect to receive the session infor right
    // after addInfoListener.
    sp<FakeECOServiceInfoListener> fakeListener =
            new FakeECOServiceInfoListener(kTestWidth, kTestHeight, kIsCameraRecording, ecoSession);

    // Create the listener config.
    ECOData listenerConfig(ECOData::DATA_TYPE_INFO_LISTENER_CONFIG,
                           systemTime(SYSTEM_TIME_BOOTTIME));
    listenerConfig.setString(KEY_LISTENER_NAME, "FakeECOServiceInfoListener");
    listenerConfig.setInt32(KEY_LISTENER_TYPE, ECOServiceInfoListener::INFO_LISTENER_TYPE_CAMERA);

    // Specify the qp thresholds for receiving notification.
    listenerConfig.setInt32(KEY_LISTENER_QP_BLOCKINESS_THRESHOLD, 40);
    listenerConfig.setInt32(KEY_LISTENER_QP_CHANGE_THRESHOLD, 5);

    ECOData info;
    bool getInfo = false;

    // Set the getInfo flag to true and copy the info from fakeListener.
    fakeListener->setInfoAvailableCallback(
            [&info, &getInfo](const ::android::media::eco::ECOData& newInfo) {
                getInfo = true;
                info = newInfo;
            });

    status = ecoSession->addInfoListener(fakeListener, listenerConfig, &res);

    // Wait as ECOService may take some time to process.
    std::this_thread::sleep_for(std::chrono::milliseconds(kServiceWaitTimeMs));

    // Check the Session info matches with the session stats sent by provider.
    EXPECT_TRUE(getInfo);
    EXPECT_TRUE(info.getDataType() == ECOData::DATA_TYPE_INFO);

    std::string infoType;
    EXPECT_TRUE(info.findString(KEY_INFO_TYPE, &infoType) == ECODataStatus::OK);
    EXPECT_EQ(infoType, VALUE_INFO_TYPE_SESSION);

    // Check the session info matches the session stats provided by FakeECOServiceStatsProvider.
    int32_t codecType;
    EXPECT_TRUE(info.findInt32(ENCODER_TYPE, &codecType) == ECODataStatus::OK);
    EXPECT_EQ(codecType, CodecTypeAVC);

    int32_t profile;
    EXPECT_TRUE(info.findInt32(ENCODER_PROFILE, &profile) == ECODataStatus::OK);
    EXPECT_EQ(profile, AVCProfileHigh);

    int32_t level;
    EXPECT_TRUE(info.findInt32(ENCODER_LEVEL, &level) == ECODataStatus::OK);
    EXPECT_EQ(level, AVCLevel52);

    int32_t bitrate;
    EXPECT_TRUE(info.findInt32(ENCODER_TARGET_BITRATE_BPS, &bitrate) == ECODataStatus::OK);
    EXPECT_EQ(bitrate, kTargetBitrateBps);

    int32_t kfi;
    EXPECT_TRUE(info.findInt32(ENCODER_KFI_FRAMES, &kfi) == ECODataStatus::OK);
    EXPECT_EQ(kfi, kKeyFrameIntervalFrames);
}

}  // namespace eco
}  // namespace media
}  // namespace android
