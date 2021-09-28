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

package android.mediav2.cts;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.util.Log;
import android.util.Pair;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;

import static android.media.MediaCodecInfo.CodecProfileLevel.*;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

/**
 * Validate profile and level configuration for listed encoder components
 */
@RunWith(Parameterized.class)
public class EncoderProfileLevelTest extends CodecEncoderTestBase {
    private static final String LOG_TAG = EncoderProfileLevelTest.class.getSimpleName();
    private static final HashMap<String, int[]> mProfileMap = new HashMap<>();
    private static final HashMap<String, Pair<int[], Integer>> mProfileLevelCdd = new HashMap<>();

    private MediaFormat mConfigFormat;

    public EncoderProfileLevelTest(String mime, int bitrate, int encoderInfo1, int encoderInfo2,
            int frameRate) {
        super(mime);
        mConfigFormat = new MediaFormat();
        mConfigFormat.setString(MediaFormat.KEY_MIME, mMime);
        mConfigFormat.setInteger(MediaFormat.KEY_BIT_RATE, bitrate);
        if (mIsAudio) {
            mSampleRate = encoderInfo1;
            mChannels = encoderInfo2;
            mConfigFormat.setInteger(MediaFormat.KEY_SAMPLE_RATE, mSampleRate);
            mConfigFormat.setInteger(MediaFormat.KEY_CHANNEL_COUNT, mChannels);
        } else {
            mWidth = encoderInfo1;
            mHeight = encoderInfo2;
            mFrameRate = frameRate;
            mConfigFormat.setInteger(MediaFormat.KEY_WIDTH, mWidth);
            mConfigFormat.setInteger(MediaFormat.KEY_HEIGHT, mHeight);
            mConfigFormat.setInteger(MediaFormat.KEY_FRAME_RATE, mFrameRate);
            mConfigFormat.setFloat(MediaFormat.KEY_I_FRAME_INTERVAL, 1.0f);
            mConfigFormat.setInteger(MediaFormat.KEY_COLOR_FORMAT,
                    MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible);
        }
    }

    @Parameterized.Parameters(name = "{index}({0})")
    public static Collection<Object[]> input() {
        ArrayList<String> cddRequiredMimeList = new ArrayList<>();
        if (isHandheld() || isTv() || isAutomotive()) {
            // sec 2.2.2, 2.3.2, 2.5.2
            cddRequiredMimeList.add(MediaFormat.MIMETYPE_AUDIO_AAC);
            cddRequiredMimeList.add(MediaFormat.MIMETYPE_VIDEO_AVC);
            cddRequiredMimeList.add(MediaFormat.MIMETYPE_VIDEO_VP8);
        }
        final List<Object[]> exhaustiveArgsList = Arrays.asList(new Object[][]{
                // Audio - CodecMime, bit-rate, sample rate, channel count
                {MediaFormat.MIMETYPE_AUDIO_AAC, 64000, 48000, 1, -1},
                {MediaFormat.MIMETYPE_AUDIO_AAC, 128000, 48000, 2, -1},

                // Video - CodecMime, bit-rate, height, width, frame-rate
                // TODO (b/151423508)
                /*{MediaFormat.MIMETYPE_VIDEO_AVC, 64000, 176, 144, 15},
                {MediaFormat.MIMETYPE_VIDEO_AVC, 128000, 176, 144, 15},
                {MediaFormat.MIMETYPE_VIDEO_AVC, 192000, 352, 288, 7},
                {MediaFormat.MIMETYPE_VIDEO_AVC, 384000, 352, 288, 15},*/
                {MediaFormat.MIMETYPE_VIDEO_AVC, 768000, 352, 288, 30},
                {MediaFormat.MIMETYPE_VIDEO_AVC, 2000000, 352, 288, 30},
                // TODO (b/151423508)
                /*{MediaFormat.MIMETYPE_VIDEO_AVC, 4000000, 352, 576, 25},
                {MediaFormat.MIMETYPE_VIDEO_AVC, 4000000, 720, 576, 12},
                {MediaFormat.MIMETYPE_VIDEO_AVC, 10000000, 720, 576, 25},*/
                {MediaFormat.MIMETYPE_VIDEO_AVC, 14000000, 1280, 720, 30},
                {MediaFormat.MIMETYPE_VIDEO_AVC, 20000000, 1280, 1024, 42},
                {MediaFormat.MIMETYPE_VIDEO_AVC, 20000000, 2048, 1024, 30},
                {MediaFormat.MIMETYPE_VIDEO_AVC, 50000000, 2048, 1024, 30},
                {MediaFormat.MIMETYPE_VIDEO_AVC, 50000000, 2048, 1080, 60},
                {MediaFormat.MIMETYPE_VIDEO_AVC, 135000000, 3672, 1536, 25},
                {MediaFormat.MIMETYPE_VIDEO_AVC, 240000000, 4096, 2304, 25},
                {MediaFormat.MIMETYPE_VIDEO_AVC, 240000000, 4096, 2304, 50},
                {MediaFormat.MIMETYPE_VIDEO_AVC, 240000000, 8192, 4320, 30},
                {MediaFormat.MIMETYPE_VIDEO_AVC, 480000000, 8192, 4320, 60},
                {MediaFormat.MIMETYPE_VIDEO_AVC, 800000000, 8192, 4320, 120},

                {MediaFormat.MIMETYPE_VIDEO_MPEG2, 4000000, 352, 288, 30},
                {MediaFormat.MIMETYPE_VIDEO_MPEG2, 15000000, 720, 576, 30},
                {MediaFormat.MIMETYPE_VIDEO_MPEG2, 60000000, 1440, 1088, 60},
                {MediaFormat.MIMETYPE_VIDEO_MPEG2, 80000000, 1920, 1088, 60},
                {MediaFormat.MIMETYPE_VIDEO_MPEG2, 80000000, 1920, 1088, 60},

                {MediaFormat.MIMETYPE_VIDEO_MPEG4, 64000, 176, 144, 15},
                {MediaFormat.MIMETYPE_VIDEO_MPEG4, 64000, 176, 144, 30},
                {MediaFormat.MIMETYPE_VIDEO_MPEG4, 128000, 176, 144, 15},
                {MediaFormat.MIMETYPE_VIDEO_MPEG4, 128000, 352, 288, 30},
                {MediaFormat.MIMETYPE_VIDEO_MPEG4, 384000, 352, 288, 30},
                {MediaFormat.MIMETYPE_VIDEO_MPEG4, 4000000, 640, 480, 30},
                {MediaFormat.MIMETYPE_VIDEO_MPEG4, 8000000, 720, 576, 30},
                {MediaFormat.MIMETYPE_VIDEO_MPEG4, 12000000, 1280, 720, 30},
                {MediaFormat.MIMETYPE_VIDEO_MPEG4, 128000, 176, 144, 30},
                {MediaFormat.MIMETYPE_VIDEO_MPEG4, 384000, 352, 288, 30},
                {MediaFormat.MIMETYPE_VIDEO_MPEG4, 768000, 352, 288, 30},
                {MediaFormat.MIMETYPE_VIDEO_MPEG4, 1500000, 352, 288, 30},
                {MediaFormat.MIMETYPE_VIDEO_MPEG4, 3000000, 704, 576, 30},
                {MediaFormat.MIMETYPE_VIDEO_MPEG4, 8000000, 720, 576, 30},

                // TODO (b/151430764)
                /*{MediaFormat.MIMETYPE_VIDEO_VP9, 200000, 256, 144, 15},
                {MediaFormat.MIMETYPE_VIDEO_VP9, 8000000, 384, 192, 30},
                {MediaFormat.MIMETYPE_VIDEO_VP9, 1800000, 480, 256, 30},
                {MediaFormat.MIMETYPE_VIDEO_VP9, 3600000, 640, 384, 30},
                {MediaFormat.MIMETYPE_VIDEO_VP9, 7200000, 1080, 512, 30},*/
                {MediaFormat.MIMETYPE_VIDEO_VP9, 12000000, 1280, 768, 30},
                {MediaFormat.MIMETYPE_VIDEO_VP9, 18000000, 2048, 1088, 30},
                {MediaFormat.MIMETYPE_VIDEO_VP9, 30000000, 2048, 1088, 60},
                {MediaFormat.MIMETYPE_VIDEO_VP9, 60000000, 4096, 2176, 30},
                {MediaFormat.MIMETYPE_VIDEO_VP9, 120000000, 4096, 2176, 60},
                {MediaFormat.MIMETYPE_VIDEO_VP9, 180000000, 4096, 2176, 120},
                {MediaFormat.MIMETYPE_VIDEO_VP9, 180000000, 8192, 4352, 30},
                {MediaFormat.MIMETYPE_VIDEO_VP9, 240000000, 8192, 4352, 60},
                {MediaFormat.MIMETYPE_VIDEO_VP9, 480000000, 8192, 4352, 120},

                {MediaFormat.MIMETYPE_VIDEO_H263, 64000, 176, 144, 15},
                {MediaFormat.MIMETYPE_VIDEO_H263, 128000, 176, 144, 15},
                {MediaFormat.MIMETYPE_VIDEO_H263, 128000, 176, 144, 30},
                {MediaFormat.MIMETYPE_VIDEO_H263, 128000, 352, 288, 15},
                {MediaFormat.MIMETYPE_VIDEO_H263, 384000, 352, 288, 30},
                {MediaFormat.MIMETYPE_VIDEO_H263, 2048000, 352, 288, 30},
                {MediaFormat.MIMETYPE_VIDEO_H263, 4096000, 352, 240, 60},
                {MediaFormat.MIMETYPE_VIDEO_H263, 4096000, 352, 288, 50},
                {MediaFormat.MIMETYPE_VIDEO_H263, 8192000, 720, 240, 60},
                {MediaFormat.MIMETYPE_VIDEO_H263, 8192000, 720, 288, 50},
                {MediaFormat.MIMETYPE_VIDEO_H263, 16384000, 720, 480, 60},
                {MediaFormat.MIMETYPE_VIDEO_H263, 16384000, 720, 576, 50},

                // TODO (b/151429828)
                //{MediaFormat.MIMETYPE_VIDEO_HEVC, 128000, 176, 144, 15},
                {MediaFormat.MIMETYPE_VIDEO_HEVC, 1500000, 352, 288, 30},
                // TODO (b/152576008) - Limit HEVC Encoder test to 512x512
                {MediaFormat.MIMETYPE_VIDEO_HEVC, 3000000, 512, 512, 30},
                //{MediaFormat.MIMETYPE_VIDEO_HEVC, 3000000, 640, 360, 30},
                //{MediaFormat.MIMETYPE_VIDEO_HEVC, 6000000, 960, 540, 30},
                {MediaFormat.MIMETYPE_VIDEO_HEVC, 10000000, 1280, 720, 33},
                {MediaFormat.MIMETYPE_VIDEO_HEVC, 12000000, 2048, 1080, 30},
                {MediaFormat.MIMETYPE_VIDEO_HEVC, 20000000, 2048, 1080, 60},
                {MediaFormat.MIMETYPE_VIDEO_HEVC, 25000000, 4096, 2160, 30},
                {MediaFormat.MIMETYPE_VIDEO_HEVC, 40000000, 4096, 2160, 60},
                {MediaFormat.MIMETYPE_VIDEO_HEVC, 60000000, 4096, 2160, 120},
                {MediaFormat.MIMETYPE_VIDEO_HEVC, 60000000, 8192, 4320, 30},
                {MediaFormat.MIMETYPE_VIDEO_HEVC, 120000000, 8192, 4320, 60},
                {MediaFormat.MIMETYPE_VIDEO_HEVC, 240000000, 8192, 4320, 120},

                {MediaFormat.MIMETYPE_VIDEO_AV1, 1500000, 426, 240, 30},
                {MediaFormat.MIMETYPE_VIDEO_AV1, 3000000, 640, 360, 30},
                {MediaFormat.MIMETYPE_VIDEO_AV1, 6000000, 854, 480, 30},
                {MediaFormat.MIMETYPE_VIDEO_AV1, 10000000, 1280, 720, 30},
                {MediaFormat.MIMETYPE_VIDEO_AV1, 12000000, 1920, 1080, 30},
                {MediaFormat.MIMETYPE_VIDEO_AV1, 20000000, 1920, 1080, 60},
                {MediaFormat.MIMETYPE_VIDEO_AV1, 30000000, 3840, 2160, 30},
                {MediaFormat.MIMETYPE_VIDEO_AV1, 40000000, 3840, 2160, 60},
                {MediaFormat.MIMETYPE_VIDEO_AV1, 60000000, 3840, 2160, 120},
                {MediaFormat.MIMETYPE_VIDEO_AV1, 60000000, 7680, 4320, 30},
                {MediaFormat.MIMETYPE_VIDEO_AV1, 100000000, 7680, 4320, 60},
                {MediaFormat.MIMETYPE_VIDEO_AV1, 160000000, 7680, 4320, 120},

                {MediaFormat.MIMETYPE_VIDEO_VP8, 512000, 176, 144, 20},
                {MediaFormat.MIMETYPE_VIDEO_VP8, 512000, 480, 360, 20},
        });
        return prepareParamList(cddRequiredMimeList, exhaustiveArgsList, true);
    }

    static {
        mProfileMap.put(MediaFormat.MIMETYPE_VIDEO_AVC,
                new int[]{AVCProfileBaseline, AVCProfileMain, AVCProfileExtended, AVCProfileHigh,
                        AVCProfileHigh10, AVCProfileHigh422, AVCProfileHigh444,
                        AVCProfileConstrainedBaseline, AVCProfileConstrainedHigh});
        mProfileMap.put(MediaFormat.MIMETYPE_VIDEO_HEVC,
                new int[]{HEVCProfileMain, HEVCProfileMain10, HEVCProfileMainStill,
                        HEVCProfileMain10HDR10, HEVCProfileMain10HDR10Plus});
        mProfileMap.put(MediaFormat.MIMETYPE_VIDEO_H263,
                new int[]{H263ProfileBaseline, H263ProfileH320Coding,
                        H263ProfileBackwardCompatible, H263ProfileISWV2, H263ProfileISWV3,
                        H263ProfileHighCompression, H263ProfileInternet, H263ProfileInterlace,
                        H263ProfileHighLatency});
        mProfileMap.put(MediaFormat.MIMETYPE_VIDEO_MPEG2,
                new int[]{MPEG2ProfileSimple, MPEG2ProfileMain, MPEG2Profile422, MPEG2ProfileSNR,
                        MPEG2ProfileSpatial, MPEG2ProfileHigh});
        mProfileMap.put(MediaFormat.MIMETYPE_VIDEO_MPEG4,
                new int[]{MPEG4ProfileSimple, MPEG4ProfileSimpleScalable, MPEG4ProfileCore,
                        MPEG4ProfileMain, MPEG4ProfileNbit, MPEG4ProfileScalableTexture,
                        MPEG4ProfileSimpleFace, MPEG4ProfileSimpleFBA, MPEG4ProfileBasicAnimated,
                        MPEG4ProfileHybrid, MPEG4ProfileAdvancedRealTime,
                        MPEG4ProfileCoreScalable, MPEG4ProfileAdvancedCoding,
                        MPEG4ProfileAdvancedCore, MPEG4ProfileAdvancedScalable,
                        MPEG4ProfileAdvancedSimple});
        mProfileMap.put(MediaFormat.MIMETYPE_VIDEO_VP8, new int[]{VP8ProfileMain});
        mProfileMap.put(MediaFormat.MIMETYPE_VIDEO_VP9,
                new int[]{VP9Profile0, VP9Profile1, VP9Profile2, VP9Profile3, VP9Profile2HDR,
                        VP9Profile3HDR, VP9Profile2HDR10Plus, VP9Profile3HDR10Plus});
        mProfileMap.put(MediaFormat.MIMETYPE_VIDEO_AV1,
                new int[]{AV1ProfileMain8, AV1ProfileMain10, AV1ProfileMain10HDR10,
                        AV1ProfileMain10HDR10Plus});
        mProfileMap.put(MediaFormat.MIMETYPE_AUDIO_AAC,
                new int[]{AACObjectMain, AACObjectLC, AACObjectSSR, AACObjectLTP, AACObjectHE,
                        AACObjectScalable, AACObjectERLC, AACObjectERScalable, AACObjectLD,
                        AACObjectELD, AACObjectXHE});
    }

    static {
        mProfileLevelCdd.put(MediaFormat.MIMETYPE_AUDIO_AAC,
                new Pair<>(new int[]{AACObjectLC, AACObjectHE, AACObjectELD}, -1));
        mProfileLevelCdd.put(MediaFormat.MIMETYPE_VIDEO_H263,
                new Pair<>(new int[]{H263ProfileBaseline}, H263Level45));
        mProfileLevelCdd.put(MediaFormat.MIMETYPE_VIDEO_AVC,
                new Pair<>(new int[]{AVCProfileBaseline}, AVCLevel3));
        mProfileLevelCdd.put(MediaFormat.MIMETYPE_VIDEO_HEVC,
                new Pair<>(new int[]{HEVCProfileMain}, HEVCMainTierLevel3));
        mProfileLevelCdd.put(MediaFormat.MIMETYPE_VIDEO_VP8,
                new Pair<>(new int[]{VP8ProfileMain}, VP8Level_Version0));
        mProfileLevelCdd.put(MediaFormat.MIMETYPE_VIDEO_VP9,
                new Pair<>(new int[]{VP9Profile0}, VP9Level3));
    }

    private int getMinLevel(String mime, int width, int height, int frameRate, int bitrate,
            int profile) {
        switch (mime) {
            case MediaFormat.MIMETYPE_VIDEO_AVC:
                return getMinLevelAVC(width, height, frameRate, bitrate);
            case MediaFormat.MIMETYPE_VIDEO_HEVC:
                return getMinLevelHEVC(width, height, frameRate, bitrate);
            case MediaFormat.MIMETYPE_VIDEO_H263:
                return getMinLevelH263(width, height, frameRate, bitrate);
            case MediaFormat.MIMETYPE_VIDEO_MPEG2:
                return getMinLevelMPEG2(width, height, frameRate, bitrate);
            case MediaFormat.MIMETYPE_VIDEO_MPEG4:
                return getMinLevelMPEG4(width, height, frameRate, bitrate, profile);
            // complex features disabled in VP8 Level/Version 0
            case MediaFormat.MIMETYPE_VIDEO_VP8:
                return VP8Level_Version0;
            case MediaFormat.MIMETYPE_VIDEO_VP9:
                return getMinLevelVP9(width, height, frameRate, bitrate);
            case MediaFormat.MIMETYPE_VIDEO_AV1:
                return getMinLevelAV1(width, height, frameRate, bitrate);
            default:
                return -1;
        }
    }

    private int getMinLevelAVC(int width, int height, int frameRate, int bitrate) {
        class LevelLimitAVC {
            private LevelLimitAVC(int level, int mbsPerSec, long mbs, int bitrate) {
                this.level = level;
                this.mbsPerSec = mbsPerSec;
                this.mbs = mbs;
                this.bitrate = bitrate;
            }

            private final int level;
            private final int mbsPerSec;
            private final long mbs;
            private final int bitrate;
        }
        LevelLimitAVC[] limitsAVC = {
                new LevelLimitAVC(AVCLevel1, 1485, 99, 64000),
                new LevelLimitAVC(AVCLevel1b, 1485, 99, 128000),
                new LevelLimitAVC(AVCLevel11, 3000, 396, 192000),
                new LevelLimitAVC(AVCLevel12, 6000, 396, 384000),
                new LevelLimitAVC(AVCLevel13, 11880, 396, 768000),
                new LevelLimitAVC(AVCLevel2, 11880, 396, 2000000),
                new LevelLimitAVC(AVCLevel21, 19800, 792, 4000000),
                new LevelLimitAVC(AVCLevel22, 20250, 1620, 4000000),
                new LevelLimitAVC(AVCLevel3, 40500, 1620, 10000000),
                new LevelLimitAVC(AVCLevel31, 108000, 3600, 14000000),
                new LevelLimitAVC(AVCLevel32, 216000, 5120, 20000000),
                new LevelLimitAVC(AVCLevel4, 245760, 8192, 20000000),
                new LevelLimitAVC(AVCLevel41, 245760, 8192, 50000000),
                new LevelLimitAVC(AVCLevel42, 522240, 8704, 50000000),
                new LevelLimitAVC(AVCLevel5, 589824, 22080, 135000000),
                new LevelLimitAVC(AVCLevel51, 983040, 36864, 240000000),
                new LevelLimitAVC(AVCLevel52, 2073600, 36864, 240000000),
                new LevelLimitAVC(AVCLevel6, 4177920, 139264, 240000000),
                new LevelLimitAVC(AVCLevel61, 8355840, 139264, 480000000),
                new LevelLimitAVC(AVCLevel62, 16711680, 139264, 800000000),
        };
        long mbs = ((width + 15) / 16) * ((height + 15) / 16);
        float mbsPerSec = mbs * frameRate;
        for (LevelLimitAVC levelLimitsAVC : limitsAVC) {
            if (mbs <= levelLimitsAVC.mbs && mbsPerSec <= levelLimitsAVC.mbsPerSec
                    && bitrate <= levelLimitsAVC.bitrate) {
                return levelLimitsAVC.level;
            }
        }
        // if none of the levels suffice, select the highest level
        return AVCLevel62;
    }

    private int getMinLevelHEVC(int width, int height, int frameRate, int bitrate) {
        class LevelLimitHEVC {
            private LevelLimitHEVC(int level, int frameRate, long samples, int bitrate) {
                this.level = level;
                this.frameRate = frameRate;
                this.samples = samples;
                this.bitrate = bitrate;
            }

            private final int level;
            private final int frameRate;
            private final long samples;
            private final int bitrate;
        }
        LevelLimitHEVC[] limitsHEVC = {
                new LevelLimitHEVC(HEVCMainTierLevel1, 15, 36864, 128000),
                new LevelLimitHEVC(HEVCMainTierLevel2, 30, 122880, 1500000),
                new LevelLimitHEVC(HEVCMainTierLevel21, 30, 245760, 3000000),
                new LevelLimitHEVC(HEVCMainTierLevel3, 30, 552960, 6000000),
                new LevelLimitHEVC(HEVCMainTierLevel31, 30, 983040, 10000000),
                new LevelLimitHEVC(HEVCMainTierLevel4, 30, 2228224, 12000000),
                new LevelLimitHEVC(HEVCHighTierLevel4, 30, 2228224, 30000000),
                new LevelLimitHEVC(HEVCMainTierLevel41, 60, 2228224, 20000000),
                new LevelLimitHEVC(HEVCHighTierLevel41, 60, 2228224, 50000000),
                new LevelLimitHEVC(HEVCMainTierLevel5, 30, 8912896, 25000000),
                new LevelLimitHEVC(HEVCHighTierLevel5, 30, 8912896, 100000000),
                new LevelLimitHEVC(HEVCMainTierLevel51, 60, 8912896, 40000000),
                new LevelLimitHEVC(HEVCHighTierLevel51, 60, 8912896, 160000000),
                new LevelLimitHEVC(HEVCMainTierLevel52, 120, 8912896, 60000000),
                new LevelLimitHEVC(HEVCHighTierLevel52, 120, 8912896, 240000000),
                new LevelLimitHEVC(HEVCMainTierLevel6, 30, 35651584, 60000000),
                new LevelLimitHEVC(HEVCHighTierLevel6, 30, 35651584, 240000000),
                new LevelLimitHEVC(HEVCMainTierLevel61, 60, 35651584, 120000000),
                new LevelLimitHEVC(HEVCHighTierLevel61, 60, 35651584, 480000000),
                new LevelLimitHEVC(HEVCMainTierLevel62, 120, 35651584, 240000000),
                new LevelLimitHEVC(HEVCHighTierLevel62, 120, 35651584, 800000000),
        };
        long samples = width * height;
        for (LevelLimitHEVC levelLimitsHEVC : limitsHEVC) {
            if (samples <= levelLimitsHEVC.samples && frameRate <= levelLimitsHEVC.frameRate
                    && bitrate <= levelLimitsHEVC.bitrate) {
                return levelLimitsHEVC.level;
            }
        }
        // if none of the levels suffice, select the highest level
        return HEVCHighTierLevel62;
    }

    private int getMinLevelH263(int width, int height, int frameRate, int bitrate) {
        class LevelLimitH263 {
            private LevelLimitH263(int level, int height, int width, int frameRate,
                    int bitrate) {
                this.level = level;
                this.height = height;
                this.width = width;
                this.frameRate = frameRate;
                this.bitrate = bitrate;
            }

            private final int level;
            private final int height;
            private final int width;
            private final int frameRate;
            private final int bitrate;
        }
        LevelLimitH263[] limitsH263 = {
                new LevelLimitH263(H263Level10, 176, 144, 15, 64000),
                new LevelLimitH263(H263Level45, 176, 144, 15, 128000),
                new LevelLimitH263(H263Level20, 176, 144, 30, 128000),
                new LevelLimitH263(H263Level20, 352, 288, 15, 128000),
                new LevelLimitH263(H263Level30, 352, 288, 30, 384000),
                new LevelLimitH263(H263Level40, 352, 288, 30, 2048000),
                new LevelLimitH263(H263Level50, 352, 240, 60, 4096000),
                new LevelLimitH263(H263Level50, 352, 288, 50, 4096000),
                new LevelLimitH263(H263Level60, 720, 240, 60, 8192000),
                new LevelLimitH263(H263Level60, 720, 288, 50, 8192000),
                new LevelLimitH263(H263Level70, 720, 480, 60, 16384000),
                new LevelLimitH263(H263Level70, 720, 576, 50, 16384000),
        };
        for (LevelLimitH263 levelLimitsH263 : limitsH263) {
            if (height <= levelLimitsH263.height && width <= levelLimitsH263.width &&
                    frameRate <= levelLimitsH263.frameRate && bitrate <= levelLimitsH263.bitrate) {
                return levelLimitsH263.level;
            }
        }
        // if none of the levels suffice, select the highest level
        return H263Level70;
    }

    private int getMinLevelVP9(int width, int height, int frameRate, int bitrate) {
        class LevelLimitVP9 {
            private LevelLimitVP9(int level, long sampleRate, int size, int breadth,
                    int bitrate) {
                this.level = level;
                this.sampleRate = sampleRate;
                this.size = size;
                this.breadth = breadth;
                this.bitrate = bitrate;
            }

            private final int level;
            private final long sampleRate;
            private final int size;
            private final int breadth;
            private final int bitrate;
        }
        LevelLimitVP9[] limitsVP9 = {
                new LevelLimitVP9(VP9Level1, 829440, 36864, 512, 200000),
                new LevelLimitVP9(VP9Level11, 2764800, 73728, 768, 800000),
                new LevelLimitVP9(VP9Level2, 4608000, 122880, 960, 1800000),
                new LevelLimitVP9(VP9Level21, 9216000, 245760, 1344, 3600000),
                new LevelLimitVP9(VP9Level3, 20736000, 552960, 2048, 7200000),
                new LevelLimitVP9(VP9Level31, 36864000, 983040, 2752, 12000000),
                new LevelLimitVP9(VP9Level4, 83558400, 2228224, 4160, 18000000),
                new LevelLimitVP9(VP9Level41, 160432128, 2228224, 4160, 30000000),
                new LevelLimitVP9(VP9Level5, 311951360, 8912896, 8384, 60000000),
                new LevelLimitVP9(VP9Level51, 588251136, 8912896, 8384, 120000000),
                new LevelLimitVP9(VP9Level52, 1176502272, 8912896, 8384, 180000000),
                new LevelLimitVP9(VP9Level6, 1176502272, 35651584, 16832, 180000000),
                new LevelLimitVP9(VP9Level61, 2353004544L, 35651584, 16832, 240000000),
                new LevelLimitVP9(VP9Level62, 4706009088L, 35651584, 16832, 480000000),
        };
        int size = width * height;
        int sampleRate = size * frameRate;
        int breadth = Math.max(width, height);
        for (LevelLimitVP9 levelLimitsVP9 : limitsVP9) {
            if (sampleRate <= levelLimitsVP9.sampleRate && size <= levelLimitsVP9.size &&
                    breadth <= levelLimitsVP9.breadth && bitrate <= levelLimitsVP9.bitrate) {
                return levelLimitsVP9.level;
            }
        }
        // if none of the levels suffice, select the highest level
        return VP9Level62;
    }

    private int getMinLevelMPEG2(int width, int height, int frameRate, int bitrate) {
        class LevelLimitMPEG2 {
            private LevelLimitMPEG2(int level, long sampleRate, int width, int height,
                    int frameRate, int bitrate) {
                this.level = level;
                this.sampleRate = sampleRate;
                this.width = width;
                this.height = height;
                this.frameRate = frameRate;
                this.bitrate = bitrate;
            }

            private final int level;
            private final long sampleRate;
            private final int width;
            private final int height;
            private final int frameRate;
            private final int bitrate;
        }
        // main profile limits, higher profiles will also support selected level
        LevelLimitMPEG2[] limitsMPEG2 = {
                new LevelLimitMPEG2(MPEG2LevelLL, 3041280, 352, 288, 30, 4000000),
                new LevelLimitMPEG2(MPEG2LevelML, 10368000, 720, 576, 30, 15000000),
                new LevelLimitMPEG2(MPEG2LevelH14, 47001600, 1440, 1088, 60, 60000000),
                new LevelLimitMPEG2(MPEG2LevelHL, 62668800, 1920, 1088, 60, 80000000),
                new LevelLimitMPEG2(MPEG2LevelHP, 125337600, 1920, 1088, 60, 80000000),
        };
        int size = width * height;
        int sampleRate = size * frameRate;
        for (LevelLimitMPEG2 levelLimitsMPEG2 : limitsMPEG2) {
            if (sampleRate <= levelLimitsMPEG2.sampleRate && width <= levelLimitsMPEG2.width &&
                    height <= levelLimitsMPEG2.height && frameRate <= levelLimitsMPEG2.frameRate &&
                    bitrate <= levelLimitsMPEG2.bitrate) {
                return levelLimitsMPEG2.level;
            }
        }
        // if none of the levels suffice, select the highest level
        return MPEG2LevelHP;
    }

    private int getMinLevelMPEG4(int width, int height, int frameRate, int bitrate, int profile) {
        class LevelLimitMPEG4 {
            private LevelLimitMPEG4(int profile, int level, long sampleRate, int width,
                    int height, int frameRate, int bitrate) {
                this.profile = profile;
                this.level = level;
                this.sampleRate = sampleRate;
                this.width = width;
                this.height = height;
                this.frameRate = frameRate;
                this.bitrate = bitrate;
            }

            private final int profile;
            private final int level;
            private final long sampleRate;
            private final int width;
            private final int height;
            private final int frameRate;
            private final int bitrate;
        }
        // simple profile limits, higher profiles will also support selected level
        LevelLimitMPEG4[] limitsMPEG4 = {
                new LevelLimitMPEG4(MPEG4ProfileSimple, MPEG4Level0, 380160, 176, 144, 15, 64000),
                new LevelLimitMPEG4(MPEG4ProfileSimple, MPEG4Level1, 380160, 176, 144, 30, 64000),
                new LevelLimitMPEG4(MPEG4ProfileSimple, MPEG4Level0b, 380160, 176, 144, 15, 128000),
                new LevelLimitMPEG4(MPEG4ProfileSimple, MPEG4Level2, 1520640, 352, 288, 30, 128000),
                new LevelLimitMPEG4(MPEG4ProfileSimple, MPEG4Level3, 3041280, 352, 288, 30, 384000),
                new LevelLimitMPEG4(
                        MPEG4ProfileSimple, MPEG4Level4a, 9216000, 640, 480, 30, 4000000),
                new LevelLimitMPEG4(
                        MPEG4ProfileSimple, MPEG4Level5, 10368000, 720, 576, 30, 8000000),
                new LevelLimitMPEG4(
                        MPEG4ProfileSimple, MPEG4Level6, 27648000, 1280, 720, 30, 12000000),
                new LevelLimitMPEG4(
                        MPEG4ProfileAdvancedSimple, MPEG4Level1, 760320, 176, 144, 30, 128000),
                new LevelLimitMPEG4(
                        MPEG4ProfileAdvancedSimple, MPEG4Level2, 1520640, 352, 288, 30, 384000),
                new LevelLimitMPEG4(
                        MPEG4ProfileAdvancedSimple, MPEG4Level3, 3041280, 352, 288, 30, 768000),
                new LevelLimitMPEG4(
                        MPEG4ProfileAdvancedSimple, MPEG4Level3b, 3041280, 352, 288, 30, 1500000),
                new LevelLimitMPEG4(
                        MPEG4ProfileAdvancedSimple, MPEG4Level4, 3041280, 704, 576, 30, 3000000),
                new LevelLimitMPEG4(
                        MPEG4ProfileAdvancedSimple, MPEG4Level5, 3041280, 720, 576, 30, 8000000),
        };
        int size = width * height;
        int sampleRate = size * frameRate;
        for (LevelLimitMPEG4 levelLimitsMPEG4 : limitsMPEG4) {
            if (((profile & (MPEG4ProfileAdvancedSimple | MPEG4ProfileSimple)) != 0) &&
                    profile != levelLimitsMPEG4.profile) continue;
            if (sampleRate <= levelLimitsMPEG4.sampleRate && width <= levelLimitsMPEG4.width &&
                    height <= levelLimitsMPEG4.height && frameRate <= levelLimitsMPEG4.frameRate &&
                    bitrate <= levelLimitsMPEG4.bitrate) {
                return levelLimitsMPEG4.level;
            }
        }
        // if none of the levels suffice, select the highest level
        return MPEG4Level6;
    }

    private int getMinLevelAV1(int width, int height, int frameRate, int bitrate) {
        class LevelLimitAV1 {
            private LevelLimitAV1(int level, int size, int width, int height, long sampleRate,
                    int bitrate) {
                this.level = level;
                this.size = size;
                this.width = width;
                this.height = height;
                this.sampleRate = sampleRate;
                this.bitrate = bitrate;
            }

            private final int level;
            private final int size;
            private final int width;
            private final int height;
            private final long sampleRate;
            private final int bitrate;
        }
        // taking bitrate from main profile, will also be supported by high profile
        LevelLimitAV1[] limitsAV1 = {
                new LevelLimitAV1(AV1Level2, 147456, 2048, 1152, 4423680, 1500000),
                new LevelLimitAV1(AV1Level21, 278784, 2816, 1584, 8363520, 3000000),
                new LevelLimitAV1(AV1Level3, 665856, 4352, 2448, 19975680, 6000000),
                new LevelLimitAV1(AV1Level31, 1065024, 5504, 3096, 31950720, 10000000),
                new LevelLimitAV1(AV1Level4, 2359296, 6144, 3456, 70778880, 12000000),
                new LevelLimitAV1(AV1Level41, 2359296, 6144, 3456, 141557760, 20000000),
                new LevelLimitAV1(AV1Level5, 8912896, 8192, 4352, 267386880, 30000000),
                new LevelLimitAV1(AV1Level51, 8912896, 8192, 4352, 534773760, 40000000),
                new LevelLimitAV1(AV1Level52, 8912896, 8192, 4352, 1069547520, 60000000),
                new LevelLimitAV1(AV1Level53, 8912896, 8192, 4352, 1069547520, 60000000),
                new LevelLimitAV1(AV1Level6, 35651584, 16384, 8704, 1069547520, 60000000),
                new LevelLimitAV1(AV1Level61, 35651584, 16384, 8704, 2139095040, 100000000),
                new LevelLimitAV1(AV1Level62, 35651584, 16384, 8704, 4278190080L, 160000000),
                new LevelLimitAV1(AV1Level63, 35651584, 16384, 8704, 4278190080L, 160000000),
        };
        int size = width * height;
        int sampleRate = size * frameRate;
        for (LevelLimitAV1 levelLimitsAV1 : limitsAV1) {
            if (size <= levelLimitsAV1.size && width <= levelLimitsAV1.width &&
                    height <= levelLimitsAV1.height && sampleRate <= levelLimitsAV1.sampleRate &&
                    bitrate <= levelLimitsAV1.bitrate) {
                return levelLimitsAV1.level;
            }
        }
        // if none of the levels suffice or high profile, select the highest level
        return AV1Level73;
    }

    @Override
    boolean isFormatSimilar(MediaFormat inpFormat, MediaFormat outFormat) {
        if (!super.isFormatSimilar(inpFormat, outFormat)) {
            Log.e(LOG_TAG, "Basic channel-rate/resolution comparisons failed");
            return false;
        }
        String inpMime = inpFormat.getString(MediaFormat.KEY_MIME);
        String outMime = outFormat.getString(MediaFormat.KEY_MIME);
        assertTrue("Input and Output mimes are different.", inpMime.equals(outMime));
        if (outMime.startsWith("audio/")) {
            if (outFormat.getString(MediaFormat.KEY_MIME).equals(MediaFormat.MIMETYPE_AUDIO_AAC)) {
                if (!outFormat.containsKey(MediaFormat.KEY_AAC_PROFILE)) {
                    Log.e(LOG_TAG, "Output format doesn't contain aac-profile key");
                    //TODO (b/151429829)
                    if (true) return true;
                    return false;
                }
                if (!inpFormat.containsKey(MediaFormat.KEY_AAC_PROFILE)) {
                    Log.e(LOG_TAG, "Input format doesn't contain aac-profile key");
                    return false;
                }
                if (outFormat.getInteger(MediaFormat.KEY_AAC_PROFILE)
                        != inpFormat.getInteger(MediaFormat.KEY_AAC_PROFILE)) {
                    Log.e(LOG_TAG, "aac-profile in output doesn't match configured input");
                    return false;
                }
            }
        } else if (outMime.startsWith("video/")) {
            if (!outFormat.containsKey(MediaFormat.KEY_PROFILE)) {
                Log.e(LOG_TAG, "Output format doesn't contain profile key");
                //TODO (b/151398466)
                if (true) return true;
                return false;
            }
            if (!outFormat.containsKey(MediaFormat.KEY_LEVEL)) {
                Log.e(LOG_TAG, "Output format doesn't contain level key");
                //TODO (b/151398466)
                if (true) return true;
                return false;
            }
            if (!inpFormat.containsKey(MediaFormat.KEY_PROFILE)) {
                Log.e(LOG_TAG, "Input format doesn't contain profile key");
                return false;
            }
            if (!inpFormat.containsKey(MediaFormat.KEY_LEVEL)) {
                Log.e(LOG_TAG, "Input format doesn't contain level key");
                return false;
            }
            if (outFormat.getInteger(MediaFormat.KEY_PROFILE)
                    != inpFormat.getInteger(MediaFormat.KEY_PROFILE)) {
                Log.e(LOG_TAG, "profile in output doesn't match configured input");
                return false;
            }
            if (outFormat.getInteger(MediaFormat.KEY_LEVEL)
                    != inpFormat.getInteger(MediaFormat.KEY_LEVEL)) {
                Log.e(LOG_TAG, "level key in output doesn't match configured input");
                return false;
            }
        } else {
            Log.w(LOG_TAG, "non media mime:" + outMime);
        }
        return true;
    }

    /**
     * Sets profile and level keys in config format for encoder and validates the keys in output
     * format if component supports the configuration and also verifies whether cdd mandated
     * (profile, level) combination is supported
     */
    @Test(timeout = PER_TEST_TIMEOUT_LARGE_TEST_MS)
    public void testValidateProfileLevel() throws IOException, InterruptedException {
        ArrayList<String> listOfEncoders = selectCodecs(mMime, null, null, true);
        assertFalse("no suitable codecs found for mime: " + mMime, listOfEncoders.isEmpty());
        int[] profiles = mProfileMap.get(mMime);
        assertTrue("no profile entry found for mime" + mMime, profiles != null);
        // cdd check initialization
        boolean cddSupportedMime = mProfileLevelCdd.get(mMime) != null;
        int[] profileCdd = new int[0];
        int levelCdd = 0;
        if (cddSupportedMime) {
            Pair<int[], Integer> cddProfileLevel = mProfileLevelCdd.get(mMime);
            profileCdd = cddProfileLevel.first;
            levelCdd = cddProfileLevel.second;
        }
        boolean[] boolStates = {true, false};
        MediaFormat format = mConfigFormat;
        mOutputBuff = new OutputManager();
        setUpSource(mInputFile);
        int supportedCddCount = listOfEncoders.size() * (cddSupportedMime ? profileCdd.length : 1);
        for (String encoder : listOfEncoders) {
            mCodec = MediaCodec.createByCodecName(encoder);
            MediaCodecInfo.CodecCapabilities codecCapabilities =
                    mCodec.getCodecInfo().getCapabilitiesForType(mMime);
            for (int profile : profiles) {
                format.setInteger(mIsAudio ? MediaFormat.KEY_AAC_PROFILE : MediaFormat.KEY_PROFILE,
                        profile);
                int level = mIsAudio ? 0 : getMinLevel(mMime, mWidth, mHeight,
                        format.getInteger(MediaFormat.KEY_FRAME_RATE),
                        format.getInteger(MediaFormat.KEY_BIT_RATE), profile);
                assertTrue("no min level found for mime" + mMime, level != -1);
                if (!mIsAudio) format.setInteger(MediaFormat.KEY_LEVEL, level);
                if (!codecCapabilities.isFormatSupported(format)) {
                    if (cddSupportedMime) {
                        if (mIsAudio) {
                            for (int cddProfile : profileCdd) {
                                if (profile == cddProfile) supportedCddCount--;
                            }
                        } else if (profile == profileCdd[0] && level == levelCdd) {
                            supportedCddCount--;
                        }
                        Log.w(LOG_TAG,
                                "Component: " + encoder + " doesn't support cdd format: " + format);
                    }
                    continue;
                }
                for (boolean isAsync : boolStates) {
                    configureCodec(format, isAsync, true, true);
                    mCodec.start();
                    doWork(1);
                    queueEOS();
                    waitForAllOutputs();
                    MediaFormat outFormat = mCodec.getOutputFormat();
                    /* TODO(b/147348711) */
                    if (false) mCodec.stop();
                    else mCodec.reset();
                    String log =
                            String.format("format: %s \n codec: %s, mode: %s:: ", format, encoder,
                                    (isAsync ? "async" : "sync"));
                    assertFalse(log + " unexpected error", mAsyncHandle.hasSeenError());
                    assertTrue(log + "configured format and output format are not similar." +
                                    (ENABLE_LOGS ? "\n output format:" + outFormat : ""),
                            isFormatSimilar(format, outFormat));
                }
            }
            mCodec.release();
        }
        assertFalse("No components support cdd requirement profile level for mime: " + mMime,
                supportedCddCount == 0);
    }
}
