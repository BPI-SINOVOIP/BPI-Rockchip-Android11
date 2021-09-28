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
package com.android.car.audio;

import android.media.AudioDeviceInfo;
import android.media.AudioDevicePort;
import android.media.AudioFormat;
import android.media.AudioGain;
import android.media.AudioGainConfig;
import android.media.AudioManager;
import android.media.AudioPort;
import android.util.Log;

import com.android.car.CarLog;
import com.android.internal.util.Preconditions;

import java.io.PrintWriter;
import java.util.Objects;

/**
 * A helper class wraps {@link AudioDeviceInfo}, and helps get/set the gain on a specific port
 * in terms of millibels.
 * Note to the reader. For whatever reason, it seems that AudioGain contains only configuration
 * information (min/max/step, etc) while the AudioGainConfig class contains the
 * actual currently active gain value(s).
 */
/* package */ class CarAudioDeviceInfo {

    private final AudioDeviceInfo mAudioDeviceInfo;
    private final int mSampleRate;
    private final int mEncodingFormat;
    private final int mChannelCount;
    private final int mDefaultGain;
    private final int mMaxGain;
    private final int mMinGain;
    private final int mStepValue;

    /**
     * We need to store the current gain because it is not accessible from the current
     * audio engine implementation. It would be nice if AudioPort#activeConfig() would return it,
     * but in the current implementation, that function actually works only for mixer ports.
     */
    private int mCurrentGain;

    CarAudioDeviceInfo(AudioDeviceInfo audioDeviceInfo) {
        mAudioDeviceInfo = audioDeviceInfo;
        mSampleRate = getMaxSampleRate(audioDeviceInfo);
        mEncodingFormat = getEncodingFormat(audioDeviceInfo);
        mChannelCount = getMaxChannels(audioDeviceInfo);
        final AudioGain audioGain = Objects.requireNonNull(
                getAudioGain(), "No audio gain on device port " + audioDeviceInfo);
        mDefaultGain = audioGain.defaultValue();
        mMaxGain = audioGain.maxValue();
        mMinGain = audioGain.minValue();
        mStepValue = audioGain.stepValue();

        mCurrentGain = -1; // Not initialized till explicitly set
    }

    AudioDeviceInfo getAudioDeviceInfo() {
        return mAudioDeviceInfo;
    }

    AudioDevicePort getAudioDevicePort() {
        return mAudioDeviceInfo.getPort();
    }

    String getAddress() {
        return mAudioDeviceInfo.getAddress();
    }

    int getDefaultGain() {
        return mDefaultGain;
    }

    int getMaxGain() {
        return mMaxGain;
    }

    int getMinGain() {
        return mMinGain;
    }

    int getSampleRate() {
        return mSampleRate;
    }

    int getEncodingFormat() {
        return mEncodingFormat;
    }

    int getChannelCount() {
        return mChannelCount;
    }

    int getStepValue() {
        return mStepValue;
    }

    // Input is in millibels
    void setCurrentGain(int gainInMillibels) {
        // Clamp the incoming value to our valid range.  Out of range values ARE legal input
        if (gainInMillibels < mMinGain) {
            gainInMillibels = mMinGain;
        } else if (gainInMillibels > mMaxGain) {
            gainInMillibels = mMaxGain;
        }

        // Push the new gain value down to our underlying port which will cause it to show up
        // at the HAL.
        AudioGain audioGain = getAudioGain();
        if (audioGain == null) {
            Log.e(CarLog.TAG_AUDIO, "getAudioGain() returned null.");
            return;
        }

        // size of gain values is 1 in MODE_JOINT
        AudioGainConfig audioGainConfig = audioGain.buildConfig(
                AudioGain.MODE_JOINT,
                audioGain.channelMask(),
                new int[] { gainInMillibels },
                0);
        if (audioGainConfig == null) {
            Log.e(CarLog.TAG_AUDIO, "Failed to construct AudioGainConfig");
            return;
        }

        int r = AudioManager.setAudioPortGain(getAudioDevicePort(), audioGainConfig);
        if (r == AudioManager.SUCCESS) {
            // Since we can't query for the gain on a device port later,
            // we have to remember what we asked for
            mCurrentGain = gainInMillibels;
        } else {
            Log.e(CarLog.TAG_AUDIO, "Failed to setAudioPortGain: " + r);
        }
    }

    private int getMaxSampleRate(AudioDeviceInfo info) {
        int[] sampleRates = info.getSampleRates();
        if (sampleRates == null || sampleRates.length == 0) {
            return 48000;
        }
        int sampleRate = sampleRates[0];
        for (int i = 1; i < sampleRates.length; i++) {
            if (sampleRates[i] > sampleRate) {
                sampleRate = sampleRates[i];
            }
        }
        return sampleRate;
    }

    /** Always returns {@link AudioFormat#ENCODING_PCM_16BIT} as for now */
    private int getEncodingFormat(AudioDeviceInfo info) {
        return AudioFormat.ENCODING_PCM_16BIT;
    }

    /**
     * Gets the maximum channel count for a given {@link AudioDeviceInfo}
     *
     * @param info {@link AudioDeviceInfo} instance to get maximum channel count for
     * @return Maximum channel count for a given {@link AudioDeviceInfo},
     * 1 (mono) if there is no channel masks configured
     */
    private int getMaxChannels(AudioDeviceInfo info) {
        int numChannels = 1;
        int[] channelMasks = info.getChannelMasks();
        if (channelMasks == null) {
            return numChannels;
        }
        for (int channelMask : channelMasks) {
            int currentNumChannels = Integer.bitCount(channelMask);
            if (currentNumChannels > numChannels) {
                numChannels = currentNumChannels;
            }
        }
        return numChannels;
    }

    /**
     * @return {@link AudioGain} with {@link AudioGain#MODE_JOINT} on a given {@link AudioPort}.
     * This is useful for inspecting the configuration data associated with this gain controller
     * (min/max/step/default).
     */
    AudioGain getAudioGain() {
        final AudioDevicePort audioPort = getAudioDevicePort();
        if (audioPort != null && audioPort.gains().length > 0) {
            for (AudioGain audioGain : audioPort.gains()) {
                if ((audioGain.mode() & AudioGain.MODE_JOINT) != 0) {
                    return checkAudioGainConfiguration(audioGain);
                }
            }
        }
        return null;
    }

    /**
     * Constraints applied to gain configuration, see also audio_policy_configuration.xml
     */
    private AudioGain checkAudioGainConfiguration(AudioGain audioGain) {
        Preconditions.checkArgument(audioGain.maxValue() >= audioGain.minValue());
        Preconditions.checkArgument((audioGain.defaultValue() >= audioGain.minValue())
                && (audioGain.defaultValue() <= audioGain.maxValue()));
        Preconditions.checkArgument(
                ((audioGain.maxValue() - audioGain.minValue()) % audioGain.stepValue()) == 0);
        Preconditions.checkArgument(
                ((audioGain.defaultValue() - audioGain.minValue()) % audioGain.stepValue()) == 0);
        return audioGain;
    }

    @Override
    public String toString() {
        return "address: " + mAudioDeviceInfo.getAddress()
                + " sampleRate: " + getSampleRate()
                + " encodingFormat: " + getEncodingFormat()
                + " channelCount: " + getChannelCount()
                + " currentGain: " + mCurrentGain
                + " maxGain: " + mMaxGain
                + " minGain: " + mMinGain;
    }

    void dump(String indent, PrintWriter writer) {
        writer.printf("%sCarAudioDeviceInfo Device(%s)\n ",
                indent, mAudioDeviceInfo.getAddress());
        writer.printf("%s\tsample rate / encoding format / channel count: %d %d %d\n",
                indent, getSampleRate(), getEncodingFormat(), getChannelCount());
        writer.printf("%s\tGain values (min / max / default/ current): %d %d %d %d\n",
                indent, mMinGain, mMaxGain, mDefaultGain, mCurrentGain);
    }
}
