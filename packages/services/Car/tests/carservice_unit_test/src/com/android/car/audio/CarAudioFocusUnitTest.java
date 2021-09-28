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

import static android.media.AudioAttributes.USAGE_ANNOUNCEMENT;
import static android.media.AudioAttributes.USAGE_ASSISTANCE_NAVIGATION_GUIDANCE;
import static android.media.AudioAttributes.USAGE_ASSISTANT;
import static android.media.AudioAttributes.USAGE_EMERGENCY;
import static android.media.AudioAttributes.USAGE_MEDIA;
import static android.media.AudioAttributes.USAGE_NOTIFICATION_RINGTONE;
import static android.media.AudioAttributes.USAGE_VEHICLE_STATUS;
import static android.media.AudioAttributes.USAGE_VOICE_COMMUNICATION;
import static android.media.AudioManager.AUDIOFOCUS_GAIN;
import static android.media.AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK;
import static android.media.AudioManager.AUDIOFOCUS_LOSS;
import static android.media.AudioManager.AUDIOFOCUS_LOSS_TRANSIENT;
import static android.media.AudioManager.AUDIOFOCUS_REQUEST_DELAYED;
import static android.media.AudioManager.AUDIOFOCUS_REQUEST_FAILED;
import static android.media.AudioManager.AUDIOFOCUS_REQUEST_GRANTED;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.pm.PackageManager;
import android.media.AudioAttributes;
import android.media.AudioAttributes.AttributeUsage;
import android.media.AudioFocusInfo;
import android.media.AudioManager;
import android.media.audiopolicy.AudioPolicy;
import android.os.Build;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(AndroidJUnit4.class)
public class CarAudioFocusUnitTest {
    private static final int CLIENT_UID = 1;
    private static final String FIRST_CLIENT_ID = "first-client-id";
    private static final String SECOND_CLIENT_ID = "second-client-id";
    private static final String THIRD_CLIENT_ID = "third-client-id";
    private static final String PACKAGE_NAME = "com.android.car.audio";
    private static final int AUDIOFOCUS_FLAG = 0;

    @Rule
    public MockitoRule rule = MockitoJUnit.rule();
    @Mock
    private AudioManager mMockAudioManager;
    @Mock
    private PackageManager mMockPackageManager;
    @Mock
    private AudioPolicy mAudioPolicy;
    @Mock
    private CarAudioSettings mCarAudioSettings;

    private FocusInteraction mFocusInteraction;


    @Before
    public void setUp() {
        mFocusInteraction = new FocusInteraction(mCarAudioSettings);
    }

    @Test
    public void onAudioFocusRequest_withNoCurrentFocusHolder_requestGranted() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(false);
        AudioFocusInfo audioFocusInfo = getInfoForFirstClientWithMedia();
        carAudioFocus.onAudioFocusRequest(audioFocusInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager).setFocusRequestResult(audioFocusInfo,
                AUDIOFOCUS_REQUEST_GRANTED, mAudioPolicy);
    }

    @Test
    public void onAudioFocusRequest_withSameClientIdSameUsage_requestGranted() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(false);
        AudioFocusInfo audioFocusInfo = getInfoForFirstClientWithMedia();
        carAudioFocus.onAudioFocusRequest(audioFocusInfo, AUDIOFOCUS_REQUEST_GRANTED);

        AudioFocusInfo sameClientAndUsageFocusInfo = getInfoForFirstClientWithMedia();
        carAudioFocus.onAudioFocusRequest(sameClientAndUsageFocusInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager, times(2)).setFocusRequestResult(sameClientAndUsageFocusInfo,
                AUDIOFOCUS_REQUEST_GRANTED, mAudioPolicy);
    }

    @Test
    public void onAudioFocusRequest_withSameClientIdDifferentUsage_requestFailed() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(false);
        requestFocusForMediaWithFirstClient(carAudioFocus);

        AudioFocusInfo sameClientFocusInfo = getInfo(USAGE_ASSISTANCE_NAVIGATION_GUIDANCE,
                FIRST_CLIENT_ID, AUDIOFOCUS_GAIN, false);
        carAudioFocus.onAudioFocusRequest(sameClientFocusInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager).setFocusRequestResult(sameClientFocusInfo,
                AUDIOFOCUS_REQUEST_FAILED, mAudioPolicy);
    }

    @Test
    public void onAudioFocusRequest_concurrentRequest_requestGranted() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(false);
        requestFocusForMediaWithFirstClient(carAudioFocus);

        AudioFocusInfo concurrentFocusInfo = getConcurrentInfo(AUDIOFOCUS_GAIN);
        carAudioFocus.onAudioFocusRequest(concurrentFocusInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager).setFocusRequestResult(concurrentFocusInfo,
                AUDIOFOCUS_REQUEST_GRANTED, mAudioPolicy);
    }

    @Test
    public void onAudioFocusRequest_concurrentRequestWithoutDucking_holderLosesFocus() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(false);
        AudioFocusInfo initialFocusInfo = requestFocusForMediaWithFirstClient(carAudioFocus);

        AudioFocusInfo concurrentFocusInfo = getConcurrentInfo(AUDIOFOCUS_GAIN);
        carAudioFocus.onAudioFocusRequest(concurrentFocusInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager).dispatchAudioFocusChange(initialFocusInfo,
                AudioManager.AUDIOFOCUS_LOSS, mAudioPolicy);
    }

    @Test
    public void onAudioFocusRequest_concurrentRequestMayDuck_holderRetainsFocus() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(false);
        AudioFocusInfo initialFocusInfo = requestFocusForMediaWithFirstClient(carAudioFocus);

        AudioFocusInfo concurrentFocusInfo = getConcurrentInfo(AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK);
        carAudioFocus.onAudioFocusRequest(concurrentFocusInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager, times(0)).dispatchAudioFocusChange(eq(initialFocusInfo),
                anyInt(), eq(mAudioPolicy));
    }

    @Test
    public void onAudioFocusRequest_exclusiveRequest_requestGranted() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(false);
        requestFocusForMediaWithFirstClient(carAudioFocus);

        AudioFocusInfo exclusiveRequestInfo = getExclusiveInfo(AUDIOFOCUS_GAIN);
        carAudioFocus.onAudioFocusRequest(exclusiveRequestInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager).setFocusRequestResult(exclusiveRequestInfo,
                AUDIOFOCUS_REQUEST_GRANTED, mAudioPolicy);
    }

    @Test
    public void onAudioFocusRequest_exclusiveRequest_holderLosesFocus() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(false);
        AudioFocusInfo initialFocusInfo = requestFocusForMediaWithFirstClient(carAudioFocus);

        AudioFocusInfo exclusiveRequestInfo = getExclusiveInfo(AUDIOFOCUS_GAIN);
        carAudioFocus.onAudioFocusRequest(exclusiveRequestInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager).dispatchAudioFocusChange(initialFocusInfo,
                AudioManager.AUDIOFOCUS_LOSS, mAudioPolicy);
    }

    @Test
    public void onAudioFocusRequest_exclusiveRequestMayDuck_holderLosesFocusTransiently() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(false);
        AudioFocusInfo initialFocusInfo = requestFocusForMediaWithFirstClient(carAudioFocus);

        AudioFocusInfo exclusiveRequestInfo = getExclusiveInfo(AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK);
        carAudioFocus.onAudioFocusRequest(exclusiveRequestInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager).dispatchAudioFocusChange(initialFocusInfo,
                AudioManager.AUDIOFOCUS_LOSS_TRANSIENT, mAudioPolicy);
    }

    @Test
    public void onAudioFocus_rejectRequest_requestFailed() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(false);
        requestFocusForUsageWithFirstClient(USAGE_ASSISTANT, carAudioFocus);

        AudioFocusInfo rejectRequestInfo = getRejectInfo();
        carAudioFocus.onAudioFocusRequest(rejectRequestInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager).setFocusRequestResult(rejectRequestInfo,
                AUDIOFOCUS_REQUEST_FAILED, mAudioPolicy);
    }

    @Test
    public void onAudioFocus_rejectRequest_holderRetainsFocus() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(false);
        AudioFocusInfo initialFocusInfo = requestFocusForUsageWithFirstClient(USAGE_ASSISTANT,
                carAudioFocus);

        AudioFocusInfo rejectRequestInfo = getRejectInfo();
        carAudioFocus.onAudioFocusRequest(rejectRequestInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager, times(0)).dispatchAudioFocusChange(eq(initialFocusInfo),
                anyInt(), eq(mAudioPolicy));
    }

    // System Usage tests

    @Test
    public void onAudioFocus_exclusiveWithSystemUsage_requestGranted() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(false);
        requestFocusForMediaWithFirstClient(carAudioFocus);

        AudioFocusInfo exclusiveSystemUsageInfo = getExclusiveWithSystemUsageInfo();
        carAudioFocus.onAudioFocusRequest(exclusiveSystemUsageInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager).setFocusRequestResult(exclusiveSystemUsageInfo,
                AUDIOFOCUS_REQUEST_GRANTED, mAudioPolicy);
    }

    @Test
    public void onAudioFocus_exclusiveWithSystemUsage_holderLosesFocus() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(false);
        AudioFocusInfo initialFocusInfo = requestFocusForMediaWithFirstClient(carAudioFocus);

        AudioFocusInfo exclusiveSystemUsageInfo = getExclusiveWithSystemUsageInfo();
        carAudioFocus.onAudioFocusRequest(exclusiveSystemUsageInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager).dispatchAudioFocusChange(initialFocusInfo,
                AUDIOFOCUS_LOSS, mAudioPolicy);
    }

    @Test
    public void onAudioFocus_concurrentWithSystemUsage_requestGranted() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(false);
        requestFocusForMediaWithFirstClient(carAudioFocus);

        AudioFocusInfo concurrentSystemUsageInfo = getConcurrentWithSystemUsageInfo();
        carAudioFocus.onAudioFocusRequest(concurrentSystemUsageInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager).setFocusRequestResult(concurrentSystemUsageInfo,
                AUDIOFOCUS_REQUEST_GRANTED, mAudioPolicy);
    }

    @Test
    public void onAudioFocus_concurrentWithSystemUsageAndConcurrent_holderRetainsFocus() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(false);
        AudioFocusInfo initialFocusInfo = requestFocusForMediaWithFirstClient(carAudioFocus);

        AudioFocusInfo concurrentSystemUsageInfo = getConcurrentWithSystemUsageInfo();
        carAudioFocus.onAudioFocusRequest(concurrentSystemUsageInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager, times(0)).dispatchAudioFocusChange(eq(initialFocusInfo),
                anyInt(), eq(mAudioPolicy));
    }

    @Test
    public void onAudioFocus_rejectWithSystemUsage_requestFailed() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(false);
        requestFocusForUsageWithFirstClient(USAGE_VOICE_COMMUNICATION, carAudioFocus);

        AudioFocusInfo rejectWithSystemUsageInfo = getRejectWithSystemUsageInfo();
        carAudioFocus.onAudioFocusRequest(rejectWithSystemUsageInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager).setFocusRequestResult(rejectWithSystemUsageInfo,
                AUDIOFOCUS_REQUEST_FAILED, mAudioPolicy);
    }

    // Delayed Focus tests
    @Test
    public void onAudioFocus_requestWithDelayedFocus_requestGranted() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(true);

        AudioFocusInfo delayedFocusInfo = getDelayedExclusiveInfo(AUDIOFOCUS_GAIN);
        carAudioFocus.onAudioFocusRequest(delayedFocusInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager)
                .setFocusRequestResult(delayedFocusInfo,
                        AUDIOFOCUS_REQUEST_GRANTED, mAudioPolicy);
    }

    @Test
    public void onAudioFocus_delayedRequestAbandonedBeforeGettingFocus_abandonSucceeds() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(true);

        AudioFocusInfo callFocusInfo = setupFocusInfoAndRequestFocusForCall(carAudioFocus);

        AudioFocusInfo delayedFocusInfo = getDelayedExclusiveInfo(AUDIOFOCUS_GAIN);
        carAudioFocus.onAudioFocusRequest(delayedFocusInfo, AUDIOFOCUS_REQUEST_GRANTED);

        carAudioFocus.onAudioFocusAbandon(delayedFocusInfo);

        verify(mMockAudioManager, never()).dispatchAudioFocusChange(
                delayedFocusInfo, AUDIOFOCUS_LOSS, mAudioPolicy);

        carAudioFocus.onAudioFocusAbandon(callFocusInfo);

        verify(mMockAudioManager, never()).dispatchAudioFocusChange(
                delayedFocusInfo, AUDIOFOCUS_GAIN, mAudioPolicy);
    }

    @Test
    public void onAudioFocus_forRequestDelayed_requestDelayed() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(true);

        setupFocusInfoAndRequestFocusForCall(carAudioFocus);

        AudioFocusInfo delayedFocusInfo = getDelayedExclusiveInfo(AUDIOFOCUS_GAIN);
        carAudioFocus.onAudioFocusRequest(delayedFocusInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager)
                .setFocusRequestResult(delayedFocusInfo,
                        AUDIOFOCUS_REQUEST_DELAYED, mAudioPolicy);
    }

    @Test
    public void onAudioFocus_forRequestDelayed_delayedFocusGained() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(true);

        AudioFocusInfo callFocusInfo = setupFocusInfoAndRequestFocusForCall(carAudioFocus);

        AudioFocusInfo delayedFocusInfo = getDelayedExclusiveInfo(AUDIOFOCUS_GAIN);
        carAudioFocus.onAudioFocusRequest(delayedFocusInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager)
                .setFocusRequestResult(delayedFocusInfo,
                        AUDIOFOCUS_REQUEST_DELAYED, mAudioPolicy);

        carAudioFocus.onAudioFocusAbandon(callFocusInfo);

        verify(mMockAudioManager)
                .dispatchAudioFocusChange(delayedFocusInfo, AUDIOFOCUS_GAIN, mAudioPolicy);
    }

    @Test
    public void onAudioFocus_multipleRequestWithDelayedFocus_requestsDelayed() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(true);

        AudioFocusInfo callFocusInfo = setupFocusInfoAndRequestFocusForCall(carAudioFocus);

        AudioFocusInfo firstRequestWithDelayedFocus = getInfo(USAGE_MEDIA, SECOND_CLIENT_ID,
                AUDIOFOCUS_GAIN, true);
        carAudioFocus.onAudioFocusRequest(firstRequestWithDelayedFocus, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager)
                .setFocusRequestResult(firstRequestWithDelayedFocus,
                        AUDIOFOCUS_REQUEST_DELAYED, mAudioPolicy);

        AudioFocusInfo secondRequestWithDelayedFocus = getInfo(USAGE_MEDIA, THIRD_CLIENT_ID,
                AUDIOFOCUS_GAIN, true);
        carAudioFocus.onAudioFocusRequest(secondRequestWithDelayedFocus,
                AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager)
                .setFocusRequestResult(secondRequestWithDelayedFocus,
                        AUDIOFOCUS_REQUEST_DELAYED, mAudioPolicy);
    }

    @Test
    public void onAudioFocus_multipleRequestWithDelayedFocus_firstRequestReceivesFocusLoss() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(true);

        AudioFocusInfo callFocusInfo = setupFocusInfoAndRequestFocusForCall(carAudioFocus);

        AudioFocusInfo firstRequestWithDelayedFocus = getInfo(USAGE_MEDIA, SECOND_CLIENT_ID,
                AUDIOFOCUS_GAIN, true);
        carAudioFocus.onAudioFocusRequest(firstRequestWithDelayedFocus, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager)
                .setFocusRequestResult(firstRequestWithDelayedFocus,
                        AUDIOFOCUS_REQUEST_DELAYED, mAudioPolicy);

        AudioFocusInfo secondRequestWithDelayedFocus = getInfo(USAGE_MEDIA, THIRD_CLIENT_ID,
                AUDIOFOCUS_GAIN, true);

        carAudioFocus.onAudioFocusRequest(secondRequestWithDelayedFocus,
                AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager).dispatchAudioFocusChange(
                firstRequestWithDelayedFocus, AUDIOFOCUS_LOSS, mAudioPolicy);
    }

    @Test
    public void onAudioFocus_multipleRequestOnlyOneWithDelayedFocus_delayedFocusNotChanged() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(true);

        AudioFocusInfo callFocusInfo = setupFocusInfoAndRequestFocusForCall(carAudioFocus);

        AudioFocusInfo firstRequestWithDelayedFocus = getInfo(USAGE_MEDIA, SECOND_CLIENT_ID,
                AUDIOFOCUS_GAIN, true);
        carAudioFocus.onAudioFocusRequest(firstRequestWithDelayedFocus, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager)
                .setFocusRequestResult(firstRequestWithDelayedFocus,
                        AUDIOFOCUS_REQUEST_DELAYED, mAudioPolicy);

        AudioFocusInfo secondRequestWithNoDelayedFocus = getInfo(USAGE_MEDIA, THIRD_CLIENT_ID,
                AUDIOFOCUS_GAIN, false);

        carAudioFocus.onAudioFocusRequest(secondRequestWithNoDelayedFocus,
                AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager)
                .setFocusRequestResult(secondRequestWithNoDelayedFocus,
                        AUDIOFOCUS_REQUEST_FAILED, mAudioPolicy);

        verify(mMockAudioManager, never()).dispatchAudioFocusChange(
                firstRequestWithDelayedFocus, AUDIOFOCUS_LOSS, mAudioPolicy);
    }

    @Test
    public void
            onAudioFocus_multipleRequestOnlyOneWithDelayedFocus_nonTransientRequestReceivesLoss() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(true);

        AudioFocusInfo mediaRequestWithOutDelayedFocus = getInfo(USAGE_MEDIA, SECOND_CLIENT_ID,
                AUDIOFOCUS_GAIN, false);
        carAudioFocus.onAudioFocusRequest(mediaRequestWithOutDelayedFocus,
                AUDIOFOCUS_REQUEST_GRANTED);

        AudioFocusInfo callFocusInfo = setupFocusInfoAndRequestFocusForCall(carAudioFocus);

        AudioFocusInfo mediaRequestWithDelayedFocus = getInfo(USAGE_MEDIA, THIRD_CLIENT_ID,
                AUDIOFOCUS_GAIN, true);

        carAudioFocus.onAudioFocusRequest(mediaRequestWithDelayedFocus,
                AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager)
                .setFocusRequestResult(mediaRequestWithDelayedFocus,
                        AUDIOFOCUS_REQUEST_DELAYED, mAudioPolicy);

        verify(mMockAudioManager).dispatchAudioFocusChange(
                mediaRequestWithOutDelayedFocus, AUDIOFOCUS_LOSS, mAudioPolicy);
    }

    @Test
    public void
            onAudioFocus_multipleRequestOnlyOneWithDelayedFocus_duckedRequestReceivesLoss() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(true);

        AudioFocusInfo navRequestWithOutDelayedFocus =
                getInfo(USAGE_ASSISTANCE_NAVIGATION_GUIDANCE, SECOND_CLIENT_ID,
                        AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK, false);
        carAudioFocus.onAudioFocusRequest(navRequestWithOutDelayedFocus,
                AUDIOFOCUS_REQUEST_GRANTED);

        AudioFocusInfo callFocusInfo = setupFocusInfoAndRequestFocusForCall(carAudioFocus);

        verify(mMockAudioManager).dispatchAudioFocusChange(
                navRequestWithOutDelayedFocus, AUDIOFOCUS_LOSS_TRANSIENT, mAudioPolicy);

        AudioFocusInfo mediaRequestWithDelayedFocus = getInfo(USAGE_MEDIA, THIRD_CLIENT_ID,
                AUDIOFOCUS_GAIN, true);

        carAudioFocus.onAudioFocusRequest(mediaRequestWithDelayedFocus,
                AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager).dispatchAudioFocusChange(
                navRequestWithOutDelayedFocus, AUDIOFOCUS_LOSS, mAudioPolicy);
    }

    @Test
    public void
            onAudioFocus_concurrentRequestAfterDelayedFocus_concurrentFocusGranted() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(true);

        AudioFocusInfo callFocusInfo = setupFocusInfoAndRequestFocusForCall(carAudioFocus);

        AudioFocusInfo delayedFocusRequest = getInfo(USAGE_MEDIA, SECOND_CLIENT_ID,
                AUDIOFOCUS_GAIN, true);

        carAudioFocus.onAudioFocusRequest(delayedFocusRequest,
                AUDIOFOCUS_REQUEST_GRANTED);

        AudioFocusInfo mapFocusInfo = getInfo(USAGE_ASSISTANCE_NAVIGATION_GUIDANCE, THIRD_CLIENT_ID,
                AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK, false);
        carAudioFocus.onAudioFocusRequest(mapFocusInfo, AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager)
                .setFocusRequestResult(mapFocusInfo,
                        AUDIOFOCUS_REQUEST_GRANTED, mAudioPolicy);
    }

    @Test
    public void
            onAudioFocus_concurrentRequestsAndAbandonsAfterDelayedFocus_noDelayedFocusChange() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(true);

        AudioFocusInfo callFocusInfo = setupFocusInfoAndRequestFocusForCall(carAudioFocus);

        AudioFocusInfo delayedFocusRequest = getInfo(USAGE_MEDIA, SECOND_CLIENT_ID,
                AUDIOFOCUS_GAIN, true);

        carAudioFocus.onAudioFocusRequest(delayedFocusRequest,
                AUDIOFOCUS_REQUEST_GRANTED);

        AudioFocusInfo mapFocusInfo = getInfo(USAGE_ASSISTANCE_NAVIGATION_GUIDANCE, THIRD_CLIENT_ID,
                AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK, false);
        carAudioFocus.onAudioFocusRequest(mapFocusInfo, AUDIOFOCUS_REQUEST_GRANTED);

        carAudioFocus.onAudioFocusAbandon(mapFocusInfo);

        verify(mMockAudioManager, never()).dispatchAudioFocusChange(
                delayedFocusRequest, AUDIOFOCUS_LOSS, mAudioPolicy);

        verify(mMockAudioManager, never()).dispatchAudioFocusChange(
                delayedFocusRequest, AUDIOFOCUS_GAIN, mAudioPolicy);
    }

    @Test
    public void
            onAudioFocus_concurrentRequestAfterDelayedFocus_delayedGainesFocus() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(true);

        AudioFocusInfo callFocusInfo = setupFocusInfoAndRequestFocusForCall(carAudioFocus);

        AudioFocusInfo delayedFocusRequest = getInfo(USAGE_MEDIA, SECOND_CLIENT_ID,
                AUDIOFOCUS_GAIN, true);

        carAudioFocus.onAudioFocusRequest(delayedFocusRequest,
                AUDIOFOCUS_REQUEST_GRANTED);

        AudioFocusInfo mapFocusInfo = getInfo(USAGE_ASSISTANCE_NAVIGATION_GUIDANCE, THIRD_CLIENT_ID,
                AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK, false);
        carAudioFocus.onAudioFocusRequest(mapFocusInfo, AUDIOFOCUS_REQUEST_GRANTED);

        carAudioFocus.onAudioFocusAbandon(mapFocusInfo);

        carAudioFocus.onAudioFocusAbandon(callFocusInfo);

        verify(mMockAudioManager).dispatchAudioFocusChange(
                delayedFocusRequest, AUDIOFOCUS_GAIN, mAudioPolicy);
    }

    @Test
    public void
            onAudioFocus_delayedFocusRequestAfterDoubleReject_delayedGainesFocus() {
        CarAudioFocus carAudioFocus = getCarAudioFocus(true);

        AudioFocusInfo callRingFocusInfo = getInfo(USAGE_NOTIFICATION_RINGTONE, FIRST_CLIENT_ID,
                AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK, false);
        carAudioFocus.onAudioFocusRequest(callRingFocusInfo, AUDIOFOCUS_REQUEST_GRANTED);
        verify(mMockAudioManager)
                .setFocusRequestResult(callRingFocusInfo,
                        AUDIOFOCUS_REQUEST_GRANTED, mAudioPolicy);

        AudioAttributes audioAttributes = new AudioAttributes.Builder()
                .setSystemUsage(USAGE_EMERGENCY)
                .build();
        AudioFocusInfo emergencyFocusInfo = getInfo(audioAttributes, SECOND_CLIENT_ID,
                AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK, false);
        carAudioFocus.onAudioFocusRequest(emergencyFocusInfo, AUDIOFOCUS_REQUEST_GRANTED);
        verify(mMockAudioManager)
                .setFocusRequestResult(emergencyFocusInfo,
                        AUDIOFOCUS_REQUEST_GRANTED, mAudioPolicy);

        verify(mMockAudioManager).dispatchAudioFocusChange(
                callRingFocusInfo, AUDIOFOCUS_LOSS_TRANSIENT, mAudioPolicy);

        AudioFocusInfo delayedFocusRequest = getInfo(USAGE_MEDIA, THIRD_CLIENT_ID,
                AUDIOFOCUS_GAIN, true);

        carAudioFocus.onAudioFocusRequest(delayedFocusRequest,
                AUDIOFOCUS_REQUEST_GRANTED);

        verify(mMockAudioManager)
                .setFocusRequestResult(delayedFocusRequest,
                        AUDIOFOCUS_REQUEST_DELAYED, mAudioPolicy);

        carAudioFocus.onAudioFocusAbandon(emergencyFocusInfo);

        verify(mMockAudioManager, never()).dispatchAudioFocusChange(
                delayedFocusRequest, AUDIOFOCUS_GAIN, mAudioPolicy);

        verify(mMockAudioManager).dispatchAudioFocusChange(
                callRingFocusInfo, AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK, mAudioPolicy);

        carAudioFocus.onAudioFocusAbandon(callRingFocusInfo);

        verify(mMockAudioManager).dispatchAudioFocusChange(
                delayedFocusRequest, AUDIOFOCUS_GAIN, mAudioPolicy);

    }

    private AudioFocusInfo setupFocusInfoAndRequestFocusForCall(CarAudioFocus carAudioFocus) {
        AudioFocusInfo callFocusInfo = getInfo(USAGE_VOICE_COMMUNICATION, FIRST_CLIENT_ID,
                AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK, false);
        carAudioFocus.onAudioFocusRequest(callFocusInfo, AUDIOFOCUS_REQUEST_GRANTED);
        verify(mMockAudioManager)
                .setFocusRequestResult(callFocusInfo,
                        AUDIOFOCUS_REQUEST_GRANTED, mAudioPolicy);
        return callFocusInfo;
    }

    // USAGE_ASSISTANCE_NAVIGATION_GUIDANCE is concurrent with USAGE_MEDIA
    private AudioFocusInfo getConcurrentInfo(int gainType) {
        return getInfo(USAGE_ASSISTANCE_NAVIGATION_GUIDANCE, SECOND_CLIENT_ID, gainType,
                false);
    }

    // USAGE_VEHICLE_STATUS is concurrent with USAGE_MEDIA
    private AudioFocusInfo getConcurrentWithSystemUsageInfo() {
        return getSystemUsageInfo(USAGE_VEHICLE_STATUS, AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK);
    }

    // USAGE_MEDIA is exclusive with USAGE_MEDIA
    private AudioFocusInfo getExclusiveInfo(int gainType) {
        return getInfo(USAGE_MEDIA, SECOND_CLIENT_ID, gainType, false);
    }

    // USAGE_MEDIA is exclusive with USAGE_MEDIA
    private AudioFocusInfo getDelayedExclusiveInfo(int gainType) {
        return getInfo(USAGE_MEDIA, SECOND_CLIENT_ID, gainType, true);
    }

    // USAGE_EMERGENCY is exclusive with USAGE_MEDIA
    private AudioFocusInfo getExclusiveWithSystemUsageInfo() {
        return getSystemUsageInfo(USAGE_EMERGENCY, AUDIOFOCUS_GAIN);
    }

    // USAGE_ASSISTANCE_NAVIGATION_GUIDANCE is rejected with USAGE_ASSISTANT
    private AudioFocusInfo getRejectInfo() {
        return getInfo(USAGE_ASSISTANCE_NAVIGATION_GUIDANCE, SECOND_CLIENT_ID,
                AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK, false);
    }

    // USAGE_ANNOUNCEMENT is rejected with USAGE_VOICE_COMMUNICATION
    private AudioFocusInfo getRejectWithSystemUsageInfo() {
        return getSystemUsageInfo(USAGE_ANNOUNCEMENT, AUDIOFOCUS_GAIN);
    }

    private AudioFocusInfo requestFocusForUsageWithFirstClient(@AttributeUsage int usage,
            CarAudioFocus carAudioFocus) {
        AudioFocusInfo initialFocusInfo = getInfo(usage, FIRST_CLIENT_ID, AUDIOFOCUS_GAIN,
                false);
        carAudioFocus.onAudioFocusRequest(initialFocusInfo, AUDIOFOCUS_REQUEST_GRANTED);
        return initialFocusInfo;
    }

    private AudioFocusInfo requestFocusForMediaWithFirstClient(CarAudioFocus carAudioFocus) {
        return requestFocusForUsageWithFirstClient(USAGE_MEDIA, carAudioFocus);
    }

    private AudioFocusInfo getInfoForFirstClientWithMedia() {
        return getInfo(USAGE_MEDIA, FIRST_CLIENT_ID, AUDIOFOCUS_GAIN, false);
    }

    private AudioFocusInfo getInfo(@AttributeUsage int usage, String clientId, int gainType,
            boolean acceptsDelayedFocus) {
        AudioAttributes audioAttributes = new AudioAttributes.Builder()
                .setUsage(usage)
                .build();
        return getInfo(audioAttributes, clientId, gainType, acceptsDelayedFocus);
    }

    private AudioFocusInfo getSystemUsageInfo(@AttributeUsage int systemUsage, int gainType) {
        AudioAttributes audioAttributes = new AudioAttributes.Builder()
                .setSystemUsage(systemUsage)
                .build();
        return getInfo(audioAttributes, SECOND_CLIENT_ID, gainType, false);
    }

    private AudioFocusInfo getInfo(AudioAttributes audioAttributes, String clientId, int gainType,
            boolean acceptsDelayedFocus) {
        int flags =  acceptsDelayedFocus ? AudioManager.AUDIOFOCUS_FLAG_DELAY_OK : AUDIOFOCUS_FLAG;
        return new AudioFocusInfo(audioAttributes, CLIENT_UID, clientId, PACKAGE_NAME,
                gainType, AudioManager.AUDIOFOCUS_NONE,
                flags, Build.VERSION.SDK_INT);
    }

    private CarAudioFocus getCarAudioFocus(boolean enableDelayAudioFocus) {
        CarAudioFocus carAudioFocus = new CarAudioFocus(mMockAudioManager, mMockPackageManager,
                mFocusInteraction, enableDelayAudioFocus);
        carAudioFocus.setOwningPolicy(mAudioPolicy);
        return carAudioFocus;
    }
}
