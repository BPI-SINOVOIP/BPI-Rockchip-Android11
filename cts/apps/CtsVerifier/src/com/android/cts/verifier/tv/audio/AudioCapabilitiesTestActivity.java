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

package com.android.cts.verifier.tv.audio;

import static android.media.AudioFormat.*;

import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioTrack;

import com.android.cts.verifier.R;
import com.android.cts.verifier.tv.TestSequence;
import com.android.cts.verifier.tv.TestStepBase;
import com.android.cts.verifier.tv.TvAppVerifierActivity;

import com.google.common.collect.ImmutableList;

import java.util.ArrayList;
import java.util.List;

/**
 * Test to verify Audio Capabilities APIs are correctly implemented.
 *
 * <p>This test checks if <br>
 * <a
 * href="https://developer.android.com/reference/android/media/AudioTrack#isDirectPlaybackSupported(android.media.AudioFormat,%20android.media.AudioAttributes)">
 * AudioTrack.isDirectPlaybackSupported()</a> <br>
 * return correct results when
 *
 * <ol>
 *   <li>No receiver or soundbar is connected
 *   <li>Receiver or soundbar is connected
 * </ol>
 */
public class AudioCapabilitiesTestActivity extends TvAppVerifierActivity {
    private static final ImmutableList<AudioFormat> TESTED_AUDIO_FORMATS =
            ImmutableList.of(
                    // PCM formats
                    makeAudioFormat(ENCODING_PCM_16BIT, 44100, 6),
                    makeAudioFormat(ENCODING_PCM_16BIT, 44100, 8),

                    // AC3 formats
                    makeAudioFormat(ENCODING_AC3, 44100, 6),
                    makeAudioFormat(ENCODING_AC3, 44100, 8),

                    // EAC3_JOC formats
                    makeAudioFormat(ENCODING_E_AC3_JOC, 44100, 8),
                    // EAC3 formats
                    makeAudioFormat(ENCODING_E_AC3, 44100, 8));

    private TestSequence mTestSequence;

    @Override
    protected void setInfoResources() {
        setInfoResources(
                R.string.tv_audio_capabilities_test, R.string.tv_audio_capabilities_test_info, -1);
    }

    @Override
    protected void createTestItems() {
        List<TestStepBase> testSteps = new ArrayList<>();
        testSteps.add(new TvTestStep(this));
        testSteps.add(new ReceiverTestStep(this));
        testSteps.add(new TvTestStep(this));
        mTestSequence = new TestSequence(this, testSteps);
        mTestSequence.init();
    }

    private class TvTestStep extends TestStep {
        public TvTestStep(TvAppVerifierActivity context) {
            super(
                    context,
                    getString(R.string.tv_audio_capabilities_no_receiver_connected),
                    R.string.tv_start_test);
        }

        @Override
        public void runTest() {
            AudioAttributes audioAttributes =
                    new AudioAttributes.Builder()
                            .setUsage(AudioAttributes.USAGE_MEDIA)
                            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                            .build();

            ImmutableList.Builder<String> actualAudioFormatStrings = ImmutableList.builder();
            for (AudioFormat audioFormat : TESTED_AUDIO_FORMATS) {
                if (AudioTrack.isDirectPlaybackSupported(audioFormat, audioAttributes)) {
                    actualAudioFormatStrings.add(toStr(audioFormat));
                }
            }

            getAsserter()
                    .withMessage("AudioTrack.isDirectPlaybackSupported only returns true for these")
                    .that(actualAudioFormatStrings.build())
                    .containsExactlyElementsIn(
                            ImmutableList.of(
                                    toStr(makeAudioFormat(ENCODING_PCM_16BIT, 44100, 6)),
                                    toStr(makeAudioFormat(ENCODING_AC3, 44100, 6))));
        }
    }

    private class ReceiverTestStep extends TestStep {
        public ReceiverTestStep(TvAppVerifierActivity context) {
            super(
                    context,
                    getString(R.string.tv_audio_capabilities_receiver_connected),
                    R.string.tv_start_test);
        }

        @Override
        public void runTest() {
            AudioAttributes audioAttributes =
                    new AudioAttributes.Builder()
                            .setUsage(AudioAttributes.USAGE_MEDIA)
                            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                            .build();

            ImmutableList.Builder<String> actualAudioFormatStrings = ImmutableList.builder();
            for (AudioFormat audioFormat : TESTED_AUDIO_FORMATS) {
                if (AudioTrack.isDirectPlaybackSupported(audioFormat, audioAttributes)) {
                    actualAudioFormatStrings.add(toStr(audioFormat));
                    break;
                }
            }

            getAsserter()
                    .withMessage("AudioTrack.isDirectPlaybackSupported only returns true for these")
                    .that(actualAudioFormatStrings.build())
                    .containsExactlyElementsIn(
                            ImmutableList.of(
                                    toStr(makeAudioFormat(ENCODING_PCM_16BIT, 41000, 8)),
                                    toStr(makeAudioFormat(ENCODING_AC3, 44100, 8)),
                                    toStr(makeAudioFormat(ENCODING_E_AC3_JOC, 44100, 8)),
                                    toStr(makeAudioFormat(ENCODING_E_AC3, 44100, 8))));
        }
    }

    /** Returns channel mask for {@code channelCount}. */
    private static int channelCountToMask(int channelCount) {
        switch (channelCount) {
            case 2:
                return CHANNEL_OUT_STEREO;
            case 6:
                return CHANNEL_OUT_5POINT1;
            case 8:
                return CHANNEL_OUT_7POINT1_SURROUND;
            default:
                return 0;
        }
    }

    /** Returns a displayable String message for {@code encodingCode}. */
    private static String encodingToString(int encodingCode) {
        switch (encodingCode) {
            case ENCODING_AC3:
                return "AC3";
            case ENCODING_E_AC3:
                return "E_AC3";
            case ENCODING_E_AC3_JOC:
                return "E_AC3_JOC";
            case ENCODING_PCM_16BIT:
                return "PCM_16BIT";
            default:
                return "Unknown Encoding";
        }
    }

    /** Converts Audio format object to string */
    private static String toStr(AudioFormat audioFormat) {
        return String.format(
                "{encoding: %s, channel count: %d}",
                encodingToString(audioFormat.getEncoding()), audioFormat.getChannelCount());
    }

    private static AudioFormat makeAudioFormat(int encoding, int sampleRate, int channelCount) {
        return new AudioFormat.Builder()
                .setEncoding(encoding)
                .setSampleRate(sampleRate)
                .setChannelMask(channelCountToMask(channelCount))
                .build();
    }
}
