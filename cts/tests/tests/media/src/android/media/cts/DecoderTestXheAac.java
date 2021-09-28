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

package android.media.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.Instrumentation;
import android.content.res.AssetFileDescriptor;
import android.content.res.Resources;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.media.cts.DecoderTest.AudioParameter;
import android.media.cts.DecoderTestAacDrc.DrcParams;
import android.media.cts.R;
import android.os.Build;
import android.util.Log;
import android.os.Bundle;

import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.ApiLevelUtil;
import com.android.compatibility.common.util.MediaUtils;

import org.junit.Before;
import org.junit.Test;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class DecoderTestXheAac {
    private static final String TAG = "DecoderTestXheAac";

    private static final boolean sIsAndroidRAndAbove =
            ApiLevelUtil.isAtLeast(Build.VERSION_CODES.R);

    private Resources mResources;

    // list of all AAC decoders as enumerated through the MediaCodecList
    // lazy initialization in setUp()
    private static ArrayList<String> sAacDecoderNames;

    @Before
    public void setUp() throws Exception {
        final Instrumentation inst = InstrumentationRegistry.getInstrumentation();
        assertNotNull(inst);
        mResources = inst.getContext().getResources();
        // build a list of all AAC decoders on which to run the test
        if (sAacDecoderNames == null) {
            sAacDecoderNames = initAacDecoderNames();
        }
    }

    protected static ArrayList<String> initAacDecoderNames() {
        // at least 1 AAC decoder expected
        ArrayList<String> aacDecoderNames = new ArrayList<String>(1);
        final MediaCodecList mediaCodecList = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
        final MediaCodecInfo[] mediaCodecInfos = mediaCodecList.getCodecInfos();
        for (MediaCodecInfo mediaCodecInfo : mediaCodecInfos) {
            if (mediaCodecInfo.isAlias()) {
                continue;
            }
            if (mediaCodecInfo.isEncoder()) {
                continue;
            }
            final String[] mimeTypes = mediaCodecInfo.getSupportedTypes();
            for (String mimeType : mimeTypes) {
                if (MediaFormat.MIMETYPE_AUDIO_AAC.equalsIgnoreCase(mimeType)) {
                    aacDecoderNames.add(mediaCodecInfo.getName());
                    break;
                }
            }
        }
        return aacDecoderNames;
    }

    /**
     * Verify the correct decoding of USAC bitstreams with different MPEG-D DRC effect types.
     */
    @Test
    public void testDecodeUsacDrcEffectTypeM4a() throws Exception {
        Log.v(TAG, "START testDecodeUsacDrcEffectTypeM4a");

        assertTrue("No AAC decoder found", sAacDecoderNames.size() > 0);

        for (String aacDecName : sAacDecoderNames) {
            try {
                runDecodeUsacDrcEffectTypeM4a(aacDecName);
            } catch (Error err) {
                throw new Error(err.getMessage() + " [dec=" + aacDecName + "]" , err);
            }
        }
    }

    private void runDecodeUsacDrcEffectTypeM4a(String aacDecName) throws Exception {
        Log.v(TAG, "testDecodeUsacDrcEffectTypeM4a running for dec=" + aacDecName);
        // test DRC effectTypeID 1 "NIGHT"
        // L -3dB -> normalization factor = 1/(10^(-3/10)) = 0.5011f
        // R +3dB -> normalization factor = 1/(10^( 3/10)) = 1.9952f
        try {
            checkUsacDrcEffectType(1, 0.5011f, 1.9952f, "Night", 2, 0, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcEffectTypeM4a Night/2/0 failed for dec=" + aacDecName);
            throw new RuntimeException(e);
        }

        // test DRC effectTypeID 2 "NOISY"
        // L +3dB -> normalization factor = 1/(10^( 3/10)) = 1.9952f
        // R -6dB -> normalization factor = 1/(10^(-6/10)) = 0.2511f
        try {
            checkUsacDrcEffectType(2, 1.9952f, 0.2511f, "Noisy", 2, 0, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcEffectTypeM4a Noisy/2/0 failed for dec=" + aacDecName);
            throw new RuntimeException(e);
        }

        // test DRC effectTypeID 3 "LIMITED"
        // L -6dB -> normalization factor = 1/(10^(-6/10)) = 0.2511f
        // R +6dB -> normalization factor = 1/(10^( 6/10)) = 3.9810f
        try {
            checkUsacDrcEffectType(3, 0.2511f, 3.9810f, "Limited", 2, 0, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcEffectTypeM4a Limited/2/0 failed for dec="
                    + aacDecName);
            throw new RuntimeException(e);
        }

        // test DRC effectTypeID 6 "GENERAL"
        // L +6dB -> normalization factor = 1/(10^( 6/10)) = 3.9810f
        // R -3dB -> normalization factor = 1/(10^(-3/10)) = 0.5011f
        try {
            checkUsacDrcEffectType(6, 3.9810f, 0.5011f, "General", 2, 0, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcEffectTypeM4a General/2/0 failed for dec="
                    + aacDecName);
            throw new RuntimeException(e);
        }

        // test DRC effectTypeID 1 "NIGHT"
        // L    -6dB -> normalization factor = 1/(10^(-6/10)) = 0.2511f
        // R    +6dB -> normalization factor = 1/(10^( 6/10)) = 3.9810f
        // mono -6dB -> normalization factor = 1/(10^(-6/10)) = 0.2511f
        try {
            checkUsacDrcEffectType(1, 0.2511f, 3.9810f, "Night", 2, 1, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcEffectTypeM4a Night/2/1 for dec=" + aacDecName);
            throw new RuntimeException(e);
        }
        try {
            checkUsacDrcEffectType(1, 0.2511f, 0.0f, "Night", 1, 1, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcEffectTypeM4a Night/1/1 for dec=" + aacDecName);
            throw new RuntimeException(e);
        }

        // test DRC effectTypeID 2 "NOISY"
        // L    +6dB -> normalization factor = 1/(10^( 6/10))   = 3.9810f
        // R    -9dB -> normalization factor = 1/(10^(-9/10))  = 0.1258f
        // mono +6dB -> normalization factor = 1/(10^( 6/10))   = 3.9810f
        try {
            checkUsacDrcEffectType(2, 3.9810f, 0.1258f, "Noisy", 2, 1, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcEffectTypeM4a Noisy/2/1 for dec=" + aacDecName);
            throw new RuntimeException(e);
        }
        try {
            checkUsacDrcEffectType(2, 3.9810f, 0.0f, "Noisy", 1, 1, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcEffectTypeM4a Night/2/1 for dec=" + aacDecName);
            throw new RuntimeException(e);
        }

        // test DRC effectTypeID 3 "LIMITED"
        // L    -9dB -> normalization factor = 1/(10^(-9/10)) = 0.1258f
        // R    +9dB -> normalization factor = 1/(10^( 9/10)) = 7.9432f
        // mono -9dB -> normalization factor = 1/(10^(-9/10)) = 0.1258f
        try {
            checkUsacDrcEffectType(3, 0.1258f, 7.9432f, "Limited", 2, 1, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcEffectTypeM4a Limited/2/1 for dec=" + aacDecName);
            throw new RuntimeException(e);
        }
        try {
            checkUsacDrcEffectType(3, 0.1258f, 0.0f, "Limited", 1, 1, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcEffectTypeM4a Limited/1/1 for dec=" + aacDecName);
            throw new RuntimeException(e);
        }

        // test DRC effectTypeID 6 "GENERAL"
        // L    +9dB -> normalization factor = 1/(10^( 9/10)) = 7.9432f
        // R    -6dB -> normalization factor = 1/(10^(-6/10))  = 0.2511f
        // mono +9dB -> normalization factor = 1/(10^( 9/10)) = 7.9432f
        try {
            checkUsacDrcEffectType(6, 7.9432f, 0.2511f, "General", 2, 1, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcEffectTypeM4a General/2/1 for dec=" + aacDecName);
            throw new RuntimeException(e);
        }
        try {
            checkUsacDrcEffectType(6, 7.9432f, 0.0f, "General", 1, 1, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcEffectTypeM4a General/1/1 for dec=" + aacDecName);
            throw new RuntimeException(e);
        }
    }

    /**
     * Verify the correct decoding of USAC bitstreams with album mode.
     */
    @Test
    public void testDecodeUsacDrcAlbumModeM4a() throws Exception {
        Log.v(TAG, "START testDecodeUsacDrcAlbumModeM4a");

        // Album mode is R feature
        if (!MediaUtils.check(sIsAndroidRAndAbove, "Album mode support requires Android R"))
                return;

        assertTrue("No AAC decoder found", sAacDecoderNames.size() > 0);

        for (String aacDecName : sAacDecoderNames) {
            try {
                runDecodeUsacDrcAlbumModeM4a(aacDecName);
            } catch (Error err) {
                throw new Error(err.getMessage() + " [dec=" + aacDecName + "]" , err);
            }
        }
    }

    private void runDecodeUsacDrcAlbumModeM4a(String aacDecName) throws Exception {
        // test DRC Album Mode
        // Track loudness = -19dB
        // Album Loudness = -21 dB
        // Fading Gains = -6 dB
        // Album Mode ON : Gains = -24 - (-21) = -3dB
        // Album Mode OFF : Gains = (-24 -(-19)) + (-6) = -11 dB
        try {
            checkUsacDrcAlbumMode(R.raw.noise_2ch_48khz_tlou_19lufs_alou_21lufs_mp4, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcAlbumModeM4a for decoder" + aacDecName);
            throw new RuntimeException(e);
        }
    }

    /**
     * Verify the correct decoding of USAC bitstreams with config changes.
     */
    @Test
    public void testDecodeUsacStreamSwitchingM4a() throws Exception {
        Log.v(TAG, "START testDecodeUsacStreamSwitchingM4a");

        assertTrue("No AAC decoder found", sAacDecoderNames.size() > 0);

        for (String aacDecName : sAacDecoderNames) {
            try {
                runDecodeUsacStreamSwitchingM4a(aacDecName);
            } catch (Error err) {
                throw new Error(err.getMessage() + " [dec=" + aacDecName + "]" , err);
            }
        }
    }

    private void runDecodeUsacStreamSwitchingM4a(String aacDecName) throws Exception {
        // Stereo
        // switch between SBR ratios and stereo modes
        try {
            checkUsacStreamSwitching(2.5459829E12f, 2,
                    R.raw.noise_2ch_44_1khz_aot42_19_lufs_config_change_mp4, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacStreamSwitchingM4a failed 2ch sbr/stereo switch for "
                    + aacDecName);
            throw new RuntimeException(e);
        }

        // Mono
        // switch between SBR ratios and stereo modes
        try {
            checkUsacStreamSwitching(2.24669126E12f, 1,
                    R.raw.noise_1ch_38_4khz_aot42_19_lufs_config_change_mp4, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacStreamSwitchingM4a failed 1ch sbr/stereo switch for "
                    + aacDecName);
            throw new RuntimeException(e);
        }

        // Stereo
        // switch between USAC modes
        try {
            checkUsacStreamSwitching(2.1E12f, 2,
                    R.raw.noise_2ch_35_28khz_aot42_19_lufs_drc_config_change_mp4, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacStreamSwitchingM4a failed 2ch USAC mode switch for "
                    + aacDecName);
            throw new RuntimeException(e);
        }

        // Mono
        // switch between USAC modes
        try {
            checkUsacStreamSwitching(1.7E12f, 1,
                    R.raw.noise_1ch_29_4khz_aot42_19_lufs_drc_config_change_mp4, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacStreamSwitchingM4a failed 1ch USAC mode switch for "
                    + aacDecName);
            throw new RuntimeException(e);
        }

    }

    /**
     * Verify the correct decoding of USAC bitstreams with various sampling rates.
     */
    @Test
    public void testDecodeUsacSamplingRatesM4a() throws Exception {
        Log.v(TAG, "START testDecodeUsacSamplingRatesM4a");

        assertTrue("No AAC decoder found", sAacDecoderNames.size() > 0);

        for (String aacDecName : sAacDecoderNames) {
            try {
                runDecodeUsacSamplingRatesM4a(aacDecName);
            } catch (Error err) {
                throw new Error(err.getMessage() + " [dec=" + aacDecName + "]" , err);
            }
        }
    }

    private void runDecodeUsacSamplingRatesM4a(String aacDecName) throws Exception {
        try {
            checkUsacSamplingRate(R.raw.noise_2ch_08khz_aot42_19_lufs_mp4, aacDecName);
            checkUsacSamplingRate(R.raw.noise_2ch_12khz_aot42_19_lufs_mp4, aacDecName);
            checkUsacSamplingRate(R.raw.noise_2ch_22_05khz_aot42_19_lufs_mp4, aacDecName);
            checkUsacSamplingRate(R.raw.noise_2ch_64khz_aot42_19_lufs_mp4, aacDecName);
            checkUsacSamplingRate(R.raw.noise_2ch_88_2khz_aot42_19_lufs_mp4, aacDecName);
            checkUsacSamplingRateWoLoudness(R.raw.noise_2ch_19_2khz_aot42_no_ludt_mp4,
                    aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacSamplingRatesM4a for decoder" + aacDecName);
            throw new RuntimeException(e);
        }
    }

    /**
     * Verify the correct decoding of USAC bitstreams with different boost and attenuation settings
     */
    @Test
    public void testDecodeUsacDrcBoostAndAttenuationM4a() throws Exception {
        Log.v(TAG, "START testDecodeUsacDrcBoostAndAttenuationM4a");

        if (!MediaUtils.check(sIsAndroidRAndAbove, "Att/Boost corrected in Android R"))
            return;

        assertTrue("No AAC decoder found", sAacDecoderNames.size() > 0);

        for (String aacDecName : sAacDecoderNames) {
            try {
                runDecodeUsacDrcBoostAndAttenuationM4a(aacDecName);
            } catch (Error err) {
                throw new Error(err.getMessage() + " [dec=" + aacDecName + "]" , err);
            }
        }
    }

    private void runDecodeUsacDrcBoostAndAttenuationM4a(String aacDecName) throws Exception {
        Log.v(TAG, "testDecodeUsacDrcBoostAndAttenuationM4a running for dec=" + aacDecName);
        // test drcBoost and drcAttenuation parameters
        // DRC effectTypeID 6 "GENERAL"
        // L +6dB -> normalization factor = 10^(6/10 * (1 - boostFactor:64/127)) = 1.9844f
        // R -3dB -> normalization factor = 10^(-3/10 * (1 - attenuationFactor:127/127)) = 1.0f
        try {
            checkUsacDrcBoostAndAttenuation(1.9844f, 1.0f, 64, 127, 2, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcBoostAndAttenuationM4a failed for dec=" + aacDecName);
            throw new RuntimeException(e);
        }

        // test drcBoost and drcAttenuation parameters
        // DRC effectTypeID 6 "GENERAL"
        // L +6dB -> normalization factor = 10^(6/10 * (1 - boostFactor:127/127)) = 1.0f
        // R -3dB -> normalization factor = 10^(-3/10 * (1 - attenuationFactor:64/127)) = 0.7099f
        try {
            checkUsacDrcBoostAndAttenuation(1.0f, 0.7099f, 127, 64, 2, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcBoostAndAttenuationM4a failed for dec=" + aacDecName);
            throw new RuntimeException(e);
        }

        // test drcBoost and drcAttenuation parameters
        // DRC effectTypeID 6 "GENERAL"
        // L +6dB -> normalization factor = 10^(6/10 * (1 - boostFactor:0/127)) = 3.9811f
        // R -3dB -> normalization factor = 10^(-3/10 * (1 - attenuationFactor:127/127)) = 1.0f
        try {
            checkUsacDrcBoostAndAttenuation(3.9811f, 1.0f, 0, 127, 2, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcBoostAndAttenuationM4a failed for dec=" + aacDecName);
            throw new RuntimeException(e);
        }

        // test drcBoost and drcAttenuation parameters
        // DRC effectTypeID 6 "GENERAL"
        // L +6dB -> normalization factor = 10^(6/10 * (1 - boostFactor:127/127)) = 1.0f
        // R -3dB -> normalization factor = 10^(-3/10 * (1 - attenuationFactor:0/127)) = 0.5012f
        try {
            checkUsacDrcBoostAndAttenuation(1.0f, 0.5012f, 127, 0, 2, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcBoostAndAttenuationM4a failed for dec=" + aacDecName);
            throw new RuntimeException(e);
        }
    }

    /**
     * verify the correct decoding of USAC bitstreams when different kinds of loudness values
     * are present
     */
    @Test
    public void testDecodeUsacDrcLoudnessPreferenceM4a() throws Exception {
        Log.v(TAG, "START testDecodeUsacDrcLoudnessPreferenceM4a");

        if (!MediaUtils.check(sIsAndroidRAndAbove, "Loudness preference in Android R"))
            return;

        assertTrue("No AAC decoder found", sAacDecoderNames.size() > 0);

        for (String aacDecName : sAacDecoderNames) {
            try {
                runDecodeUsacDrcLoudnessPreferenceM4a(aacDecName);
            } catch (Error err) {
                throw new Error(err.getMessage() + " [dec=" + aacDecName + "]" , err);
            }
        }
    }

    private void runDecodeUsacDrcLoudnessPreferenceM4a(String aacDecName) throws Exception {
        Log.v(TAG, "testDecodeUsacDrcLoudnessPreferenceM4a running for dec=" + aacDecName);
        // test drc loudness preference
        // anchor loudness (-17 LUFS) and program loudness (-19 LUFS) are present in one stream
        // -> anchor loudness should be selected
        // the bitstream is decoded with targetLoudnessLevel = -16 LUFS and
        // checked against the energy of the decoded signal without loudness normalization
        // normfactor = loudness of waveform - targetLoudnessLevel = -1dB = 0.7943
        try {
            checkUsacDrcLoudnessPreference(
                    R.raw.noise_2ch_48khz_tlou_19lufs_anchor_17lufs_mp4, 0.7943f, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcLoudnessPreferenceM4a failed for dec=" + aacDecName);
            throw new RuntimeException(e);
        }

        // test drc loudness preference
        // expert loudness (-23 LUFS) and program loudness (-19 LUFS) are present in one stream
        // -> expert loudness should be selected
        // the bitstream is decoded with targetLoudnessLevel = -16 LUFS and
        // checked against the energy of the decoded signal without loudness normalization
        // normfactor = loudness of waveform - targetLoudnessLevel = -7dB = 0.1995
        try {
            checkUsacDrcLoudnessPreference(
                    R.raw.noise_2ch_48khz_tlou_19lufs_expert_23lufs_mp4, 0.1995f, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcLoudnessPreferenceM4a failed for dec=" + aacDecName);
            throw new RuntimeException(e);
        }
    }

    /**
     * Verify that the correct output loudness values are returned when decoding USAC bitstreams
     */
    @Test
    public void testDecodeUsacDrcOutputLoudnessM4a() throws Exception {
        Log.v(TAG, "START testDecodeUsacDrcOutputLoudnessM4a");

        assertTrue("No AAC decoder found", sAacDecoderNames.size() > 0);

        for (String aacDecName : sAacDecoderNames) {
            try {
                runDecodeUsacDrcOutputLoudnessM4a(aacDecName);
            } catch (Error err) {
                throw new Error(err.getMessage() + " [dec=" + aacDecName + "]" , err);
            }
        }
    }

    private void runDecodeUsacDrcOutputLoudnessM4a(String aacDecName) throws Exception {
        Log.v(TAG, "testDecodeUsacDrcOutputLoudnessM4a running for dec=" + aacDecName);
        // test drc output loudness
        // testfile without loudness metadata and loudness normalization off -> expected value: -1
        try {
            checkUsacDrcOutputLoudness(
                    R.raw.noise_2ch_19_2khz_aot42_no_ludt_mp4, -1, -1, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcOutputLoudnessM4a failed for dec=" + aacDecName);
            throw new RuntimeException(e);
        }

        Log.v(TAG, "testDecodeUsacDrcOutputLoudnessM4a running for dec=" + aacDecName);
        // test drc output loudness
        // testfile without loudness metadata and loudness normalization on
        // -> expected value: -1
        try {
            checkUsacDrcOutputLoudness(
                    R.raw.noise_2ch_19_2khz_aot42_no_ludt_mp4, 64, -1, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcOutputLoudnessM4a failed for dec=" + aacDecName);
            throw new RuntimeException(e);
        }

        // test drc output loudness
        // testfile with MPEG-D DRC loudness metadata and loudness normalization off
        // -> expected value: loudness metadata in bitstream (-19*-4 = 76)
        try {
            checkUsacDrcOutputLoudness(
                    R.raw.noise_2ch_08khz_aot42_19_lufs_mp4, -1, 76, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcOutputLoudnessM4a failed for dec=" + aacDecName);
            throw new RuntimeException(e);
        }

        // test drc output loudness
        // testfile with MPEG-D DRC loudness metadata and loudness normalization off
        // -> expected value: loudness metadata in bitstream (-22*-4 = 88)
        try {
            checkUsacDrcOutputLoudness(
                    R.raw.noise_1ch_38_4khz_aot42_19_lufs_config_change_mp4, -1, 88, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcOutputLoudnessM4a failed for dec=" + aacDecName);
            throw new RuntimeException(e);
        }

        // test drc output loudness
        // testfile with MPEG-D DRC loudness metadata and loudness normalization on
        // -> expected value: target loudness value (92)
        try {
            checkUsacDrcOutputLoudness(
                    R.raw.noise_2ch_08khz_aot42_19_lufs_mp4, 92, 92, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacDrcOutputLoudnessM4a failed for dec=" + aacDecName);
            throw new RuntimeException(e);
        }
    }


    /**
     * Verify that seeking works correctly for USAC.
     * Sync samples have to be taken into consideration.
     */
    @Test
    public void testDecodeUsacSyncSampleSeekingM4a() throws Exception {
        Log.v(TAG, "START testDecodeUsacSyncSampleSeekingM4a");

        assertTrue("No AAC decoder found", sAacDecoderNames.size() > 0);

        for (String aacDecName : sAacDecoderNames) {
            try {
                runDecodeUsacSyncSampleSeekingM4a(aacDecName);
            } catch (Error err) {
                throw new Error(err.getMessage() + " [dec=" + aacDecName + "]" , err);
            }
        }
    }

    private void runDecodeUsacSyncSampleSeekingM4a(String aacDecName) throws Exception {
        Log.v(TAG, "testDecodeUsacSyncSampleSeekingM4a running for dec=" + aacDecName);
        // test usac seeking
        try {
            checkUsacSyncSampleSeeking(R.raw.sine_2ch_48khz_aot42_seek_mp4, aacDecName);
        } catch (Exception e) {
            Log.v(TAG, "testDecodeUsacSyncSampleSeekingM4a failed for dec=" + aacDecName);
            throw new RuntimeException(e);
        }
        Log.v(TAG, "testDecodeUsacSyncSampleSeekingM4a running for dec=" + aacDecName);
    }

    /**
     *  Internal utilities
     */

    /**
     * USAC test DRC Effect Type
     */
    private void checkUsacDrcEffectType(int effectTypeID, float normFactor_L, float normFactor_R,
                 String effectTypeName, int nCh, int aggressiveDrc, String decoderName)
                         throws Exception {
        for (boolean runtimeChange : new boolean[] {false, true}) {
            if (runtimeChange && !sIsAndroidRAndAbove) {
                // changing decoder configuration after it has been initialized requires R and above
                continue;
            }
            int testinput = -1;
            AudioParameter decParams = new AudioParameter();
            DrcParams drcParams_def  = new DrcParams(127, 127, 96, 0, -1);
            DrcParams drcParams_test = new DrcParams(127, 127, 96, 0, effectTypeID);

            if (aggressiveDrc == 0) {
                testinput = R.raw.noise_2ch_32khz_aot42_19_lufs_drc_mp4;
            } else {
                if (nCh == 2) {
                    testinput = R.raw.noise_2ch_35_28khz_aot42_19_lufs_drc_config_change_mp4;
                } else if (nCh == 1){
                    testinput = R.raw.noise_1ch_29_4khz_aot42_19_lufs_drc_config_change_mp4;
                }
            }

            short[] decSamples_def  = decodeToMemory(decParams, testinput,
                    -1, null, drcParams_def, decoderName);
            short[] decSamples_test = decodeToMemory(decParams, testinput,
                    -1, null, drcParams_test, decoderName, runtimeChange);

            float[] nrg_def  = checkEnergyUSAC(decSamples_def, decParams, nCh, 1, 0);
            float[] nrg_test = checkEnergyUSAC(decSamples_test, decParams, nCh, 1, 1);

            if (nCh == 2) {
                float nrgRatio_L = (nrg_test[1]/nrg_def[1])/normFactor_L;
                float nrgRatio_R = (nrg_test[2]/nrg_def[2])/normFactor_R;
                if ((nrgRatio_R > 1.05f || nrgRatio_R < 0.95f)
                        || (nrgRatio_L > 1.05f || nrgRatio_L < 0.95f) ){
                    throw new Exception("DRC Effect Type '" + effectTypeName + "' not as expected");
                }
            } else if (nCh == 1){
                float nrgRatio_L = (nrg_test[0]/nrg_def[0])/normFactor_L;
                if (nrgRatio_L > 1.05f || nrgRatio_L < 0.95f){
                    throw new Exception("DRC Effect Type '" + effectTypeName + "' not as expected");
                }
            }
        }
    }

    /**
     * USAC test stream switching
     */
    private void checkUsacStreamSwitching(float nrg_ref, int encNch, int testinput,
            String decoderName) throws Exception
    {
        AudioParameter decParams = new AudioParameter();
        DrcParams drcParams      = new DrcParams(127, 127, 64, 0, -1);

        // Check stereo stream switching
        short[] decSamples = decodeToMemory(decParams, testinput,
                -1, null, drcParams, decoderName);
        float[] nrg = checkEnergyUSAC(decSamples, decParams, encNch, 1);

        float nrgRatio = nrg[0] / nrg_ref;

        // Check if energy levels are within 15% of the reference
        // Energy drops within the decoded stream are checked by checkEnergyUSAC() within every
        // 250ms interval
        if (nrgRatio > 1.15f || nrgRatio < 0.85f ) {
            throw new Exception("Config switching not as expected");
        }
    }

    /**
     * USAC test sampling rate
     */
    private void checkUsacSamplingRate(int testinput, String decoderName) throws Exception {
        AudioParameter decParams  = new AudioParameter();
        DrcParams drcParams_def   = new DrcParams(127, 127, 64, 0, -1);
        DrcParams drcParams_test  = new DrcParams(127, 127, 96, 0, -1);

        short[] decSamples_def  = decodeToMemory(decParams, testinput,
                -1, null, drcParams_def, decoderName);
        short[] decSamples_test = decodeToMemory(decParams, testinput,
                -1, null, drcParams_test, decoderName);

        float[] nrg_def  = checkEnergyUSAC(decSamples_def, decParams, 2, 1);
        float[] nrg_test = checkEnergyUSAC(decSamples_test, decParams, 2, 1);

        float nrgRatio = nrg_def[0]/nrg_test[0];

        // normFactor = 1/(10^(-8/10)) = 6.3f
        nrgRatio = nrgRatio / 6.3f;

        // Check whether behavior is as expected
        if (nrgRatio > 1.05f || nrgRatio < 0.95f ){
            throw new Exception("Sampling rate not supported");
        }
    }

    /**
     * USAC test sampling rate for streams without loudness application
     */
    private void checkUsacSamplingRateWoLoudness(int testinput, String decoderName) throws Exception
    {
        AudioParameter decParams  = new AudioParameter();
        DrcParams drcParams       = new DrcParams();

        short[] decSamples = decodeToMemory(decParams, testinput, -1, null, drcParams, decoderName);

        float[] nrg = checkEnergyUSAC(decSamples, decParams, 2, 1);

        float nrg_ref  = 3.15766394E12f;
        float nrgRatio = nrg_ref/nrg[0];

        // Check whether behavior is as expected
        if (nrgRatio > 1.05f || nrgRatio < 0.95f ){
            throw new Exception("Sampling rate not supported");
        }
    }

    /**
     * USAC test DRC Album Mode
     */
    private void checkUsacDrcAlbumMode(int testinput, String decoderName) throws Exception {
        for (boolean runtimeChange : new boolean[] {false, true}) {
            AudioParameter decParams = new AudioParameter();
            DrcParams drcParams_album_off = new DrcParams(127, 127, 64, 0, 0, 0);
            DrcParams drcParams_album_on  = new DrcParams(127, 127, 64, 0, 0, 1);

            short[] decSamples_album_off = decodeToMemory(
                    decParams, testinput, -1, null, drcParams_album_off, decoderName);
            short[] decSamples_album_on = decodeToMemory(
                    decParams, testinput, -1, null, drcParams_album_on, decoderName, runtimeChange);

            float[] nrg_album_off  = checkEnergyUSAC(decSamples_album_off, decParams, 2, 1);
            float[] nrg_album_on = checkEnergyUSAC(decSamples_album_on, decParams, 2, 1);

            float normFactor = 6.3095f;

            float nrgRatio = (nrg_album_on[0]/nrg_album_off[0])/normFactor;
            float nrgRatio_L = (nrg_album_on[1]/nrg_album_off[1])/normFactor;
            float nrgRatio_R = (nrg_album_on[2]/nrg_album_off[2])/normFactor;

            if (nrgRatio > 1.05f || nrgRatio < 0.95f ){
                throw new Exception("DRC Album Mode not supported, energy ratio " + nrgRatio);
            }
        }
    }

    /**
     * USAC test DRC Boost and Attenuation
     */
    private void checkUsacDrcBoostAndAttenuation(float normFactor_L, float normFactor_R,
                                                 int boostFactor, int attenuationFactor,
                                                 int nCh, String decoderName) throws Exception {
        for (boolean runtimeChange : new boolean[] {false, true}) {
            if (runtimeChange && !sIsAndroidRAndAbove) {
                // changing decoder configuration after it has been initialized requires R and above
                continue;
            }

            int testinput = R.raw.noise_2ch_32khz_aot42_19_lufs_drc_mp4;

            AudioParameter decParams = new AudioParameter();
            DrcParams drcParams_def = new DrcParams(127, 127, 64, 0, 6);
            DrcParams drcParams_test = new DrcParams(boostFactor, attenuationFactor, 64, 0, 6);

            short[] decSamples_def = decodeToMemory(decParams, testinput, -1, null,
                    drcParams_def, decoderName);
            short[] decSamples_test = decodeToMemory(decParams, testinput, -1, null,
                    drcParams_test, decoderName, runtimeChange);

            float[] nrg_def = checkEnergyUSAC(decSamples_def, decParams, 2, 1);
            float[] nrg_test = checkEnergyUSAC(decSamples_test, decParams, 2, 1);

            float nrgRatioLeft = nrg_test[1] / nrg_def[1];
            float nrgRatioRight = nrg_test[2] / nrg_def[2];

            float testValueLeft = normFactor_L * nrgRatioLeft;
            float testValueRight = normFactor_R * nrgRatioRight;

            // Check whether loudness behavior is as expected
            if (testValueLeft > 1.05f || testValueLeft < 0.95f) {
                throw new Exception("DRC boost/attenuation behavior not as expected");
            }
            if (testValueRight > 1.05f || testValueRight < 0.95f) {
                throw new Exception("DRC boost/attenuation behavior not as expected");
            }
        }
    }

    /**
    * USAC test Loudness Preference
    */
    private void checkUsacDrcLoudnessPreference(int testInput, float normFactor, String decoderName) throws Exception {

        AudioParameter decParams = new AudioParameter();
        DrcParams drcParams_def = new DrcParams(127, 127, -1, 0, 6);
        DrcParams drcParams_test = new DrcParams(127, 127, 64, 0, 6);

        // Check drc loudness preference
        short[] decSamples_def = decodeToMemory(decParams, testInput, -1, null, drcParams_def, decoderName);
        short[] decSamples_test = decodeToMemory(decParams, testInput, -1, null, drcParams_test, decoderName);

        float[] nrg_def  = checkEnergyUSAC(decSamples_def, decParams, 2, 1);
        float[] nrg_test = checkEnergyUSAC(decSamples_test, decParams, 2, 1);

        float nrgRatio = (nrg_test[0]/nrg_def[0]);
        nrgRatio = nrgRatio * normFactor;

        if (nrgRatio > 1.05f || nrgRatio < 0.95f ){
            throw new Exception("DRC Loudness preference not as expected");
        }
    }

    /**
    * USAC test Output Loudness
    */
    private void checkUsacDrcOutputLoudness(int testInput, int decoderTargetLevel,
            int expectedOutputLoudness, String decoderName) throws Exception {
        for (boolean runtimeChange : new boolean[] {false, true}) {
            AudioParameter decParams = new AudioParameter();
            DrcParams drcParams_test = new DrcParams(127, 127, decoderTargetLevel, 0, 6);

            // Check drc loudness preference
            short[] decSamples_test = decodeToMemory(
                    decParams, testInput, -1, null, drcParams_test,
                    decoderName, runtimeChange, expectedOutputLoudness);
        }
    }

    private void checkUsacSyncSampleSeeking(int testInput, String decoderName) throws Exception {

        AudioParameter decParams = new AudioParameter();
        DrcParams drcParams_def = new DrcParams();

        short[] decSamples_seek_next_sync = decodeToMemory(decParams, testInput, -1, null,
                drcParams_def, decoderName, false, -2, true, 1100000,
                MediaExtractor.SEEK_TO_NEXT_SYNC);
        float[] nrg_seek_next_sync = checkEnergyUSAC(decSamples_seek_next_sync, decParams, 2, 1);
    }

    /**
     * Perform a segmented energy analysis on given audio signal samples and run several tests on
     * the energy values.
     *
     * The main purpose is to verify whether a USAC decoder implementation applies Spectral Band
     * Replication (SBR), Parametric Stereo (PS) and Dynamic Range Control (DRC) correctly. All
     * tools are inherent parts to either the MPEG-D USAC audio codec or the MPEG-D DRC tool.
     *
     * In addition, this test can verify the correct decoding of multi-channel (e.g. 5.1 channel)
     * streams or the creation of a downmixed signal.
     *
     * Note: This test procedure is not an MPEG Conformance Test and can not serve as a replacement.
     *
     * @param decSamples the decoded audio samples to be tested
     * @param decParams the audio parameters of the given audio samples (decSamples)
     * @param encNch the encoded number of audio channels (number of channels of the original
     *               input)
     * @param drcContext indicate to use test criteria applicable for DRC testing
     * @return array of energies, at index 0: accumulated energy of all channels, and
     *     index 1 and over contain the individual channel energies
     * @throws RuntimeException
     */
    protected float[] checkEnergyUSAC(short[] decSamples, AudioParameter decParams,
                                      int encNch, int drcContext)
    {
        final float[] nrg = checkEnergyUSAC(decSamples, decParams, encNch, drcContext,  0);
        return nrg;
    }

    /**
     * Same as {@link #checkEnergyUSAC(short[], AudioParameter, int, int)} but with DRC effect type
     * @param decSamples
     * @param decParams
     * @param encNch
     * @param drcContext
     * @param drcApplied indicate if MPEG-D DRC Effect Type has been applied
     * @return
     * @throws RuntimeException
     */
    private float[] checkEnergyUSAC(short[] decSamples, AudioParameter decParams,
                                    int encNch, int drcContext,  int drcApplied)
            throws RuntimeException
    {
        String localTag = TAG + "#checkEnergyUSAC";

        // the number of segments per block
        final int nSegPerBlk = 4;
        // the number of input channels
        final int nCh = encNch;
        // length of one (LB/HB) block [samples]
        final int nBlkSmp = decParams.getSamplingRate();
        // length of one segment [samples]
        final int nSegSmp = nBlkSmp / nSegPerBlk;
        // actual # samples per channel (total)
        final int smplPerChan = decSamples.length / nCh;
        // actual # samples per segment (all ch)
        final int nSegSmpTot = nSegSmp * nCh;
        // signal offset between chans [segments]
        final int nSegChOffst = 2 * nSegPerBlk;
        // // the number of channels to be analyzed
        final int procNch = Math.min(nCh, encNch);
        // all original configs with more than five channel have an LFE
        final int encEffNch = (encNch > 5) ? encNch-1 : encNch;
        // expected number of decoded audio samples
        final int expSmplPerChan = Math.max(encEffNch, 2) * nSegChOffst * nSegSmp;
        // flag telling that input is dmx signal
        final boolean isDmx = nCh < encNch;
        final float nrgRatioThresh = 0.0f;
        // the num analyzed channels with signal
        int effProcNch = procNch;

        // get the signal offset by counting zero samples at the very beginning (over all channels)
        // sample value threshold 4 signal search
        final int zeroSigThresh = 1;
        // receives the number of samples that are in front of the actual signal
        int signalStart = smplPerChan;
        // receives the number of null samples (per chan) at the very beginning
        int noiseStart = signalStart;

        for (int smpl = 0; smpl < decSamples.length; smpl++) {
            int value = Math.abs(decSamples[smpl]);

            if (value > 0 && noiseStart == signalStart) {
                // store start of prepended noise (can be same as signalStart)
                noiseStart = smpl / nCh;
            }

            if (value > zeroSigThresh) {
                // store signal start offset [samples]
                signalStart = smpl / nCh;
                break;
            }
        }

        signalStart = (signalStart > noiseStart+1) ? signalStart : noiseStart;

        // check if the decoder reproduced a waveform or kept silent
        assertTrue ("no signal found in any channel!", signalStart < smplPerChan);

        // max num seg that fit into signal
        final int totSeg = (smplPerChan - signalStart) / nSegSmp;
        // max num relevant samples (per channel)
        final int totSmp = nSegSmp * totSeg;

        // check if the number of reproduced segments in the audio file is valid
        assertTrue("no segments left to test after signal search", totSeg > 0);

        // get the energies and the channel offsets by searching for the first segment above the
        // energy threshold:
        // ratio of zeroNrgThresh to the max nrg
        final double zeroMaxNrgRatio = 0.001f;
        // threshold to classify segment energies
        double zeroNrgThresh = nSegSmp * nSegSmp;
        // will store the max seg nrg over all ch
        double totMaxNrg = 0.0f;
        // array receiving the segment energies
        double[][] nrg = new double[procNch][totSeg];
        // array for channel offsets
        int[] offset = new int[procNch];
        // array receiving the segment energy status over all channels
        boolean[] sigSeg = new boolean[totSeg];
        // energy per channel
        double[] nrgPerChannel = new double[procNch];
        // return value: [0]: total energy of all channels
        //               [1 ... procNch + 1]: energy of the individual channels
        float[] nrgTotal = new float[procNch + 1];
        // mapping array to sort channels
        int[] chMap = new int[nCh];

        // calculate the segmental energy for each channel
        for (int ch = 0; ch < procNch; ch++) {
            offset[ch] = -1;

            for (int seg = 0; seg < totSeg; seg++) {
                final int smpStart = (signalStart * nCh) + (seg * nSegSmpTot) + ch;
                final int smpStop = smpStart + nSegSmpTot;

                for (int smpl = smpStart; smpl < smpStop; smpl += nCh) {
                    // accumulate total energy per channel
                    nrgPerChannel[ch] += decSamples[smpl] * decSamples[smpl];
                    // accumulate segment energy
                    nrg[ch][seg] += nrgPerChannel[ch];
                }

                // store 1st segment (index) per channel which has energy above the threshold to get
                // the ch offsets
                if (nrg[ch][seg] > zeroNrgThresh && offset[ch] < 0) {
                    offset[ch] = seg / nSegChOffst;
                }

                // store the max segment nrg over all ch
                if (nrg[ch][seg] > totMaxNrg) {
                    totMaxNrg = nrg[ch][seg];
                }

                // store whether the channel has energy in this segment
                sigSeg[seg] |= nrg[ch][seg] > zeroNrgThresh;
            }

            // if one channel has no signal it is most probably the LFE the LFE is no
            // effective channel
            if (offset[ch] < 0) {
                effProcNch -= 1;
                offset[ch] = effProcNch;
            }

            // recalculate the zero signal threshold based on the 1st channels max energy for
            // all subsequent checks
            if (ch == 0) {
                zeroNrgThresh = zeroMaxNrgRatio * totMaxNrg;
            }
        }

        // check if the LFE is decoded properly
        assertTrue("more than one LFE detected", effProcNch >= procNch - 1);

        // check if the amount of samples is valid
        assertTrue(String.format("less samples decoded than expected: %d < %d",
                                 decSamples.length - (signalStart * nCh),
                                 totSmp * effProcNch),
                                 decSamples.length - (signalStart * nCh) >= totSmp * effProcNch);

        // for multi-channel signals the only valid front channel orders
        // are L, R, C or C, L, R (L=left, R=right, C=center)
        if (procNch >= 5) {
            final int[] frontChMap1 = {2, 0, 1};
            final int[] frontChMap2 = {0, 1, 2};

            // check if the channel mapping is valid
            if (!(Arrays.equals(Arrays.copyOfRange(offset, 0, 3), frontChMap1)
                    || Arrays.equals(Arrays.copyOfRange(offset, 0, 3), frontChMap2))) {
                fail("wrong front channel mapping");
            }
        }

        // create mapping table to address channels from front to back the LFE must be last
        if (drcContext == 0) {
            // check whether every channel occurs exactly once
            for (int ch = 0; ch < effProcNch; ch++) {
                int occurred = 0;

                for (int idx = 0; idx < procNch; idx++) {
                    if (offset[idx] == ch) {
                        occurred += 1;
                        chMap[ch] = idx;
                    }
                }

                // check if one channel is mapped more than one time
                assertTrue(String.format("channel %d occurs %d times in the mapping", ch, occurred),
                        occurred == 1);
            }
        } else {
            for (int ch = 0; ch < procNch; ch++) {
                chMap[ch] = ch;
            }
        }

        // reference min energy for the 1st ch; others will be compared against 1st
        double refMinNrg = zeroNrgThresh;

        // calculate total energy, min and max energy
        for (int ch = 0; ch < procNch; ch++) {
            // resolve channel mapping
            int idx = chMap[ch];
            // signal offset [segments]
            final int ofst = offset[idx] * nSegChOffst;

            if (ch <= effProcNch && ofst < totSeg) {
                // the last segment that has energy
                int nrgSegEnd;
                // the number of segments with energy
                int nrgSeg;

                if (drcContext == 0) {

                    // the first channel of a mono or stereo signal has full signal all others have
                    // one LB + one HB block
                    if ((encNch <= 2) && (ch == 0)) {
                        nrgSeg = totSeg;
                    } else {
                        nrgSeg = Math.min(totSeg, (2 * nSegPerBlk) + ofst) - ofst;
                    }
                } else {
                    nrgSeg = totSeg;
                }

                nrgSegEnd = ofst + nrgSeg;

                // find min and max energy of all segments that should have signal
                double minNrg = nrg[idx][ofst]; // channels minimum segment energy
                double maxNrg = nrg[idx][ofst]; // channels maximum segment energy

                // values of 1st segment already assigned
                for (int seg = ofst + 1; seg < nrgSegEnd; seg++) {
                    if (nrg[idx][seg] < minNrg) {
                        minNrg = nrg[idx][seg];
                    }

                    if (nrg[idx][seg] > maxNrg) {
                        maxNrg = nrg[idx][seg];
                    }
                }

                // check if the energy of this channel is > 0
                assertTrue(String.format("max energy of channel %d is zero", ch),maxNrg > 0.0f);

                if (drcContext == 0) {
                    // check the channels minimum energy >= refMinNrg
                    assertTrue(String.format("channel %d has not enough energy", ch),
                            minNrg >= refMinNrg);

                    if (ch == 0) {
                        // use 85% of 1st channels min energy as reference the other chs must meet
                        refMinNrg = minNrg * 0.85f;
                    } else if (isDmx && (ch == 1)) {
                        // in case of downmixed signal the energy can be lower depending on the
                        refMinNrg *= 0.50f;
                    }

                    // calculate and check the energy ratio
                    final double nrgRatio = minNrg / maxNrg;

                    // check if the threshold is exceeded
                    assertTrue(String.format("energy ratio of channel %d below threshold", ch),
                            nrgRatio >= nrgRatioThresh);

                    if (!isDmx) {
                        if (nrgSegEnd < totSeg) {
                            // consider that some noise can extend into the subsequent segment
                            // allow this to be at max 20% of the channels minimum energy
                            assertTrue(
                                    String.format("min energy after noise above threshold (%.2f)",
                                    nrg[idx][nrgSegEnd]),
                                    nrg[idx][nrgSegEnd] < minNrg * 0.20f);
                            nrgSegEnd += 1;
                        }
                    } else {
                        // ignore all subsequent segments in case of a downmixed signal
                        nrgSegEnd = totSeg;
                    }

                    // zero-out the verified energies to simplify the subsequent check
                    for (int seg = ofst; seg < nrgSegEnd; seg++) {
                        nrg[idx][seg] = 0.0f;
                    }

                    // check zero signal parts
                    for (int seg = 0; seg < totSeg; seg++) {
                        assertTrue(String.format("segment %d in channel %d has signal where should "
                                + "be none (%.2f)", seg, ch, nrg[idx][seg]),
                                nrg[idx][seg] < zeroNrgThresh);
                    }
                }
            }

            // test whether each segment has energy in at least one channel
            for (int seg = 0; seg < totSeg; seg++) {
                assertTrue(String.format("no channel has energy in segment %d", seg), sigSeg[seg]);
            }

            nrgTotal[0]     += (float)nrgPerChannel[ch];
            nrgTotal[ch + 1] = (float)nrgPerChannel[ch];
        }

        float errorMargin = 0.0f;
        if (drcApplied == 0) {
            errorMargin = 0.25f;
        } else if (drcApplied == 1) {
            errorMargin = 0.40f;
        }

        float totSegEnergy = 0.0f;
        float[] segEnergy = new float[totSeg];
        for (int seg = totSeg - 1; seg >= 0; seg--) {
            if (seg != 0) {
                for (int ch = 0; ch < nCh; ch++) {
                    segEnergy[seg] += nrg[ch][seg] - nrg[ch][seg - 1];
                }
                totSegEnergy += segEnergy[seg];
            } else {
                for (int ch = 0; ch < nCh; ch++) {
                    segEnergy[seg] += nrg[ch][seg];
                }
            }
        }
        float avgSegEnergy = totSegEnergy / (totSeg - 1);
        for (int seg = 1; seg < totSeg; seg++) {
            float energyRatio = segEnergy[seg] / avgSegEnergy;
            if ((energyRatio > (1.0f + errorMargin) ) || (energyRatio < (1.0f - errorMargin) )) {
                fail("Energy drop out");
            }
        }

        // return nrgTotal: [0]: accumulated energy of all channels, [1 ... ] channel energies
        return nrgTotal;
    }

    /**
     * Decodes a compressed bitstream in the ISOBMFF into the RAM of the device.
     *
     * The decoder decodes compressed audio data as stored in the ISO Base Media File Format
     * (ISOBMFF) aka .mp4/.m4a. The decoder is not reproducing the waveform but stores the decoded
     * samples in the RAM of the device under test.
     *
     * @param audioParams the decoder parameter configuration
     * @param testinput the compressed audio stream
     * @param eossample the End-Of-Stream indicator
     * @param timestamps the time stamps to decode
     * @param drcParams the MPEG-D DRC decoder parameter configuration
     * @param decoderName if non null, the name of the decoder to use for the decoding, otherwise
     *     the default decoder for the format will be used
     * @param runtimeChange defines whether the decoder is configured at runtime or configured
     *                      before starting to decode
     * @param expectedOutputLoudness value to check if the correct output loudness is returned
     *     by the decoder
     * @param seek_enable defines whether there will be an initial seek
     * @param seek_duration seeking duration in microseconds
     * @param seek_mode seeking mode 
     *
     * @throws RuntimeException
     */
    public short[] decodeToMemory(AudioParameter audioParams, int testinput, int eossample,
            List<Long> timestamps, DrcParams drcParams, String decoderName, boolean runtimeChange,
            int expectedOutputLoudness,
            boolean seek_enable, int seek_duration, int seek_mode) throws IOException {
        // TODO: code is the same as in DecoderTest, differences are:
        //          - addition of application of DRC parameters
        //          - no need/use of resetMode, configMode
        //       Split method so code can be shared

        final String localTag = TAG + "#decodeToMemory";
        short [] decoded = new short[0];
        int decodedIdx = 0;

        AssetFileDescriptor testFd = mResources.openRawResourceFd(testinput);

        MediaExtractor extractor;
        MediaCodec codec;
        ByteBuffer[] codecInputBuffers;
        ByteBuffer[] codecOutputBuffers;

        extractor = new MediaExtractor();
        extractor.setDataSource(testFd.getFileDescriptor(), testFd.getStartOffset(),
                testFd.getLength());
        testFd.close();

        assertEquals("wrong number of tracks", 1, extractor.getTrackCount());
        MediaFormat format = extractor.getTrackFormat(0);
        String mime = format.getString(MediaFormat.KEY_MIME);
        assertTrue("not an audio file", mime.startsWith("audio/"));

        MediaFormat configFormat = format;
        if (decoderName == null) {
            codec = MediaCodec.createDecoderByType(mime);
        } else {
            codec = MediaCodec.createByCodecName(decoderName);
        }

        // set DRC parameters
        if (drcParams != null) {
            configFormat.setInteger(MediaFormat.KEY_AAC_DRC_HEAVY_COMPRESSION, drcParams.mHeavy);
            if (!runtimeChange) {
                configFormat.setInteger(MediaFormat.KEY_AAC_DRC_BOOST_FACTOR, drcParams.mBoost);
                configFormat.setInteger(MediaFormat.KEY_AAC_DRC_ATTENUATION_FACTOR, drcParams.mCut);
                if (drcParams.mDecoderTargetLevel != 0) {
                    configFormat.setInteger(MediaFormat.KEY_AAC_DRC_TARGET_REFERENCE_LEVEL,
                            drcParams.mDecoderTargetLevel);
                }
                if (drcParams.mEffectType != 0){
                    configFormat.setInteger(MediaFormat.KEY_AAC_DRC_EFFECT_TYPE,
                            drcParams.mEffectType);
                }
                if (drcParams.mAlbumMode != 0) {
                    configFormat.setInteger(MediaFormat.KEY_AAC_DRC_ALBUM_MODE,
                            drcParams.mAlbumMode);
                }
            }
        }

        Log.v(localTag, "configuring with " + configFormat);
        codec.configure(configFormat, null /* surface */, null /* crypto */, 0 /* flags */);

        if (drcParams != null && sIsAndroidRAndAbove) { // querying output format requires R
            if(!runtimeChange) {
                if (drcParams.mAlbumMode != 0) {
                    int albumModeFromCodec = codec.getOutputFormat()
                            .getInteger(MediaFormat.KEY_AAC_DRC_ALBUM_MODE);
                    if (albumModeFromCodec != drcParams.mAlbumMode) {
                        fail("Drc AlbumMode received from MediaCodec is not the Album Mode set");
                    }
                }
                if (drcParams.mEffectType != 0) {
                    final int effectTypeFromCodec = codec.getOutputFormat()
                            .getInteger(MediaFormat.KEY_AAC_DRC_EFFECT_TYPE);
                    if (effectTypeFromCodec != drcParams.mEffectType) {
                        fail("Drc Effect Type received from MediaCodec is not the Effect Type set");
                    }
                }
                if (drcParams.mDecoderTargetLevel != 0) {
                    final int targetLevelFromCodec = codec.getOutputFormat()
                            .getInteger(MediaFormat.KEY_AAC_DRC_TARGET_REFERENCE_LEVEL);
                    if (targetLevelFromCodec != drcParams.mDecoderTargetLevel) {
                        fail("Drc Target Reference Level received from MediaCodec is not the Target Reference Level set");
                    }
                }
            }
        }

        codec.start();
        codecInputBuffers = codec.getInputBuffers();
        codecOutputBuffers = codec.getOutputBuffers();

        if (drcParams != null) {
            if (runtimeChange) {
                Bundle b = new Bundle();
                b.putInt(MediaFormat.KEY_AAC_DRC_BOOST_FACTOR, drcParams.mBoost);
                b.putInt(MediaFormat.KEY_AAC_DRC_ATTENUATION_FACTOR, drcParams.mCut);
                if (drcParams.mEffectType != 0) {
                    b.putInt(MediaFormat.KEY_AAC_DRC_EFFECT_TYPE, drcParams.mEffectType);
                }
                if (drcParams.mDecoderTargetLevel != 0) {
                    b.putInt(MediaFormat.KEY_AAC_DRC_TARGET_REFERENCE_LEVEL,
                            drcParams.mDecoderTargetLevel);
                }
                if (drcParams.mAlbumMode != 0) {
                    b.putInt(MediaFormat.KEY_AAC_DRC_ALBUM_MODE, drcParams.mAlbumMode);
                }
                codec.setParameters(b);
            }
        }

        extractor.selectTrack(0);

        // execute initial seeking if specified
        if (seek_enable) {
            codec.flush();
            extractor.seekTo(seek_duration, seek_mode);
        }

        // start decoding
        final long kTimeOutUs = 5000;
        MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
        boolean sawInputEOS = false;
        boolean sawOutputEOS = false;
        int noOutputCounter = 0;
        int samplecounter = 0;

        // main decoding loop
        while (!sawOutputEOS && noOutputCounter < 50) {
            noOutputCounter++;
            if (!sawInputEOS) {
                int inputBufIndex = codec.dequeueInputBuffer(kTimeOutUs);

                if (inputBufIndex >= 0) {
                    ByteBuffer dstBuf = codecInputBuffers[inputBufIndex];

                    int sampleSize =
                        extractor.readSampleData(dstBuf, 0 /* offset */);

                    long presentationTimeUs = 0;

                    if (sampleSize < 0 && eossample > 0) {
                        fail("test is broken: never reached eos sample");
                    }

                    if (sampleSize < 0) {
                        Log.d(TAG, "saw input EOS.");
                        sawInputEOS = true;
                        sampleSize = 0;
                    } else {
                        if (samplecounter == eossample) {
                            sawInputEOS = true;
                        }
                        samplecounter++;
                        presentationTimeUs = extractor.getSampleTime();
                    }

                    codec.queueInputBuffer(
                            inputBufIndex,
                            0 /* offset */,
                            sampleSize,
                            presentationTimeUs,
                            sawInputEOS ? MediaCodec.BUFFER_FLAG_END_OF_STREAM : 0);

                    if (!sawInputEOS) {
                        extractor.advance();
                    }
                }
            }

            int res = codec.dequeueOutputBuffer(info, kTimeOutUs);

            if (res >= 0) {
                //Log.d(TAG, "got frame, size " + info.size + "/" + info.presentationTimeUs);

                if (info.size > 0) {
                    noOutputCounter = 0;
                    if (timestamps != null) {
                        timestamps.add(info.presentationTimeUs);
                    }
                }

                int outputBufIndex = res;
                ByteBuffer buf = codecOutputBuffers[outputBufIndex];

                if (decodedIdx + (info.size / 2) >= decoded.length) {
                    decoded = Arrays.copyOf(decoded, decodedIdx + (info.size / 2));
                }

                buf.position(info.offset);
                for (int i = 0; i < info.size; i += 2) {
                    decoded[decodedIdx++] = buf.getShort();
                }

                codec.releaseOutputBuffer(outputBufIndex, false /* render */);

                if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                    Log.d(TAG, "saw output EOS.");
                    sawOutputEOS = true;
                }
            } else if (res == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
                codecOutputBuffers = codec.getOutputBuffers();

                Log.d(TAG, "output buffers have changed.");
            } else if (res == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                MediaFormat oformat = codec.getOutputFormat();
                audioParams.setNumChannels(oformat.getInteger(MediaFormat.KEY_CHANNEL_COUNT));
                audioParams.setSamplingRate(oformat.getInteger(MediaFormat.KEY_SAMPLE_RATE));
                Log.d(TAG, "output format has changed to " + oformat);
            } else {
                Log.d(TAG, "dequeueOutputBuffer returned " + res);
            }
        }

        if (noOutputCounter >= 50) {
            fail("decoder stopped outputting data");
        }

        // check if MediaCodec gives back correct drc parameters
        if (drcParams != null && sIsAndroidRAndAbove) {
            if (drcParams.mAlbumMode != 0) {
                final int albumModeFromCodec = codec.getOutputFormat()
                        .getInteger(MediaFormat.KEY_AAC_DRC_ALBUM_MODE);
                assertEquals("DRC AlbumMode received from MediaCodec is not the Album Mode set"
                        + " runtime:" + runtimeChange, drcParams.mAlbumMode, albumModeFromCodec);
            }
            if (drcParams.mEffectType != 0) {
                final int effectTypeFromCodec = codec.getOutputFormat()
                        .getInteger(MediaFormat.KEY_AAC_DRC_EFFECT_TYPE);
                assertEquals("DRC Effect Type received from MediaCodec is not the Effect Type set"
                        + " runtime:" + runtimeChange, drcParams.mEffectType, effectTypeFromCodec);
            }
            if (drcParams.mDecoderTargetLevel != 0) {
                final int targetLevelFromCodec = codec.getOutputFormat()
                        .getInteger(MediaFormat.KEY_AAC_DRC_TARGET_REFERENCE_LEVEL);
                assertEquals("DRC Target Ref Level received from MediaCodec is not the level set"
                        + " runtime:" + runtimeChange,
                        drcParams.mDecoderTargetLevel, targetLevelFromCodec);
            }

            final MediaFormat outputFormat = codec.getOutputFormat();
            final int cutFromCodec = outputFormat.getInteger(
                    MediaFormat.KEY_AAC_DRC_ATTENUATION_FACTOR);
            assertEquals("Attenuation factor received from MediaCodec differs from set:",
                    drcParams.mCut, cutFromCodec);
            final int boostFromCodec = outputFormat.getInteger(
                    MediaFormat.KEY_AAC_DRC_BOOST_FACTOR);
            assertEquals("Boost factor received from MediaCodec differs from set:",
                    drcParams.mBoost, boostFromCodec);
        }

        // expectedOutputLoudness == -2 indicates that output loudness is not tested
        if (expectedOutputLoudness != -2 && sIsAndroidRAndAbove) {
            final int outputLoudnessFromCodec = codec.getOutputFormat()
                    .getInteger(MediaFormat.KEY_AAC_DRC_OUTPUT_LOUDNESS);
            if (outputLoudnessFromCodec != expectedOutputLoudness) {
                fail("Received decoder output loudness is not the expected value");
            }
        }

        codec.stop();
        codec.release();
        return decoded;
    }

    private short[] decodeToMemory(AudioParameter audioParams, int testinput,
            int eossample, List<Long> timestamps, DrcParams drcParams, String decoderName)
            throws IOException
    {
        final short[] decoded = decodeToMemory(audioParams, testinput, eossample, timestamps,
                drcParams, decoderName, false, -2, false, 0, 0);
        return decoded;
    }

    private short[] decodeToMemory(AudioParameter audioParams, int testinput,
            int eossample, List<Long> timestamps, DrcParams drcParams, String decoderName,
            boolean runtimeChange)
        throws IOException
    {
        final short[] decoded = decodeToMemory(audioParams, testinput, eossample, timestamps,
                drcParams, decoderName, runtimeChange, -2, false, 0, 0);
        return decoded;
    }

    private short[] decodeToMemory(AudioParameter audioParams, int testinput,
        int eossample, List<Long> timestamps, DrcParams drcParams, String decoderName,
        boolean runtimeChange, int expectedOutputLoudness)
        throws IOException
    {
        short [] decoded = decodeToMemory(audioParams, testinput, eossample, timestamps, drcParams,
                decoderName, runtimeChange, expectedOutputLoudness, false, 0, 0);
        return decoded;
    }
}

