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

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;

import android.car.Car;
import android.car.media.CarAudioManager;
import android.content.pm.PackageManager;
import android.media.AudioAttributes;
import android.media.AudioFocusInfo;
import android.media.AudioManager;
import android.os.Bundle;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(AndroidJUnit4.class)
public class FocusEntryTest {

    private static final int CLIENT_UID = 0;
    private static final String CLIENT_ID = "0";
    private static final String PACKAGE_NAME = "PACKAGE_NAME";
    private static final int LOSS_RECEIVED = 0;
    private static final int DEFAULT_FLAGS = 0;
    private static final int SDK = 0;

    @Rule
    public MockitoRule rule = MockitoJUnit.rule();

    @Mock
    private PackageManager mMockPM;

    @Test
    public void wantsPauseInsteadOfDucking_whenFlagIsSet_returnsTrue() {
        AudioFocusInfo info = getInfoWithFlags(
                AudioManager.AUDIOFOCUS_FLAG_PAUSES_ON_DUCKABLE_LOSS);

        FocusEntry focusEntry = new FocusEntry(info, CarAudioContext.MUSIC, mMockPM);

        assertThat(focusEntry.wantsPauseInsteadOfDucking()).isTrue();
    }

    @Test
    public void wantsPauseInsteadOfDucking_whenFlagIsNotSet_returnsFalse() {
        AudioFocusInfo info = getInfoWithFlags(0);

        FocusEntry focusEntry = new FocusEntry(info, CarAudioContext.MUSIC, mMockPM);

        assertThat(focusEntry.wantsPauseInsteadOfDucking()).isFalse();
    }

    @Test
    public void receivesDuckEvents_whenBundleDoesNotReceiveDuckingEvents_returnsFalse() {
        AudioFocusInfo info = getInfoThatReceivesDuckingEvents(false);
        FocusEntry focusEntry = new FocusEntry(info, CarAudioContext.MUSIC, mMockPM);

        assertThat(focusEntry.receivesDuckEvents()).isFalse();
    }

    @Test
    public void receivesDuckEvents_withoutReceiveCarAudioDuckingEventsPermission_returnsFalse() {
        withoutPermission();
        AudioFocusInfo info = getInfoThatReceivesDuckingEvents(true);

        FocusEntry focusEntry = new FocusEntry(info, CarAudioContext.MUSIC, mMockPM);

        assertThat(focusEntry.receivesDuckEvents()).isFalse();
    }

    @Test
    public void receivesDuckEvents_withReceiveCarAudioDuckingEventsPermission_returnsTrue() {
        withPermission();
        AudioFocusInfo info = getInfoThatReceivesDuckingEvents(true);

        FocusEntry focusEntry = new FocusEntry(info, CarAudioContext.MUSIC, mMockPM);

        assertThat(focusEntry.receivesDuckEvents()).isTrue();
    }

    private void withPermission() {
        when(mMockPM.checkPermission(Car.PERMISSION_RECEIVE_CAR_AUDIO_DUCKING_EVENTS, PACKAGE_NAME))
                .thenReturn(PackageManager.PERMISSION_GRANTED);
    }

    private void withoutPermission() {
        when(mMockPM.checkPermission(Car.PERMISSION_RECEIVE_CAR_AUDIO_DUCKING_EVENTS, PACKAGE_NAME))
                .thenReturn(PackageManager.PERMISSION_DENIED);
    }

    private AudioFocusInfo getInfoWithFlags(int flags) {
        AudioAttributes attributes = new AudioAttributes.Builder().build();
        return getInfo(attributes, flags);
    }

    private AudioFocusInfo getInfoThatReceivesDuckingEvents(boolean receivesDuckingEvents) {
        Bundle bundle = new Bundle();
        bundle.putBoolean(CarAudioManager.AUDIOFOCUS_EXTRA_RECEIVE_DUCKING_EVENTS,
                receivesDuckingEvents);
        AudioAttributes attributes = new AudioAttributes.Builder()
                .addBundle(bundle)
                .build();
        return getInfo(attributes, DEFAULT_FLAGS);
    }

    private AudioFocusInfo getInfo(AudioAttributes attributes, int flags) {
        return new AudioFocusInfo(attributes, CLIENT_UID, CLIENT_ID, PACKAGE_NAME,
                AudioManager.AUDIOFOCUS_GAIN, LOSS_RECEIVED, flags, SDK);
    }
}
