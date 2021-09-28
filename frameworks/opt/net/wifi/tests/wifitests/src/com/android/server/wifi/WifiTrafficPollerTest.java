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
package com.android.server.wifi;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.net.wifi.ITrafficStateCallback;
import android.net.wifi.WifiManager;
import android.os.Handler;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.test.TestLooper;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/**
 * Unit tests for {@link com.android.server.wifi.WifiTrafficPoller}.
 */
@SmallTest
public class WifiTrafficPollerTest extends WifiBaseTest {
    public static final String TAG = "WifiTrafficPollerTest";

    private TestLooper mLooper;
    private WifiTrafficPoller mWifiTrafficPoller;
    private final static long DEFAULT_PACKET_COUNT = 10;
    private final static long TX_PACKET_COUNT = 40;
    private final static long RX_PACKET_COUNT = 50;
    private static final int TEST_TRAFFIC_STATE_CALLBACK_IDENTIFIER = 14;
    private static final int TEST_TRAFFIC_STATE_CALLBACK_IDENTIFIER2 = 42;

    @Mock IBinder mAppBinder;
    @Mock ITrafficStateCallback mTrafficStateCallback;

    @Mock IBinder mAppBinder2;
    @Mock ITrafficStateCallback mTrafficStateCallback2;

    /**
     * Called before each test
     */
    @Before
    public void setUp() throws Exception {
        // Ensure looper exists
        mLooper = new TestLooper();
        MockitoAnnotations.initMocks(this);

        mWifiTrafficPoller = new WifiTrafficPoller(new Handler(mLooper.getLooper()));

        // Set the current mTxPkts and mRxPkts to DEFAULT_PACKET_COUNT
        mWifiTrafficPoller.notifyOnDataActivity(DEFAULT_PACKET_COUNT, DEFAULT_PACKET_COUNT);
    }

    /**
     * Verify that clients should be notified of activity in case Tx/Rx packet count changes.
     */
    @Test
    public void testClientNotification() throws RemoteException {
        // Register Client to verify that Tx/RX packet message is properly received.
        mWifiTrafficPoller.addCallback(
                mAppBinder, mTrafficStateCallback, TEST_TRAFFIC_STATE_CALLBACK_IDENTIFIER);
        mWifiTrafficPoller.notifyOnDataActivity(TX_PACKET_COUNT, RX_PACKET_COUNT);

        // Client should get the DATA_ACTIVITY_NOTIFICATION
        verify(mTrafficStateCallback).onStateChanged(
                WifiManager.TrafficStateCallback.DATA_ACTIVITY_INOUT);
    }

    /**
     * Verify that remove client should be handled
     */
    @Test
    public void testRemoveClient() throws RemoteException {
        // Register Client to verify that Tx/RX packet message is properly received.
        mWifiTrafficPoller.addCallback(
                mAppBinder, mTrafficStateCallback, TEST_TRAFFIC_STATE_CALLBACK_IDENTIFIER);
        mWifiTrafficPoller.removeCallback(TEST_TRAFFIC_STATE_CALLBACK_IDENTIFIER);
        verify(mAppBinder).unlinkToDeath(any(), anyInt());

        mWifiTrafficPoller.notifyOnDataActivity(TX_PACKET_COUNT, RX_PACKET_COUNT);

        // Client should not get any message after the client is removed.
        verify(mTrafficStateCallback, never()).onStateChanged(anyInt());
    }

    /**
     * Verify that remove client ignores when callback identifier is wrong.
     */
    @Test
    public void testRemoveClientWithWrongIdentifier() throws RemoteException {
        // Register Client to verify that Tx/RX packet message is properly received.
        mWifiTrafficPoller.addCallback(
                mAppBinder, mTrafficStateCallback, TEST_TRAFFIC_STATE_CALLBACK_IDENTIFIER);
        mWifiTrafficPoller.removeCallback(TEST_TRAFFIC_STATE_CALLBACK_IDENTIFIER + 5);
        mLooper.dispatchAll();

        mWifiTrafficPoller.notifyOnDataActivity(TX_PACKET_COUNT, RX_PACKET_COUNT);

        // Client should get the DATA_ACTIVITY_NOTIFICATION
        verify(mTrafficStateCallback).onStateChanged(
                WifiManager.TrafficStateCallback.DATA_ACTIVITY_INOUT);
    }

    /**
     *
     * Verify that traffic poller registers for death notification on adding client.
     */
    @Test
    public void registersForBinderDeathOnAddClient() throws Exception {
        mWifiTrafficPoller.addCallback(
                mAppBinder, mTrafficStateCallback, TEST_TRAFFIC_STATE_CALLBACK_IDENTIFIER);
        verify(mAppBinder).linkToDeath(any(IBinder.DeathRecipient.class), anyInt());
    }

    /**
     *
     * Verify that traffic poller registers for death notification on adding client.
     */
    @Test
    public void addCallbackFailureOnLinkToDeath() throws Exception {
        doThrow(new RemoteException())
                .when(mAppBinder).linkToDeath(any(IBinder.DeathRecipient.class), anyInt());
        mWifiTrafficPoller.addCallback(
                mAppBinder, mTrafficStateCallback, TEST_TRAFFIC_STATE_CALLBACK_IDENTIFIER);
        verify(mAppBinder).linkToDeath(any(IBinder.DeathRecipient.class), anyInt());

        mWifiTrafficPoller.notifyOnDataActivity(TX_PACKET_COUNT, RX_PACKET_COUNT);

        // Client should not get any message callback add failed.
        verify(mTrafficStateCallback, never()).onStateChanged(anyInt());
    }

    /** Test that if the data activity didn't change, the client is not notified. */
    @Test
    public void unchangedDataActivityNotNotified() throws Exception {
        mWifiTrafficPoller.addCallback(
                mAppBinder, mTrafficStateCallback, TEST_TRAFFIC_STATE_CALLBACK_IDENTIFIER);
        mWifiTrafficPoller.notifyOnDataActivity(TX_PACKET_COUNT, RX_PACKET_COUNT);

        verify(mTrafficStateCallback).onStateChanged(
                WifiManager.TrafficStateCallback.DATA_ACTIVITY_INOUT);

        // since TX and RX both increased, should still be INOUT. But since it's the same data
        // activity as before, the callback should not be triggered again.
        mWifiTrafficPoller.notifyOnDataActivity(TX_PACKET_COUNT + 1, RX_PACKET_COUNT + 1);

        // still only called once
        verify(mTrafficStateCallback).onStateChanged(anyInt());
    }

    /**
     * If there are 2 callbacks, but the data activity only changed for one of them, only that
     * callback should be triggered.
     */
    @Test
    public void multipleCallbacksOnlyChangedNotified() throws Exception {
        mWifiTrafficPoller.addCallback(
                mAppBinder, mTrafficStateCallback, TEST_TRAFFIC_STATE_CALLBACK_IDENTIFIER);
        mWifiTrafficPoller.notifyOnDataActivity(TX_PACKET_COUNT, RX_PACKET_COUNT);

        verify(mTrafficStateCallback).onStateChanged(
                WifiManager.TrafficStateCallback.DATA_ACTIVITY_INOUT);
        verify(mTrafficStateCallback2, never()).onStateChanged(anyInt());

        mWifiTrafficPoller.addCallback(
                mAppBinder2, mTrafficStateCallback2, TEST_TRAFFIC_STATE_CALLBACK_IDENTIFIER2);
        mWifiTrafficPoller.notifyOnDataActivity(TX_PACKET_COUNT + 1, RX_PACKET_COUNT + 1);

        // still only called once
        verify(mTrafficStateCallback).onStateChanged(anyInt());
        // called for the first time with INOUT
        verify(mTrafficStateCallback2)
                .onStateChanged(WifiManager.TrafficStateCallback.DATA_ACTIVITY_INOUT);
        // not called with anything else
        verify(mTrafficStateCallback2).onStateChanged(anyInt());

        // now only TX increased
        mWifiTrafficPoller.notifyOnDataActivity(TX_PACKET_COUNT + 2, RX_PACKET_COUNT + 1);

        // called once with OUT
        verify(mTrafficStateCallback)
                .onStateChanged(WifiManager.TrafficStateCallback.DATA_ACTIVITY_OUT);
        // called twice total
        verify(mTrafficStateCallback, times(2)).onStateChanged(anyInt());

        // called once with OUT
        verify(mTrafficStateCallback2)
                .onStateChanged(WifiManager.TrafficStateCallback.DATA_ACTIVITY_OUT);
        // called twice total
        verify(mTrafficStateCallback2, times(2)).onStateChanged(anyInt());
    }
}
