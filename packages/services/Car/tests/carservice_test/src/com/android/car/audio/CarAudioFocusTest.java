/*
 * Copyright (C) 2019 The Android Open Source Project
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

import static android.media.AudioManager.AUDIOFOCUS_GAIN;
import static android.media.AudioManager.AUDIOFOCUS_LOSS;
import static android.media.AudioManager.AUDIOFOCUS_REQUEST_DELAYED;
import static android.media.AudioManager.AUDIOFOCUS_REQUEST_FAILED;
import static android.media.AudioManager.AUDIOFOCUS_REQUEST_GRANTED;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assume.assumeTrue;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Looper;
import android.util.Log;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.FlakyTest;
import androidx.test.filters.RequiresDevice;
import androidx.test.platform.app.InstrumentationRegistry;

import com.android.car.R;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

@RunWith(AndroidJUnit4.class)
public class CarAudioFocusTest {

    private static final String TAG = CarAudioFocusTest.class.getSimpleName();
    private static final boolean DEBUG = false;
    private static final long TEST_TIMING_TOLERANCE_MS = 100;
    private static final int TEST_TORELANCE_MAX_ITERATIONS = 5;
    private static final int INTERACTION_REJECT = 0;  // Focus not granted
    private static final int INTERACTION_EXCLUSIVE = 1;  // Focus granted, others loose focus
    private static final int INTERACTION_CONCURRENT = 2;  // Focus granted, others keep focus

    // CarAudioContext.INVALID
    private static final AudioAttributes ATTR_INVALID = new AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_VIRTUAL_SOURCE)
            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
            .build();
    // CarAudioContext.MUSIC
    private static final AudioAttributes ATTR_MEDIA = new AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_MEDIA)
            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
            .build();
    // CarAudioContext.NAVIGATION
    private static final AudioAttributes ATTR_NAVIGATION = new AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_ASSISTANCE_NAVIGATION_GUIDANCE)
            .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
            .build();
    // CarAudioContext.VOICE_COMMAND
    private static final AudioAttributes ATTR_VOICE_COMMAND = new AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_ASSISTANCE_ACCESSIBILITY)
            .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
            .build();
    // CarAudioContext.CALL_RING
    private static final AudioAttributes ATTR_CALL_RING = new AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_NOTIFICATION_RINGTONE)
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
            .build();
    // CarAudioContext.CALL
    private static final AudioAttributes ATTR_CALL = new AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
            .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
            .build();
    // CarAudioContext.ALARM
    private static final AudioAttributes ATTR_ALARM = new AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_ALARM)
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
            .build();
    // CarAudioContext.NOTIFICATION
    private static final AudioAttributes ATTR_NOTIFICATION = new AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_NOTIFICATION)
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
            .build();
    // CarAudioContext.SYSTEM_SOUND
    private static final AudioAttributes ATTR_SYSTEM_SOUND = new AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_ASSISTANCE_SONIFICATION)
            .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
            .build();
    // CarAudioContext.EMERGENCY
    private static final AudioAttributes ATTR_EMERGENCY = new AudioAttributes.Builder()
            .setSystemUsage(AudioAttributes.USAGE_EMERGENCY)
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
            .build();
    // CarAudioContext.SAFETY
    private static final AudioAttributes ATTR_SAFETY = new AudioAttributes.Builder()
            .setSystemUsage(AudioAttributes.USAGE_SAFETY)
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
            .build();
    // CarAudioContext.VEHICLE_STATUS
    private static final AudioAttributes ATTR_VEHICLE_STATUS = new AudioAttributes.Builder()
            .setSystemUsage(AudioAttributes.USAGE_VEHICLE_STATUS)
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
            .build();
    // CarAudioContext.ANNOUNCEMENT
    private static final AudioAttributes ATTR_ANNOUNCEMENT = new AudioAttributes.Builder()
            .setSystemUsage(AudioAttributes.USAGE_ANNOUNCEMENT)
            .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
            .build();

    private final Set<AudioFocusRequest> mAudioFocusRequestsSet = new HashSet<>();

    private AudioManager mAudioManager;

    @Before
    public void setUp() {
        Context context = InstrumentationRegistry.getInstrumentation().getContext();
        mAudioManager = (AudioManager) context.getSystemService(AudioManager.class);

        boolean isDynamicRoutingEnabled = context.getResources().getBoolean(
                R.bool.audioUseDynamicRouting);
        assumeTrue("Dynamic routing must be enabled to run CarAudioFocusTests",
                isDynamicRoutingEnabled);
    }

    @After
    public void cleanUp() {
        Iterator<AudioFocusRequest> iterator = mAudioFocusRequestsSet.iterator();
        while (iterator.hasNext()) {
            AudioFocusRequest request = iterator.next();
            mAudioManager.abandonAudioFocusRequest(request);
            if (DEBUG) {
                Log.d(TAG, "cleanUp Removing: "
                        + request.getAudioAttributes().usageToString());
            }
        }
    }

    @Test
    public void requestAudioFocus_forRequestWithDelayedFocus_requestGranted() {
        AudioFocusRequest mediaAudioFocusRequest = delayedFocusRequestBuilder().build();

        mAudioFocusRequestsSet.add(mediaAudioFocusRequest);
        assertThat(mAudioManager.requestAudioFocus(mediaAudioFocusRequest))
                .isEqualTo(AUDIOFOCUS_REQUEST_GRANTED);
    }

    @Test
    public void requestAudioFocus_forRequestWithDelayedFocus_whileOneCall_requestDelayed() {
        AudioFocusRequest phoneAudioFocusRequest = phoneFocusRequestBuilder().build();

        mAudioFocusRequestsSet.add(phoneAudioFocusRequest);
        mAudioManager.requestAudioFocus(phoneAudioFocusRequest);

        AudioFocusRequest mediaAudioFocusRequest = delayedFocusRequestBuilder().build();

        mAudioFocusRequestsSet.add(mediaAudioFocusRequest);
        assertThat(mAudioManager.requestAudioFocus(mediaAudioFocusRequest))
                .isEqualTo(AUDIOFOCUS_REQUEST_DELAYED);
    }

    @Test
    public void abandonAudioFocusRequest_forCall_whileFocusDelayed_focusGained() {
        AudioFocusRequest phoneAudioFocusRequest = phoneFocusRequestBuilder().build();

        mAudioManager.requestAudioFocus(phoneAudioFocusRequest);
        mAudioFocusRequestsSet.add(phoneAudioFocusRequest);

        FocusChangeListener mediaFocusChangeListener = new FocusChangeListener();
        AudioFocusRequest mediaAudioFocusRequest = delayedFocusRequestBuilder()
                .setOnAudioFocusChangeListener(mediaFocusChangeListener).build();

        mAudioManager.requestAudioFocus(mediaAudioFocusRequest);
        mAudioFocusRequestsSet.add(mediaAudioFocusRequest);

        mAudioManager.abandonAudioFocusRequest(phoneAudioFocusRequest);
        mAudioFocusRequestsSet.remove(phoneAudioFocusRequest);

        assertThat(mediaFocusChangeListener
                .waitForFocusChangeAndAssertFocus(TEST_TIMING_TOLERANCE_MS, AUDIOFOCUS_GAIN,
                        "Could not gain focus for delayed focus after call ended"))
                .isTrue();
    }

    @Test
    public void abandonAudioFocusRequest_forDelayedRequest_whileOnCall_requestGranted() {
        AudioFocusRequest phoneAudioFocusRequest = phoneFocusRequestBuilder().build();

        mAudioManager.requestAudioFocus(phoneAudioFocusRequest);
        mAudioFocusRequestsSet.add(phoneAudioFocusRequest);

        AudioFocusRequest mediaAudioFocusRequest = delayedFocusRequestBuilder().build();

        mAudioManager.requestAudioFocus(mediaAudioFocusRequest);
        mAudioFocusRequestsSet.add(mediaAudioFocusRequest);

        assertThat(mAudioManager.abandonAudioFocusRequest(mediaAudioFocusRequest))
                .isEqualTo(AUDIOFOCUS_REQUEST_GRANTED);
        mAudioFocusRequestsSet.remove(mediaAudioFocusRequest);
    }

    @Test
    public void
            abandonAudioFocusRequest_forCall_afterDelayedAbandon_delayedRequestDoesNotGainsFocus() {
        AudioFocusRequest phoneAudioFocusRequest = phoneFocusRequestBuilder().build();

        mAudioManager.requestAudioFocus(phoneAudioFocusRequest);
        mAudioFocusRequestsSet.add(phoneAudioFocusRequest);

        FocusChangeListener mediaFocusChangeListener = new FocusChangeListener();
        AudioFocusRequest mediaAudioFocusRequest = delayedFocusRequestBuilder()
                .setOnAudioFocusChangeListener(mediaFocusChangeListener).build();

        mAudioManager.requestAudioFocus(mediaAudioFocusRequest);
        mAudioFocusRequestsSet.add(mediaAudioFocusRequest);

        mAudioManager.abandonAudioFocusRequest(mediaAudioFocusRequest);

        mAudioManager.abandonAudioFocusRequest(phoneAudioFocusRequest);
        mAudioFocusRequestsSet.remove(phoneAudioFocusRequest);

        assertThat(mediaFocusChangeListener
                .waitForFocusChangeAndAssertFocus(TEST_TIMING_TOLERANCE_MS, AUDIOFOCUS_GAIN,
                        "Focus gained for abandoned delayed request after call"))
                .isFalse();
        mAudioFocusRequestsSet.remove(mediaAudioFocusRequest);
    }

    @Test
    public void
            requestAudioFocus_multipleTimesForSameDelayedRequest_delayedRequestDoesNotGainsFocus() {
        AudioFocusRequest phoneAudioFocusRequest = phoneFocusRequestBuilder().build();

        mAudioManager.requestAudioFocus(phoneAudioFocusRequest);
        mAudioFocusRequestsSet.add(phoneAudioFocusRequest);

        FocusChangeListener mediaFocusChangeListener = new FocusChangeListener();
        AudioFocusRequest mediaAudioFocusRequest = delayedFocusRequestBuilder()
                .setOnAudioFocusChangeListener(mediaFocusChangeListener).build();

        mAudioManager.requestAudioFocus(mediaAudioFocusRequest);
        mAudioFocusRequestsSet.add(mediaAudioFocusRequest);

        mAudioManager.requestAudioFocus(mediaAudioFocusRequest);

        assertThat(mediaFocusChangeListener
                .waitForFocusChangeAndAssertFocus(TEST_TIMING_TOLERANCE_MS,
                        AUDIOFOCUS_LOSS,
                        "Focus gained for delayed request after same request"))
                .isFalse();
    }

    @Test
    public void requestAudioFocus_multipleTimesForSameFocusListener_requestFailed() {
        AudioFocusRequest phoneAudioFocusRequest = phoneFocusRequestBuilder().build();

        mAudioManager.requestAudioFocus(phoneAudioFocusRequest);
        mAudioFocusRequestsSet.add(phoneAudioFocusRequest);

        FocusChangeListener focusChangeLister = new FocusChangeListener();
        AudioFocusRequest mediaAudioFocusRequest = delayedFocusRequestBuilder()
                .setOnAudioFocusChangeListener(focusChangeLister).build();

        mAudioManager.requestAudioFocus(mediaAudioFocusRequest);
        mAudioFocusRequestsSet.add(mediaAudioFocusRequest);

        AudioFocusRequest systemSoundRequest = delayedFocusRequestBuilder()
                .setOnAudioFocusChangeListener(focusChangeLister)
                .setAudioAttributes(ATTR_SYSTEM_SOUND).build();

        mAudioFocusRequestsSet.add(systemSoundRequest);
        assertThat(mAudioManager.requestAudioFocus(systemSoundRequest))
                .isEqualTo(AUDIOFOCUS_REQUEST_FAILED);
        mAudioFocusRequestsSet.remove(systemSoundRequest);
    }

    @Test
    public void individualAttributeFocusRequest_focusRequestGranted() {
        // Make sure each usage is able to request and release audio focus individually
        requestAndLoseFocusForAttribute(ATTR_INVALID);
        requestAndLoseFocusForAttribute(ATTR_MEDIA);
        requestAndLoseFocusForAttribute(ATTR_NAVIGATION);
        requestAndLoseFocusForAttribute(ATTR_VOICE_COMMAND);
        requestAndLoseFocusForAttribute(ATTR_CALL_RING);
        requestAndLoseFocusForAttribute(ATTR_CALL);
        requestAndLoseFocusForAttribute(ATTR_ALARM);
        requestAndLoseFocusForAttribute(ATTR_NOTIFICATION);
        requestAndLoseFocusForAttribute(ATTR_SYSTEM_SOUND);
        requestAndLoseFocusForAttribute(ATTR_EMERGENCY);
        requestAndLoseFocusForAttribute(ATTR_SAFETY);
        requestAndLoseFocusForAttribute(ATTR_VEHICLE_STATUS);
        requestAndLoseFocusForAttribute(ATTR_ANNOUNCEMENT);
    }

    @Test
    @FlakyTest
    public void exclusiveInteractionsForFocusGain_requestGrantedAndFocusLossSent() {
        // For each interaction the focus request is granted and on the second request
        // focus lost is dispatched to the first focus listener

        // Test Exclusive interactions with audio focus gain request without pause
        // instead of ducking
        testExclusiveInteractions(AUDIOFOCUS_GAIN, false);
        // Test Exclusive interactions with audio focus gain request with pause instead of ducking
        testExclusiveInteractions(AUDIOFOCUS_GAIN, true);
    }

    @Test
    public void exclusiveInteractionsTransient_requestGrantedAndFocusLossSent() {
        // For each interaction the focus request is granted and on the second request
        // focus lost transient is dispatched to the first focus listener

        // Test Exclusive interactions with audio focus gain transient request
        // without pause instead of ducking
        testExclusiveInteractions(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT, false);
        // Test Exclusive interactions with audio focus gain transient request
        // with pause instead of ducking
        testExclusiveInteractions(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT, true);
    }

    @RequiresDevice
    @Test
    public void exclusiveInteractionsTransientMayDuck_requestGrantedAndFocusLossSent() {
        // For each interaction the focus request is granted and on the second request
        // focus lost transient is dispatched to the first focus listener

        // Test exclusive interactions with audio focus transient may duck focus request
        // without pause instead of ducking
        testExclusiveInteractions(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK, false);
        // Test exclusive interactions with audio focus transient may duck focus request
        // with pause instead of ducking
        testExclusiveInteractions(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK, true);
    }

    @RequiresDevice
    @Test
    public void rejectedInteractions_focusRequestRejected() {
        // Test different paired interaction between different usages
        // for each interaction pair the first focus request will be granted but the second
        // will be rejected
        int interaction = INTERACTION_REJECT;
        int gain = AUDIOFOCUS_GAIN;
        testInteraction(ATTR_INVALID, ATTR_INVALID, interaction, gain, false);
        testInteraction(ATTR_INVALID, ATTR_MEDIA, interaction, gain, false);
        testInteraction(ATTR_INVALID, ATTR_NAVIGATION, interaction, gain, false);
        testInteraction(ATTR_INVALID, ATTR_VOICE_COMMAND, interaction, gain, false);
        testInteraction(ATTR_INVALID, ATTR_CALL_RING, interaction, gain, false);
        testInteraction(ATTR_INVALID, ATTR_CALL, interaction, gain, false);
        testInteraction(ATTR_INVALID, ATTR_ALARM, interaction, gain, false);
        testInteraction(ATTR_INVALID, ATTR_NOTIFICATION, interaction, gain, false);
        testInteraction(ATTR_INVALID, ATTR_SYSTEM_SOUND, interaction, gain, false);
        testInteraction(ATTR_INVALID, ATTR_VEHICLE_STATUS, interaction, gain, false);
        testInteraction(ATTR_INVALID, ATTR_ANNOUNCEMENT, interaction, gain, false);

        testInteraction(ATTR_MEDIA, ATTR_INVALID, interaction, gain, false);

        testInteraction(ATTR_NAVIGATION, ATTR_INVALID, interaction, gain, false);

        testInteraction(ATTR_VOICE_COMMAND, ATTR_INVALID, interaction, gain, false);
        testInteraction(ATTR_VOICE_COMMAND, ATTR_NAVIGATION, interaction, gain, false);
        testInteraction(ATTR_VOICE_COMMAND, ATTR_NOTIFICATION, interaction, gain, false);
        testInteraction(ATTR_VOICE_COMMAND, ATTR_SYSTEM_SOUND, interaction, gain, false);
        testInteraction(ATTR_VOICE_COMMAND, ATTR_ANNOUNCEMENT, interaction, gain, false);

        testInteraction(ATTR_CALL_RING, ATTR_INVALID, interaction, gain, false);
        testInteraction(ATTR_CALL_RING, ATTR_MEDIA, interaction, gain, false);
        testInteraction(ATTR_CALL_RING, ATTR_ALARM, interaction, gain, false);
        testInteraction(ATTR_CALL_RING, ATTR_NOTIFICATION, interaction, gain, false);
        testInteraction(ATTR_CALL_RING, ATTR_ANNOUNCEMENT, interaction, gain, false);

        testInteraction(ATTR_CALL, ATTR_INVALID, interaction, gain, false);
        testInteraction(ATTR_CALL, ATTR_MEDIA, interaction, gain, false);
        testInteraction(ATTR_CALL, ATTR_VOICE_COMMAND, interaction, gain, false);
        testInteraction(ATTR_CALL, ATTR_SYSTEM_SOUND, interaction, gain, false);
        testInteraction(ATTR_CALL, ATTR_ANNOUNCEMENT, interaction, gain, false);

        testInteraction(ATTR_ALARM, ATTR_INVALID, interaction, gain, false);
        testInteraction(ATTR_ALARM, ATTR_ANNOUNCEMENT, interaction, gain, false);

        testInteraction(ATTR_NOTIFICATION, ATTR_INVALID, interaction, gain, false);

        testInteraction(ATTR_SYSTEM_SOUND, ATTR_INVALID, interaction, gain, false);

        testInteraction(ATTR_EMERGENCY, ATTR_INVALID, interaction, gain, false);
        testInteraction(ATTR_EMERGENCY, ATTR_MEDIA, interaction, gain, false);
        testInteraction(ATTR_EMERGENCY, ATTR_NAVIGATION, interaction, gain, false);
        testInteraction(ATTR_EMERGENCY, ATTR_VOICE_COMMAND, interaction, gain, false);
        testInteraction(ATTR_EMERGENCY, ATTR_CALL_RING, interaction, gain, false);
        testInteraction(ATTR_EMERGENCY, ATTR_ALARM, interaction, gain, false);
        testInteraction(ATTR_EMERGENCY, ATTR_NOTIFICATION, interaction, gain, false);
        testInteraction(ATTR_EMERGENCY, ATTR_SYSTEM_SOUND, interaction, gain, false);
        testInteraction(ATTR_EMERGENCY, ATTR_VEHICLE_STATUS, interaction, gain, false);
        testInteraction(ATTR_EMERGENCY, ATTR_ANNOUNCEMENT, interaction, gain, false);

        testInteraction(ATTR_SAFETY, ATTR_INVALID, interaction, gain, false);

        testInteraction(ATTR_VEHICLE_STATUS, ATTR_INVALID, interaction, gain, false);

        testInteraction(ATTR_ANNOUNCEMENT, ATTR_INVALID, interaction, gain, false);
    }

    @Test
    public void concurrentInteractionsFocusGain_requestGrantedAndFocusLossSent() {
        // Test concurrent interactions i.e. interactions that can
        // potentially gain focus at the same time.
        // For this test permanent focus gain is requested by two usages.
        // The focus request will be granted for both and on the second focus request focus
        // lost will dispatched to the first focus listener listener.
        testConcurrentInteractions(AUDIOFOCUS_GAIN, false);
    }

    @Test
    @FlakyTest
    public void concurrentInteractionsTransientGain_requestGrantedAndFocusLossTransientSent() {
        // Test concurrent interactions i.e. interactions that can
        // potentially gain focus at the same time.
        // For this test permanent focus gain is requested by first usage and focus gain transient
        // is requested by second usage.
        // The focus request will be granted for both and on the second focus request focus
        // lost transient will dispatched to the first focus listener listener.
        testConcurrentInteractions(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT, false);
        // Repeat the test this time with pause for ducking on first listener
        testConcurrentInteractions(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT, true);
    }

    @RequiresDevice
    @Test
    public void concurrentInteractionsTransientGainMayDuck_requestGrantedAndNoFocusLossSent() {
        // Test concurrent interactions i.e. interactions that can
        // potentially gain focus at the same time.
        // For this test permanent focus gain is requested by first usage and focus gain transient
        // may duck is requested by second usage.
        // The focus request will be granted for both but no focus lost is sent to the first focus
        // listener, as each usage actually has shared focus and  should play at the same time.
        testConcurrentInteractions(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK, false);
        // Test the same behaviour but this time with pause for ducking on the first focus listener
        testConcurrentInteractions(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK, true);
    }

    private void testConcurrentInteractions(int gain, boolean pauseForDucking) {
        // Test paired concurrent interactions i.e. interactions that can
        // potentially gain focus at the same time.
        int interaction = INTERACTION_CONCURRENT;
        testInteraction(ATTR_MEDIA, ATTR_NAVIGATION, interaction, gain, pauseForDucking);
        testInteraction(ATTR_MEDIA, ATTR_NOTIFICATION, interaction, gain, pauseForDucking);
        testInteraction(ATTR_MEDIA, ATTR_SYSTEM_SOUND, interaction, gain, pauseForDucking);
        testInteraction(ATTR_MEDIA, ATTR_SAFETY, interaction, gain, pauseForDucking);
        testInteraction(ATTR_MEDIA, ATTR_VEHICLE_STATUS, interaction, gain, pauseForDucking);

        testInteraction(ATTR_NAVIGATION, ATTR_MEDIA, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NAVIGATION, ATTR_NAVIGATION, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NAVIGATION, ATTR_CALL_RING, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NAVIGATION, ATTR_ALARM, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NAVIGATION, ATTR_NOTIFICATION, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NAVIGATION, ATTR_SYSTEM_SOUND, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NAVIGATION, ATTR_SAFETY, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NAVIGATION, ATTR_VEHICLE_STATUS, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NAVIGATION, ATTR_ANNOUNCEMENT, interaction, gain, pauseForDucking);

        testInteraction(ATTR_VOICE_COMMAND, ATTR_MEDIA, interaction, gain, pauseForDucking);
        testInteraction(ATTR_VOICE_COMMAND, ATTR_VOICE_COMMAND, interaction, gain, pauseForDucking);
        testInteraction(ATTR_VOICE_COMMAND, ATTR_SAFETY, interaction, gain, pauseForDucking);
        testInteraction(ATTR_VOICE_COMMAND, ATTR_VEHICLE_STATUS, interaction, gain,
                pauseForDucking);

        testInteraction(ATTR_CALL_RING, ATTR_NAVIGATION, interaction, gain, pauseForDucking);
        testInteraction(ATTR_CALL_RING, ATTR_VOICE_COMMAND, interaction, gain, pauseForDucking);
        testInteraction(ATTR_CALL_RING, ATTR_CALL_RING, interaction, gain, pauseForDucking);
        testInteraction(ATTR_CALL_RING, ATTR_CALL, interaction, gain, pauseForDucking);
        testInteraction(ATTR_CALL_RING, ATTR_SAFETY, interaction, gain, pauseForDucking);
        testInteraction(ATTR_CALL_RING, ATTR_VEHICLE_STATUS, interaction, gain, pauseForDucking);

        testInteraction(ATTR_CALL, ATTR_NAVIGATION, interaction, gain, pauseForDucking);
        testInteraction(ATTR_CALL, ATTR_CALL_RING, interaction, gain, pauseForDucking);
        testInteraction(ATTR_CALL, ATTR_CALL, interaction, gain, pauseForDucking);
        testInteraction(ATTR_CALL, ATTR_ALARM, interaction, gain, pauseForDucking);
        testInteraction(ATTR_CALL, ATTR_NOTIFICATION, interaction, gain, pauseForDucking);
        testInteraction(ATTR_CALL, ATTR_EMERGENCY, interaction, gain, pauseForDucking);
        testInteraction(ATTR_CALL, ATTR_SAFETY, interaction, gain, pauseForDucking);
        testInteraction(ATTR_CALL, ATTR_VEHICLE_STATUS, interaction, gain, pauseForDucking);

        testInteraction(ATTR_ALARM, ATTR_MEDIA, interaction, gain, pauseForDucking);
        testInteraction(ATTR_ALARM, ATTR_NAVIGATION, interaction, gain, pauseForDucking);
        testInteraction(ATTR_ALARM, ATTR_ALARM, interaction, gain, pauseForDucking);
        testInteraction(ATTR_ALARM, ATTR_NOTIFICATION, interaction, gain, pauseForDucking);
        testInteraction(ATTR_ALARM, ATTR_SYSTEM_SOUND, interaction, gain, pauseForDucking);
        testInteraction(ATTR_ALARM, ATTR_SAFETY, interaction, gain, pauseForDucking);
        testInteraction(ATTR_ALARM, ATTR_VEHICLE_STATUS, interaction, gain, pauseForDucking);

        testInteraction(ATTR_NOTIFICATION, ATTR_MEDIA, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NOTIFICATION, ATTR_NAVIGATION, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NOTIFICATION, ATTR_ALARM, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NOTIFICATION, ATTR_NOTIFICATION, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NOTIFICATION, ATTR_SYSTEM_SOUND, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NOTIFICATION, ATTR_SAFETY, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NOTIFICATION, ATTR_VEHICLE_STATUS, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NOTIFICATION, ATTR_ANNOUNCEMENT, interaction, gain, pauseForDucking);

        testInteraction(ATTR_SYSTEM_SOUND, ATTR_MEDIA, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SYSTEM_SOUND, ATTR_NAVIGATION, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SYSTEM_SOUND, ATTR_ALARM, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SYSTEM_SOUND, ATTR_NOTIFICATION, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SYSTEM_SOUND, ATTR_SYSTEM_SOUND, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SYSTEM_SOUND, ATTR_SAFETY, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SYSTEM_SOUND, ATTR_VEHICLE_STATUS, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SYSTEM_SOUND, ATTR_ANNOUNCEMENT, interaction, gain, pauseForDucking);

        testInteraction(ATTR_EMERGENCY, ATTR_CALL, interaction, gain, pauseForDucking);
        testInteraction(ATTR_EMERGENCY, ATTR_EMERGENCY, interaction, gain, pauseForDucking);
        testInteraction(ATTR_EMERGENCY, ATTR_SAFETY, interaction, gain, pauseForDucking);

        testInteraction(ATTR_SAFETY, ATTR_MEDIA, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SAFETY, ATTR_NAVIGATION, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SAFETY, ATTR_CALL_RING, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SAFETY, ATTR_CALL, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SAFETY, ATTR_ALARM, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SAFETY, ATTR_NOTIFICATION, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SAFETY, ATTR_SYSTEM_SOUND, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SAFETY, ATTR_EMERGENCY, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SAFETY, ATTR_SAFETY, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SAFETY, ATTR_VEHICLE_STATUS, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SAFETY, ATTR_ANNOUNCEMENT, interaction, gain, pauseForDucking);

        testInteraction(ATTR_VEHICLE_STATUS, ATTR_MEDIA, interaction, gain, pauseForDucking);
        testInteraction(ATTR_VEHICLE_STATUS, ATTR_NAVIGATION, interaction, gain, pauseForDucking);
        testInteraction(ATTR_VEHICLE_STATUS, ATTR_CALL_RING, interaction, gain, pauseForDucking);
        testInteraction(ATTR_VEHICLE_STATUS, ATTR_CALL, interaction, gain, pauseForDucking);
        testInteraction(ATTR_VEHICLE_STATUS, ATTR_ALARM, interaction, gain, pauseForDucking);
        testInteraction(ATTR_VEHICLE_STATUS, ATTR_NOTIFICATION, interaction, gain, pauseForDucking);
        testInteraction(ATTR_VEHICLE_STATUS, ATTR_SYSTEM_SOUND, interaction, gain, pauseForDucking);
        testInteraction(ATTR_VEHICLE_STATUS, ATTR_SAFETY, interaction, gain, pauseForDucking);
        testInteraction(ATTR_VEHICLE_STATUS, ATTR_VEHICLE_STATUS, interaction, gain,
                pauseForDucking);
        testInteraction(ATTR_VEHICLE_STATUS, ATTR_ANNOUNCEMENT, interaction, gain, pauseForDucking);

        testInteraction(ATTR_ANNOUNCEMENT, ATTR_NAVIGATION, interaction, gain, pauseForDucking);
        testInteraction(ATTR_ANNOUNCEMENT, ATTR_NOTIFICATION, interaction, gain, pauseForDucking);
        testInteraction(ATTR_ANNOUNCEMENT, ATTR_SYSTEM_SOUND, interaction, gain, pauseForDucking);
        testInteraction(ATTR_ANNOUNCEMENT, ATTR_SAFETY, interaction, gain, pauseForDucking);
        testInteraction(ATTR_ANNOUNCEMENT, ATTR_VEHICLE_STATUS, interaction, gain, pauseForDucking);
    }

    private void testExclusiveInteractions(int gain, boolean pauseForDucking) {
        // Test exclusive interaction, interaction where each usage will not share focus with other
        // another usage. As a result once focus is gained any current focus listener
        // in this interaction will lose focus.
        int interaction = INTERACTION_EXCLUSIVE;

        testInteraction(ATTR_INVALID, ATTR_EMERGENCY, interaction, gain, pauseForDucking);
        testInteraction(ATTR_INVALID, ATTR_SAFETY, interaction, gain, pauseForDucking);

        testInteraction(ATTR_MEDIA, ATTR_MEDIA, interaction, gain, pauseForDucking);
        testInteraction(ATTR_MEDIA, ATTR_VOICE_COMMAND, interaction, gain, pauseForDucking);
        testInteraction(ATTR_MEDIA, ATTR_CALL_RING, interaction, gain, pauseForDucking);
        testInteraction(ATTR_MEDIA, ATTR_CALL, interaction, gain, pauseForDucking);
        testInteraction(ATTR_MEDIA, ATTR_ALARM, interaction, gain, pauseForDucking);
        testInteraction(ATTR_MEDIA, ATTR_EMERGENCY, interaction, gain, pauseForDucking);
        testInteraction(ATTR_MEDIA, ATTR_ANNOUNCEMENT, interaction, gain, pauseForDucking);

        testInteraction(ATTR_NAVIGATION, ATTR_VOICE_COMMAND, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NAVIGATION, ATTR_CALL, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NAVIGATION, ATTR_EMERGENCY, interaction, gain, pauseForDucking);

        testInteraction(ATTR_VOICE_COMMAND, ATTR_CALL_RING, interaction, gain, pauseForDucking);
        testInteraction(ATTR_VOICE_COMMAND, ATTR_CALL, interaction, gain, pauseForDucking);
        testInteraction(ATTR_VOICE_COMMAND, ATTR_EMERGENCY, interaction, gain, pauseForDucking);

        testInteraction(ATTR_CALL_RING, ATTR_EMERGENCY, interaction, gain, pauseForDucking);

        testInteraction(ATTR_ALARM, ATTR_VOICE_COMMAND, interaction, gain, pauseForDucking);
        testInteraction(ATTR_ALARM, ATTR_CALL_RING, interaction, gain, pauseForDucking);
        testInteraction(ATTR_ALARM, ATTR_CALL, interaction, gain, pauseForDucking);
        testInteraction(ATTR_ALARM, ATTR_EMERGENCY, interaction, gain, pauseForDucking);

        testInteraction(ATTR_NOTIFICATION, ATTR_VOICE_COMMAND, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NOTIFICATION, ATTR_CALL_RING, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NOTIFICATION, ATTR_CALL, interaction, gain, pauseForDucking);
        testInteraction(ATTR_NOTIFICATION, ATTR_EMERGENCY, interaction, gain, pauseForDucking);

        testInteraction(ATTR_SYSTEM_SOUND, ATTR_VOICE_COMMAND, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SYSTEM_SOUND, ATTR_CALL_RING, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SYSTEM_SOUND, ATTR_CALL, interaction, gain, pauseForDucking);
        testInteraction(ATTR_SYSTEM_SOUND, ATTR_EMERGENCY, interaction, gain, pauseForDucking);

        testInteraction(ATTR_VEHICLE_STATUS, ATTR_EMERGENCY, interaction, gain, pauseForDucking);

        testInteraction(ATTR_ANNOUNCEMENT, ATTR_MEDIA, interaction, gain, pauseForDucking);
        testInteraction(ATTR_ANNOUNCEMENT, ATTR_VOICE_COMMAND, interaction, gain, pauseForDucking);
        testInteraction(ATTR_ANNOUNCEMENT, ATTR_CALL_RING, interaction, gain, pauseForDucking);
        testInteraction(ATTR_ANNOUNCEMENT, ATTR_CALL, interaction, gain, pauseForDucking);
        testInteraction(ATTR_ANNOUNCEMENT, ATTR_ALARM, interaction, gain, pauseForDucking);
        testInteraction(ATTR_ANNOUNCEMENT, ATTR_EMERGENCY, interaction, gain, pauseForDucking);
        testInteraction(ATTR_ANNOUNCEMENT, ATTR_ANNOUNCEMENT, interaction, gain, pauseForDucking);
    }


    /**
     * Test paired usage interactions with gainType and pause instead ducking
     *
     * @param attributes1     Attributes of the first usage (first focus requester) in the
     *                        interaction
     * @param attributes2     Attributes of the second usage (second focus requester) in the
     *                        interaction
     * @param interaction     type of interaction {@link INTERACTION_REJECT}, {@link
     *                        INTERACTION_EXCLUSIVE}, {@link INTERACTION_CONCURRENT}
     * @param gainType        Type of gain {@link AUDIOFOCUS_GAIN} , {@link
     *                        CarAudioFocus.AUDIOFOCUS_GAIN_TRANSIENT}, {@link
     *                        CarAudioFocus.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK}
     * @param pauseForDucking flag to indicate if the first focus listener should pause instead of
     *                        ducking
     * @throws Exception
     */
    private void testInteraction(AudioAttributes attributes1,
            AudioAttributes attributes2,
            int interaction,
            int gainType,
            boolean pauseForDucking) {
        final FocusChangeListener focusChangeListener1 = new FocusChangeListener();
        final AudioFocusRequest audioFocusRequest1 = new AudioFocusRequest
                .Builder(AUDIOFOCUS_GAIN)
                .setAudioAttributes(attributes1)
                .setOnAudioFocusChangeListener(focusChangeListener1)
                .setForceDucking(false)
                .setWillPauseWhenDucked(pauseForDucking)
                .build();

        final FocusChangeListener focusChangeListener2 = new FocusChangeListener();
        final AudioFocusRequest audioFocusRequest2 = new AudioFocusRequest
                .Builder(gainType)
                .setAudioAttributes(attributes2)
                .setOnAudioFocusChangeListener(focusChangeListener2)
                .setForceDucking(false)
                .build();

        int expectedLoss = 0;

        // Each focus gain type will return a different focus lost type
        switch (gainType) {
            case AUDIOFOCUS_GAIN:
                expectedLoss = AUDIOFOCUS_LOSS;
                break;
            case AudioManager.AUDIOFOCUS_GAIN_TRANSIENT:
                expectedLoss = AudioManager.AUDIOFOCUS_LOSS_TRANSIENT;
                break;
            case AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK:
                expectedLoss = AudioManager.AUDIOFOCUS_LOSS_TRANSIENT;
                // Note loss or gain will not be sent as both can live concurrently
                if (interaction == INTERACTION_CONCURRENT && !pauseForDucking) {
                    expectedLoss = AudioManager.AUDIOFOCUS_NONE;
                }
                break;
        }

        int secondRequestResultsExpected = AUDIOFOCUS_REQUEST_GRANTED;

        if (interaction == INTERACTION_REJECT) {
            secondRequestResultsExpected = AUDIOFOCUS_REQUEST_FAILED;
        }

        int requestResult = mAudioManager.requestAudioFocus(audioFocusRequest1);
        String message = "Focus gain request failed  for 1st "
                + AudioAttributes.usageToString(attributes1.getSystemUsage());
        assertEquals(message, AUDIOFOCUS_REQUEST_GRANTED, requestResult);
        mAudioFocusRequestsSet.add(audioFocusRequest1);

        requestResult = mAudioManager.requestAudioFocus(audioFocusRequest2);
        message = "Focus gain request failed for 2nd "
                + AudioAttributes.usageToString(attributes2.getSystemUsage());
        assertEquals(message, secondRequestResultsExpected, requestResult);
        mAudioFocusRequestsSet.add(audioFocusRequest2);

        // If the results is rejected for second one we only have to clean up first
        // as the second focus request is rejected
        if (interaction == INTERACTION_REJECT) {
            requestResult = mAudioManager.abandonAudioFocusRequest(audioFocusRequest1);
            mAudioFocusRequestsSet.clear();
            message = "Focus loss request failed for 1st "
                    + AudioAttributes.usageToString(attributes1.getSystemUsage());
            assertEquals(message, AUDIOFOCUS_REQUEST_GRANTED, requestResult);
        }

        // If exclusive we expect to lose focus on 1st one
        // unless we have a concurrent interaction
        if (interaction == INTERACTION_EXCLUSIVE || interaction == INTERACTION_CONCURRENT) {
            message = "Focus change was not dispatched for 1st "
                    + AudioAttributes.usageToString(attributes1.getSystemUsage());
            boolean shouldStop = false;
            int counter = 0;
            while (!shouldStop && counter++ < TEST_TORELANCE_MAX_ITERATIONS) {
                boolean gainedFocusLoss = focusChangeListener1.waitForFocusChangeAndAssertFocus(
                        TEST_TIMING_TOLERANCE_MS, expectedLoss, message);
                shouldStop = gainedFocusLoss
                        || (expectedLoss == AudioManager.AUDIOFOCUS_NONE);
            }
            assertThat(shouldStop).isTrue();
            focusChangeListener1.resetFocusChangeAndWait();

            if (expectedLoss == AUDIOFOCUS_LOSS) {
                mAudioFocusRequestsSet.remove(audioFocusRequest1);
            }
            requestResult = mAudioManager.abandonAudioFocusRequest(audioFocusRequest2);
            mAudioFocusRequestsSet.remove(audioFocusRequest2);
            message = "Focus loss request failed  for 2nd "
                    + AudioAttributes.usageToString(attributes2.getSystemUsage());
            assertEquals(message, AUDIOFOCUS_REQUEST_GRANTED, requestResult);


            // If the loss was transient then we should have received back on 1st
            if ((gainType == AudioManager.AUDIOFOCUS_GAIN_TRANSIENT
                    || gainType == AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK)) {

                // Since ducking and concurrent can exist together
                // this needs to be skipped as the focus lost is not sent
                if (!(interaction == INTERACTION_CONCURRENT
                        && gainType == AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK)) {
                    message = "Focus change was not dispatched for 1st "
                            + AudioAttributes.usageToString(attributes1.getSystemUsage());

                    boolean focusGained = false;
                    int count = 0;
                    while (!focusGained && count++ < TEST_TORELANCE_MAX_ITERATIONS) {
                        focusGained = focusChangeListener1.waitForFocusChangeAndAssertFocus(
                                TEST_TIMING_TOLERANCE_MS,
                                AUDIOFOCUS_GAIN, message);
                    }
                    assertThat(focusGained).isTrue();
                    focusChangeListener1.resetFocusChangeAndWait();
                }
                // For concurrent focus interactions still needs to be released
                message = "Focus loss request failed  for 1st  "
                        + AudioAttributes.usageToString(attributes1.getSystemUsage());
                requestResult = mAudioManager.abandonAudioFocusRequest(audioFocusRequest1);
                mAudioFocusRequestsSet.remove(audioFocusRequest1);
                assertEquals(message, AUDIOFOCUS_REQUEST_GRANTED,
                        requestResult);
            }
        }
    }

    /**
     * Verifies usage can request audio focus and release it
     *
     * @param attribute usage attribute to request focus
     */
    private void requestAndLoseFocusForAttribute(AudioAttributes attribute) {
        FocusChangeListener focusChangeListener = new FocusChangeListener();
        AudioFocusRequest audioFocusRequest = new AudioFocusRequest
                .Builder(AUDIOFOCUS_GAIN)
                .setAudioAttributes(attribute)
                .setOnAudioFocusChangeListener(focusChangeListener)
                .setForceDucking(false)
                .build();


        int requestResult = mAudioManager.requestAudioFocus(audioFocusRequest);
        String message = "Focus gain request failed  for "
                + AudioAttributes.usageToString(attribute.getSystemUsage());
        assertEquals(message, AUDIOFOCUS_REQUEST_GRANTED, requestResult);
        mAudioFocusRequestsSet.add(audioFocusRequest);

        // Verify no focus changed dispatched
        message = "Focus change was dispatched for "
                + AudioAttributes.usageToString(attribute.getSystemUsage());

        assertThat(focusChangeListener.waitForFocusChangeAndAssertFocus(
                TEST_TIMING_TOLERANCE_MS, AudioManager.AUDIOFOCUS_NONE, message)).isFalse();
        focusChangeListener.resetFocusChangeAndWait();

        requestResult = mAudioManager.abandonAudioFocusRequest(audioFocusRequest);
        message = "Focus loss request failed  for "
                + AudioAttributes.usageToString(attribute.getSystemUsage());
        assertEquals(message, AUDIOFOCUS_REQUEST_GRANTED, requestResult);
        mAudioFocusRequestsSet.remove(audioFocusRequest);
    }

    private static AudioFocusRequest.Builder delayedFocusRequestBuilder() {
        AudioManager.OnAudioFocusChangeListener listener = new FocusChangeListener();
        return new AudioFocusRequest.Builder(AUDIOFOCUS_GAIN)
                .setAcceptsDelayedFocusGain(true).setAudioAttributes(ATTR_MEDIA)
                .setOnAudioFocusChangeListener(listener);
    }

    private static AudioFocusRequest.Builder phoneFocusRequestBuilder() {
        AudioManager.OnAudioFocusChangeListener listener = new FocusChangeListener();
        return new AudioFocusRequest.Builder(AUDIOFOCUS_GAIN)
                .setAudioAttributes(ATTR_CALL)
                .setOnAudioFocusChangeListener(listener);
    }

    private static class FocusChangeListener implements AudioManager.OnAudioFocusChangeListener {
        private final Semaphore mChangeEventSignal = new Semaphore(0);
        private int mFocusChange = AudioManager.AUDIOFOCUS_NONE;

        private boolean waitForFocusChangeAndAssertFocus(long timeoutMs, int expectedFocus,
                String message) {
            boolean acquired = false;
            try {
                acquired = mChangeEventSignal.tryAcquire(timeoutMs, TimeUnit.MILLISECONDS);
            } catch (Exception ignored) {

            }
            if (acquired) {
                assertEquals(message, expectedFocus, mFocusChange);
            }
            return acquired;
        }

        private void resetFocusChangeAndWait() {
            mFocusChange = AudioManager.AUDIOFOCUS_NONE;
            mChangeEventSignal.drainPermits();
        }

        @Override
        public void onAudioFocusChange(int focusChange) {
            // should be dispatched to main thread.
            assertThat(Looper.getMainLooper()).isEqualTo(Looper.myLooper());
            mFocusChange = focusChange;
            mChangeEventSignal.release();
        }
    }
}
