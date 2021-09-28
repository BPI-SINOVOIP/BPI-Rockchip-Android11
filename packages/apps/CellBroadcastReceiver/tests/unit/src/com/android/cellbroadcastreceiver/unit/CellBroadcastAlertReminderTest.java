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

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.HandlerThread;

import com.android.cellbroadcastreceiver.CellBroadcastAlertReminder;
import com.android.cellbroadcastreceiver.CellBroadcastSettings;

import org.junit.After;
import org.junit.Before;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

public class CellBroadcastAlertReminderTest extends
        CellBroadcastServiceTestCase<CellBroadcastAlertReminder> {

    private static final long MAX_INIT_WAIT_MS = 5000;

    @Mock
    private SharedPreferences mSharedPreferences;
    @Mock
    private Context mMockContext;

    private Object mLock = new Object();
    private boolean mReady;

    public CellBroadcastAlertReminderTest() {
        super(CellBroadcastAlertReminder.class);
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
    }

    @After
    public void tearDown() throws Exception {
        super.tearDown();
    }

    /**
     * When the reminder is set to vibrate, vibrate method should be called once.
     */
    public void testStartServiceVibrate() throws Throwable {
        PhoneStateListenerHandler phoneStateListenerHandler = new PhoneStateListenerHandler(
                "testStartServiceVibrate",
                () -> {
                    Intent intent = new Intent(mContext, CellBroadcastAlertReminder.class);
                    intent.setAction(CellBroadcastAlertReminder.ACTION_PLAY_ALERT_REMINDER);
                    intent.putExtra(CellBroadcastAlertReminder.ALERT_REMINDER_VIBRATE_EXTRA,
                            true);
                    startService(intent);
                });
        phoneStateListenerHandler.start();
        waitUntilReady();

        verify(mMockedVibrator).vibrate(any());
        phoneStateListenerHandler.quit();
    }

    /**
     * When the reminder is not set to vibrate, vibrate method should not be called.
     */
    public void testStartServiceNotVibrate() throws Throwable {
        PhoneStateListenerHandler phoneStateListenerHandler = new PhoneStateListenerHandler(
                "testStartServiceNotVibrate",
                () -> {
                    Intent intent = new Intent(mContext, CellBroadcastAlertReminder.class);
                    intent.setAction(CellBroadcastAlertReminder.ACTION_PLAY_ALERT_REMINDER);
                    intent.putExtra(CellBroadcastAlertReminder.ALERT_REMINDER_VIBRATE_EXTRA,
                            false);
                    startService(intent);
                });
        phoneStateListenerHandler.start();
        waitUntilReady();

        verify(mMockedVibrator, never()).vibrate(any());
        phoneStateListenerHandler.quit();
    }

    public void testQueueAlertReminderReturnFalseIfIntervalNull() {
        doReturn(mSharedPreferences).when(mMockContext).getSharedPreferences(anyString(), anyInt());
        doReturn(null).when(mSharedPreferences).getString(
                CellBroadcastSettings.KEY_ALERT_REMINDER_INTERVAL, null);
        assertFalse(CellBroadcastAlertReminder.queueAlertReminder(mMockContext, 1, true));
    }

    public void testQueueAlertReminderReturnFalseIfIntervalStringNotANumber() {
        doReturn(mSharedPreferences).when(mMockContext).getSharedPreferences(anyString(), anyInt());
        doReturn("NotANumber").when(mSharedPreferences).getString(
                CellBroadcastSettings.KEY_ALERT_REMINDER_INTERVAL, null);
        assertFalse(CellBroadcastAlertReminder.queueAlertReminder(mMockContext, 1, true));
    }

    public void testQueueAlertReminderReturnFalseIfIntervalZero() {
        doReturn(mSharedPreferences).when(mMockContext).getSharedPreferences(anyString(), anyInt());
        doReturn("0").when(mSharedPreferences).getString(
                CellBroadcastSettings.KEY_ALERT_REMINDER_INTERVAL, null);
        assertFalse(CellBroadcastAlertReminder.queueAlertReminder(mMockContext, 1, true));
    }

    public void testQueueAlertReminderReturnFalseIfIntervalOneButNotFirstTime() {
        doReturn(mSharedPreferences).when(mMockContext).getSharedPreferences(anyString(), anyInt());
        doReturn("1").when(mSharedPreferences).getString(
                CellBroadcastSettings.KEY_ALERT_REMINDER_INTERVAL, null);
        assertFalse(CellBroadcastAlertReminder.queueAlertReminder(mMockContext, 1, false));
    }
}
