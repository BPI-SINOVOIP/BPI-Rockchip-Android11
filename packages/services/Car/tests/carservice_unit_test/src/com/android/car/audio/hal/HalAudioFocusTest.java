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

import static android.media.AudioAttributes.USAGE_ALARM;
import static android.media.AudioAttributes.USAGE_MEDIA;
import static android.media.AudioManager.AUDIOFOCUS_GAIN;
import static android.media.AudioManager.AUDIOFOCUS_LOSS;
import static android.media.AudioManager.AUDIOFOCUS_LOSS_TRANSIENT;
import static android.media.AudioManager.AUDIOFOCUS_REQUEST_FAILED;
import static android.media.AudioManager.AUDIOFOCUS_REQUEST_GRANTED;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.testng.Assert.assertThrows;

import android.car.media.CarAudioManager;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.media.AudioManager.OnAudioFocusChangeListener;
import android.os.Bundle;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(AndroidJUnit4.class)
public class HalAudioFocusTest {
    private static final int[] AUDIO_ZONE_IDS = {0, 1, 2, 3};
    private static final int ZONE_ID = 0;
    private static final int SECOND_ZONE_ID = 1;
    private static final int INVALID_ZONE_ID = 5;

    @Rule
    public MockitoRule rule = MockitoJUnit.rule();

    @Mock
    AudioManager mMockAudioManager;
    @Mock
    AudioControlWrapper mAudioControlWrapper;

    private HalAudioFocus mHalAudioFocus;

    @Before
    public void setUp() {
        mHalAudioFocus = new HalAudioFocus(mMockAudioManager, mAudioControlWrapper, AUDIO_ZONE_IDS);
    }

    @Test
    public void registerFocusListener_succeeds() {
        mHalAudioFocus.registerFocusListener();

        verify(mAudioControlWrapper).registerFocusListener(mHalAudioFocus);
    }

    @Test
    public void requestAudioFocus_notifiesHalOfFocusChange() {
        whenAnyFocusRequestGranted();

        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);

        verify(mAudioControlWrapper).onAudioFocusChange(USAGE_MEDIA, ZONE_ID,
                AUDIOFOCUS_REQUEST_GRANTED);
    }

    @Test
    public void requestAudioFocus_specifiesUsage() {
        whenAnyFocusRequestGranted();

        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);

        AudioFocusRequest actualRequest = getLastRequest();
        assertThat(actualRequest.getAudioAttributes().getUsage()).isEqualTo(USAGE_MEDIA);
    }

    @Test
    public void requestAudioFocus_specifiesFocusGain() {
        whenAnyFocusRequestGranted();

        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);

        AudioFocusRequest actualRequest = getLastRequest();
        assertThat(actualRequest.getFocusGain()).isEqualTo(AUDIOFOCUS_GAIN);
    }

    @Test
    public void requestAudioFocus_specifiesZoneId() {
        whenAnyFocusRequestGranted();

        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);

        AudioFocusRequest actualRequest = getLastRequest();
        Bundle bundle = actualRequest.getAudioAttributes().getBundle();
        assertThat(bundle.getInt(CarAudioManager.AUDIOFOCUS_EXTRA_REQUEST_ZONE_ID)).isEqualTo(
                ZONE_ID);
    }

    @Test
    public void requestAudioFocus_providesFocusChangeListener() {
        whenAnyFocusRequestGranted();

        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);

        AudioFocusRequest actualRequest = getLastRequest();
        assertThat(actualRequest.getOnAudioFocusChangeListener()).isNotNull();
    }

    @Test
    public void requestAudioFocus_withInvalidZone_throws() {
        whenAnyFocusRequestGranted();

        assertThrows(IllegalArgumentException.class,
                () -> mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, INVALID_ZONE_ID,
                        AUDIOFOCUS_GAIN));
    }

    @Test
    public void requestAudioFocus_withSameZoneAndUsage_keepsExistingRequest() {
        whenAnyFocusRequestGranted();

        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);
        AudioFocusRequest firstRequest = getLastRequest();
        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);

        verify(mMockAudioManager, never()).abandonAudioFocusRequest(firstRequest);
    }

    @Test
    public void requestAudioFocus_withSameZoneAndUsage_notifiesHalOfExistingRequestStatus() {
        whenAnyFocusRequestGranted();

        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);
        AudioFocusRequest firstRequest = getLastRequest();
        OnAudioFocusChangeListener listener = firstRequest.getOnAudioFocusChangeListener();
        listener.onAudioFocusChange(AUDIOFOCUS_LOSS_TRANSIENT);

        verify(mAudioControlWrapper).onAudioFocusChange(USAGE_MEDIA, ZONE_ID,
                AUDIOFOCUS_LOSS_TRANSIENT);

        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);

        verify(mAudioControlWrapper, times(2)).onAudioFocusChange(USAGE_MEDIA, ZONE_ID,
                AUDIOFOCUS_LOSS_TRANSIENT);
    }

    @Test
    public void requestAudioFocus_withDifferentZoneAndSameUsage_keepsExistingRequest() {
        whenAnyFocusRequestGranted();

        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);
        AudioFocusRequest firstRequest = getLastRequest();
        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, SECOND_ZONE_ID, AUDIOFOCUS_GAIN);

        verify(mMockAudioManager, never()).abandonAudioFocusRequest(firstRequest);
    }

    @Test
    public void requestAudioFocus_withSameZoneAndDifferentUsage_keepsExistingRequest() {
        whenAnyFocusRequestGranted();

        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);
        AudioFocusRequest firstRequest = getLastRequest();
        mHalAudioFocus.requestAudioFocus(USAGE_ALARM, ZONE_ID, AUDIOFOCUS_GAIN);

        verify(mMockAudioManager, never()).abandonAudioFocusRequest(firstRequest);
    }

    @Test
    public void requestAudioFocus_withPreviouslyFailedRequest_doesNothingForOldRequest() {
        when(mMockAudioManager.requestAudioFocus(any())).thenReturn(AUDIOFOCUS_REQUEST_FAILED,
                AUDIOFOCUS_REQUEST_GRANTED);

        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);
        AudioFocusRequest firstRequest = getLastRequest();
        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);

        verify(mMockAudioManager, never()).abandonAudioFocusRequest(firstRequest);
    }

    @Test
    public void onAudioFocusChange_notifiesHalOfChange() {
        whenAnyFocusRequestGranted();

        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);
        verify(mAudioControlWrapper, never()).onAudioFocusChange(USAGE_MEDIA, ZONE_ID,
                AUDIOFOCUS_LOSS_TRANSIENT);

        AudioFocusRequest actualRequest = getLastRequest();
        OnAudioFocusChangeListener listener = actualRequest.getOnAudioFocusChangeListener();
        listener.onAudioFocusChange(AUDIOFOCUS_LOSS_TRANSIENT);

        verify(mAudioControlWrapper).onAudioFocusChange(USAGE_MEDIA, ZONE_ID,
                AUDIOFOCUS_LOSS_TRANSIENT);
    }

    @Test
    public void abandonAudioFocus_withNoCurrentRequest_doesNothing() throws Exception {
        whenAnyFocusRequestGranted();

        mHalAudioFocus.abandonAudioFocus(USAGE_MEDIA, ZONE_ID);

        verify(mMockAudioManager, never()).abandonAudioFocusRequest(any());
    }

    @Test
    public void abandonAudioFocus_withInvalidZone_throws() {
        whenAnyFocusRequestGranted();

        assertThrows(IllegalArgumentException.class,
                () -> mHalAudioFocus.abandonAudioFocus(USAGE_MEDIA, INVALID_ZONE_ID));
    }

    @Test
    public void abandonAudioFocus_withCurrentRequest_abandonsExistingFocus() throws Exception {
        whenAnyFocusRequestGranted();
        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);
        AudioFocusRequest actualRequest = getLastRequest();

        mHalAudioFocus.abandonAudioFocus(USAGE_MEDIA, ZONE_ID);

        verify(mMockAudioManager).abandonAudioFocusRequest(actualRequest);
    }

    @Test
    public void abandonAudioFocus_withCurrentRequest_notifiesHalOfFocusChange() throws Exception {
        whenAnyFocusRequestGranted();
        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);
        AudioFocusRequest actualRequest = getLastRequest();
        when(mMockAudioManager.abandonAudioFocusRequest(actualRequest)).thenReturn(
                AUDIOFOCUS_REQUEST_GRANTED);

        mHalAudioFocus.abandonAudioFocus(USAGE_MEDIA, ZONE_ID);

        verify(mAudioControlWrapper).onAudioFocusChange(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_LOSS);
    }

    @Test
    public void abandonAudioFocus_withFocusAlreadyLost_doesNothing() throws Exception {
        whenAnyFocusRequestGranted();
        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);
        AudioFocusRequest actualRequest = getLastRequest();
        OnAudioFocusChangeListener listener = actualRequest.getOnAudioFocusChangeListener();
        listener.onAudioFocusChange(AUDIOFOCUS_LOSS);

        mHalAudioFocus.abandonAudioFocus(USAGE_MEDIA, ZONE_ID);

        verify(mMockAudioManager, never()).abandonAudioFocusRequest(actualRequest);
    }

    @Test
    public void abandonAudioFocus_withFocusTransientlyLost_abandonsExistingFocus()
            throws Exception {
        whenAnyFocusRequestGranted();
        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);
        AudioFocusRequest actualRequest = getLastRequest();
        OnAudioFocusChangeListener listener = actualRequest.getOnAudioFocusChangeListener();
        listener.onAudioFocusChange(AUDIOFOCUS_LOSS_TRANSIENT);

        mHalAudioFocus.abandonAudioFocus(USAGE_MEDIA, ZONE_ID);

        verify(mMockAudioManager).abandonAudioFocusRequest(actualRequest);
    }

    @Test
    public void abandonAudioFocus_withExistingRequestOfDifferentUsage_doesNothing()
            throws Exception {
        whenAnyFocusRequestGranted();
        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);

        mHalAudioFocus.abandonAudioFocus(USAGE_ALARM, ZONE_ID);

        verify(mMockAudioManager, never()).abandonAudioFocusRequest(any());
    }

    @Test
    public void abandonAudioFocus_withExistingRequestOfDifferentZoneId_doesNothing()
            throws Exception {
        whenAnyFocusRequestGranted();
        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);

        mHalAudioFocus.abandonAudioFocus(USAGE_MEDIA, SECOND_ZONE_ID);

        verify(mMockAudioManager, never()).abandonAudioFocusRequest(any());
    }

    @Test
    public void abandonAudioFocus_withFailedRequest_doesNotNotifyHal() throws Exception {
        whenAnyFocusRequestGranted();
        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);
        AudioFocusRequest request = getLastRequest();

        when(mMockAudioManager.abandonAudioFocusRequest(request))
                .thenReturn(AUDIOFOCUS_REQUEST_FAILED);

        mHalAudioFocus.abandonAudioFocus(USAGE_MEDIA, ZONE_ID);

        verify(mAudioControlWrapper, never())
                .onAudioFocusChange(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_LOSS);
    }

    @Test
    public void reset_abandonsExistingRequests() {
        whenAnyFocusRequestGranted();
        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);
        AudioFocusRequest mediaRequest = getLastRequest();
        mHalAudioFocus.requestAudioFocus(USAGE_ALARM, ZONE_ID, AUDIOFOCUS_GAIN);
        AudioFocusRequest alarmRequest = getLastRequest();

        verify(mMockAudioManager, never()).abandonAudioFocusRequest(any());

        mHalAudioFocus.reset();

        verify(mMockAudioManager).abandonAudioFocusRequest(mediaRequest);
        verify(mMockAudioManager).abandonAudioFocusRequest(alarmRequest);
        verifyNoMoreInteractions(mMockAudioManager);
    }

    @Test
    public void reset_notifiesHal() {
        whenAnyFocusRequestGranted();
        mHalAudioFocus.requestAudioFocus(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_GAIN);
        mHalAudioFocus.requestAudioFocus(USAGE_ALARM, ZONE_ID, AUDIOFOCUS_GAIN);

        verify(mAudioControlWrapper, never()).onAudioFocusChange(anyInt(), eq(ZONE_ID),
                eq(AUDIOFOCUS_LOSS));
        when(mMockAudioManager.abandonAudioFocusRequest(any())).thenReturn(
                AUDIOFOCUS_REQUEST_GRANTED);

        mHalAudioFocus.reset();

        verify(mAudioControlWrapper).onAudioFocusChange(USAGE_MEDIA, ZONE_ID, AUDIOFOCUS_LOSS);
        verify(mAudioControlWrapper).onAudioFocusChange(USAGE_ALARM, ZONE_ID, AUDIOFOCUS_LOSS);
    }

    private void whenAnyFocusRequestGranted() {
        when(mMockAudioManager.requestAudioFocus(any())).thenReturn(AUDIOFOCUS_REQUEST_GRANTED);
    }

    private AudioFocusRequest getLastRequest() {
        ArgumentCaptor<AudioFocusRequest> captor = ArgumentCaptor.forClass(AudioFocusRequest.class);
        verify(mMockAudioManager, atLeastOnce()).requestAudioFocus(captor.capture());
        return captor.getValue();
    }
}
