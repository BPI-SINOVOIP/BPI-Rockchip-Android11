// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//#define LOG_NDEBUG 0
#define LOG_TAG "V4L2EncodeInterface"

#include <v4l2_codec2/components/V4L2EncodeInterface.h>

#include <inttypes.h>

#include <C2PlatformSupport.h>
#include <SimpleC2Interface.h>
#include <android/hardware/graphics/common/1.0/types.h>
#include <media/stagefright/MediaDefs.h>
#include <utils/Log.h>

#include <v4l2_device.h>
#include <v4l2_codec2/common/V4L2ComponentCommon.h>
#include <video_codecs.h>

using android::hardware::graphics::common::V1_0::BufferUsage;

namespace android {

namespace {

// Use basic linear block pool/allocator as default.
constexpr C2BlockPool::local_id_t kDefaultOutputBlockPool = C2BlockPool::BASIC_LINEAR;
// Default input and output allocators.
constexpr C2Allocator::id_t kDefaultInputAllocator = C2PlatformAllocatorStore::GRALLOC;
constexpr C2Allocator::id_t kDefaultOutputAllocator = C2PlatformAllocatorStore::BLOB;

// The default output framerate in frames per second.
// TODO: increase to 60 fps in the future.
constexpr float kDefaultFrameRate = 30.0;
// The default output bitrate in bits per second. Use the max bitrate of AVC Level1.0 as default.
constexpr uint32_t kDefaultBitrate = 64000;

// The maximal output bitrate in bits per second. It's the max bitrate of AVC Level4.1.
// TODO: increase this in the future for supporting higher level/resolution encoding.
constexpr uint32_t kMaxBitrate = 50000000;

// The frame size of 1080p video.
constexpr uint32_t kFrameSize1080P = 1920 * 1080;

C2Config::profile_t videoCodecProfileToC2Profile(media::VideoCodecProfile profile) {
    switch (profile) {
    case media::VideoCodecProfile::H264PROFILE_BASELINE:
        return C2Config::PROFILE_AVC_BASELINE;
    case media::VideoCodecProfile::H264PROFILE_MAIN:
        return C2Config::PROFILE_AVC_MAIN;
    case media::VideoCodecProfile::H264PROFILE_EXTENDED:
        return C2Config::PROFILE_AVC_EXTENDED;
    case media::VideoCodecProfile::H264PROFILE_HIGH:
        return C2Config::PROFILE_AVC_HIGH;
    case media::VideoCodecProfile::H264PROFILE_HIGH10PROFILE:
        return C2Config::PROFILE_AVC_HIGH_10;
    case media::VideoCodecProfile::H264PROFILE_HIGH422PROFILE:
        return C2Config::PROFILE_AVC_HIGH_422;
    case media::VideoCodecProfile::H264PROFILE_HIGH444PREDICTIVEPROFILE:
        return C2Config::PROFILE_AVC_HIGH_444_PREDICTIVE;
    case media::VideoCodecProfile::H264PROFILE_SCALABLEBASELINE:
        return C2Config::PROFILE_AVC_SCALABLE_BASELINE;
    case media::VideoCodecProfile::H264PROFILE_SCALABLEHIGH:
        return C2Config::PROFILE_AVC_SCALABLE_HIGH;
    case media::VideoCodecProfile::H264PROFILE_STEREOHIGH:
        return C2Config::PROFILE_AVC_STEREO_HIGH;
    case media::VideoCodecProfile::H264PROFILE_MULTIVIEWHIGH:
        return C2Config::PROFILE_AVC_MULTIVIEW_HIGH;
    default:
        ALOGE("Unrecognizable profile (value = %d)...", profile);
        return C2Config::PROFILE_UNUSED;
    }
}

std::optional<media::VideoCodec> getCodecFromComponentName(const std::string& name) {
    if (name == V4L2ComponentName::kH264Encoder)
        return media::VideoCodec::kCodecH264;

    ALOGE("Unknown name: %s", name.c_str());
    return std::nullopt;
}

}  // namespace

// static
C2R V4L2EncodeInterface::ProfileLevelSetter(bool mayBlock,
                                            C2P<C2StreamProfileLevelInfo::output>& info,
                                            const C2P<C2StreamPictureSizeInfo::input>& videoSize,
                                            const C2P<C2StreamFrameRateInfo::output>& frameRate,
                                            const C2P<C2StreamBitrateInfo::output>& bitrate) {
    (void)mayBlock;

    static C2Config::level_t lowestConfigLevel = C2Config::LEVEL_UNUSED;

    // Use at least PROFILE_AVC_MAIN as default for 1080p input video and up.
    // TODO (b/114332827): Find root cause of bad quality of Baseline encoding.
    C2Config::profile_t defaultMinProfile = C2Config::PROFILE_AVC_BASELINE;
    if (videoSize.v.width * videoSize.v.height >= kFrameSize1080P) {
        defaultMinProfile = C2Config::PROFILE_AVC_MAIN;
    }

    // Adopt default minimal profile instead if the requested profile is not supported, or lower
    // than the default minimal one.
    if (!info.F(info.v.profile).supportsAtAll(info.v.profile) ||
        info.v.profile < defaultMinProfile) {
        if (info.F(info.v.profile).supportsAtAll(defaultMinProfile)) {
            ALOGV("Set profile to default (%u) instead.", defaultMinProfile);
            info.set().profile = defaultMinProfile;
        } else {
            ALOGE("Unable to set either requested profile (%u) or default profile (%u).",
                  info.v.profile, defaultMinProfile);
            return C2R(C2SettingResultBuilder::BadValue(info.F(info.v.profile)));
        }
    }

    // Table A-1 in spec
    struct LevelLimits {
        C2Config::level_t level;
        float maxMBPS;   // max macroblock processing rate in macroblocks per second
        uint64_t maxFS;  // max frame size in macroblocks
        uint32_t maxBR;  // max video bitrate in bits per second
    };
    constexpr LevelLimits kLimits[] = {
            {C2Config::LEVEL_AVC_1, 1485, 99, 64000},
            {C2Config::LEVEL_AVC_1B, 1485, 99, 128000},
            {C2Config::LEVEL_AVC_1_1, 3000, 396, 192000},
            {C2Config::LEVEL_AVC_1_2, 6000, 396, 384000},
            {C2Config::LEVEL_AVC_1_3, 11880, 396, 768000},
            {C2Config::LEVEL_AVC_2, 11880, 396, 2000000},
            {C2Config::LEVEL_AVC_2_1, 19800, 792, 4000000},
            {C2Config::LEVEL_AVC_2_2, 20250, 1620, 4000000},
            {C2Config::LEVEL_AVC_3, 40500, 1620, 10000000},
            {C2Config::LEVEL_AVC_3_1, 108000, 3600, 14000000},
            {C2Config::LEVEL_AVC_3_2, 216000, 5120, 20000000},
            {C2Config::LEVEL_AVC_4, 245760, 8192, 20000000},
            {C2Config::LEVEL_AVC_4_1, 245760, 8192, 50000000},
            {C2Config::LEVEL_AVC_4_2, 522240, 8704, 50000000},
            {C2Config::LEVEL_AVC_5, 589824, 22080, 135000000},
            {C2Config::LEVEL_AVC_5_1, 983040, 36864, 240000000},
            {C2Config::LEVEL_AVC_5_2, 2073600, 36864, 240000000},
    };

    uint64_t targetFS =
            static_cast<uint64_t>((videoSize.v.width + 15) / 16) * ((videoSize.v.height + 15) / 16);
    float targetMBPS = static_cast<float>(targetFS) * frameRate.v.value;

    // Try the recorded lowest configed level. This level should become adaptable after input size,
    // frame rate, and bitrate are all set.
    if (lowestConfigLevel != C2Config::LEVEL_UNUSED && lowestConfigLevel < info.v.level) {
        info.set().level = lowestConfigLevel;
    }

    // Check if the supplied level meets the requirements. If not, update the level with the lowest
    // level meeting the requirements.

    bool found = false;
    bool needsUpdate = !info.F(info.v.level).supportsAtAll(info.v.level);
    for (const LevelLimits& limit : kLimits) {
        if (!info.F(info.v.level).supportsAtAll(limit.level)) {
            continue;
        }

        // Table A-2 in spec
        // The maximum bit rate for High Profile is 1.25 times that of the Base/Extended/Main
        // Profiles, 3 times for Hi10P, and 4 times for Hi422P/Hi444PP.
        uint32_t maxBR = limit.maxBR;
        if (info.v.profile >= C2Config::PROFILE_AVC_HIGH_422) {
            maxBR *= 4;
        } else if (info.v.profile >= C2Config::PROFILE_AVC_HIGH_10) {
            maxBR *= 3;
        } else if (info.v.profile >= C2Config::PROFILE_AVC_HIGH) {
            maxBR = maxBR * 5.0 / 4.0;
        }

        if (targetFS <= limit.maxFS && targetMBPS <= limit.maxMBPS && bitrate.v.value <= maxBR) {
            // This is the lowest level that meets the requirements, and if
            // we haven't seen the supplied level yet, that means we don't
            // need the update.
            if (needsUpdate) {
                // Since current config update is sequential, there is an issue when we want to set
                // lower level for small input size, frame rate, and bitrate, if we set level first,
                // it will be adjusted to a higher one because the default input size or others are
                // above the limit. Use lowestConfigLevel to record the level we have tried to
                // config (but failed).
                // TODO(johnylin): remove this solution after b/140407694 has proper fix.
                lowestConfigLevel = info.v.level;

                ALOGD("Given level %u does not cover current configuration: "
                      "adjusting to %u",
                      info.v.level, limit.level);
                info.set().level = limit.level;
            }
            found = true;
            break;
        }
        if (info.v.level <= limit.level) {
            // We break out of the loop when the lowest feasible level is found. The fact that we're
            // here means that our level doesn't meet the requirement and needs to be updated.
            needsUpdate = true;
        }
    }
    if (!found) {
        ALOGE("Unable to find proper level with current config, requested level (%u).",
              info.v.level);
        return C2R(C2SettingResultBuilder::BadValue(info.F(info.v.level)));
    }

    return C2R::Ok();
}

// static
C2R V4L2EncodeInterface::SizeSetter(bool mayBlock, C2P<C2StreamPictureSizeInfo::input>& videoSize) {
    (void)mayBlock;
    // TODO: maybe apply block limit?
    return videoSize.F(videoSize.v.width)
            .validatePossible(videoSize.v.width)
            .plus(videoSize.F(videoSize.v.height).validatePossible(videoSize.v.height));
}

// static
C2R V4L2EncodeInterface::IntraRefreshPeriodSetter(bool mayBlock,
                                                  C2P<C2StreamIntraRefreshTuning::output>& period) {
    (void)mayBlock;
    if (period.v.period < 1) {
        period.set().mode = C2Config::INTRA_REFRESH_DISABLED;
        period.set().period = 0;
    } else {
        // Only support arbitrary mode (cyclic in our case).
        period.set().mode = C2Config::INTRA_REFRESH_ARBITRARY;
    }
    return C2R::Ok();
}

V4L2EncodeInterface::V4L2EncodeInterface(
        const C2String& name, std::shared_ptr<C2ReflectorHelper> helper)
      : C2InterfaceHelper(std::move(helper)) {
    ALOGV("%s(%s)", __func__, name.c_str());

    setDerivedInstance(this);

    Initialize(name);
}

void V4L2EncodeInterface::Initialize(const C2String& name) {
    scoped_refptr<media::V4L2Device> device = media::V4L2Device::Create();
    if (!device) {
        ALOGE("Failed to create V4L2 device");
        mInitStatus = C2_CORRUPTED;
        return;
    }

    // Use type=unsigned int here, otherwise it will cause compile error in
    // C2F(mProfileLevel, profile).oneOf(profiles) since std::vector<C2Config::profile_t> cannot
    // convert to std::vector<unsigned int>.
    std::vector<unsigned int> profiles;
    media::Size maxSize;
    for (const auto& supportedProfile : device->GetSupportedEncodeProfiles()) {
        C2Config::profile_t profile = videoCodecProfileToC2Profile(supportedProfile.profile);
        if (profile == C2Config::PROFILE_UNUSED) {
            continue;  // neglect unrecognizable profile
        }
        ALOGV("Queried c2_profile = 0x%x : max_size = %d x %d", profile,
              supportedProfile.max_resolution.width(), supportedProfile.max_resolution.height());
        profiles.push_back(static_cast<unsigned int>(profile));
        maxSize.set_width(std::max(maxSize.width(), supportedProfile.max_resolution.width()));
        maxSize.set_height(std::max(maxSize.height(), supportedProfile.max_resolution.height()));
    }

    if (profiles.empty()) {
        ALOGD("No supported profiles");
        mInitStatus = C2_BAD_VALUE;
        return;
    }

    C2Config::profile_t minProfile =
            static_cast<C2Config::profile_t>(*std::min_element(profiles.begin(), profiles.end()));

    // Special note: the order of addParameter matters if your setters are dependent on other
    //               parameters. Please make sure the dependent parameters are added prior to the
    //               one needs the setter dependency.

    addParameter(DefineParam(mInputVisibleSize, C2_PARAMKEY_PICTURE_SIZE)
                         .withDefault(new C2StreamPictureSizeInfo::input(0u, 320, 240))
                         .withFields({
                                 C2F(mInputVisibleSize, width).inRange(2, maxSize.width(), 2),
                                 C2F(mInputVisibleSize, height).inRange(2, maxSize.height(), 2),
                         })
                         .withSetter(SizeSetter)
                         .build());

    addParameter(DefineParam(mFrameRate, C2_PARAMKEY_FRAME_RATE)
                         .withDefault(new C2StreamFrameRateInfo::output(0u, kDefaultFrameRate))
                         // TODO: More restriction?
                         .withFields({C2F(mFrameRate, value).greaterThan(0.)})
                         .withSetter(Setter<decltype(*mFrameRate)>::StrictValueWithNoDeps)
                         .build());

    addParameter(DefineParam(mBitrate, C2_PARAMKEY_BITRATE)
                         .withDefault(new C2StreamBitrateInfo::output(0u, kDefaultBitrate))
                         .withFields({C2F(mBitrate, value).inRange(0, kMaxBitrate)})
                         .withSetter(Setter<decltype(*mBitrate)>::StrictValueWithNoDeps)
                         .build());

    std::string outputMime;
    if (getCodecFromComponentName(name) == media::VideoCodec::kCodecH264) {
        outputMime = MEDIA_MIMETYPE_VIDEO_AVC;
        addParameter(
                DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                        .withDefault(new C2StreamProfileLevelInfo::output(0u, minProfile,
                                                                          C2Config::LEVEL_AVC_4_1))
                        .withFields(
                                {C2F(mProfileLevel, profile).oneOf(profiles),
                                 C2F(mProfileLevel, level)
                                         // TODO: query supported levels from adaptor.
                                         .oneOf({C2Config::LEVEL_AVC_1, C2Config::LEVEL_AVC_1B,
                                                 C2Config::LEVEL_AVC_1_1, C2Config::LEVEL_AVC_1_2,
                                                 C2Config::LEVEL_AVC_1_3, C2Config::LEVEL_AVC_2,
                                                 C2Config::LEVEL_AVC_2_1, C2Config::LEVEL_AVC_2_2,
                                                 C2Config::LEVEL_AVC_3, C2Config::LEVEL_AVC_3_1,
                                                 C2Config::LEVEL_AVC_3_2, C2Config::LEVEL_AVC_4,
                                                 C2Config::LEVEL_AVC_4_1, C2Config::LEVEL_AVC_5,
                                                 C2Config::LEVEL_AVC_5_1})})
                        .withSetter(ProfileLevelSetter, mInputVisibleSize, mFrameRate, mBitrate)
                        .build());
    } else {
        // TODO(johnylin): implement VP8/VP9 encoder in the future.
        ALOGE("Unsupported component name: %s", name.c_str());
        mInitStatus = C2_BAD_VALUE;
        return;
    }

    addParameter(
            DefineParam(mInputFormat, C2_PARAMKEY_INPUT_STREAM_BUFFER_TYPE)
                    .withConstValue(new C2StreamBufferTypeSetting::input(0u, C2BufferData::GRAPHIC))
                    .build());

    // TODO(b/167640667) Add VIDEO_ENCODER flag once input convertor is not enabled by default.
    // When using the format convertor (which is currently always enabled) it's not useful to add
    // the VIDEO_ENCODER buffer flag for input buffers here. Currently zero-copy is not supported
    // yet, so when using this flag an additional buffer will be allocated on host side and a copy
    // will be performed between the guest and host buffer to keep them in sync. This is wasteful as
    // the buffer is only used on guest side by the format convertor which converts and copies the
    // buffer into another buffer.
    //addParameter(DefineParam(mInputMemoryUsage, C2_PARAMKEY_INPUT_STREAM_USAGE)
    //                     .withConstValue(new C2StreamUsageTuning::input(
    //                             0u, static_cast<uint64_t>(BufferUsage::VIDEO_ENCODER)))
    //                     .build());

    addParameter(
            DefineParam(mOutputFormat, C2_PARAMKEY_OUTPUT_STREAM_BUFFER_TYPE)
                    .withConstValue(new C2StreamBufferTypeSetting::output(0u, C2BufferData::LINEAR))
                    .build());

    addParameter(DefineParam(mInputMediaType, C2_PARAMKEY_INPUT_MEDIA_TYPE)
                         .withConstValue(AllocSharedString<C2PortMediaTypeSetting::input>(
                                 MEDIA_MIMETYPE_VIDEO_RAW))
                         .build());

    addParameter(DefineParam(mOutputMediaType, C2_PARAMKEY_OUTPUT_MEDIA_TYPE)
                         .withConstValue(AllocSharedString<C2PortMediaTypeSetting::output>(
                                 outputMime.c_str()))
                         .build());

    addParameter(DefineParam(mIntraRefreshPeriod, C2_PARAMKEY_INTRA_REFRESH)
                         .withDefault(new C2StreamIntraRefreshTuning::output(
                                 0u, C2Config::INTRA_REFRESH_DISABLED, 0.))
                         .withFields({C2F(mIntraRefreshPeriod, mode)
                                              .oneOf({C2Config::INTRA_REFRESH_DISABLED,
                                                      C2Config::INTRA_REFRESH_ARBITRARY}),
                                      C2F(mIntraRefreshPeriod, period).any()})
                         .withSetter(IntraRefreshPeriodSetter)
                         .build());

    addParameter(DefineParam(mRequestKeyFrame, C2_PARAMKEY_REQUEST_SYNC_FRAME)
                         .withDefault(new C2StreamRequestSyncFrameTuning::output(0u, C2_FALSE))
                         .withFields({C2F(mRequestKeyFrame, value).oneOf({C2_FALSE, C2_TRUE})})
                         .withSetter(Setter<decltype(*mRequestKeyFrame)>::NonStrictValueWithNoDeps)
                         .build());

    addParameter(DefineParam(mKeyFramePeriodUs, C2_PARAMKEY_SYNC_FRAME_INTERVAL)
                         .withDefault(new C2StreamSyncFrameIntervalTuning::output(0u, 1000000))
                         .withFields({C2F(mKeyFramePeriodUs, value).any()})
                         .withSetter(Setter<decltype(*mKeyFramePeriodUs)>::StrictValueWithNoDeps)
                         .build());

    C2Allocator::id_t inputAllocators[] = {kDefaultInputAllocator};

    C2Allocator::id_t outputAllocators[] = {kDefaultOutputAllocator};

    addParameter(
            DefineParam(mInputAllocatorIds, C2_PARAMKEY_INPUT_ALLOCATORS)
                    .withConstValue(C2PortAllocatorsTuning::input::AllocShared(inputAllocators))
                    .build());

    addParameter(
            DefineParam(mOutputAllocatorIds, C2_PARAMKEY_OUTPUT_ALLOCATORS)
                    .withConstValue(C2PortAllocatorsTuning::output::AllocShared(outputAllocators))
                    .build());

    C2BlockPool::local_id_t outputBlockPools[] = {kDefaultOutputBlockPool};

    addParameter(
            DefineParam(mOutputBlockPoolIds, C2_PARAMKEY_OUTPUT_BLOCK_POOLS)
                    .withDefault(C2PortBlockPoolsTuning::output::AllocShared(outputBlockPools))
                    .withFields({C2F(mOutputBlockPoolIds, m.values[0]).any(),
                                 C2F(mOutputBlockPoolIds, m.values).inRange(0, 1)})
                    .withSetter(Setter<C2PortBlockPoolsTuning::output>::NonStrictValuesWithNoDeps)
                    .build());

    mInitStatus = C2_OK;
}

uint32_t V4L2EncodeInterface::getKeyFramePeriod() const {
    if (mKeyFramePeriodUs->value < 0 || mKeyFramePeriodUs->value == INT64_MAX) {
        return 0;
    }
    double period = mKeyFramePeriodUs->value / 1e6 * mFrameRate->value;
    return static_cast<uint32_t>(std::max(std::min(std::round(period), double(UINT32_MAX)), 1.));
}

}  // namespace android
