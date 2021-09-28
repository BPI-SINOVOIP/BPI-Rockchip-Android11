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

package com.android.cellbroadcastreceiver.unit;

import static com.android.cellbroadcastreceiver.CellBroadcastAlertService.SHOW_NEW_ALERT_ACTION;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Intent;
import android.content.res.Configuration;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.HandlerThread;
import android.telephony.TelephonyManager;

import com.android.cellbroadcastreceiver.CellBroadcastAlertAudio;

import org.junit.After;
import org.junit.Before;
import org.mockito.MockitoAnnotations;

public class CellBroadcastAlertAudioTest extends
        CellBroadcastServiceTestCase<CellBroadcastAlertAudio> {

    private static final String TEST_MESSAGE_BODY = "test message body";
    private static final int[] TEST_VIBRATION_PATTERN = new int[]{0, 1, 0, 1};
    private static final String TEST_MESSAGE_LANGUAGE = "en";
    private static final int TEST_MAX_VOLUME = 1001;
    private static final long MAX_INIT_WAIT_MS = 5000;

    private Configuration mConfiguration = new Configuration();
    private AudioDeviceInfo[] mDevices = new AudioDeviceInfo[0];
    private Object mLock = new Object();
    private boolean mReady;

    public CellBroadcastAlertAudioTest() {
        super(CellBroadcastAlertAudio.class);
    }

    private class PhoneStateListenerHandler extends HandlerThread {

        private Runnable mFunction;

        PhoneStateListenerHandler(String name, Runnable func) {
            super(name);
            mFunction = func;
        }

        @Override
        public void onLooperPrepared() {
            mFunction.run();
            setReady(true);
        }
    }

    protected void waitUntilReady() {
        synchronized (mLock) {
            if (!mReady) {
                try {
                    mLock.wait(MAX_INIT_WAIT_MS);
                } catch (InterruptedException ie) {
                }

                if (!mReady) {
                    fail("Telephony tests failed to initialize");
                }
            }
        }
    }

    protected void setReady(boolean ready) {
        synchronized (mLock) {
            mReady = ready;
            mLock.notifyAll();
        }
    }

    @Before
    public void setUp() throws Exception {
        super.setUp();
        MockitoAnnotations.initMocks(this);
        doReturn(mConfiguration).when(mResources).getConfiguration();
        doReturn(mDevices).when(mMockedAudioManager).getDevices(anyInt());

    }

    @After
    public void tearDown() throws Exception {
        super.tearDown();
    }

    public void testStartService() throws Throwable {
        PhoneStateListenerHandler phoneStateListenerHandler = new PhoneStateListenerHandler(
                "testStartService",
                () -> {
                    doReturn(AudioManager.RINGER_MODE_NORMAL).when(
                            mMockedAudioManager).getRingerMode();

                    Intent intent = new Intent(mContext, CellBroadcastAlertAudio.class);
                    intent.setAction(SHOW_NEW_ALERT_ACTION);
                    intent.putExtra(CellBroadcastAlertAudio.ALERT_AUDIO_MESSAGE_BODY,
                            TEST_MESSAGE_BODY);
                    intent.putExtra(CellBroadcastAlertAudio.ALERT_AUDIO_VIBRATION_PATTERN_EXTRA,
                            TEST_VIBRATION_PATTERN);
                    intent.putExtra(CellBroadcastAlertAudio.ALERT_AUDIO_MESSAGE_LANGUAGE,
                            TEST_MESSAGE_LANGUAGE);
                    intent.putExtra(CellBroadcastAlertAudio.ALERT_AUDIO_OVERRIDE_DND_EXTRA,
                            true);
                    startService(intent);
                });
        phoneStateListenerHandler.start();
        waitUntilReady();
        verify(mMockedAudioManager).getRingerMode();
        verify(mMockedVibrator).vibrate(any(), any());
        phoneStateListenerHandler.quit();
    }

    /**
     * If the user is currently not in a call and the override DND flag is set, the volume will be
     * set to max.
     */
    public void testStartServiceNotInCallOverrideDnd() throws Throwable {
        PhoneStateListenerHandler phoneStateListenerHandler = new PhoneStateListenerHandler(
                "testStartServiceNotInCallOverrideDnd",
                () -> {
                    doReturn(AudioManager.RINGER_MODE_SILENT).when(
                            mMockedAudioManager).getRingerMode();
                    doReturn(TelephonyManager.CALL_STATE_IDLE).when(
                            mMockedTelephonyManager).getCallState();
                    doReturn(TEST_MAX_VOLUME).when(mMockedAudioManager).getStreamMaxVolume(
                            anyInt());

                    Intent intent = new Intent(mContext, CellBroadcastAlertAudio.class);
                    intent.setAction(SHOW_NEW_ALERT_ACTION);
                    intent.putExtra(CellBroadcastAlertAudio.ALERT_AUDIO_MESSAGE_BODY,
                            TEST_MESSAGE_BODY);
                    intent.putExtra(CellBroadcastAlertAudio.ALERT_AUDIO_VIBRATION_PATTERN_EXTRA,
                            TEST_VIBRATION_PATTERN);
                    intent.putExtra(CellBroadcastAlertAudio.ALERT_AUDIO_OVERRIDE_DND_EXTRA, true);
                    startService(intent);
                });
        phoneStateListenerHandler.start();
        waitUntilReady();
        verify(mMockedAudioManager).getRingerMode();
        verify(mMockedVibrator).vibrate(any(), any());
        verify(mMockedTelephonyManager, atLeastOnce()).getCallState();
        verify(mMockedAudioManager).requestAudioFocus(any(), anyInt(), anyInt());
        verify(mMockedAudioManager).getDevices(anyInt());
        verify(mMockedAudioManager).setStreamVolume(anyInt(), eq(TEST_MAX_VOLUME), anyInt());
        phoneStateListenerHandler.quit();
    }

    public void testStartServiceEnableLedFlash() throws Throwable {
        PhoneStateListenerHandler phoneStateListenerHandler = new PhoneStateListenerHandler(
                "testStartServiceEnableLedFlash",
                () -> {
                    doReturn(AudioManager.RINGER_MODE_NORMAL).when(
                            mMockedAudioManager).getRingerMode();
                    doReturn(true).when(mResources).getBoolean(
                            eq(com.android.cellbroadcastreceiver.R.bool.enable_led_flash));

                    Intent intent = new Intent(mContext, CellBroadcastAlertAudio.class);
                    intent.setAction(SHOW_NEW_ALERT_ACTION);
                    intent.putExtra(CellBroadcastAlertAudio.ALERT_AUDIO_MESSAGE_BODY,
                            TEST_MESSAGE_BODY);
                    intent.putExtra(CellBroadcastAlertAudio.ALERT_AUDIO_VIBRATION_PATTERN_EXTRA,
                            TEST_VIBRATION_PATTERN);
                    startService(intent);
                });
        phoneStateListenerHandler.start();
        waitUntilReady();
        // TODO(b/134400042): we can't mock CameraManager because it's final, but let's at least
        //                    make sure the code doesn't crash. If we switch to Mockito 2 this
        //                    will be mockable.
        //verify(mMockedCameraManager).setTorchMode(anyString(), true);
        phoneStateListenerHandler.quit();
    }

    public void testStartServiceSilentRinger() throws Throwable {
        PhoneStateListenerHandler phoneStateListenerHandler = new PhoneStateListenerHandler(
                "testStartServiceSilentRinger",
                () -> {
                    doReturn(AudioManager.RINGER_MODE_SILENT).when(
                            mMockedAudioManager).getRingerMode();

                    Intent intent = new Intent(mContext, CellBroadcastAlertAudio.class);
                    intent.setAction(SHOW_NEW_ALERT_ACTION);
                    intent.putExtra(CellBroadcastAlertAudio.ALERT_AUDIO_MESSAGE_BODY,
                            TEST_MESSAGE_BODY);
                    intent.putExtra(CellBroadcastAlertAudio.ALERT_AUDIO_VIBRATION_PATTERN_EXTRA,
                            TEST_VIBRATION_PATTERN);
                    startService(intent);
                });
        phoneStateListenerHandler.start();
        waitUntilReady();
        verify(mMockedAudioManager).getRingerMode();
        verify(mMockedVibrator, times(0)).vibrate(any(), any());
        phoneStateListenerHandler.quit();
    }

    public void testStartServiceVibrateRinger() throws Throwable {
        PhoneStateListenerHandler phoneStateListenerHandler = new PhoneStateListenerHandler(
                "testStartServiceVibrateRinger",
                () -> {
                    doReturn(AudioManager.RINGER_MODE_VIBRATE).when(
                            mMockedAudioManager).getRingerMode();

                    Intent intent = new Intent(mContext, CellBroadcastAlertAudio.class);
                    intent.setAction(SHOW_NEW_ALERT_ACTION);
                    intent.putExtra(CellBroadcastAlertAudio.ALERT_AUDIO_MESSAGE_BODY,
                            TEST_MESSAGE_BODY);
                    intent.putExtra(CellBroadcastAlertAudio.ALERT_AUDIO_VIBRATION_PATTERN_EXTRA,
                            TEST_VIBRATION_PATTERN);
                    startService(intent);
                });
        phoneStateListenerHandler.start();
        waitUntilReady();
        verify(mMockedAudioManager).getRingerMode();
        verify(mMockedVibrator).vibrate(any(), any());
        phoneStateListenerHandler.quit();
    }

    /**
     * When an alert is triggered while an alert is already happening, the system needs to stop
     * the previous alert.
     */
    public void testStartServiceAndStop() throws Throwable {
        Intent intent = new Intent(mContext, CellBroadcastAlertAudio.class);
        intent.setAction(SHOW_NEW_ALERT_ACTION);
        intent.putExtra(CellBroadcastAlertAudio.ALERT_AUDIO_MESSAGE_BODY,
                TEST_MESSAGE_BODY);
        intent.putExtra(CellBroadcastAlertAudio.ALERT_AUDIO_VIBRATION_PATTERN_EXTRA,
                TEST_VIBRATION_PATTERN);
        doReturn(AudioManager.RINGER_MODE_NORMAL).when(
                mMockedAudioManager).getRingerMode();

        PhoneStateListenerHandler phoneStateListenerHandler = new PhoneStateListenerHandler(
                "testStartServiceStop",
                () -> {
                    startService(intent);
                    startService(intent);
                });
        phoneStateListenerHandler.start();
        waitUntilReady();
        verify(mMockedVibrator, atLeastOnce()).cancel();
        phoneStateListenerHandler.quit();
        waitUntilReady();
    }
}
