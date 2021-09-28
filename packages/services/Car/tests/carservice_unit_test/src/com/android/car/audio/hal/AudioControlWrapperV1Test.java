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

package com.android.car.audio.hal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.testng.Assert.assertThrows;

import android.hardware.automotive.audiocontrol.V1_0.IAudioControl;
import android.hardware.automotive.audiocontrol.V2_0.IFocusListener;
import android.media.AudioAttributes;
import android.media.AudioManager;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(AndroidJUnit4.class)
public class AudioControlWrapperV1Test {
    private static final float FADE_VALUE = 5;
    private static final float BALANCE_VALUE = 6;
    private static final int CONTEXT_NUMBER = 3;
    private static final int USAGE = AudioAttributes.USAGE_MEDIA;
    private static final int ZONE_ID = 2;
    private static final int FOCUS_GAIN = AudioManager.AUDIOFOCUS_GAIN;

    @Rule
    public MockitoRule rule = MockitoJUnit.rule();

    @Mock
    IAudioControl mAudioControlV1;

    @Test
    public void setFadeTowardFront_succeeds() throws Exception {
        AudioControlWrapperV1 audioControlWrapperV1 = new AudioControlWrapperV1(mAudioControlV1);
        audioControlWrapperV1.setFadeTowardFront(FADE_VALUE);

        verify(mAudioControlV1).setFadeTowardFront(FADE_VALUE);
    }

    @Test
    public void setBalanceTowardRight_succeeds() throws Exception {
        AudioControlWrapperV1 audioControlWrapperV1 = new AudioControlWrapperV1(mAudioControlV1);
        audioControlWrapperV1.setBalanceTowardRight(BALANCE_VALUE);

        verify(mAudioControlV1).setBalanceTowardRight(BALANCE_VALUE);
    }

    @Test
    public void getBusForContext_returnsBusNumber() throws Exception {
        AudioControlWrapperV1 audioControlWrapperV1 = new AudioControlWrapperV1(mAudioControlV1);
        int busNumber = 1;
        when(mAudioControlV1.getBusForContext(CONTEXT_NUMBER)).thenReturn(busNumber);

        int actualBus = audioControlWrapperV1.getBusForContext(CONTEXT_NUMBER);
        assertThat(actualBus).isEqualTo(busNumber);
    }

    @Test
    public void supportsHalAudioFocus_returnsFalse() {
        AudioControlWrapperV1 audioControlWrapperV1 = new AudioControlWrapperV1(mAudioControlV1);

        assertThat(audioControlWrapperV1.supportsHalAudioFocus()).isFalse();
    }

    @Test
    public void registerFocusListener_throws() {
        AudioControlWrapperV1 audioControlWrapperV1 = new AudioControlWrapperV1(mAudioControlV1);
        IFocusListener mockListener = mock(IFocusListener.class);

        assertThrows(UnsupportedOperationException.class,
                () -> audioControlWrapperV1.registerFocusListener(mockListener));
    }

    @Test
    public void unregisterFocusListener_throws() {
        AudioControlWrapperV1 audioControlWrapperV1 = new AudioControlWrapperV1(mAudioControlV1);
        IFocusListener mockListener = mock(IFocusListener.class);

        assertThrows(UnsupportedOperationException.class,
                () -> audioControlWrapperV1.unregisterFocusListener());
    }

    @Test
    public void onAudioFocusChange_throws() {
        AudioControlWrapperV1 audioControlWrapperV1 = new AudioControlWrapperV1(mAudioControlV1);

        assertThrows(UnsupportedOperationException.class,
                () -> audioControlWrapperV1.onAudioFocusChange(USAGE, ZONE_ID, FOCUS_GAIN));
    }
}
