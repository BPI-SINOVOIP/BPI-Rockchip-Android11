/*
 * Copyright (C) 2013 The Android Open Source Project
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
package android.media.cts;

import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.AudioCapabilities;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecInfo.CodecProfileLevel;
import android.media.MediaCodecInfo.VideoCapabilities;
import static android.media.MediaCodecInfo.CodecProfileLevel.*;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import static android.media.MediaFormat.MIMETYPE_VIDEO_AVC;
import static android.media.MediaFormat.MIMETYPE_VIDEO_H263;
import static android.media.MediaFormat.MIMETYPE_VIDEO_HEVC;
import static android.media.MediaFormat.MIMETYPE_VIDEO_MPEG4;
import static android.media.MediaFormat.MIMETYPE_VIDEO_VP8;
import static android.media.MediaFormat.MIMETYPE_VIDEO_VP9;
import android.media.MediaPlayer;
import android.os.Build;
import android.platform.test.annotations.AppModeFull;
import android.util.Log;
import android.util.Range;
import android.util.Size;

import com.android.compatibility.common.util.ApiLevelUtil;
import com.android.compatibility.common.util.DynamicConfigDeviceSide;
import com.android.compatibility.common.util.MediaUtils;

import java.io.IOException;
import java.util.HashSet;
import java.util.Set;
import java.util.Arrays;
import java.util.Vector;

/**
 * Basic sanity test of data returned by MediaCodeCapabilities.
 */
@AppModeFull(reason = "Dynamic config disabled.")
public class MediaCodecCapabilitiesTest extends MediaPlayerTestBase {

    private static final String TAG = "MediaCodecCapabilitiesTest";
    private static final int PLAY_TIME_MS = 30000;
    private static final int TIMEOUT_US = 1000000;  // 1 sec
    private static final int IFRAME_INTERVAL = 10;          // 10 seconds between I-frames

    private final MediaCodecList mAllCodecs =
            new MediaCodecList(MediaCodecList.ALL_CODECS);
    private final MediaCodecInfo[] mAllInfos =
            mAllCodecs.getCodecInfos();

    private static final String AVC_BASELINE_12_KEY =
            "media_codec_capabilities_test_avc_baseline12";
    private static final String AVC_BASELINE_30_KEY =
            "media_codec_capabilities_test_avc_baseline30";
    private static final String AVC_HIGH_31_KEY = "media_codec_capabilities_test_avc_high31";
    private static final String AVC_HIGH_40_KEY = "media_codec_capabilities_test_avc_high40";
    private static final String MODULE_NAME = "CtsMediaTestCases";
    private DynamicConfigDeviceSide dynamicConfig;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        dynamicConfig = new DynamicConfigDeviceSide(MODULE_NAME);
    }

    // Android device implementations with H.264 encoders, MUST support Baseline Profile Level 3.
    // SHOULD support Main Profile/ Level 4, if supported the device must also support Main
    // Profile/Level 4 decoding.
    public void testH264EncoderProfileAndLevel() throws Exception {
        if (!MediaUtils.checkEncoder(MIMETYPE_VIDEO_AVC)) {
            return; // skip
        }

        assertTrue(
                "H.264 must support Baseline Profile Level 3",
                hasEncoder(MIMETYPE_VIDEO_AVC, AVCProfileBaseline, AVCLevel3));

        if (hasEncoder(MIMETYPE_VIDEO_AVC, AVCProfileMain, AVCLevel4)) {
            assertTrue(
                    "H.264 decoder must support Main Profile Level 4 if it can encode it",
                    hasDecoder(MIMETYPE_VIDEO_AVC, AVCProfileMain, AVCLevel4));
        }
    }

    // Android device implementations with H.264 decoders, MUST support Baseline Profile Level 3.
    // Android Television Devices MUST support High Profile Level 4.2.
    public void testH264DecoderProfileAndLevel() throws Exception {
        if (!MediaUtils.checkDecoder(MIMETYPE_VIDEO_AVC)) {
            return; // skip
        }

        assertTrue(
                "H.264 must support Baseline Profile Level 3",
                hasDecoder(MIMETYPE_VIDEO_AVC, AVCProfileBaseline, AVCLevel3));

        if (isTv()) {
            assertTrue(
                    "H.264 must support High Profile Level 4.2 on TV",
                    checkDecoder(MIMETYPE_VIDEO_AVC, AVCProfileHigh, AVCLevel42));
        }
    }

    // Android device implementations with H.263 encoders, MUST support Level 45.
    public void testH263EncoderProfileAndLevel() throws Exception {
        if (!MediaUtils.checkEncoder(MIMETYPE_VIDEO_H263)) {
            return; // skip
        }

        assertTrue(
                "H.263 must support Level 45",
                hasEncoder(MIMETYPE_VIDEO_H263, MPEG4ProfileSimple, H263Level45));
    }

    // Android device implementations with H.263 decoders, MUST support Level 30.
    public void testH263DecoderProfileAndLevel() throws Exception {
        if (!MediaUtils.checkDecoder(MIMETYPE_VIDEO_H263)) {
            return; // skip
        }

        assertTrue(
                "H.263 must support Level 30",
                hasDecoder(MIMETYPE_VIDEO_H263, MPEG4ProfileSimple, H263Level30));
    }

    // Android device implementations with MPEG-4 decoders, MUST support Simple Profile Level 3.
    public void testMpeg4DecoderProfileAndLevel() throws Exception {
        if (!MediaUtils.checkDecoder(MIMETYPE_VIDEO_MPEG4)) {
            return; // skip
        }

        assertTrue(
                "MPEG-4 must support Simple Profile Level 3",
                hasDecoder(MIMETYPE_VIDEO_MPEG4, MPEG4ProfileSimple, MPEG4Level3));
    }

    // Android device implementations, when supporting H.265 codec MUST support the Main Profile
    // Level 3 Main tier.
    // Android Television Devices MUST support the Main Profile Level 4.1 Main tier.
    // When the UHD video decoding profile is supported, it MUST support Main10 Level 5 Main
    // Tier profile.
    public void testH265DecoderProfileAndLevel() throws Exception {
        if (!MediaUtils.checkDecoder(MIMETYPE_VIDEO_HEVC)) {
            return; // skip
        }

        assertTrue(
                "H.265 must support Main Profile Main Tier Level 3",
                hasDecoder(MIMETYPE_VIDEO_HEVC, HEVCProfileMain, HEVCMainTierLevel3));

        if (isTv()) {
            assertTrue(
                    "H.265 must support Main Profile Main Tier Level 4.1 on TV",
                    hasDecoder(MIMETYPE_VIDEO_HEVC, HEVCProfileMain, HEVCMainTierLevel41));
        }

        if (isTv() && MediaUtils.canDecodeVideo(MIMETYPE_VIDEO_HEVC, 3840, 2160, 30)) {
            assertTrue(
                    "H.265 must support Main10 Profile Main Tier Level 5 if UHD is supported",
                    hasDecoder(MIMETYPE_VIDEO_HEVC, HEVCProfileMain10, HEVCMainTierLevel5));
        }
    }

    public void testVp9DecoderCapabilitiesOnTv() throws Exception {
        if (isTv() && MediaUtils.hasHardwareCodec(MIMETYPE_VIDEO_VP9, /* encode = */ false)) {
            // CDD [5.3.7.4/T-1-1]
            assertTrue(MediaUtils.canDecodeVideo(MIMETYPE_VIDEO_VP9, 1920, 1080, 60 /* fps */,
                    VP9Profile0, null, null));
            if (MediaUtils.canDecodeVideo(MIMETYPE_VIDEO_VP9, 3840, 2160, 60 /* fps */)) {
                // CDD [5.3.7.5/T-2-1]
                assertTrue(MediaUtils.canDecodeVideo(MIMETYPE_VIDEO_VP9, 3840, 2160, 60 /* fps */,
                        VP9Profile0, null, null));
            }
        }
    }

    public void testAvcBaseline1() throws Exception {
        if (!checkDecoder(MIMETYPE_VIDEO_AVC, AVCProfileBaseline, AVCLevel1)) {
            return; // skip
        }

        // TODO: add a test stream
        MediaUtils.skipTest(TAG, "no test stream");
    }

    public void testAvcBaseline12() throws Exception {
        if (!checkDecoder(MIMETYPE_VIDEO_AVC, AVCProfileBaseline, AVCLevel12)) {
            return; // skip
        }

        if (checkDecodeWithDefaultPlayer(MIMETYPE_VIDEO_AVC, AVCProfileBaseline, AVCLevel12)) {
            String urlString = dynamicConfig.getValue(AVC_BASELINE_12_KEY);
            playVideoWithRetries(urlString, 256, 144, PLAY_TIME_MS);
        }
    }

    public void testAvcBaseline30() throws Exception {
        if (!checkDecoder(MIMETYPE_VIDEO_AVC, AVCProfileBaseline, AVCLevel3)) {
            return; // skip
        }

        if (checkDecodeWithDefaultPlayer(MIMETYPE_VIDEO_AVC, AVCProfileBaseline, AVCLevel3)) {
            String urlString = dynamicConfig.getValue(AVC_BASELINE_30_KEY);
            playVideoWithRetries(urlString, 640, 360, PLAY_TIME_MS);
        }
    }

    public void testAvcHigh31() throws Exception {
        if (!checkDecoder(MIMETYPE_VIDEO_AVC, AVCProfileHigh, AVCLevel31)) {
            return; // skip
        }

        if (checkDecodeWithDefaultPlayer(MIMETYPE_VIDEO_AVC, AVCProfileHigh, AVCLevel31)) {
            String urlString = dynamicConfig.getValue(AVC_HIGH_31_KEY);
            playVideoWithRetries(urlString, 1280, 720, PLAY_TIME_MS);
        }
    }

    public void testAvcHigh40() throws Exception {
        if (!checkDecoder(MIMETYPE_VIDEO_AVC, AVCProfileHigh, AVCLevel4)) {
            return; // skip
        }
        if (ApiLevelUtil.isBefore(18)) {
            MediaUtils.skipTest(TAG, "fragmented mp4 not supported");
            return;
        }

        if (checkDecodeWithDefaultPlayer(MIMETYPE_VIDEO_AVC, AVCProfileHigh, AVCLevel4)) {
            String urlString = dynamicConfig.getValue(AVC_HIGH_40_KEY);
            playVideoWithRetries(urlString, 1920, 1080, PLAY_TIME_MS);
        }
    }

    public void testHevcMain1() throws Exception {
        if (!checkDecoder(MIMETYPE_VIDEO_HEVC, HEVCProfileMain, HEVCMainTierLevel1)) {
            return; // skip
        }

        // TODO: add a test stream
        MediaUtils.skipTest(TAG, "no test stream");
    }

    public void testHevcMain2() throws Exception {
        if (!checkDecoder(MIMETYPE_VIDEO_HEVC, HEVCProfileMain, HEVCMainTierLevel2)) {
            return; // skip
        }

        // TODO: add a test stream
        MediaUtils.skipTest(TAG, "no test stream");
    }

    public void testHevcMain21() throws Exception {
        if (!checkDecoder(MIMETYPE_VIDEO_HEVC, HEVCProfileMain, HEVCMainTierLevel21)) {
            return; // skip
        }

        // TODO: add a test stream
        MediaUtils.skipTest(TAG, "no test stream");
    }

    public void testHevcMain3() throws Exception {
        if (!checkDecoder(MIMETYPE_VIDEO_HEVC, HEVCProfileMain, HEVCMainTierLevel3)) {
            return; // skip
        }

        // TODO: add a test stream
        MediaUtils.skipTest(TAG, "no test stream");
    }

    public void testHevcMain31() throws Exception {
        if (!checkDecoder(MIMETYPE_VIDEO_HEVC, HEVCProfileMain, HEVCMainTierLevel31)) {
            return; // skip
        }

        // TODO: add a test stream
        MediaUtils.skipTest(TAG, "no test stream");
    }

    public void testHevcMain4() throws Exception {
        if (!checkDecoder(MIMETYPE_VIDEO_HEVC, HEVCProfileMain, HEVCMainTierLevel4)) {
            return; // skip
        }

        // TODO: add a test stream
        MediaUtils.skipTest(TAG, "no test stream");
    }

    public void testHevcMain41() throws Exception {
        if (!checkDecoder(MIMETYPE_VIDEO_HEVC, HEVCProfileMain, HEVCMainTierLevel41)) {
            return; // skip
        }

        // TODO: add a test stream
        MediaUtils.skipTest(TAG, "no test stream");
    }

    public void testHevcMain5() throws Exception {
        if (!checkDecoder(MIMETYPE_VIDEO_HEVC, HEVCProfileMain, HEVCMainTierLevel5)) {
            return; // skip
        }

        // TODO: add a test stream
        MediaUtils.skipTest(TAG, "no test stream");
    }

    public void testHevcMain51() throws Exception {
        if (!checkDecoder(MIMETYPE_VIDEO_HEVC, HEVCProfileMain, HEVCMainTierLevel51)) {
            return; // skip
        }

        // TODO: add a test stream
        MediaUtils.skipTest(TAG, "no test stream");
    }

    private boolean checkDecoder(String mime, int profile, int level) {
        if (!hasDecoder(mime, profile, level)) {
            MediaUtils.skipTest(TAG, "no " + mime + " decoder for profile "
                    + profile + " and level " + level);
            return false;
        }
        return true;
    }

    private boolean hasDecoder(String mime, int profile, int level) {
        return supports(mime, false /* isEncoder */, profile, level, false /* defaultOnly */);
    }

    private boolean hasEncoder(String mime, int profile, int level) {
        return supports(mime, true /* isEncoder */, profile, level, false /* defaultOnly */);
    }

    // Checks whether the default AOSP player can play back a specific profile and level for a
    // given media type. If it cannot, it automatically logs that the test is skipped.
    private boolean checkDecodeWithDefaultPlayer(String mime, int profile, int level) {
        if (!supports(mime, false /* isEncoder */, profile, level, true /* defaultOnly */)) {
            MediaUtils.skipTest(TAG, "default player cannot test codec");
            return false;
        }
        return true;
    }

    private boolean supports(
            String mime, boolean isEncoder, int profile, int level,
            boolean defaultOnly) {
        MediaCodecList mcl = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
        for (MediaCodecInfo info : mcl.getCodecInfos()) {
            if (isEncoder != info.isEncoder()) {
                continue;
            }
            try {
                CodecCapabilities caps = info.getCapabilitiesForType(mime);
                for (CodecProfileLevel pl : caps.profileLevels) {
                    if (pl.profile != profile) {
                        continue;
                    }

                    // H.263 levels are not completely ordered:
                    // Level45 support only implies Level10 support
                    if (mime.equalsIgnoreCase(MIMETYPE_VIDEO_H263)) {
                        if (pl.level != level && pl.level == H263Level45 && level > H263Level10) {
                            continue;
                        }
                    }
                    if (pl.level >= level) {
                        return true;
                    }
                }
                // the default AOSP player picks the first codec for a specific mime type, so
                // we can stop after the first one found
                if (defaultOnly) {
                    return false;
                }
            } catch (IllegalArgumentException e) {
            }
        }
        return false;
    }

    private boolean isVideoMime(String mime) {
        return mime.toLowerCase().startsWith("video/");
    }

    private Set<String> requiredAdaptiveFormats() {
        Set<String> adaptiveFormats = new HashSet<String>();
        adaptiveFormats.add(MediaFormat.MIMETYPE_VIDEO_AVC);
        adaptiveFormats.add(MediaFormat.MIMETYPE_VIDEO_HEVC);
        adaptiveFormats.add(MediaFormat.MIMETYPE_VIDEO_VP8);
        adaptiveFormats.add(MediaFormat.MIMETYPE_VIDEO_VP9);
        return adaptiveFormats;
    }

    public void testHaveAdaptiveVideoDecoderForAllSupportedFormats() {
        Set<String> supportedFormats = new HashSet<String>();
        boolean skipped = true;

        // gather all supported video formats
        for (MediaCodecInfo info : mAllInfos) {
            if (info.isEncoder()) {
                continue;
            }
            for (String mime : info.getSupportedTypes()) {
                if (isVideoMime(mime)) {
                    supportedFormats.add(mime);
                }
            }
        }

        // limit to CDD-required formats for now
        supportedFormats.retainAll(requiredAdaptiveFormats());

        // check if there is an adaptive decoder for each
        for (String mime : supportedFormats) {
            skipped = false;
            // implicit assumption that QCIF video is always valid.
            MediaFormat format = MediaFormat.createVideoFormat(mime, 176, 144);
            format.setFeatureEnabled(CodecCapabilities.FEATURE_AdaptivePlayback, true);
            String codec = mAllCodecs.findDecoderForFormat(format);
            assertTrue(
                    "could not find adaptive decoder for " + mime, codec != null);
        }
        if (skipped) {
            MediaUtils.skipTest("no video decoders that are required to be adaptive found");
        }
    }

    public void testAllVideoDecodersAreAdaptive() {
        Set<String> adaptiveFormats = requiredAdaptiveFormats();
        boolean skipped = true;
        for (MediaCodecInfo info : mAllInfos) {
            if (info.isEncoder()) {
                continue;
            }
            for (String mime : info.getSupportedTypes()) {
                if (!isVideoMime(mime)
                        // limit to CDD-required formats for now
                        || !adaptiveFormats.contains(mime)) {
                    continue;
                }
                skipped = false;
                CodecCapabilities caps = info.getCapabilitiesForType(mime);
                assertTrue(
                    info.getName() + " is not adaptive for " + mime,
                    caps.isFeatureSupported(CodecCapabilities.FEATURE_AdaptivePlayback));
            }
        }
        if (skipped) {
            MediaUtils.skipTest("no video decoders that are required to be adaptive found");
        }
    }

    private MediaFormat createReasonableVideoFormat(
            CodecCapabilities caps, String mime, boolean encoder, int width, int height) {
        VideoCapabilities vidCaps = caps.getVideoCapabilities();
        MediaFormat format = MediaFormat.createVideoFormat(mime, width, height);
        if (encoder) {
            // bitrate
            int maxWidth = vidCaps.getSupportedWidths().getUpper();
            int maxHeight = vidCaps.getSupportedHeightsFor(width).getUpper();
            int maxRate = vidCaps.getSupportedFrameRatesFor(width, height).getUpper().intValue();
            int bitrate = vidCaps.getBitrateRange().clamp(
                    (int)(vidCaps.getBitrateRange().getUpper()
                            / Math.sqrt((double)maxWidth * maxHeight / width / height)));
            Log.i(TAG, "reasonable bitrate for " + width + "x" + height + "@" + maxRate
                    + " " + mime + " = " + bitrate);
            format.setInteger(format.KEY_BIT_RATE, bitrate);
            format.setInteger(format.KEY_FRAME_RATE, maxRate);
            format.setInteger(format.KEY_I_FRAME_INTERVAL, IFRAME_INTERVAL);
        }
        return format;
    }

    public void testSecureCodecsAdvertiseSecurePlayback() throws IOException {
        boolean skipped = true;
        for (MediaCodecInfo info : mAllInfos) {
            boolean isEncoder = info.isEncoder();
            if (isEncoder || !info.getName().endsWith(".secure")) {
                continue;
            }
            for (String mime : info.getSupportedTypes()) {
                if (!isVideoMime(mime)) {
                    continue;
                }
                skipped = false;
                CodecCapabilities caps = info.getCapabilitiesForType(mime);
                assertTrue(
                        info.getName() + " does not advertise secure playback",
                        caps.isFeatureSupported(CodecCapabilities.FEATURE_SecurePlayback));
            }
        }
        if (skipped) {
            MediaUtils.skipTest("no video decoders found ending in .secure");
        }
    }

    private Size getVideoSizeForTest(VideoCapabilities vidCaps) {
        Size size = new Size(176, 144);  // Use QCIF by default.
        if (vidCaps != null && !vidCaps.isSizeSupported(size.getWidth(), size.getHeight())) {
            int minWidth = vidCaps.getSupportedWidths().getLower();
            int minHeight = vidCaps.getSupportedHeightsFor(minWidth).getLower();
            size = new Size(minWidth, minHeight);
        }
        return size;
    }

    private MediaFormat createVideoFormatForBitrateMode(String mime, int width, int height,
            int bitrateMode, CodecCapabilities caps) {
        MediaCodecInfo.EncoderCapabilities encoderCaps = caps.getEncoderCapabilities();
        if (!encoderCaps.isBitrateModeSupported(bitrateMode)) {
            return null;
        }

        VideoCapabilities vidCaps = caps.getVideoCapabilities();
        MediaFormat format = MediaFormat.createVideoFormat(mime, width, height);

        // bitrate
        int maxWidth = vidCaps.getSupportedWidths().getUpper();
        int maxHeight = vidCaps.getSupportedHeightsFor(width).getUpper();
        int maxRate = vidCaps.getSupportedFrameRatesFor(width, height).getUpper().intValue();
        format.setInteger(MediaFormat.KEY_BITRATE_MODE, bitrateMode);
        if (bitrateMode == MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_CQ) {
            int quality = encoderCaps.getQualityRange().getLower();
            Log.i(TAG, "reasonable quality for " + width + "x" + height + "@" + maxRate
                    + " " + mime + " = " + quality);
            format.setInteger(MediaFormat.KEY_QUALITY, quality);
        } else {
            int bitrate = vidCaps.getBitrateRange().clamp(
                    (int)(vidCaps.getBitrateRange().getUpper()
                            / Math.sqrt((double)maxWidth * maxHeight / width / height)));
            Log.i(TAG, "reasonable bitrate for " + width + "x" + height + "@" + maxRate
                    + " " + mime + " = " + bitrate + " mode " + bitrateMode);
            format.setInteger(MediaFormat.KEY_BIT_RATE, bitrate);
        }
        format.setInteger(MediaFormat.KEY_FRAME_RATE, maxRate);
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, IFRAME_INTERVAL);
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT,
                CodecCapabilities.COLOR_FormatYUV420Flexible);

        return format;
    }

    public void testAllAdvertisedVideoEncoderBitrateModes() throws IOException {
        boolean skipped = true;
        final int[] modes = {
                MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_CQ,
                MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_VBR,
                MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_CBR
        };
        for (MediaCodecInfo info : mAllInfos) {
            if (!info.isEncoder()) {
                continue;
            }

            for (String mime: info.getSupportedTypes()) {
                boolean isVideo = isVideoMime(mime);
                if (!isVideo) {
                    continue;
                }
                CodecCapabilities caps = info.getCapabilitiesForType(mime);
                Size size = getVideoSizeForTest(caps.getVideoCapabilities());
                skipped = false;

                int numSupportedModes = 0;
                for (int mode : modes) {
                    MediaFormat format = createVideoFormatForBitrateMode(
                            mime, size.getWidth(), size.getHeight(), mode, caps);
                    if (format == null) {
                        continue;
                    }
                    MediaCodec codec = null;
                    try {
                        codec = MediaCodec.createByCodecName(info.getName());
                        codec.configure(format, null /* surface */, null /* crypto */,
                                MediaCodec.CONFIGURE_FLAG_ENCODE);
                    } finally {
                        if (codec != null) {
                            codec.release();
                        }
                    }
                    numSupportedModes++;
                }
                assertTrue(info.getName() + " has no supported bitrate mode",
                        numSupportedModes > 0);
            }
        }
        if (skipped) {
            MediaUtils.skipTest("no video encoders found");
        }
    }

    public void testAllNonTunneledVideoCodecsSupportFlexibleYUV() throws IOException {
        boolean skipped = true;
        for (MediaCodecInfo info : mAllInfos) {
            boolean isEncoder = info.isEncoder();
            for (String mime: info.getSupportedTypes()) {
                if (!isVideoMime(mime)) {
                    continue;
                }
                CodecCapabilities caps = info.getCapabilitiesForType(mime);
                if (caps.isFeatureRequired(CodecCapabilities.FEATURE_TunneledPlayback)
                        || caps.isFeatureRequired(CodecCapabilities.FEATURE_SecurePlayback)) {
                    continue;
                }
                skipped = false;
                boolean found = false;
                for (int c : caps.colorFormats) {
                    if (c == caps.COLOR_FormatYUV420Flexible) {
                        found = true;
                        break;
                    }
                }
                assertTrue(
                    info.getName() + " does not advertise COLOR_FormatYUV420Flexible",
                    found);

                MediaCodec codec = null;
                MediaFormat format = null;
                try {
                    Size size = getVideoSizeForTest(caps.getVideoCapabilities());
                    codec = MediaCodec.createByCodecName(info.getName());
                    format = createReasonableVideoFormat(
                            caps, mime, isEncoder, size.getWidth(), size.getHeight());
                    format.setInteger(
                            MediaFormat.KEY_COLOR_FORMAT,
                            caps.COLOR_FormatYUV420Flexible);

                    codec.configure(format, null /* surface */, null /* crypto */,
                            isEncoder ? codec.CONFIGURE_FLAG_ENCODE : 0);
                    MediaFormat configuredFormat =
                            isEncoder ? codec.getInputFormat() : codec.getOutputFormat();
                    Log.d(TAG, "color format is " + configuredFormat.getInteger(
                            MediaFormat.KEY_COLOR_FORMAT));
                    if (isEncoder) {
                        codec.start();
                        int ix = codec.dequeueInputBuffer(TIMEOUT_US);
                        assertNotNull(
                                info.getName() + " encoder has non-flexYUV input buffer #" + ix,
                                codec.getInputImage(ix));
                    } else {
                        // TODO: test these on various decoders (need test streams)
                    }
                } finally {
                    if (codec != null) {
                        codec.release();
                    }
                }
            }
        }
        if (skipped) {
            MediaUtils.skipTest("no non-tunneled/non-secure video decoders found");
        }
    }

    private static MediaFormat createMinFormat(String mime, CodecCapabilities caps) {
        MediaFormat format;
        if (caps.getVideoCapabilities() != null) {
            VideoCapabilities vcaps = caps.getVideoCapabilities();
            int minWidth = vcaps.getSupportedWidths().getLower();
            int minHeight = vcaps.getSupportedHeightsFor(minWidth).getLower();
            int minBitrate = vcaps.getBitrateRange().getLower();
            int minFrameRate = Math.max(vcaps.getSupportedFrameRatesFor(minWidth, minHeight)
                    .getLower().intValue(), 1);
            format = MediaFormat.createVideoFormat(mime, minWidth, minHeight);
            format.setInteger(MediaFormat.KEY_COLOR_FORMAT, caps.colorFormats[0]);
            format.setInteger(MediaFormat.KEY_BIT_RATE, minBitrate);
            format.setInteger(MediaFormat.KEY_FRAME_RATE, minFrameRate);
            format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, IFRAME_INTERVAL);
        } else {
            AudioCapabilities acaps = caps.getAudioCapabilities();
            int minSampleRate = acaps.getSupportedSampleRateRanges()[0].getLower();
            int minChannelCount = 1;
            int minBitrate = acaps.getBitrateRange().getLower();
            format = MediaFormat.createAudioFormat(mime, minSampleRate, minChannelCount);
            format.setInteger(MediaFormat.KEY_BIT_RATE, minBitrate);
        }

        return format;
    }

    private int getActualMax(
            boolean isEncoder, String name, String mime, CodecCapabilities caps, int max) {
        int flag = isEncoder ? MediaCodec.CONFIGURE_FLAG_ENCODE : 0;
        boolean memory_limited = false;
        MediaFormat format = createMinFormat(mime, caps);
        Log.d(TAG, "Test format " + format);
        Vector<MediaCodec> codecs = new Vector<MediaCodec>();
        MediaCodec codec = null;
        ActivityManager am = (ActivityManager)
                mContext.getSystemService(Context.ACTIVITY_SERVICE);
        ActivityManager.MemoryInfo outInfo = new ActivityManager.MemoryInfo();
        for (int i = 0; i < max; ++i) {
            try {
                Log.d(TAG, "Create codec " + name + " #" + i);
                codec = MediaCodec.createByCodecName(name);
                codec.configure(format, null, null, flag);
                codec.start();
                codecs.add(codec);
                codec = null;

                am.getMemoryInfo(outInfo);
                if (outInfo.lowMemory) {
                    Log.d(TAG, "System is in low memory condition, stopping. max: " + i);
                    memory_limited = true;
                    break;
                }
            } catch (IllegalArgumentException e) {
                fail("Got unexpected IllegalArgumentException " + e.getMessage());
            } catch (IOException e) {
                fail("Got unexpected IOException " + e.getMessage());
            } catch (MediaCodec.CodecException e) {
                // ERROR_INSUFFICIENT_RESOURCE is expected as the test keep creating codecs.
                // But other exception should be treated as failure.
                if (e.getErrorCode() == MediaCodec.CodecException.ERROR_INSUFFICIENT_RESOURCE) {
                    Log.d(TAG, "Got CodecException with ERROR_INSUFFICIENT_RESOURCE.");
                    break;
                } else {
                    fail("Unexpected CodecException " + e.getDiagnosticInfo());
                }
            } finally {
                if (codec != null) {
                    Log.d(TAG, "release codec");
                    codec.release();
                    codec = null;
                }
            }
        }
        int actualMax = codecs.size();
        for (int i = 0; i < codecs.size(); ++i) {
            Log.d(TAG, "release codec #" + i);
            codecs.get(i).release();
        }
        codecs.clear();
        // encode both actual max and whether we ran out of memory
        if (memory_limited) {
            actualMax = -actualMax;
        }
        return actualMax;
    }

    private boolean knownTypes(String type) {
        return (type.equalsIgnoreCase(MediaFormat.MIMETYPE_AUDIO_AAC  ) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_AUDIO_AC3      ) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_AUDIO_AMR_NB   ) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_AUDIO_AMR_WB   ) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_AUDIO_EAC3     ) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_AUDIO_FLAC     ) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_AUDIO_G711_ALAW) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_AUDIO_G711_MLAW) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_AUDIO_MPEG     ) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_AUDIO_MSGSM    ) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_AUDIO_OPUS     ) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_AUDIO_RAW      ) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_AUDIO_VORBIS   ) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_VIDEO_AV1      ) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_VIDEO_AVC      ) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_VIDEO_H263     ) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_VIDEO_HEVC     ) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_VIDEO_MPEG2    ) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_VIDEO_MPEG4    ) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_VIDEO_VP8      ) ||
            type.equalsIgnoreCase(MediaFormat.MIMETYPE_VIDEO_VP9      ));
    }

    public void testGetMaxSupportedInstances() {
        StringBuilder xmlOverrides = new StringBuilder();
        MediaCodecList allCodecs = new MediaCodecList(MediaCodecList.ALL_CODECS);
        final boolean isLowRam = ActivityManager.isLowRamDeviceStatic();
        for (MediaCodecInfo info : allCodecs.getCodecInfos()) {
            Log.d(TAG, "codec: " + info.getName());
            Log.d(TAG, "  isEncoder = " + info.isEncoder());

            // don't bother testing aliases
            if (info.isAlias()) {
                Log.d(TAG, "skipping: " + info.getName() + " is an alias for " +
                                info.getCanonicalName());
                continue;
            }

            String[] types = info.getSupportedTypes();
            for (int j = 0; j < types.length; ++j) {
                if (!knownTypes(types[j])) {
                    Log.d(TAG, "skipping unknown type " + types[j]);
                    continue;
                }
                Log.d(TAG, "calling getCapabilitiesForType " + types[j]);
                CodecCapabilities caps = info.getCapabilitiesForType(types[j]);
                int advertised = caps.getMaxSupportedInstances();
                Log.d(TAG, "getMaxSupportedInstances returns " + advertised);
                assertTrue(advertised > 0);

                // see how well the declared max matches against reality

                int tryMax = isLowRam ? 16 : 32;
                int tryMin = isLowRam ? 4 : 16;

                int trials = Math.min(advertised + 2, tryMax);
                int actualMax = getActualMax(
                        info.isEncoder(), info.getName(), types[j], caps, trials);
                Log.d(TAG, "actualMax " + actualMax + " vs advertised " + advertised
                                + " tryMin " + tryMin + " tryMax " + tryMax);

                boolean memory_limited = false;
                if (actualMax < 0) {
                    memory_limited = true;
                    actualMax = -actualMax;
                }

                boolean compliant = true;
                if (info.isHardwareAccelerated()) {
                    // very specific bounds for HW codecs
                    // so the adv+2 above is to see if the HW codec lets us go beyond adv
                    // (it should not)
                    if (actualMax != Math.min(advertised, tryMax)) {
                        Log.d(TAG, "NO: hwcodec " + actualMax + " != min(" + advertised +
                                            "," + tryMax + ")");
                        compliant = false;
                    }
                } else {
                    // sw codecs get a little more relaxation due to memory pressure
                    if (actualMax >= Math.min(advertised, tryMax)) {
                        // no memory issues, and we allocated them all
                        Log.d(TAG, "OK: swcodec " + actualMax + " >= min(" + advertised +
                                        "," + tryMax + ")");
                    } else if (actualMax >= Math.min(advertised, tryMin) &&
                                    memory_limited) {
                        // memory issues, but we hit our floors
                        Log.d(TAG, "OK: swcodec " + actualMax + " >= min(" + advertised +
                                        "," + tryMin + ") + memory limited");
                    } else {
                        Log.d(TAG, "NO: swcodec didn't meet criteria");
                        compliant = false;
                    }
                }

                if (!compliant) {
                    String codec = "<MediaCodec name=\"" + info.getName() +
                            "\" type=\"" + types[j] + "\" >";
                    String limit = "    <Limit name=\"concurrent-instances\" max=\"" +
                            actualMax + "\" />";
                    xmlOverrides.append(codec);
                    xmlOverrides.append("\n");
                    xmlOverrides.append(limit);
                    xmlOverrides.append("\n");
                    xmlOverrides.append("</MediaCodec>\n");
                }
            }
        }

        if (xmlOverrides.length() > 0) {
            String failMessage = "In order to pass the test, please publish following " +
                    "codecs' concurrent instances limit in /etc/media_codecs.xml: \n";
           fail(failMessage + xmlOverrides.toString());
        }
    }

    public void testGetSupportedFrameRates() throws IOException {
        // Chose MediaFormat.MIMETYPE_VIDEO_H263 randomly
        CodecCapabilities codecCap = CodecCapabilities.createFromProfileLevel(
                MediaFormat.MIMETYPE_VIDEO_H263, H263ProfileBaseline, H263Level45);
        Range<Integer> supportedFrameRates =
                codecCap.getVideoCapabilities().getSupportedFrameRates();
        Log.d(TAG, "Supported Frame Rates : " + supportedFrameRates.toString());
        /*
            ITU-T Rec. H.263/Annex X (03/2004) says that for H263ProfileBaseline and H263Level45,
            the device has to support 15 fps.
        */
        assertTrue("Invalid framerate range", Range.create(1, 15).equals(supportedFrameRates));
    }

    public void testIsSampleRateSupported() throws IOException {
        if (!MediaUtils.checkDecoder(MediaFormat.MIMETYPE_AUDIO_AAC)) {
            return; // skip
        }
        // Chose AAC Decoder/MediaFormat.MIMETYPE_AUDIO_AAC randomly
        MediaCodec codec = MediaCodec.createDecoderByType(MediaFormat.MIMETYPE_AUDIO_AAC);
        MediaCodecInfo.AudioCapabilities audioCap = codec.getCodecInfo()
                    .getCapabilitiesForType(MediaFormat.MIMETYPE_AUDIO_AAC).getAudioCapabilities();
        final int[] validSampleRates = {8000, 16000, 22050, 44100};
        for(int sampleRate : validSampleRates) {
            Log.d(TAG, "SampleRate = " + sampleRate);
            assertTrue("Expected True for isSampleRateSupported",
                audioCap.isSampleRateSupported(sampleRate));
        }
        final int[] invalidSampleRates = {-1, 0, 1, Integer.MAX_VALUE};
        for(int sampleRate : invalidSampleRates) {
            Log.d(TAG, "SampleRate = " + sampleRate);
            assertFalse("Expected False for isSampleRateSupported",
                audioCap.isSampleRateSupported(sampleRate));
        }
        codec.release();
    }

    // API test coverage for MediaCodecInfo.EncoderCapabilities.getComplexityRange()
    public void testGetComplexityRange() throws IOException {
        boolean skipTest = true;
        if (MediaUtils.hasEncoder(MediaFormat.MIMETYPE_AUDIO_AAC)) {
            // Chose AAC Encoder/MediaFormat.MIMETYPE_AUDIO_AAC randomly
            MediaCodec codec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_AUDIO_AAC);
            Range<Integer> complexityRange =
                    codec.getCodecInfo()
                            .getCapabilitiesForType(MediaFormat.MIMETYPE_AUDIO_AAC)
                            .getEncoderCapabilities()
                            .getComplexityRange();
            Log.d(TAG, "AAC ComplexityRange : " + complexityRange.toString());
            assertTrue("AAC ComplexityRange invalid low value", complexityRange.getLower() >= 0);
            codec.release();
            skipTest = false;
        }
        if (MediaUtils.hasEncoder(MediaFormat.MIMETYPE_AUDIO_FLAC)) {
            // Repeat test with FLAC Encoder
            MediaCodec codec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_AUDIO_FLAC);
            Range<Integer> complexityRange =
                    codec.getCodecInfo()
                            .getCapabilitiesForType(MediaFormat.MIMETYPE_AUDIO_FLAC)
                            .getEncoderCapabilities()
                            .getComplexityRange();
            Log.d(TAG, "FLAC ComplexityRange : " + complexityRange.toString());
            assertTrue("FLAC ComplexityRange invalid low value", complexityRange.getLower() >= 0);
            codec.release();
            skipTest = false;
        }
        if (skipTest) {
            MediaUtils.skipTest(TAG, "AAC and FLAC encoders not present");
        }
    }

    public void testLowLatencyFeatureIsSupportedOnly() throws IOException {
        MediaCodecList list = new MediaCodecList(MediaCodecList.ALL_CODECS);
        for (MediaCodecInfo info : list.getCodecInfos()) {
            for (String type : info.getSupportedTypes()) {
                CodecCapabilities caps = info.getCapabilitiesForType(type);
                if (caps.isFeatureSupported(CodecCapabilities.FEATURE_LowLatency)) {
                    assertFalse(
                        info.getName() + "(" + type + ") "
                            + " supports low latency, but low latency shall not be required",
                        caps.isFeatureRequired(CodecCapabilities.FEATURE_LowLatency));
                }
            }
        }
    }
}
