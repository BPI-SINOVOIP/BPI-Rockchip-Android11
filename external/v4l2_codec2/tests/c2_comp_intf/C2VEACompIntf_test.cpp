// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//#define LOG_NDEBUG 0
#define LOG_TAG "C2VDEACompIntf_test"

#include <C2CompIntfTest.h>

#include <inttypes.h>
#include <stdio.h>
#include <limits>

#include <C2PlatformSupport.h>
#include <SimpleC2Interface.h>
#include <gtest/gtest.h>
#include <utils/Log.h>

#include <v4l2_codec2/components/V4L2EncodeComponent.h>
#include <v4l2_codec2/components/V4L2EncodeInterface.h>

namespace android {

constexpr const char* testCompName = "c2.v4l2.avc.encoder";
constexpr c2_node_id_t testCompNodeId = 12345;

constexpr const char* MEDIA_MIMETYPE_VIDEO_RAW = "video/raw";
constexpr const char* MEDIA_MIMETYPE_VIDEO_AVC = "video/avc";

constexpr C2Allocator::id_t kInputAllocators[] = {C2PlatformAllocatorStore::GRALLOC};
constexpr C2Allocator::id_t kOutputAllocators[] = {C2PlatformAllocatorStore::BLOB};
constexpr C2BlockPool::local_id_t kDefaultOutputBlockPool = C2BlockPool::BASIC_LINEAR;

class C2VEACompIntfTest: public C2CompIntfTest {
protected:
    C2VEACompIntfTest() {
        mReflector = std::make_shared<C2ReflectorHelper>();
        auto componentInterface = std::make_shared<V4L2EncodeInterface>(testCompName, mReflector);
        mIntf = std::shared_ptr<C2ComponentInterface>(new SimpleInterface<V4L2EncodeInterface>(
                testCompName, testCompNodeId, componentInterface));
    }
    ~C2VEACompIntfTest() override {
    }
};

#define TRACED_FAILURE(func)                            \
    do {                                                \
        SCOPED_TRACE(#func);                            \
        func;                                           \
        if (::testing::Test::HasFatalFailure()) return; \
    } while (false)

TEST_F(C2VEACompIntfTest, CreateInstance) {
    auto name = mIntf->getName();
    auto id = mIntf->getId();
    printf("name = %s\n", name.c_str());
    printf("node_id = %u\n", id);
    EXPECT_STREQ(name.c_str(), testCompName);
    EXPECT_EQ(id, testCompNodeId);
}

TEST_F(C2VEACompIntfTest, TestInputFormat) {
    C2StreamBufferTypeSetting::input expected(0u, C2BufferData::GRAPHIC);
    C2StreamBufferTypeSetting::input invalid(0u, C2BufferData::LINEAR);
    TRACED_FAILURE(testReadOnlyParam(&expected, &invalid));
}

TEST_F(C2VEACompIntfTest, TestOutputFormat) {
    C2StreamBufferTypeSetting::output expected(0u, C2BufferData::LINEAR);
    C2StreamBufferTypeSetting::output invalid(0u, C2BufferData::GRAPHIC);
    TRACED_FAILURE(testReadOnlyParam(&expected, &invalid));
}

TEST_F(C2VEACompIntfTest, TestInputPortMime) {
    std::shared_ptr<C2PortMediaTypeSetting::input> expected(
            AllocSharedString<C2PortMediaTypeSetting::input>(MEDIA_MIMETYPE_VIDEO_RAW));
    std::shared_ptr<C2PortMediaTypeSetting::input> invalid(
            AllocSharedString<C2PortMediaTypeSetting::input>(MEDIA_MIMETYPE_VIDEO_AVC));
    TRACED_FAILURE(testReadOnlyParamOnHeap(expected.get(), invalid.get()));
}

TEST_F(C2VEACompIntfTest, TestOutputPortMime) {
    std::shared_ptr<C2PortMediaTypeSetting::output> expected(
            AllocSharedString<C2PortMediaTypeSetting::output>(MEDIA_MIMETYPE_VIDEO_AVC));
    std::shared_ptr<C2PortMediaTypeSetting::output> invalid(
            AllocSharedString<C2PortMediaTypeSetting::output>(MEDIA_MIMETYPE_VIDEO_RAW));
    TRACED_FAILURE(testReadOnlyParamOnHeap(expected.get(), invalid.get()));
}

TEST_F(C2VEACompIntfTest, TestProfileLevel) {
    // Configure input size, framerate, and bitrate to values which are capable of the lowest
    // profile and level. (176x144, 15fps, 64000bps)
    C2StreamPictureSizeInfo::input videoSize(0u, 176, 144);
    C2StreamFrameRateInfo::output frameRate(0u, 15.);
    C2StreamBitrateInfo::output bitrate(0u, 64000);

    // Configure and check if value is configured.
    TRACED_FAILURE(testWritableParam(&videoSize));
    TRACED_FAILURE(testWritableParam(&frameRate));
    TRACED_FAILURE(testWritableParam(&bitrate));

    // Iterate all possible profile and level combination
    TRACED_FAILURE(testWritableProfileLevelParam<C2StreamProfileLevelInfo::output>());
}

TEST_F(C2VEACompIntfTest, TestVideoSize) {
    C2StreamPictureSizeInfo::input videoSize;
    videoSize.setStream(0);  // only support single stream
    std::vector<C2FieldSupportedValuesQuery> widthC2FSV = {
            {C2ParamField(&videoSize, &C2StreamPictureSizeInfo::width),
             C2FieldSupportedValuesQuery::CURRENT},
    };
    ASSERT_EQ(C2_OK, mIntf->querySupportedValues_vb(widthC2FSV, C2_DONT_BLOCK));
    std::vector<C2FieldSupportedValuesQuery> heightC2FSV = {
            {C2ParamField(&videoSize, &C2StreamPictureSizeInfo::height),
             C2FieldSupportedValuesQuery::CURRENT},
    };
    ASSERT_EQ(C2_OK, mIntf->querySupportedValues_vb(heightC2FSV, C2_DONT_BLOCK));

    // Configure input size may take more time since the profile level setter also depends on it.
    // Just limit the test range to 1080p and step up to 16 to make test run faster.
    ASSERT_EQ(1u, widthC2FSV.size());
    ASSERT_EQ(C2_OK, widthC2FSV[0].status);
    ASSERT_EQ(C2FieldSupportedValues::RANGE, widthC2FSV[0].values.type);
    auto& widthFSVRange = widthC2FSV[0].values.range;
    int32_t widthMin = widthFSVRange.min.i32;
    int32_t widthMax = std::min(widthFSVRange.max.i32, 1920);
    int32_t widthStep = std::max(widthFSVRange.step.i32, 16);

    ASSERT_EQ(1u, heightC2FSV.size());
    ASSERT_EQ(C2_OK, heightC2FSV[0].status);
    ASSERT_EQ(C2FieldSupportedValues::RANGE, heightC2FSV[0].values.type);
    auto& heightFSVRange = heightC2FSV[0].values.range;
    int32_t heightMin = heightFSVRange.min.i32;
    int32_t heightMax = std::min(heightFSVRange.max.i32, 1080);
    int32_t heightStep = std::max(heightFSVRange.step.i32, 16);

    // test updating valid and invalid values
    TRACED_FAILURE(testWritableVideoSizeParam<C2StreamPictureSizeInfo::input>(
            widthMin, widthMax, widthStep, heightMin, heightMax, heightStep));
}

TEST_F(C2VEACompIntfTest, TestBitrate) {
    C2StreamBitrateInfo::output bitrate;
    std::vector<C2FieldSupportedValuesQuery> valueC2FSV = {
            {C2ParamField(&bitrate, &C2StreamBitrateInfo::value),
             C2FieldSupportedValuesQuery::CURRENT},
    };
    ASSERT_EQ(C2_OK, mIntf->querySupportedValues_vb(valueC2FSV, C2_DONT_BLOCK));
    ASSERT_EQ(1u, valueC2FSV.size());
    ASSERT_EQ(C2_OK, valueC2FSV[0].status);
    ASSERT_EQ(C2FieldSupportedValues::RANGE, valueC2FSV[0].values.type);
    auto& valueFSVRange = valueC2FSV[0].values.range;
    uint32_t bitrateMin = valueFSVRange.min.u32;
    uint32_t bitrateMax = valueFSVRange.max.u32;
    uint32_t bitrateStep = valueFSVRange.step.u32;
    bitrate.value = bitrateMin;
    TRACED_FAILURE(testWritableParam(&bitrate));
    bitrate.value = bitrateMax;
    TRACED_FAILURE(testWritableParam(&bitrate));
    // Choose the value which is half steps from bitrateMin than bitrateMax.
    uint32_t steps = (bitrateMax - bitrateMin) / bitrateStep;
    bitrate.value = bitrateMin + steps / 2 * bitrateStep;
    TRACED_FAILURE(testWritableParam(&bitrate));
    // TODO: Add invalid value test after validate possible values in C2InterfaceHelper is
    //       implemented.
}

TEST_F(C2VEACompIntfTest, TestFrameRate) {
    C2StreamFrameRateInfo::output frameRate;
    frameRate.setStream(0);  // only support single stream
    std::vector<C2Param*> stackParams{&frameRate};
    ASSERT_EQ(C2_OK, mIntf->query_vb(stackParams, {}, C2_DONT_BLOCK, nullptr));

    float defaultFrameRate = frameRate.value;
    frameRate.value = defaultFrameRate / 2;
    TRACED_FAILURE(testWritableParam(&frameRate));
    frameRate.value = defaultFrameRate;
    TRACED_FAILURE(testWritableParam(&frameRate));
    // TODO: Add invalid value test after validate possible values in C2InterfaceHelper is
    //       implemented.
}

TEST_F(C2VEACompIntfTest, TestIntraRefreshPeriod) {
    C2StreamIntraRefreshTuning::output period(0u, C2Config::INTRA_REFRESH_ARBITRARY, 30.);
    TRACED_FAILURE(testWritableParam(&period));
    period.mode = C2Config::INTRA_REFRESH_DISABLED;
    period.period = 0;
    TRACED_FAILURE(testWritableParam(&period));
}

TEST_F(C2VEACompIntfTest, TestRequestKeyFrame) {
    C2StreamRequestSyncFrameTuning::output request(0u, C2_TRUE);
    TRACED_FAILURE(testWritableParam(&request));
    request.value = C2_FALSE;
    TRACED_FAILURE(testWritableParam(&request));
}

TEST_F(C2VEACompIntfTest, TestKeyFramePeriodUs) {
    C2StreamSyncFrameIntervalTuning::output period(0u, 500000);
    TRACED_FAILURE(testWritableParam(&period));
    period.value = 1500000;
    TRACED_FAILURE(testWritableParam(&period));
}

TEST_F(C2VEACompIntfTest, TestInputAllocatorIds) {
    std::shared_ptr<C2PortAllocatorsTuning::input> expected(
            C2PortAllocatorsTuning::input::AllocShared(kInputAllocators));
    std::shared_ptr<C2PortAllocatorsTuning::input> invalid(
            C2PortAllocatorsTuning::input::AllocShared(kOutputAllocators));
    TRACED_FAILURE(testReadOnlyParamOnHeap(expected.get(), invalid.get()));
}

TEST_F(C2VEACompIntfTest, TestOutputAllocatorIds) {
    std::shared_ptr<C2PortAllocatorsTuning::output> expected(
            C2PortAllocatorsTuning::output::AllocShared(kOutputAllocators));
    std::shared_ptr<C2PortAllocatorsTuning::output> invalid(
            C2PortAllocatorsTuning::output::AllocShared(kInputAllocators));
    TRACED_FAILURE(testReadOnlyParamOnHeap(expected.get(), invalid.get()));
}

TEST_F(C2VEACompIntfTest, TestOutputBlockPoolIds) {
    std::vector<std::unique_ptr<C2Param>> heapParams;
    C2Param::Index index = C2PortBlockPoolsTuning::output::PARAM_TYPE;

    // Query the param and check the default value.
    ASSERT_EQ(C2_OK, mIntf->query_vb({}, {index}, C2_DONT_BLOCK, &heapParams));
    ASSERT_EQ(1u, heapParams.size());
    C2BlockPool::local_id_t value =
            reinterpret_cast<C2PortBlockPoolsTuning*>(heapParams[0].get())->m.values[0];
    ASSERT_EQ(kDefaultOutputBlockPool, value);

    // Configure the param.
    C2BlockPool::local_id_t configBlockPools[] = {C2BlockPool::PLATFORM_START + 1};
    std::shared_ptr<C2PortBlockPoolsTuning::output> newParam(
            C2PortBlockPoolsTuning::output::AllocShared(configBlockPools));

    std::vector<C2Param*> params{newParam.get()};
    std::vector<std::unique_ptr<C2SettingResult>> failures;
    ASSERT_EQ(C2_OK, mIntf->config_vb(params, C2_DONT_BLOCK, &failures));
    EXPECT_EQ(0u, failures.size());

    // Query the param again and check the value is as configured
    heapParams.clear();
    ASSERT_EQ(C2_OK, mIntf->query_vb({}, {index}, C2_DONT_BLOCK, &heapParams));
    ASSERT_EQ(1u, heapParams.size());
    value = ((C2PortBlockPoolsTuning*)heapParams[0].get())->m.values[0];
    ASSERT_EQ(configBlockPools[0], value);
}

TEST_F(C2VEACompIntfTest, TestUnsupportedParam) {
    C2ComponentTimeStretchTuning unsupportedParam;
    std::vector<C2Param*> stackParams{&unsupportedParam};
    ASSERT_EQ(C2_BAD_INDEX, mIntf->query_vb(stackParams, {}, C2_DONT_BLOCK, nullptr));
    EXPECT_EQ(0u, unsupportedParam.size());  // invalidated
}

TEST_F(C2VEACompIntfTest, TestAvcLevelDependency) {
    C2StreamProfileLevelInfo::output info;
    info.setStream(0);

    // Read out the default profile and level.
    std::vector<C2Param*> stackParams{&info};
    ASSERT_EQ(C2_OK, mIntf->query_vb(stackParams, {}, C2_DONT_BLOCK, nullptr));

    // The default profile should be the lowest one component could support, and we would expect
    // either BASELINE or MAIN is supported. In addition, profiles higher than HIGH will have
    // different bitrate limit for levels, we don't want to make this test too complicated.
    ASSERT_LT(info.profile, PROFILE_AVC_HIGH);

    // Set AVC level to 1.2.
    // Configure input size, framerate, and bitrate to values which are capable of level 1.2.
    C2StreamPictureSizeInfo::input videoSize(0u, 320, 240);
    C2StreamFrameRateInfo::output frameRate(0u, 15.);
    C2StreamBitrateInfo::output bitrate(0u, 384000);
    info.level = LEVEL_AVC_1_2;

    std::vector<C2Param*> params{&videoSize, &frameRate, &bitrate, &info};
    std::vector<std::unique_ptr<C2SettingResult>> failures;
    ASSERT_EQ(C2_OK, mIntf->config_vb(params, C2_DONT_BLOCK, &failures));
    EXPECT_EQ(0u, failures.size());

    // Check AVC level is 1.2.
    std::vector<std::unique_ptr<C2Param>> heapParams;
    C2Param::Index index = C2StreamProfileLevelInfo::output::PARAM_TYPE;
    ASSERT_EQ(C2_OK, mIntf->query_vb({}, {index}, C2_DONT_BLOCK, &heapParams));
    ASSERT_EQ(1u, heapParams.size());
    EXPECT_EQ(((C2StreamProfileLevelInfo*)heapParams[0].get())->level, LEVEL_AVC_1_2);

    // Configure input size, framerate, and bitrate to values which are capable of level 4.0.
    videoSize.width = 2048;
    videoSize.height = 1024;
    frameRate.value = 30;
    bitrate.value = 20000000;

    failures.clear();
    ASSERT_EQ(C2_OK, mIntf->config_vb(params, C2_DONT_BLOCK, &failures));
    EXPECT_EQ(0u, failures.size());

    // Check AVC level is adjusted to 4.0.
    heapParams.clear();
    ASSERT_EQ(C2_OK, mIntf->query_vb({}, {index}, C2_DONT_BLOCK, &heapParams));
    ASSERT_EQ(1u, heapParams.size());
    EXPECT_EQ(((C2StreamProfileLevelInfo*)heapParams[0].get())->level, LEVEL_AVC_4);
}

TEST_F(C2VEACompIntfTest, TestBug114332827) {
    // Use at least PROFILE_AVC_MAIN for 1080p input video and up. b/114332827

    // Config input video size to 1080p.
    C2StreamPictureSizeInfo::input videoSize(0u, 1920, 1080);

    std::vector<C2Param*> params{&videoSize};
    std::vector<std::unique_ptr<C2SettingResult>> failures;
    ASSERT_EQ(C2_OK, mIntf->config_vb(params, C2_DONT_BLOCK, &failures));
    EXPECT_EQ(0u, failures.size());

    // Query video size back to check it is 1080p.
    std::vector<std::unique_ptr<C2Param>> heapParams;
    C2Param::Index index = C2StreamPictureSizeInfo::input::PARAM_TYPE;
    ASSERT_EQ(C2_OK, mIntf->query_vb({}, {index}, C2_DONT_BLOCK, &heapParams));
    ASSERT_EQ(1u, heapParams.size());
    EXPECT_EQ(1920u, ((C2StreamPictureSizeInfo*)heapParams[0].get())->width);
    EXPECT_EQ(1080u, ((C2StreamPictureSizeInfo*)heapParams[0].get())->height);

    // Check profile should be PROFILE_AVC_MAIN or higher.
    heapParams.clear();
    index = C2StreamProfileLevelInfo::output::PARAM_TYPE;
    ASSERT_EQ(C2_OK, mIntf->query_vb({}, {index}, C2_DONT_BLOCK, &heapParams));
    ASSERT_EQ(1u, heapParams.size());
    EXPECT_GE(((C2StreamProfileLevelInfo*)heapParams[0].get())->profile, PROFILE_AVC_MAIN);
}

TEST_F(C2VEACompIntfTest, ParamReflector) {
    dumpParamDescriptions();
}
}  // namespace android
