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

import static android.media.AudioAttributes.USAGE_ASSISTANCE_NAVIGATION_GUIDANCE;
import static android.media.AudioAttributes.USAGE_MEDIA;
import static android.media.AudioAttributes.USAGE_VOICE_COMMUNICATION;
import static android.media.AudioManager.AUDIOFOCUS_GAIN;
import static android.media.AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK;
import static android.media.AudioManager.AUDIOFOCUS_REQUEST_DELAYED;
import static android.media.AudioManager.AUDIOFOCUS_REQUEST_FAILED;
import static android.media.AudioManager.AUDIOFOCUS_REQUEST_GRANTED;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.car.media.CarAudioManager;
import android.content.ContentResolver;
import android.content.pm.PackageManager;
import android.media.AudioAttributes;
import android.media.AudioFocusInfo;
import android.media.AudioManager;
import android.media.audiopolicy.AudioPolicy;
import android.os.Build;
import android.os.Bundle;
import android.util.SparseArray;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(AndroidJUnit4.class)
public class CarZonesAudioFocusTest {
    private static final String MEDIA_CLIENT_ID = "media-client-id";
    private static final String NAVIGATION_CLIENT_ID = "nav-client-id";
    private static final String CALL_CLIENT_ID = "call-client-id";
    private static final String PACKAGE_NAME = "com.android.car.audio";
    private static final int AUDIOFOCUS_FLAG = 0;
    private static final int PRIMARY_ZONE_ID = CarAudioManager.PRIMARY_AUDIO_ZONE;
    private static final int SECONDARY_ZONE_ID = CarAudioManager.PRIMARY_AUDIO_ZONE + 1;
    private static final int MEDIA_CLIENT_UID_1 = 1086753;
    private static final int MEDIA_CLIENT_UID_2 = 1000009;
    private static final int NAVIGATION_CLIENT_UID = 1010101;
    private static final int TEST_USER_ID = 10;
    private static final int CALL_CLIENT_UID = 1086753;

    @Rule
    public MockitoRule rule = MockitoJUnit.rule();
    @Mock
    private AudioManager mMockAudioManager;
    @Mock
    private AudioPolicy mAudioPolicy;
    @Mock
    private CarAudioService mCarAudioService;
    @Mock
    private ContentResolver mContentResolver;
    @Mock
    private CarAudioSettings mCarAudioSettings;

    @Test
    public void onAudioFocusRequest_withNoCurrentFocusHolder_requestGranted() {
        CarZonesAudioFocus carZonesAudioFocus = getCarZonesAudioFocus(false);
        when(mCarAudioService.getZoneIdForUid(MEDIA_CLIENT_UID_1)).thenReturn(PRIMARY_ZONE_ID);
        AudioFocusInfo audioFocusInfo = new AudioFocusInfoBuilder().setUsage(USAGE_MEDIA)
                .setClientId(MEDIA_CLIENT_ID).setGainRequest(AUDIOFOCUS_GAIN)
                .setClientUid(MEDIA_CLIENT_UID_1).createAudioFocusInfo();

        requestFocusAndAssertIfRequestNotGranted(carZonesAudioFocus, audioFocusInfo);

        verify(mMockAudioManager, never())
                .dispatchAudioFocusChange(eq(audioFocusInfo), anyInt(), eq(mAudioPolicy));
    }

    @Test
    public void onAudioFocusRequest_forTwoDifferentZones_requestGranted() {
        CarZonesAudioFocus carZonesAudioFocus = getCarZonesAudioFocus(false);
        when(mCarAudioService.getZoneIdForUid(MEDIA_CLIENT_UID_1)).thenReturn(PRIMARY_ZONE_ID);
        AudioFocusInfo audioFocusInfoClient1 = new AudioFocusInfoBuilder().setUsage(USAGE_MEDIA)
                .setClientId(MEDIA_CLIENT_ID).setGainRequest(AUDIOFOCUS_GAIN)
                .setClientUid(MEDIA_CLIENT_UID_1).createAudioFocusInfo();

        requestFocusAndAssertIfRequestNotGranted(carZonesAudioFocus, audioFocusInfoClient1);

        when(mCarAudioService.getZoneIdForUid(MEDIA_CLIENT_UID_2)).thenReturn(SECONDARY_ZONE_ID);
        AudioFocusInfo audioFocusInfoClient2 = new AudioFocusInfoBuilder().setUsage(USAGE_MEDIA)
                .setClientId(MEDIA_CLIENT_ID).setGainRequest(AUDIOFOCUS_GAIN)
                .setClientUid(MEDIA_CLIENT_UID_2).createAudioFocusInfo();

        requestFocusAndAssertIfRequestNotGranted(carZonesAudioFocus, audioFocusInfoClient2);

        verify(mMockAudioManager, never())
                .dispatchAudioFocusChange(eq(audioFocusInfoClient1), anyInt(), eq(mAudioPolicy));

        verify(mMockAudioManager, never())
                .dispatchAudioFocusChange(eq(audioFocusInfoClient2), anyInt(), eq(mAudioPolicy));
    }

    @Test
    public void onAudioFocusRequest_forTwoDifferentZones_abandonInOne_requestGranted() {
        CarZonesAudioFocus carZonesAudioFocus = getCarZonesAudioFocus(false);
        when(mCarAudioService.getZoneIdForUid(MEDIA_CLIENT_UID_1)).thenReturn(PRIMARY_ZONE_ID);
        AudioFocusInfo audioFocusInfoClient1 = new AudioFocusInfoBuilder().setUsage(USAGE_MEDIA)
                .setClientId(MEDIA_CLIENT_ID).setGainRequest(AUDIOFOCUS_GAIN)
                .setClientUid(MEDIA_CLIENT_UID_1).createAudioFocusInfo();

        requestFocusAndAssertIfRequestNotGranted(carZonesAudioFocus, audioFocusInfoClient1);

        when(mCarAudioService.getZoneIdForUid(MEDIA_CLIENT_UID_2)).thenReturn(SECONDARY_ZONE_ID);
        AudioFocusInfo audioFocusInfoClient2 = new AudioFocusInfoBuilder().setUsage(USAGE_MEDIA)
                .setClientId(MEDIA_CLIENT_ID).setGainRequest(AUDIOFOCUS_GAIN)
                .setClientUid(MEDIA_CLIENT_UID_2).createAudioFocusInfo();

        requestFocusAndAssertIfRequestNotGranted(carZonesAudioFocus, audioFocusInfoClient2);

        carZonesAudioFocus.onAudioFocusAbandon(audioFocusInfoClient2);

        verify(mMockAudioManager, never())
                .dispatchAudioFocusChange(eq(audioFocusInfoClient1), anyInt(), eq(mAudioPolicy));

        verify(mMockAudioManager, never())
                .dispatchAudioFocusChange(eq(audioFocusInfoClient2), anyInt(), eq(mAudioPolicy));
    }

    @Test
    public void onAudioFocusRequest_withBundleFocusRequest_requestGranted() {
        CarZonesAudioFocus carZonesAudioFocus = getCarZonesAudioFocus(false);
        when(mCarAudioService.isAudioZoneIdValid(PRIMARY_ZONE_ID)).thenReturn(true);

        when(mCarAudioService.getZoneIdForUid(MEDIA_CLIENT_UID_1)).thenReturn(PRIMARY_ZONE_ID);
        Bundle bundle = new Bundle();
        bundle.putInt(CarAudioManager.AUDIOFOCUS_EXTRA_REQUEST_ZONE_ID,
                PRIMARY_ZONE_ID);
        AudioFocusInfo audioFocusInfoClient = new AudioFocusInfoBuilder().setUsage(USAGE_MEDIA)
                .setClientId(MEDIA_CLIENT_ID).setGainRequest(AUDIOFOCUS_GAIN)
                .setClientUid(MEDIA_CLIENT_UID_1).setBundle(bundle).createAudioFocusInfo();

        requestFocusAndAssertIfRequestNotGranted(carZonesAudioFocus, audioFocusInfoClient);

        verify(mMockAudioManager, never())
                .dispatchAudioFocusChange(eq(audioFocusInfoClient), anyInt(), eq(mAudioPolicy));
    }

    @Test
    public void onAudioFocusRequest_repeatForSameZone_requestGranted() {
        CarZonesAudioFocus carZonesAudioFocus = getCarZonesAudioFocus(false);
        when(mCarAudioService.getZoneIdForUid(MEDIA_CLIENT_UID_1)).thenReturn(PRIMARY_ZONE_ID);
        AudioFocusInfo audioFocusInfoMediaClient = new AudioFocusInfoBuilder().setUsage(USAGE_MEDIA)
                        .setClientId(MEDIA_CLIENT_ID).setGainRequest(AUDIOFOCUS_GAIN)
                        .setClientUid(MEDIA_CLIENT_UID_1).createAudioFocusInfo();

        requestFocusAndAssertIfRequestNotGranted(carZonesAudioFocus, audioFocusInfoMediaClient);

        when(mCarAudioService.getZoneIdForUid(NAVIGATION_CLIENT_UID)).thenReturn(PRIMARY_ZONE_ID);
        AudioFocusInfo audioFocusInfoNavClient =
                new AudioFocusInfoBuilder().setUsage(USAGE_ASSISTANCE_NAVIGATION_GUIDANCE)
                .setClientId(NAVIGATION_CLIENT_ID).setGainRequest(AUDIOFOCUS_GAIN)
                .setClientUid(NAVIGATION_CLIENT_UID).createAudioFocusInfo();

        requestFocusAndAssertIfRequestNotGranted(carZonesAudioFocus, audioFocusInfoNavClient);

        verify(mMockAudioManager)
                .dispatchAudioFocusChange(eq(audioFocusInfoMediaClient),
                        anyInt(), eq(mAudioPolicy));

        verify(mMockAudioManager, never())
                .dispatchAudioFocusChange(eq(audioFocusInfoNavClient),
                        anyInt(), eq(mAudioPolicy));
    }

    @Test
    public void onAudioFocusRequest_forNavigationWhileOnCall_rejectNavOnCall_requestFailed() {
        CarZonesAudioFocus carZonesAudioFocus = getCarZonesAudioFocus(false);
        when(mCarAudioService.isAudioZoneIdValid(PRIMARY_ZONE_ID)).thenReturn(true);
        setUpRejectNavigationOnCallValue(true);
        carZonesAudioFocus.updateUserForZoneId(PRIMARY_ZONE_ID, TEST_USER_ID);

        when(mCarAudioService.getZoneIdForUid(CALL_CLIENT_UID)).thenReturn(PRIMARY_ZONE_ID);
        AudioFocusInfo audioFocusInfoCallClient = new AudioFocusInfoBuilder()
                .setUsage(USAGE_VOICE_COMMUNICATION)
                .setGainRequest(AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK)
                .setClientId(CALL_CLIENT_ID)
                .setClientUid(CALL_CLIENT_UID).createAudioFocusInfo();

        requestFocusAndAssertIfRequestNotGranted(carZonesAudioFocus, audioFocusInfoCallClient);

        when(mCarAudioService.getZoneIdForUid(NAVIGATION_CLIENT_UID)).thenReturn(PRIMARY_ZONE_ID);
        AudioFocusInfo audioFocusInfoNavClient =
                new AudioFocusInfoBuilder().setUsage(USAGE_ASSISTANCE_NAVIGATION_GUIDANCE)
                        .setGainRequest(AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK)
                        .setClientId(NAVIGATION_CLIENT_ID)
                        .setClientUid(NAVIGATION_CLIENT_UID).createAudioFocusInfo();

        carZonesAudioFocus
                .onAudioFocusRequest(audioFocusInfoNavClient, AUDIOFOCUS_REQUEST_GRANTED);
        verify(mMockAudioManager).setFocusRequestResult(audioFocusInfoNavClient,
                AUDIOFOCUS_REQUEST_FAILED, mAudioPolicy);
    }

    @Test
    public void onAudioFocusRequest_forNavigationWhileOnCall_noRejectNavOnCall_requestSucceeds() {
        CarZonesAudioFocus carZonesAudioFocus = getCarZonesAudioFocus(false);
        when(mCarAudioService.isAudioZoneIdValid(PRIMARY_ZONE_ID)).thenReturn(true);
        setUpRejectNavigationOnCallValue(false);
        carZonesAudioFocus.updateUserForZoneId(PRIMARY_ZONE_ID, TEST_USER_ID);

        when(mCarAudioService.getZoneIdForUid(CALL_CLIENT_UID)).thenReturn(PRIMARY_ZONE_ID);
        AudioFocusInfo audioFocusInfoCallClient = new AudioFocusInfoBuilder()
                .setUsage(USAGE_VOICE_COMMUNICATION)
                .setGainRequest(AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK)
                .setClientId(CALL_CLIENT_ID)
                .setClientUid(CALL_CLIENT_UID).createAudioFocusInfo();

        requestFocusAndAssertIfRequestNotGranted(carZonesAudioFocus, audioFocusInfoCallClient);

        when(mCarAudioService.getZoneIdForUid(NAVIGATION_CLIENT_UID)).thenReturn(PRIMARY_ZONE_ID);
        AudioFocusInfo audioFocusInfoNavClient =
                new AudioFocusInfoBuilder().setUsage(USAGE_ASSISTANCE_NAVIGATION_GUIDANCE)
                        .setGainRequest(AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK)
                        .setClientId(NAVIGATION_CLIENT_ID)
                        .setClientUid(NAVIGATION_CLIENT_UID).createAudioFocusInfo();


        carZonesAudioFocus
                .onAudioFocusRequest(audioFocusInfoNavClient, AUDIOFOCUS_REQUEST_GRANTED);
        verify(mMockAudioManager).setFocusRequestResult(audioFocusInfoNavClient,
                AUDIOFOCUS_REQUEST_GRANTED, mAudioPolicy);
    }

    @Test
    public void onAudioFocusRequest_forMediaWhileOnCall_withDelayedEnable_delayedSucceeds() {
        CarZonesAudioFocus carZonesAudioFocus = getCarZonesAudioFocus(true);
        when(mCarAudioService.isAudioZoneIdValid(PRIMARY_ZONE_ID)).thenReturn(true);

        when(mCarAudioService.getZoneIdForUid(CALL_CLIENT_UID)).thenReturn(PRIMARY_ZONE_ID);
        AudioFocusInfo audioFocusInfoCallClient = new AudioFocusInfoBuilder()
                .setUsage(USAGE_VOICE_COMMUNICATION)
                .setGainRequest(AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK)
                .setClientId(CALL_CLIENT_ID)
                .setClientUid(CALL_CLIENT_UID).createAudioFocusInfo();

        requestFocusAndAssertIfRequestNotGranted(carZonesAudioFocus, audioFocusInfoCallClient);

        when(mCarAudioService.getZoneIdForUid(MEDIA_CLIENT_UID_1)).thenReturn(PRIMARY_ZONE_ID);
        AudioFocusInfo audioFocusMediaClient =
                new AudioFocusInfoBuilder().setUsage(USAGE_MEDIA)
                        .setGainRequest(AUDIOFOCUS_GAIN)
                        .setClientId(MEDIA_CLIENT_ID)
                        .setDelayedFocusRequestEnable(true)
                        .setClientUid(MEDIA_CLIENT_UID_1).createAudioFocusInfo();


        carZonesAudioFocus
                .onAudioFocusRequest(audioFocusMediaClient, AUDIOFOCUS_REQUEST_GRANTED);
        verify(mMockAudioManager).setFocusRequestResult(audioFocusMediaClient,
                AUDIOFOCUS_REQUEST_DELAYED, mAudioPolicy);
    }

    private void requestFocusAndAssertIfRequestNotGranted(CarZonesAudioFocus carZonesAudioFocus,
            AudioFocusInfo audioFocusClient) {
        requestFocusAndAssertIfRequestDiffers(carZonesAudioFocus, audioFocusClient,
                AUDIOFOCUS_REQUEST_GRANTED);
    }

    private void requestFocusAndAssertIfRequestDiffers(CarZonesAudioFocus carZonesAudioFocus,
            AudioFocusInfo audioFocusClient, int expectedAudioFocusResults) {
        carZonesAudioFocus.onAudioFocusRequest(audioFocusClient, expectedAudioFocusResults);
        verify(mMockAudioManager)
                .setFocusRequestResult(audioFocusClient, expectedAudioFocusResults, mAudioPolicy);
    }

    private SparseArray<CarAudioZone> generateAudioZones() {
        SparseArray<CarAudioZone> zones = new SparseArray<>();
        zones.put(PRIMARY_ZONE_ID, new CarAudioZone(PRIMARY_ZONE_ID, "Primary zone"));
        zones.put(SECONDARY_ZONE_ID, new CarAudioZone(SECONDARY_ZONE_ID, "Secondary zone"));
        return zones;
    }

    private CarZonesAudioFocus getCarZonesAudioFocus(boolean enableDelayedFocus) {
        SparseArray<CarAudioZone> zones = generateAudioZones();
        PackageManager mockPackageManager = mock(PackageManager.class);
        CarZonesAudioFocus carZonesAudioFocus =
                new CarZonesAudioFocus(mMockAudioManager, mockPackageManager, zones,
                        mCarAudioSettings, enableDelayedFocus);
        carZonesAudioFocus.setOwningPolicy(mCarAudioService, mAudioPolicy);


        return carZonesAudioFocus;
    }

    private void setUpRejectNavigationOnCallValue(boolean rejectNavigationOnCall) {
        when(mCarAudioSettings.getContentResolver()).thenReturn(mContentResolver);
        when(mCarAudioSettings.isRejectNavigationOnCallEnabledInSettings(TEST_USER_ID))
                .thenReturn(rejectNavigationOnCall);
    }

    public class AudioFocusInfoBuilder {
        private int mUsage;
        private int mClientUid;
        private String mClientId;
        private int mGainRequest;
        private String mPackageName = PACKAGE_NAME;
        private Bundle mBundle = null;
        private int mLossReceived = AudioManager.AUDIOFOCUS_NONE;
        private int mFlags = AUDIOFOCUS_FLAG;
        private int mSdk = Build.VERSION.SDK_INT;
        private boolean mDelayedFocusRequestEnabled = false;

        public AudioFocusInfoBuilder setUsage(int usage) {
            mUsage = usage;
            return this;
        }

        public AudioFocusInfoBuilder setClientUid(int clientUid) {
            mClientUid = clientUid;
            return this;
        }

        public AudioFocusInfoBuilder setClientId(String clientId) {
            mClientId = clientId;
            return this;
        }

        public AudioFocusInfoBuilder setPackageName(String packageName) {
            mPackageName = packageName;
            return this;
        }

        public AudioFocusInfoBuilder setGainRequest(int gainRequest) {
            mGainRequest = gainRequest;
            return this;
        }

        public AudioFocusInfoBuilder setLossReceived(int lossReceived) {
            mLossReceived = lossReceived;
            return this;
        }

        public AudioFocusInfoBuilder setSdk(int sdk) {
            mSdk = sdk;
            return this;
        }

        public AudioFocusInfoBuilder setBundle(Bundle bundle) {
            mBundle = bundle;
            return this;
        }

        public AudioFocusInfoBuilder setDelayedFocusRequestEnable(boolean b) {
            mDelayedFocusRequestEnabled = b;
            return this;
        }

        public AudioFocusInfo createAudioFocusInfo() {
            AudioAttributes.Builder builder = new AudioAttributes.Builder().setUsage(mUsage);
            if (mBundle != null) {
                builder = builder.addBundle(mBundle);
            }
            if (mDelayedFocusRequestEnabled) {
                mFlags = mFlags | AudioManager.AUDIOFOCUS_FLAG_DELAY_OK;
            }
            AudioAttributes audioAttributes = builder.build();
            return new AudioFocusInfo(audioAttributes, mClientUid, mClientId,
                    mPackageName, mGainRequest, mLossReceived, mFlags, mSdk);
        }
    }
}
