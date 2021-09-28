// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//#define LOG_NDEBUG 0
#define LOG_TAG "V4L2DecodeInterface"

#include <v4l2_codec2/components/V4L2DecodeInterface.h>

#include <C2PlatformSupport.h>
#include <SimpleC2Interface.h>
#include <android/hardware/graphics/common/1.0/types.h>
#include <log/log.h>
#include <media/stagefright/foundation/MediaDefs.h>

#include <v4l2_codec2/common/V4L2ComponentCommon.h>
#include <v4l2_codec2/plugin_store/V4L2AllocatorId.h>
#include <v4l2_device.h>

namespace android {
namespace {

constexpr size_t k1080pArea = 1920 * 1088;
constexpr size_t k4KArea = 3840 * 2160;
// Input bitstream buffer size for up to 1080p streams.
constexpr size_t kInputBufferSizeFor1080p = 1024 * 1024;  // 1MB
// Input bitstream buffer size for up to 4k streams.
constexpr size_t kInputBufferSizeFor4K = 4 * kInputBufferSizeFor1080p;

std::optional<VideoCodec> getCodecFromComponentName(const std::string& name) {
    if (name == V4L2ComponentName::kH264Decoder || name == V4L2ComponentName::kH264SecureDecoder)
        return VideoCodec::H264;
    if (name == V4L2ComponentName::kVP8Decoder || name == V4L2ComponentName::kVP8SecureDecoder)
        return VideoCodec::VP8;
    if (name == V4L2ComponentName::kVP9Decoder || name == V4L2ComponentName::kVP9SecureDecoder)
        return VideoCodec::VP9;

    ALOGE("Unknown name: %s", name.c_str());
    return std::nullopt;
}

size_t calculateInputBufferSize(size_t area) {
    if (area > k4KArea) {
        ALOGW("Input buffer size for video size (%zu) larger than 4K (%zu) might be too small.",
              area, k4KArea);
    }

    // Enlarge the input buffer for 4k video
    if (area > k1080pArea) return kInputBufferSizeFor4K;
    return kInputBufferSizeFor1080p;
}

uint32_t getOutputDelay(VideoCodec codec) {
    switch (codec) {
    case VideoCodec::H264:
        // Due to frame reordering an H264 decoder might need multiple additional input frames to be
        // queued before being able to output the associated decoded buffers. We need to tell the
        // codec2 framework that it should not stop queuing new work items until the maximum number
        // of frame reordering is reached, to avoid stalling the decoder.
        return 16;
    case VideoCodec::VP8:
        return 0;
    case VideoCodec::VP9:
        return 0;
    }
}

}  // namespace

// static
C2R V4L2DecodeInterface::ProfileLevelSetter(bool /* mayBlock */,
                                            C2P<C2StreamProfileLevelInfo::input>& info) {
    return info.F(info.v.profile)
            .validatePossible(info.v.profile)
            .plus(info.F(info.v.level).validatePossible(info.v.level));
}

// static
C2R V4L2DecodeInterface::SizeSetter(bool /* mayBlock */,
                                    C2P<C2StreamPictureSizeInfo::output>& videoSize) {
    return videoSize.F(videoSize.v.width)
            .validatePossible(videoSize.v.width)
            .plus(videoSize.F(videoSize.v.height).validatePossible(videoSize.v.height));
}

// static
template <typename T>
C2R V4L2DecodeInterface::DefaultColorAspectsSetter(bool /* mayBlock */, C2P<T>& def) {
    if (def.v.range > C2Color::RANGE_OTHER) {
        def.set().range = C2Color::RANGE_OTHER;
    }
    if (def.v.primaries > C2Color::PRIMARIES_OTHER) {
        def.set().primaries = C2Color::PRIMARIES_OTHER;
    }
    if (def.v.transfer > C2Color::TRANSFER_OTHER) {
        def.set().transfer = C2Color::TRANSFER_OTHER;
    }
    if (def.v.matrix > C2Color::MATRIX_OTHER) {
        def.set().matrix = C2Color::MATRIX_OTHER;
    }
    return C2R::Ok();
}

// static
C2R V4L2DecodeInterface::MergedColorAspectsSetter(
        bool /* mayBlock */, C2P<C2StreamColorAspectsInfo::output>& merged,
        const C2P<C2StreamColorAspectsTuning::output>& def,
        const C2P<C2StreamColorAspectsInfo::input>& coded) {
    // Take coded values for all specified fields, and default values for unspecified ones.
    merged.set().range = coded.v.range == RANGE_UNSPECIFIED ? def.v.range : coded.v.range;
    merged.set().primaries =
            coded.v.primaries == PRIMARIES_UNSPECIFIED ? def.v.primaries : coded.v.primaries;
    merged.set().transfer =
            coded.v.transfer == TRANSFER_UNSPECIFIED ? def.v.transfer : coded.v.transfer;
    merged.set().matrix = coded.v.matrix == MATRIX_UNSPECIFIED ? def.v.matrix : coded.v.matrix;
    return C2R::Ok();
}

// static
C2R V4L2DecodeInterface::MaxInputBufferSizeCalculator(
        bool /* mayBlock */, C2P<C2StreamMaxBufferSizeInfo::input>& me,
        const C2P<C2StreamPictureSizeInfo::output>& size) {
    me.set().value = calculateInputBufferSize(size.v.width * size.v.height);
    return C2R::Ok();
}

V4L2DecodeInterface::V4L2DecodeInterface(const std::string& name,
                                         const std::shared_ptr<C2ReflectorHelper>& helper)
      : C2InterfaceHelper(helper), mInitStatus(C2_OK) {
    ALOGV("%s(%s)", __func__, name.c_str());

    setDerivedInstance(this);

    mVideoCodec = getCodecFromComponentName(name);
    if (!mVideoCodec) {
        ALOGE("Invalid component name: %s", name.c_str());
        mInitStatus = C2_BAD_VALUE;
        return;
    }

    std::string inputMime;
    switch (*mVideoCodec) {
    case VideoCodec::H264:
        inputMime = MEDIA_MIMETYPE_VIDEO_AVC;
        addParameter(
                DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                        .withDefault(new C2StreamProfileLevelInfo::input(
                                0u, C2Config::PROFILE_AVC_MAIN, C2Config::LEVEL_AVC_4))
                        .withFields(
                                {C2F(mProfileLevel, profile)
                                         .oneOf({C2Config::PROFILE_AVC_BASELINE,
                                                 C2Config::PROFILE_AVC_CONSTRAINED_BASELINE,
                                                 C2Config::PROFILE_AVC_MAIN,
                                                 C2Config::PROFILE_AVC_HIGH,
                                                 C2Config::PROFILE_AVC_CONSTRAINED_HIGH}),
                                 C2F(mProfileLevel, level)
                                         .oneOf({C2Config::LEVEL_AVC_1, C2Config::LEVEL_AVC_1B,
                                                 C2Config::LEVEL_AVC_1_1, C2Config::LEVEL_AVC_1_2,
                                                 C2Config::LEVEL_AVC_1_3, C2Config::LEVEL_AVC_2,
                                                 C2Config::LEVEL_AVC_2_1, C2Config::LEVEL_AVC_2_2,
                                                 C2Config::LEVEL_AVC_3, C2Config::LEVEL_AVC_3_1,
                                                 C2Config::LEVEL_AVC_3_2, C2Config::LEVEL_AVC_4,
                                                 C2Config::LEVEL_AVC_4_1, C2Config::LEVEL_AVC_4_2,
                                                 C2Config::LEVEL_AVC_5, C2Config::LEVEL_AVC_5_1,
                                                 C2Config::LEVEL_AVC_5_2})})
                        .withSetter(ProfileLevelSetter)
                        .build());
        break;

    case VideoCodec::VP8:
        inputMime = MEDIA_MIMETYPE_VIDEO_VP8;
        addParameter(DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                             .withConstValue(new C2StreamProfileLevelInfo::input(
                                     0u, C2Config::PROFILE_UNUSED, C2Config::LEVEL_UNUSED))
                             .build());
        break;

    case VideoCodec::VP9:
        inputMime = MEDIA_MIMETYPE_VIDEO_VP9;
        addParameter(
                DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                        .withDefault(new C2StreamProfileLevelInfo::input(
                                0u, C2Config::PROFILE_VP9_0, C2Config::LEVEL_VP9_5))
                        .withFields({C2F(mProfileLevel, profile).oneOf({C2Config::PROFILE_VP9_0}),
                                     C2F(mProfileLevel, level)
                                             .oneOf({C2Config::LEVEL_VP9_1, C2Config::LEVEL_VP9_1_1,
                                                     C2Config::LEVEL_VP9_2, C2Config::LEVEL_VP9_2_1,
                                                     C2Config::LEVEL_VP9_3, C2Config::LEVEL_VP9_3_1,
                                                     C2Config::LEVEL_VP9_4, C2Config::LEVEL_VP9_4_1,
                                                     C2Config::LEVEL_VP9_5})})
                        .withSetter(ProfileLevelSetter)
                        .build());
        break;
    }

    addParameter(
            DefineParam(mInputFormat, C2_PARAMKEY_INPUT_STREAM_BUFFER_TYPE)
                    .withConstValue(new C2StreamBufferTypeSetting::input(0u, C2BufferData::LINEAR))
                    .build());
    addParameter(
            DefineParam(mInputMemoryUsage, C2_PARAMKEY_INPUT_STREAM_USAGE)
                    .withConstValue(new C2StreamUsageTuning::input(
                            0u, static_cast<uint64_t>(android::hardware::graphics::common::V1_0::
                                                              BufferUsage::VIDEO_DECODER)))
                    .build());

    addParameter(DefineParam(mOutputFormat, C2_PARAMKEY_OUTPUT_STREAM_BUFFER_TYPE)
                         .withConstValue(
                                 new C2StreamBufferTypeSetting::output(0u, C2BufferData::GRAPHIC))
                         .build());
    addParameter(
            DefineParam(mOutputDelay, C2_PARAMKEY_OUTPUT_DELAY)
                    .withConstValue(new C2PortDelayTuning::output(getOutputDelay(*mVideoCodec)))
                    .build());

    addParameter(DefineParam(mInputMediaType, C2_PARAMKEY_INPUT_MEDIA_TYPE)
                         .withConstValue(AllocSharedString<C2PortMediaTypeSetting::input>(
                                 inputMime.c_str()))
                         .build());

    addParameter(DefineParam(mOutputMediaType, C2_PARAMKEY_OUTPUT_MEDIA_TYPE)
                         .withConstValue(AllocSharedString<C2PortMediaTypeSetting::output>(
                                 MEDIA_MIMETYPE_VIDEO_RAW))
                         .build());

    // Note(b/165826281): The check is not used at Android framework currently.
    // In order to fasten the bootup time, we use the maximum supported size instead of querying the
    // capability from the V4L2 device.
    addParameter(DefineParam(mSize, C2_PARAMKEY_PICTURE_SIZE)
                         .withDefault(new C2StreamPictureSizeInfo::output(0u, 320, 240))
                         .withFields({
                                 C2F(mSize, width).inRange(16, 4096, 16),
                                 C2F(mSize, height).inRange(16, 4096, 16),
                         })
                         .withSetter(SizeSetter)
                         .build());

    addParameter(
            DefineParam(mMaxInputSize, C2_PARAMKEY_INPUT_MAX_BUFFER_SIZE)
                    .withDefault(new C2StreamMaxBufferSizeInfo::input(0u, kInputBufferSizeFor1080p))
                    .withFields({
                            C2F(mMaxInputSize, value).any(),
                    })
                    .calculatedAs(MaxInputBufferSizeCalculator, mSize)
                    .build());

    bool secureMode = name.find(".secure") != std::string::npos;
    const C2Allocator::id_t inputAllocators[] = {secureMode ? V4L2AllocatorId::SECURE_LINEAR
                                                            : C2PlatformAllocatorStore::BLOB};

    const C2Allocator::id_t outputAllocators[] = {V4L2AllocatorId::V4L2_BUFFERPOOL};
    const C2Allocator::id_t surfaceAllocator =
            secureMode ? V4L2AllocatorId::SECURE_GRAPHIC : V4L2AllocatorId::V4L2_BUFFERQUEUE;
    const C2BlockPool::local_id_t outputBlockPools[] = {C2BlockPool::BASIC_GRAPHIC};

    addParameter(
            DefineParam(mInputAllocatorIds, C2_PARAMKEY_INPUT_ALLOCATORS)
                    .withConstValue(C2PortAllocatorsTuning::input::AllocShared(inputAllocators))
                    .build());

    addParameter(
            DefineParam(mOutputAllocatorIds, C2_PARAMKEY_OUTPUT_ALLOCATORS)
                    .withConstValue(C2PortAllocatorsTuning::output::AllocShared(outputAllocators))
                    .build());

    addParameter(DefineParam(mOutputSurfaceAllocatorId, C2_PARAMKEY_OUTPUT_SURFACE_ALLOCATOR)
                         .withConstValue(new C2PortSurfaceAllocatorTuning::output(surfaceAllocator))
                         .build());

    addParameter(
            DefineParam(mOutputBlockPoolIds, C2_PARAMKEY_OUTPUT_BLOCK_POOLS)
                    .withDefault(C2PortBlockPoolsTuning::output::AllocShared(outputBlockPools))
                    .withFields({C2F(mOutputBlockPoolIds, m.values[0]).any(),
                                 C2F(mOutputBlockPoolIds, m.values).inRange(0, 1)})
                    .withSetter(Setter<C2PortBlockPoolsTuning::output>::NonStrictValuesWithNoDeps)
                    .build());

    addParameter(
            DefineParam(mDefaultColorAspects, C2_PARAMKEY_DEFAULT_COLOR_ASPECTS)
                    .withDefault(new C2StreamColorAspectsTuning::output(
                            0u, C2Color::RANGE_UNSPECIFIED, C2Color::PRIMARIES_UNSPECIFIED,
                            C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED))
                    .withFields(
                            {C2F(mDefaultColorAspects, range)
                                     .inRange(C2Color::RANGE_UNSPECIFIED, C2Color::RANGE_OTHER),
                             C2F(mDefaultColorAspects, primaries)
                                     .inRange(C2Color::PRIMARIES_UNSPECIFIED,
                                              C2Color::PRIMARIES_OTHER),
                             C2F(mDefaultColorAspects, transfer)
                                     .inRange(C2Color::TRANSFER_UNSPECIFIED,
                                              C2Color::TRANSFER_OTHER),
                             C2F(mDefaultColorAspects, matrix)
                                     .inRange(C2Color::MATRIX_UNSPECIFIED, C2Color::MATRIX_OTHER)})
                    .withSetter(DefaultColorAspectsSetter)
                    .build());

    addParameter(
            DefineParam(mCodedColorAspects, C2_PARAMKEY_VUI_COLOR_ASPECTS)
                    .withDefault(new C2StreamColorAspectsInfo::input(
                            0u, C2Color::RANGE_LIMITED, C2Color::PRIMARIES_UNSPECIFIED,
                            C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED))
                    .withFields(
                            {C2F(mCodedColorAspects, range)
                                     .inRange(C2Color::RANGE_UNSPECIFIED, C2Color::RANGE_OTHER),
                             C2F(mCodedColorAspects, primaries)
                                     .inRange(C2Color::PRIMARIES_UNSPECIFIED,
                                              C2Color::PRIMARIES_OTHER),
                             C2F(mCodedColorAspects, transfer)
                                     .inRange(C2Color::TRANSFER_UNSPECIFIED,
                                              C2Color::TRANSFER_OTHER),
                             C2F(mCodedColorAspects, matrix)
                                     .inRange(C2Color::MATRIX_UNSPECIFIED, C2Color::MATRIX_OTHER)})
                    .withSetter(DefaultColorAspectsSetter)
                    .build());

    addParameter(
            DefineParam(mColorAspects, C2_PARAMKEY_COLOR_ASPECTS)
                    .withDefault(new C2StreamColorAspectsInfo::output(
                            0u, C2Color::RANGE_UNSPECIFIED, C2Color::PRIMARIES_UNSPECIFIED,
                            C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED))
                    .withFields(
                            {C2F(mColorAspects, range)
                                     .inRange(C2Color::RANGE_UNSPECIFIED, C2Color::RANGE_OTHER),
                             C2F(mColorAspects, primaries)
                                     .inRange(C2Color::PRIMARIES_UNSPECIFIED,
                                              C2Color::PRIMARIES_OTHER),
                             C2F(mColorAspects, transfer)
                                     .inRange(C2Color::TRANSFER_UNSPECIFIED,
                                              C2Color::TRANSFER_OTHER),
                             C2F(mColorAspects, matrix)
                                     .inRange(C2Color::MATRIX_UNSPECIFIED, C2Color::MATRIX_OTHER)})
                    .withSetter(MergedColorAspectsSetter, mDefaultColorAspects, mCodedColorAspects)
                    .build());
}

size_t V4L2DecodeInterface::getInputBufferSize() const {
    return calculateInputBufferSize(getMaxSize().GetArea());
}

c2_status_t V4L2DecodeInterface::queryColorAspects(
        std::shared_ptr<C2StreamColorAspectsInfo::output>* targetColorAspects) {
    std::unique_ptr<C2StreamColorAspectsInfo::output> colorAspects =
            std::make_unique<C2StreamColorAspectsInfo::output>(
                    0u, C2Color::RANGE_UNSPECIFIED, C2Color::PRIMARIES_UNSPECIFIED,
                    C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED);
    c2_status_t status = query({colorAspects.get()}, {}, C2_DONT_BLOCK, nullptr);
    if (status == C2_OK) {
        *targetColorAspects = std::move(colorAspects);
    }
    return status;
}

}  // namespace android
