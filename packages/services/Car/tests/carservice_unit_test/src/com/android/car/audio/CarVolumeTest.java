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

package com.android.car.audio;

import static android.media.AudioAttributes.USAGE_ALARM;
import static android.media.AudioAttributes.USAGE_ASSISTANCE_NAVIGATION_GUIDANCE;
import static android.media.AudioAttributes.USAGE_MEDIA;
import static android.media.AudioAttributes.USAGE_NOTIFICATION;
import static android.media.AudioAttributes.USAGE_VIRTUAL_SOURCE;
import static android.media.AudioAttributes.USAGE_VOICE_COMMUNICATION;
import static android.telephony.TelephonyManager.CALL_STATE_IDLE;
import static android.telephony.TelephonyManager.CALL_STATE_OFFHOOK;
import static android.telephony.TelephonyManager.CALL_STATE_RINGING;

import static com.android.car.audio.CarAudioContext.ALARM;
import static com.android.car.audio.CarAudioContext.CALL;
import static com.android.car.audio.CarAudioContext.CALL_RING;
import static com.android.car.audio.CarAudioContext.NAVIGATION;
import static com.android.car.audio.CarAudioService.DEFAULT_AUDIO_CONTEXT;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.media.AudioAttributes;
import android.media.AudioAttributes.AttributeUsage;
import android.media.AudioPlaybackConfiguration;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.audio.CarAudioContext.AudioContext;

import com.google.common.collect.ImmutableList;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

@RunWith(AndroidJUnit4.class)
public class CarVolumeTest {
    @Test
    public void getSuggestedAudioContext_withNoConfigurationsAndIdleTelephony_returnsDefault() {
        @AudioContext int suggestedContext = CarVolume.getSuggestedAudioContext(new ArrayList<>(),
                CALL_STATE_IDLE);

        assertThat(suggestedContext).isEqualTo(CarAudioService.DEFAULT_AUDIO_CONTEXT);
    }

    @Test
    public void getSuggestedAudioContext_withOneConfiguration_returnsAssociatedContext() {
        List<AudioPlaybackConfiguration> configurations = ImmutableList.of(
                new Builder().setUsage(USAGE_ALARM).build()
        );

        @AudioContext int suggestedContext = CarVolume.getSuggestedAudioContext(configurations,
                CALL_STATE_IDLE);

        assertThat(suggestedContext).isEqualTo(ALARM);
    }

    @Test
    public void getSuggestedAudioContext_withCallStateOffHook_returnsCallContext() {
        @AudioContext int suggestedContext = CarVolume.getSuggestedAudioContext(new ArrayList<>(),
                CALL_STATE_OFFHOOK);

        assertThat(suggestedContext).isEqualTo(CALL);
    }

    @Test
    public void getSuggestedAudioContext_withCallStateRinging_returnsCallRingContext() {
        @AudioContext int suggestedContext = CarVolume.getSuggestedAudioContext(new ArrayList<>(),
                CALL_STATE_RINGING);

        assertThat(suggestedContext).isEqualTo(CALL_RING);
    }

    @Test
    public void getSuggestedAudioContext_withConfigurations_returnsHighestPriorityContext() {
        List<AudioPlaybackConfiguration> configurations = ImmutableList.of(
                new Builder().setUsage(USAGE_ALARM).build(),
                new Builder().setUsage(USAGE_VOICE_COMMUNICATION).build(),
                new Builder().setUsage(USAGE_NOTIFICATION).build()
        );

        @AudioContext int suggestedContext = CarVolume.getSuggestedAudioContext(configurations,
                CALL_STATE_IDLE);

        assertThat(suggestedContext).isEqualTo(CALL);
    }

    @Test
    public void getSuggestedAudioContext_ignoresInactiveConfigurations() {
        List<AudioPlaybackConfiguration> configurations = ImmutableList.of(
                new Builder().setUsage(USAGE_ALARM).build(),
                new Builder().setUsage(USAGE_VOICE_COMMUNICATION).setInactive().build(),
                new Builder().setUsage(USAGE_NOTIFICATION).build()
        );

        @AudioContext int suggestedContext = CarVolume.getSuggestedAudioContext(configurations,
                CALL_STATE_IDLE);

        assertThat(suggestedContext).isEqualTo(ALARM);
    }

    @Test
    public void getSuggestedAudioContext_withLowerPriorityConfigurationsAndCall_returnsCall() {
        List<AudioPlaybackConfiguration> configurations = ImmutableList.of(
                new Builder().setUsage(USAGE_ALARM).build(),
                new Builder().setUsage(USAGE_NOTIFICATION).build()
        );

        @AudioContext int suggestedContext = CarVolume.getSuggestedAudioContext(configurations,
                CALL_STATE_OFFHOOK);

        assertThat(suggestedContext).isEqualTo(CALL);
    }

    @Test
    public void getSuggestedAudioContext_withNavigationConfigurationAndCall_returnsNavigation() {
        List<AudioPlaybackConfiguration> configurations = ImmutableList.of(
                new Builder().setUsage(USAGE_ASSISTANCE_NAVIGATION_GUIDANCE).build()
        );

        @AudioContext int suggestedContext = CarVolume.getSuggestedAudioContext(configurations,
                CALL_STATE_OFFHOOK);

        assertThat(suggestedContext).isEqualTo(NAVIGATION);
    }

    @Test
    public void getSuggestedAudioContext_withUnprioritizedUsage_returnsDefault() {
        List<AudioPlaybackConfiguration> configurations = ImmutableList.of(
                new Builder().setUsage(USAGE_VIRTUAL_SOURCE).build()
        );

        @AudioContext int suggestedContext = CarVolume.getSuggestedAudioContext(configurations,
                CALL_STATE_IDLE);

        assertThat(suggestedContext).isEqualTo(DEFAULT_AUDIO_CONTEXT);
    }

    private static class Builder {
        private @AttributeUsage int mUsage = USAGE_MEDIA;
        private boolean mIsActive = true;

        Builder setUsage(@AttributeUsage int usage) {
            mUsage = usage;
            return this;
        }

        Builder setInactive() {
            mIsActive = false;
            return this;
        }

        AudioPlaybackConfiguration build() {
            AudioPlaybackConfiguration configuration = mock(AudioPlaybackConfiguration.class);
            AudioAttributes attributes = new AudioAttributes.Builder().setUsage(mUsage).build();
            when(configuration.getAudioAttributes()).thenReturn(attributes);
            when(configuration.isActive()).thenReturn(mIsActive);
            return configuration;
        }
    }
}
