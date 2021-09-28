/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.net.wifi.aware.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.Mockito.mock;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.location.LocationManager;
import android.net.ConnectivityManager;
import android.net.MacAddress;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.wifi.WifiManager;
import android.net.wifi.aware.AttachCallback;
import android.net.wifi.aware.Characteristics;
import android.net.wifi.aware.DiscoverySession;
import android.net.wifi.aware.DiscoverySessionCallback;
import android.net.wifi.aware.IdentityChangedListener;
import android.net.wifi.aware.ParcelablePeerHandle;
import android.net.wifi.aware.PeerHandle;
import android.net.wifi.aware.PublishConfig;
import android.net.wifi.aware.PublishDiscoverySession;
import android.net.wifi.aware.SubscribeConfig;
import android.net.wifi.aware.SubscribeDiscoverySession;
import android.net.wifi.aware.WifiAwareManager;
import android.net.wifi.aware.WifiAwareNetworkSpecifier;
import android.net.wifi.aware.WifiAwareSession;
import android.net.wifi.cts.WifiJUnit3TestBase;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Parcel;
import android.platform.test.annotations.AppModeFull;
import android.test.AndroidTestCase;

import com.android.compatibility.common.util.SystemUtil;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Wi-Fi Aware CTS test suite: single device testing. Performs tests on a single
 * device to validate Wi-Fi Aware.
 */
@AppModeFull(reason = "Cannot get WifiAwareManager in instant app mode")
public class SingleDeviceTest extends WifiJUnit3TestBase {
    private static final String TAG = "WifiAwareCtsTests";

    // wait for Wi-Fi Aware state changes & network requests callbacks
    private static final int WAIT_FOR_AWARE_CHANGE_SECS = 15; // 15 seconds
    private static final int WAIT_FOR_NETWORK_STATE_CHANGE_SECS = 25; // 25 seconds
    private static final int INTERVAL_BETWEEN_TESTS_SECS = 3; // 3 seconds
    private static final int WAIT_FOR_AWARE_INTERFACE_CREATION_SEC = 3; // 3 seconds
    private static final int MIN_DISTANCE_MM = 1 * 1000;
    private static final int MAX_DISTANCE_MM = 3 * 1000;
    private static final byte[] PMK_VALID = "01234567890123456789012345678901".getBytes();

    private final Object mLock = new Object();
    private final HandlerThread mHandlerThread = new HandlerThread("SingleDeviceTest");
    private final Handler mHandler;
    {
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
    }

    private WifiAwareManager mWifiAwareManager;
    private WifiManager mWifiManager;
    private WifiManager.WifiLock mWifiLock;
    private ConnectivityManager mConnectivityManager;

    // used to store any WifiAwareSession allocated during tests - will clean-up after tests
    private List<WifiAwareSession> mSessions = new ArrayList<>();

    private class WifiAwareBroadcastReceiver extends BroadcastReceiver {
        private CountDownLatch mBlocker = new CountDownLatch(1);

        @Override
        public void onReceive(Context context, Intent intent) {
            if (WifiAwareManager.ACTION_WIFI_AWARE_STATE_CHANGED.equals(intent.getAction())) {
                mBlocker.countDown();
            }
        }

        boolean waitForStateChange() throws InterruptedException {
            return mBlocker.await(WAIT_FOR_AWARE_CHANGE_SECS, TimeUnit.SECONDS);
        }
    }

    private class AttachCallbackTest extends AttachCallback {
        static final int ATTACHED = 0;
        static final int ATTACH_FAILED = 1;
        static final int ERROR = 2; // no callback: timeout, interruption

        private CountDownLatch mBlocker = new CountDownLatch(1);
        private int mCallbackCalled = ERROR; // garbage init
        private WifiAwareSession mSession = null;

        @Override
        public void onAttached(WifiAwareSession session) {
            mCallbackCalled = ATTACHED;
            mSession = session;
            synchronized (mLock) {
                mSessions.add(session);
            }
            mBlocker.countDown();
        }

        @Override
        public void onAttachFailed() {
            mCallbackCalled = ATTACH_FAILED;
            mBlocker.countDown();
        }

        /**
         * Waits for any of the callbacks to be called - or an error (timeout, interruption).
         * Returns one of the ATTACHED, ATTACH_FAILED, or ERROR values.
         */
        int waitForAnyCallback() {
            try {
                boolean noTimeout = mBlocker.await(WAIT_FOR_AWARE_CHANGE_SECS, TimeUnit.SECONDS);
                if (noTimeout) {
                    return mCallbackCalled;
                } else {
                    return ERROR;
                }
            } catch (InterruptedException e) {
                return ERROR;
            }
        }

        /**
         * Access the session created by a callback. Only useful to be called after calling
         * waitForAnyCallback() and getting the ATTACHED code back.
         */
        WifiAwareSession getSession() {
            return mSession;
        }
    }

    private class IdentityChangedListenerTest extends IdentityChangedListener {
        private CountDownLatch mBlocker = new CountDownLatch(1);
        private byte[] mMac = null;

        @Override
        public void onIdentityChanged(byte[] mac) {
            mMac = mac;
            mBlocker.countDown();
        }

        /**
         * Waits for the listener callback to be called - or an error (timeout, interruption).
         * Returns true on callback called, false on error (timeout, interruption).
         */
        boolean waitForListener() {
            try {
                return mBlocker.await(WAIT_FOR_AWARE_CHANGE_SECS, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                return false;
            }
        }

        /**
         * Returns the MAC address of the discovery interface supplied to the triggered callback.
         */
        byte[] getMac() {
            return mMac;
        }
    }

    private class DiscoverySessionCallbackTest extends DiscoverySessionCallback {
        static final int ON_PUBLISH_STARTED = 0;
        static final int ON_SUBSCRIBE_STARTED = 1;
        static final int ON_SESSION_CONFIG_UPDATED = 2;
        static final int ON_SESSION_CONFIG_FAILED = 3;
        static final int ON_SESSION_TERMINATED = 4;
        static final int ON_SERVICE_DISCOVERED = 5;
        static final int ON_MESSAGE_SEND_SUCCEEDED = 6;
        static final int ON_MESSAGE_SEND_FAILED = 7;
        static final int ON_MESSAGE_RECEIVED = 8;

        private final Object mLocalLock = new Object();

        private CountDownLatch mBlocker;
        private int mCurrentWaitForCallback;
        private ArrayDeque<Integer> mCallbackQueue = new ArrayDeque<>();

        private PublishDiscoverySession mPublishDiscoverySession;
        private SubscribeDiscoverySession mSubscribeDiscoverySession;

        private void processCallback(int callback) {
            synchronized (mLocalLock) {
                if (mBlocker != null && mCurrentWaitForCallback == callback) {
                    mBlocker.countDown();
                } else {
                    mCallbackQueue.addLast(callback);
                }
            }
        }

        @Override
        public void onPublishStarted(PublishDiscoverySession session) {
            mPublishDiscoverySession = session;
            processCallback(ON_PUBLISH_STARTED);
        }

        @Override
        public void onSubscribeStarted(SubscribeDiscoverySession session) {
            mSubscribeDiscoverySession = session;
            processCallback(ON_SUBSCRIBE_STARTED);
        }

        @Override
        public void onSessionConfigUpdated() {
            processCallback(ON_SESSION_CONFIG_UPDATED);
        }

        @Override
        public void onSessionConfigFailed() {
            processCallback(ON_SESSION_CONFIG_FAILED);
        }

        @Override
        public void onSessionTerminated() {
            processCallback(ON_SESSION_TERMINATED);
        }

        @Override
        public void onServiceDiscovered(PeerHandle peerHandle, byte[] serviceSpecificInfo,
                List<byte[]> matchFilter) {
            processCallback(ON_SERVICE_DISCOVERED);
        }

        @Override
        public void onMessageSendSucceeded(int messageId) {
            processCallback(ON_MESSAGE_SEND_SUCCEEDED);
        }

        @Override
        public void onMessageSendFailed(int messageId) {
            processCallback(ON_MESSAGE_SEND_FAILED);
        }

        @Override
        public void onMessageReceived(PeerHandle peerHandle, byte[] message) {
            processCallback(ON_MESSAGE_RECEIVED);
        }

        /**
         * Wait for the specified callback - any of the ON_* constants. Returns a true
         * on success (specified callback triggered) or false on failure (timed-out or
         * interrupted while waiting for the requested callback).
         *
         * Note: other callbacks happening while while waiting for the specified callback will
         * be queued.
         */
        boolean waitForCallback(int callback) {
            return waitForCallback(callback, WAIT_FOR_AWARE_CHANGE_SECS);
        }

        /**
         * Wait for the specified callback - any of the ON_* constants. Returns a true
         * on success (specified callback triggered) or false on failure (timed-out or
         * interrupted while waiting for the requested callback).
         *
         * Same as waitForCallback(int callback) execpt that allows specifying a custom timeout.
         * The default timeout is a short value expected to be sufficient for all behaviors which
         * should happen relatively quickly. Specifying a custom timeout should only be done for
         * those cases which are known to take a specific longer period of time.
         *
         * Note: other callbacks happening while while waiting for the specified callback will
         * be queued.
         */
        boolean waitForCallback(int callback, int timeoutSec) {
            synchronized (mLocalLock) {
                boolean found = mCallbackQueue.remove(callback);
                if (found) {
                    return true;
                }

                mCurrentWaitForCallback = callback;
                mBlocker = new CountDownLatch(1);
            }

            try {
                return mBlocker.await(timeoutSec, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                return false;
            }
        }

        /**
         * Indicates whether the specified callback (any of the ON_* constants) has already
         * happened and in the queue. Useful when the order of events is important.
         */
        boolean hasCallbackAlreadyHappened(int callback) {
            synchronized (mLocalLock) {
                return mCallbackQueue.contains(callback);
            }
        }

        /**
         * Returns the last created publish discovery session.
         */
        PublishDiscoverySession getPublishDiscoverySession() {
            PublishDiscoverySession session = mPublishDiscoverySession;
            mPublishDiscoverySession = null;
            return session;
        }

        /**
         * Returns the last created subscribe discovery session.
         */
        SubscribeDiscoverySession getSubscribeDiscoverySession() {
            SubscribeDiscoverySession session = mSubscribeDiscoverySession;
            mSubscribeDiscoverySession = null;
            return session;
        }
    }

    private class NetworkCallbackTest extends ConnectivityManager.NetworkCallback {
        private CountDownLatch mBlocker = new CountDownLatch(1);

        @Override
        public void onUnavailable() {
            mBlocker.countDown();
        }

        /**
         * Wait for the onUnavailable() callback to be triggered. Returns true if triggered,
         * otherwise (timed-out, interrupted) returns false.
         */
        boolean waitForOnUnavailable() {
            try {
                return mBlocker.await(WAIT_FOR_NETWORK_STATE_CHANGE_SECS, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                return false;
            }
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        if (!TestUtils.shouldTestWifiAware(getContext())) {
            return;
        }

        assertTrue("Wi-Fi Aware requires Location to be Enabled",
                ((LocationManager) getContext().getSystemService(
                        Context.LOCATION_SERVICE)).isLocationEnabled());

        mWifiAwareManager = (WifiAwareManager) getContext().getSystemService(
                Context.WIFI_AWARE_SERVICE);
        assertNotNull("Wi-Fi Aware Manager", mWifiAwareManager);

        mWifiManager = (WifiManager) getContext().getSystemService(Context.WIFI_SERVICE);
        assertNotNull("Wi-Fi Manager", mWifiManager);
        mWifiLock = mWifiManager.createWifiLock(TAG);
        mWifiLock.acquire();
        if (!mWifiManager.isWifiEnabled()) {
            SystemUtil.runShellCommand("svc wifi enable");
        }

        mConnectivityManager = (ConnectivityManager) getContext().getSystemService(
                Context.CONNECTIVITY_SERVICE);
        assertNotNull("Connectivity Manager", mConnectivityManager);

        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(WifiAwareManager.ACTION_WIFI_AWARE_STATE_CHANGED);
        WifiAwareBroadcastReceiver receiver = new WifiAwareBroadcastReceiver();
        mContext.registerReceiver(receiver, intentFilter);
        if (!mWifiAwareManager.isAvailable()) {
            assertTrue("Timeout waiting for Wi-Fi Aware to change status",
                    receiver.waitForStateChange());
            assertTrue("Wi-Fi Aware is not available (should be)", mWifiAwareManager.isAvailable());
        }
    }

    @Override
    protected void tearDown() throws Exception {
        if (!TestUtils.shouldTestWifiAware(getContext())) {
            super.tearDown();
            return;
        }

        synchronized (mLock) {
            for (WifiAwareSession session : mSessions) {
                // no damage from destroying twice (i.e. ok if test cleaned up after itself already)
                session.close();
            }
            mSessions.clear();
        }

        super.tearDown();
        Thread.sleep(INTERVAL_BETWEEN_TESTS_SECS * 1000);
    }

    /**
     * Validate:
     * - Characteristics are available
     * - Characteristics values are legitimate. Not in the CDD. However, the tested values are
     *   based on the Wi-Fi Aware protocol.
     */
    public void testCharacteristics() {
        if (!TestUtils.shouldTestWifiAware(getContext())) {
            return;
        }

        Characteristics characteristics = mWifiAwareManager.getCharacteristics();
        assertNotNull("Wi-Fi Aware characteristics are null", characteristics);
        assertEquals("Service Name Length", characteristics.getMaxServiceNameLength(), 255);
        assertEquals("Service Specific Information Length",
                characteristics.getMaxServiceSpecificInfoLength(), 255);
        assertEquals("Match Filter Length", characteristics.getMaxMatchFilterLength(), 255);
        assertNotEquals("Cipher suites", characteristics.getSupportedCipherSuites(), 0);
    }

    /**
     * Validate that on Wi-Fi Aware availability change we get a broadcast + the API returns
     * correct status.
     */
    public void testAvailabilityStatusChange() throws Exception {
        if (!TestUtils.shouldTestWifiAware(getContext())) {
            return;
        }

        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(WifiAwareManager.ACTION_WIFI_AWARE_STATE_CHANGED);

        // 1. Disable Wi-Fi
        WifiAwareBroadcastReceiver receiver1 = new WifiAwareBroadcastReceiver();
        mContext.registerReceiver(receiver1, intentFilter);
        SystemUtil.runShellCommand("svc wifi disable");

        assertTrue("Timeout waiting for Wi-Fi Aware to change status",
                receiver1.waitForStateChange());
        assertFalse("Wi-Fi Aware is available (should not be)", mWifiAwareManager.isAvailable());

        // 2. Enable Wi-Fi
        WifiAwareBroadcastReceiver receiver2 = new WifiAwareBroadcastReceiver();
        mContext.registerReceiver(receiver2, intentFilter);
        SystemUtil.runShellCommand("svc wifi enable");

        assertTrue("Timeout waiting for Wi-Fi Aware to change status",
                receiver2.waitForStateChange());
        assertTrue("Wi-Fi Aware is not available (should be)", mWifiAwareManager.isAvailable());
    }

    /**
     * Validate that can attach to Wi-Fi Aware.
     */
    public void testAttachNoIdentity() {
        if (!TestUtils.shouldTestWifiAware(getContext())) {
            return;
        }

        WifiAwareSession session = attachAndGetSession();
        session.close();
    }

    /**
     * Validate that can attach to Wi-Fi Aware and get identity information. Use the identity
     * information to validate that MAC address changes on every attach.
     *
     * Note: relies on no other entity using Wi-Fi Aware during the CTS test. Since if it is used
     * then the attach/destroy will not correspond to enable/disable and will not result in a new
     * MAC address being generated.
     */
    public void testAttachDiscoveryAddressChanges() {
        if (!TestUtils.shouldTestWifiAware(getContext())) {
            return;
        }

        final int numIterations = 10;
        Set<TestUtils.MacWrapper> macs = new HashSet<>();

        for (int i = 0; i < numIterations; ++i) {
            AttachCallbackTest attachCb = new AttachCallbackTest();
            IdentityChangedListenerTest identityL = new IdentityChangedListenerTest();
            mWifiAwareManager.attach(attachCb, identityL, mHandler);
            assertEquals("Wi-Fi Aware attach: iteration " + i, AttachCallbackTest.ATTACHED,
                    attachCb.waitForAnyCallback());
            assertTrue("Wi-Fi Aware attach: iteration " + i, identityL.waitForListener());

            WifiAwareSession session = attachCb.getSession();
            assertNotNull("Wi-Fi Aware session: iteration " + i, session);

            byte[] mac = identityL.getMac();
            assertNotNull("Wi-Fi Aware discovery MAC: iteration " + i, mac);

            session.close();

            macs.add(new TestUtils.MacWrapper(mac));
        }

        assertEquals("", numIterations, macs.size());
    }

    /**
     * Validate a successful publish discovery session lifetime: publish, update publish, destroy.
     */
    public void testPublishDiscoverySuccess() {
        if (!TestUtils.shouldTestWifiAware(getContext())) {
            return;
        }

        final String serviceName = "ValidName";

        WifiAwareSession session = attachAndGetSession();

        PublishConfig publishConfig = new PublishConfig.Builder().setServiceName(
                serviceName).build();
        DiscoverySessionCallbackTest discoveryCb = new DiscoverySessionCallbackTest();

        // 1. publish
        session.publish(publishConfig, discoveryCb, mHandler);
        assertTrue("Publish started",
                discoveryCb.waitForCallback(DiscoverySessionCallbackTest.ON_PUBLISH_STARTED));
        PublishDiscoverySession discoverySession = discoveryCb.getPublishDiscoverySession();
        assertNotNull("Publish session", discoverySession);

        // 2. update-publish
        publishConfig = new PublishConfig.Builder().setServiceName(
                serviceName).setServiceSpecificInfo("extras".getBytes()).build();
        discoverySession.updatePublish(publishConfig);
        assertTrue("Publish update", discoveryCb.waitForCallback(
                DiscoverySessionCallbackTest.ON_SESSION_CONFIG_UPDATED));

        // 3. destroy
        assertFalse("Publish not terminated", discoveryCb.hasCallbackAlreadyHappened(
                DiscoverySessionCallbackTest.ON_SESSION_TERMINATED));
        discoverySession.close();

        // 4. try update post-destroy: should time-out waiting for cb
        discoverySession.updatePublish(publishConfig);
        assertFalse("Publish update post destroy", discoveryCb.waitForCallback(
                DiscoverySessionCallbackTest.ON_SESSION_CONFIG_UPDATED));

        session.close();
    }

    /**
     * Validate that publish with a Time To Live (TTL) setting expires within the specified
     * time (and validates that the terminate callback is triggered).
     */
    public void testPublishLimitedTtlSuccess() {
        if (!TestUtils.shouldTestWifiAware(getContext())) {
            return;
        }

        final String serviceName = "ValidName";
        final int ttlSec = 5;

        WifiAwareSession session = attachAndGetSession();

        PublishConfig publishConfig = new PublishConfig.Builder().setServiceName(
                serviceName).setTtlSec(ttlSec).setTerminateNotificationEnabled(true).build();
        DiscoverySessionCallbackTest discoveryCb = new DiscoverySessionCallbackTest();

        // 1. publish
        session.publish(publishConfig, discoveryCb, mHandler);
        assertTrue("Publish started",
                discoveryCb.waitForCallback(DiscoverySessionCallbackTest.ON_PUBLISH_STARTED));
        PublishDiscoverySession discoverySession = discoveryCb.getPublishDiscoverySession();
        assertNotNull("Publish session", discoverySession);

        // 2. wait for terminate within 'ttlSec'.
        assertTrue("Publish terminated",
                discoveryCb.waitForCallback(DiscoverySessionCallbackTest.ON_SESSION_TERMINATED,
                        ttlSec + 5));

        // 3. try update post-termination: should time-out waiting for cb
        publishConfig = new PublishConfig.Builder().setServiceName(
                serviceName).setServiceSpecificInfo("extras".getBytes()).build();
        discoverySession.updatePublish(publishConfig);
        assertFalse("Publish update post terminate", discoveryCb.waitForCallback(
                DiscoverySessionCallbackTest.ON_SESSION_CONFIG_UPDATED));

        session.close();
    }

    /**
     * Validate a successful subscribe discovery session lifetime: subscribe, update subscribe,
     * destroy.
     */
    public void testSubscribeDiscoverySuccess() {
        if (!TestUtils.shouldTestWifiAware(getContext())) {
            return;
        }

        final String serviceName = "ValidName";

        WifiAwareSession session = attachAndGetSession();

        SubscribeConfig subscribeConfig = new SubscribeConfig.Builder().setServiceName(
                serviceName).build();
        DiscoverySessionCallbackTest discoveryCb = new DiscoverySessionCallbackTest();

        // 1. subscribe
        session.subscribe(subscribeConfig, discoveryCb, mHandler);
        assertTrue("Subscribe started",
                discoveryCb.waitForCallback(DiscoverySessionCallbackTest.ON_SUBSCRIBE_STARTED));
        SubscribeDiscoverySession discoverySession = discoveryCb.getSubscribeDiscoverySession();
        assertNotNull("Subscribe session", discoverySession);

        // 2. update-subscribe
        subscribeConfig = new SubscribeConfig.Builder().setServiceName(
                serviceName).setServiceSpecificInfo("extras".getBytes())
                .setMinDistanceMm(MIN_DISTANCE_MM).build();
        discoverySession.updateSubscribe(subscribeConfig);
        assertTrue("Subscribe update", discoveryCb.waitForCallback(
                DiscoverySessionCallbackTest.ON_SESSION_CONFIG_UPDATED));

        // 3. destroy
        assertFalse("Subscribe not terminated", discoveryCb.hasCallbackAlreadyHappened(
                DiscoverySessionCallbackTest.ON_SESSION_TERMINATED));
        discoverySession.close();

        // 4. try update post-destroy: should time-out waiting for cb
        discoverySession.updateSubscribe(subscribeConfig);
        assertFalse("Subscribe update post destroy", discoveryCb.waitForCallback(
                DiscoverySessionCallbackTest.ON_SESSION_CONFIG_UPDATED));

        session.close();
    }

    /**
     * Validate that subscribe with a Time To Live (TTL) setting expires within the specified
     * time (and validates that the terminate callback is triggered).
     */
    public void testSubscribeLimitedTtlSuccess() {
        if (!TestUtils.shouldTestWifiAware(getContext())) {
            return;
        }

        final String serviceName = "ValidName";
        final int ttlSec = 5;

        WifiAwareSession session = attachAndGetSession();

        SubscribeConfig subscribeConfig = new SubscribeConfig.Builder().setServiceName(
                serviceName).setTtlSec(ttlSec).setTerminateNotificationEnabled(true).build();
        DiscoverySessionCallbackTest discoveryCb = new DiscoverySessionCallbackTest();

        // 1. subscribe
        session.subscribe(subscribeConfig, discoveryCb, mHandler);
        assertTrue("Subscribe started",
                discoveryCb.waitForCallback(DiscoverySessionCallbackTest.ON_SUBSCRIBE_STARTED));
        SubscribeDiscoverySession discoverySession = discoveryCb.getSubscribeDiscoverySession();
        assertNotNull("Subscribe session", discoverySession);

        // 2. wait for terminate within 'ttlSec'.
        assertTrue("Subscribe terminated",
                discoveryCb.waitForCallback(DiscoverySessionCallbackTest.ON_SESSION_TERMINATED,
                        ttlSec + 5));

        // 3. try update post-termination: should time-out waiting for cb
        subscribeConfig = new SubscribeConfig.Builder().setServiceName(
                serviceName).setServiceSpecificInfo("extras".getBytes()).build();
        discoverySession.updateSubscribe(subscribeConfig);
        assertFalse("Subscribe update post terminate", discoveryCb.waitForCallback(
                DiscoverySessionCallbackTest.ON_SESSION_CONFIG_UPDATED));

        session.close();
    }

    /**
     * Test the send message flow. Since testing single device cannot send to a real peer -
     * validate that sending to a bogus peer fails.
     */
    public void testSendMessageFail() {
        if (!TestUtils.shouldTestWifiAware(getContext())) {
            return;
        }

        WifiAwareSession session = attachAndGetSession();

        PublishConfig publishConfig = new PublishConfig.Builder().setServiceName(
                "ValidName").build();
        DiscoverySessionCallbackTest discoveryCb = new DiscoverySessionCallbackTest();

        // 1. publish
        session.publish(publishConfig, discoveryCb, mHandler);
        assertTrue("Publish started",
                discoveryCb.waitForCallback(DiscoverySessionCallbackTest.ON_PUBLISH_STARTED));
        PublishDiscoverySession discoverySession = discoveryCb.getPublishDiscoverySession();
        assertNotNull("Publish session", discoverySession);

        // 2. send a message with a null peer-handle - expect exception
        try {
            discoverySession.sendMessage(null, -1290, "some message".getBytes());
            fail("Expected IllegalArgumentException");
        } catch (IllegalArgumentException e) {
            // empty
        }

        discoverySession.close();
        session.close();
    }

    /**
     * Request an Aware data-path (open) as a Responder with an arbitrary peer MAC address. Validate
     * that receive an onUnavailable() callback.
     */
    public void testDataPathOpenOutOfBandFail() throws InterruptedException {
        if (!TestUtils.shouldTestWifiAware(getContext())) {
            return;
        }
        MacAddress mac = MacAddress.fromString("00:01:02:03:04:05");

        // 1. initialize Aware: only purpose is to make sure it is available for OOB data-path
        WifiAwareSession session = attachAndGetSession();

        PublishConfig publishConfig = new PublishConfig.Builder().setServiceName(
                "ValidName").build();
        DiscoverySessionCallbackTest discoveryCb = new DiscoverySessionCallbackTest();
        session.publish(publishConfig, discoveryCb, mHandler);
        assertTrue("Publish started",
                discoveryCb.waitForCallback(DiscoverySessionCallbackTest.ON_PUBLISH_STARTED));
        Thread.sleep(WAIT_FOR_AWARE_INTERFACE_CREATION_SEC * 1000);

        // 2. request an AWARE network
        NetworkCallbackTest networkCb = new NetworkCallbackTest();
        NetworkRequest nr = new NetworkRequest.Builder().addTransportType(
                NetworkCapabilities.TRANSPORT_WIFI_AWARE).setNetworkSpecifier(
                session.createNetworkSpecifierOpen(
                        WifiAwareManager.WIFI_AWARE_DATA_PATH_ROLE_INITIATOR,
                        mac.toByteArray())).build();
        mConnectivityManager.requestNetwork(nr, networkCb);
        assertTrue("OnUnavailable not received", networkCb.waitForOnUnavailable());

        session.close();
    }

    /**
     * Request an Aware data-path (encrypted with Passphrase) as a Responder with an arbitrary peer
     * MAC address.
     * Validate that receive an onUnavailable() callback.
     */
    public void testDataPathPassphraseOutOfBandFail() throws InterruptedException {
        if (!TestUtils.shouldTestWifiAware(getContext())) {
            return;
        }
        MacAddress mac = MacAddress.fromString("00:01:02:03:04:05");

        // 1. initialize Aware: only purpose is to make sure it is available for OOB data-path
        WifiAwareSession session = attachAndGetSession();

        PublishConfig publishConfig = new PublishConfig.Builder().setServiceName(
                "ValidName").build();
        DiscoverySessionCallbackTest discoveryCb = new DiscoverySessionCallbackTest();
        session.publish(publishConfig, discoveryCb, mHandler);
        assertTrue("Publish started",
                discoveryCb.waitForCallback(DiscoverySessionCallbackTest.ON_PUBLISH_STARTED));
        Thread.sleep(WAIT_FOR_AWARE_INTERFACE_CREATION_SEC * 1000);

        // 2. request an AWARE network
        NetworkCallbackTest networkCb = new NetworkCallbackTest();
        NetworkRequest nr = new NetworkRequest.Builder().addTransportType(
                NetworkCapabilities.TRANSPORT_WIFI_AWARE).setNetworkSpecifier(
                session.createNetworkSpecifierPassphrase(
                        WifiAwareManager.WIFI_AWARE_DATA_PATH_ROLE_INITIATOR, mac.toByteArray(),
                        "abcdefghihk")).build();
        mConnectivityManager.requestNetwork(nr, networkCb);
        assertTrue("OnUnavailable not received", networkCb.waitForOnUnavailable());

        session.close();
    }

    /**
     * Request an Aware data-path (encrypted with PMK) as a Responder with an arbitrary peer MAC
     * address.
     * Validate that receive an onUnavailable() callback.
     */
    public void testDataPathPmkOutOfBandFail() throws InterruptedException {
        if (!TestUtils.shouldTestWifiAware(getContext())) {
            return;
        }
        MacAddress mac = MacAddress.fromString("00:01:02:03:04:05");

        // 1. initialize Aware: only purpose is to make sure it is available for OOB data-path
        WifiAwareSession session = attachAndGetSession();

        PublishConfig publishConfig = new PublishConfig.Builder().setServiceName(
                "ValidName").build();
        DiscoverySessionCallbackTest discoveryCb = new DiscoverySessionCallbackTest();
        session.publish(publishConfig, discoveryCb, mHandler);
        assertTrue("Publish started",
                discoveryCb.waitForCallback(DiscoverySessionCallbackTest.ON_PUBLISH_STARTED));
        Thread.sleep(WAIT_FOR_AWARE_INTERFACE_CREATION_SEC * 1000);

        // 2. request an AWARE network
        NetworkCallbackTest networkCb = new NetworkCallbackTest();
        NetworkRequest nr = new NetworkRequest.Builder().addTransportType(
                NetworkCapabilities.TRANSPORT_WIFI_AWARE).setNetworkSpecifier(
                session.createNetworkSpecifierPmk(
                        WifiAwareManager.WIFI_AWARE_DATA_PATH_ROLE_INITIATOR, mac.toByteArray(),
                        PMK_VALID)).build();
        mConnectivityManager.requestNetwork(nr, networkCb);
        assertTrue("OnUnavailable not received", networkCb.waitForOnUnavailable());

        session.close();
    }

    /**
     * Test WifiAwareNetworkSpecifier.
     */
    public void testWifiAwareNetworkSpecifier() {
        DiscoverySession session = mock(DiscoverySession.class);
        PeerHandle handle = mock(PeerHandle.class);
        WifiAwareNetworkSpecifier networkSpecifier =
                new WifiAwareNetworkSpecifier.Builder(session, handle).build();
        assertFalse(networkSpecifier.canBeSatisfiedBy(null));
        assertTrue(networkSpecifier.canBeSatisfiedBy(networkSpecifier));

        WifiAwareNetworkSpecifier anotherNetworkSpecifier =
                new WifiAwareNetworkSpecifier.Builder(session, handle).setPmk(PMK_VALID).build();
        assertFalse(networkSpecifier.canBeSatisfiedBy(anotherNetworkSpecifier));
    }

    /**
     * Test ParcelablePeerHandle parcel.
     */
    public void testParcelablePeerHandle() {
        PeerHandle peerHandle = mock(PeerHandle.class);
        ParcelablePeerHandle parcelablePeerHandle = new ParcelablePeerHandle(peerHandle);
        Parcel parcelW = Parcel.obtain();
        parcelablePeerHandle.writeToParcel(parcelW, 0);
        byte[] bytes = parcelW.marshall();
        parcelW.recycle();

        Parcel parcelR = Parcel.obtain();
        parcelR.unmarshall(bytes, 0, bytes.length);
        parcelR.setDataPosition(0);
        ParcelablePeerHandle rereadParcelablePeerHandle =
                ParcelablePeerHandle.CREATOR.createFromParcel(parcelR);

        assertEquals(parcelablePeerHandle, rereadParcelablePeerHandle);
        assertEquals(parcelablePeerHandle.hashCode(), rereadParcelablePeerHandle.hashCode());
    }

    // local utilities

    private WifiAwareSession attachAndGetSession() {
        AttachCallbackTest attachCb = new AttachCallbackTest();
        mWifiAwareManager.attach(attachCb, mHandler);
        int cbCalled = attachCb.waitForAnyCallback();
        assertEquals("Wi-Fi Aware attach", AttachCallbackTest.ATTACHED, cbCalled);

        WifiAwareSession session = attachCb.getSession();
        assertNotNull("Wi-Fi Aware session", session);

        return session;
    }
}
