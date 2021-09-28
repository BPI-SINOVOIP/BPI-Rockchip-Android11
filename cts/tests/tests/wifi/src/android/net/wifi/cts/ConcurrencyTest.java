/*
 * Copyright (C) 2012 The Android Open Source Project
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

package android.net.wifi.cts;

import static org.junit.Assert.assertNotEquals;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.NetworkRequest;
import android.net.wifi.WifiManager;
import android.net.wifi.p2p.WifiP2pDevice;
import android.net.wifi.p2p.WifiP2pGroup;
import android.net.wifi.p2p.WifiP2pGroupList;
import android.net.wifi.p2p.WifiP2pInfo;
import android.net.wifi.p2p.WifiP2pManager;
import android.net.wifi.p2p.nsd.WifiP2pServiceInfo;
import android.net.wifi.p2p.nsd.WifiP2pUpnpServiceInfo;
import android.provider.Settings;
import android.platform.test.annotations.AppModeFull;
import android.test.AndroidTestCase;
import android.util.Log;

import com.android.compatibility.common.util.ShellIdentityUtils;
import com.android.compatibility.common.util.SystemUtil;

import java.util.Arrays;
import java.util.ArrayList;
import java.util.BitSet;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;

@AppModeFull(reason = "Cannot get WifiManager in instant app mode")
public class ConcurrencyTest extends WifiJUnit3TestBase {
    private class MySync {
        static final int WIFI_STATE = 0;
        static final int P2P_STATE = 1;
        static final int DISCOVERY_STATE = 2;
        static final int NETWORK_INFO = 3;

        public BitSet pendingSync = new BitSet();

        public int expectedWifiState;
        public int expectedP2pState;
        public int expectedDiscoveryState;
        public NetworkInfo expectedNetworkInfo;
    }

    private class MyResponse {
        public boolean valid = false;

        public boolean success;
        public int failureReason;
        public int p2pState;
        public int discoveryState;
        public NetworkInfo networkInfo;
        public WifiP2pInfo p2pInfo;
        public String deviceName;
        public WifiP2pGroupList persistentGroups;
        public WifiP2pGroup group = new WifiP2pGroup();
    }

    private WifiManager mWifiManager;
    private WifiP2pManager mWifiP2pManager;
    private WifiP2pManager.Channel mWifiP2pChannel;
    private MySync mMySync = new MySync();
    private MyResponse mMyResponse = new MyResponse();
    private boolean mWasVerboseLoggingEnabled;

    private static final String TAG = "ConcurrencyTest";
    private static final int TIMEOUT_MSEC = 6000;
    private static final int WAIT_MSEC = 60;
    private static final int DURATION = 10000;
    private IntentFilter mIntentFilter;
    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            final String action = intent.getAction();
            if (action.equals(WifiManager.WIFI_STATE_CHANGED_ACTION)) {
                synchronized (mMySync) {
                    mMySync.pendingSync.set(MySync.WIFI_STATE);
                    mMySync.expectedWifiState = intent.getIntExtra(WifiManager.EXTRA_WIFI_STATE,
                            WifiManager.WIFI_STATE_DISABLED);
                    mMySync.notify();
                }
            } else if(action.equals(WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION)) {
                synchronized (mMySync) {
                    mMySync.pendingSync.set(MySync.P2P_STATE);
                    mMySync.expectedP2pState = intent.getIntExtra(WifiP2pManager.EXTRA_WIFI_STATE,
                            WifiP2pManager.WIFI_P2P_STATE_DISABLED);
                    mMySync.notify();
                }
            } else if (action.equals(WifiP2pManager.WIFI_P2P_DISCOVERY_CHANGED_ACTION)) {
                synchronized (mMySync) {
                    mMySync.pendingSync.set(MySync.DISCOVERY_STATE);
                    mMySync.expectedDiscoveryState = intent.getIntExtra(
                            WifiP2pManager.EXTRA_DISCOVERY_STATE,
                            WifiP2pManager.WIFI_P2P_DISCOVERY_STOPPED);
                    mMySync.notify();
                }
            } else if (action.equals(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION)) {
                synchronized (mMySync) {
                    mMySync.pendingSync.set(MySync.NETWORK_INFO);
                    mMySync.expectedNetworkInfo = (NetworkInfo) intent.getExtra(
                            WifiP2pManager.EXTRA_NETWORK_INFO, null);
                    Log.d(TAG, "Get WIFI_P2P_CONNECTION_CHANGED_ACTION: "
                            + mMySync.expectedNetworkInfo);
                    mMySync.notify();
                }
            }
        }
    };

    private WifiP2pManager.ActionListener mActionListener = new WifiP2pManager.ActionListener() {
        @Override
        public void onSuccess() {
            synchronized (mMyResponse) {
                mMyResponse.valid = true;
                mMyResponse.success = true;
                mMyResponse.notify();
            }
        }

        @Override
        public void onFailure(int reason) {
            synchronized (mMyResponse) {
                Log.d(TAG, "failure reason: " + reason);
                mMyResponse.valid = true;
                mMyResponse.success = false;
                mMyResponse.failureReason = reason;
                mMyResponse.notify();
            }
        }
    };

    @Override
    protected void setUp() throws Exception {
       super.setUp();
       if (!WifiFeature.isWifiSupported(getContext()) &&
                !WifiFeature.isP2pSupported(getContext())) {
            // skip the test if WiFi && p2p are not supported
            return;
        }

        mIntentFilter = new IntentFilter();
        mIntentFilter.addAction(WifiManager.WIFI_STATE_CHANGED_ACTION);
        mIntentFilter.addAction(WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION);
        mIntentFilter.addAction(WifiP2pManager.WIFI_P2P_DISCOVERY_CHANGED_ACTION);
        mIntentFilter.addAction(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION);

        mContext.registerReceiver(mReceiver, mIntentFilter);
        mWifiManager = (WifiManager) getContext().getSystemService(Context.WIFI_SERVICE);
        assertNotNull(mWifiManager);
        if (mWifiManager.isWifiEnabled()) {
            SystemUtil.runShellCommand("svc wifi disable");
            Thread.sleep(DURATION);
        }

        // turn on verbose logging for tests
        mWasVerboseLoggingEnabled = ShellIdentityUtils.invokeWithShellPermissions(
                () -> mWifiManager.isVerboseLoggingEnabled());
        ShellIdentityUtils.invokeWithShellPermissions(
                () -> mWifiManager.setVerboseLoggingEnabled(true));

        assertTrue(!mWifiManager.isWifiEnabled());
        mMySync.expectedWifiState = WifiManager.WIFI_STATE_DISABLED;
        mMySync.expectedP2pState = WifiP2pManager.WIFI_P2P_STATE_DISABLED;
        mMySync.expectedDiscoveryState = WifiP2pManager.WIFI_P2P_DISCOVERY_STOPPED;
        mMySync.expectedNetworkInfo = null;
    }

    @Override
    protected void tearDown() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext()) &&
                !WifiFeature.isP2pSupported(getContext())) {
            // skip the test if WiFi and p2p are not supported
            super.tearDown();
            return;
        }
        mContext.unregisterReceiver(mReceiver);

        ShellIdentityUtils.invokeWithShellPermissions(
                () -> mWifiManager.setVerboseLoggingEnabled(mWasVerboseLoggingEnabled));

        enableWifi();
        super.tearDown();
    }

    private boolean waitForBroadcasts(List<Integer> waitSyncList) {
        synchronized (mMySync) {
            long timeout = System.currentTimeMillis() + TIMEOUT_MSEC;
            while (System.currentTimeMillis() < timeout) {
                List<Integer> handledSyncList = waitSyncList.stream()
                        .filter(w -> mMySync.pendingSync.get(w))
                        .collect(Collectors.toList());
                handledSyncList.forEach(w -> mMySync.pendingSync.clear(w));
                waitSyncList.removeAll(handledSyncList);
                if (waitSyncList.isEmpty()) {
                    break;
                }
                try {
                    mMySync.wait(WAIT_MSEC);
                } catch (InterruptedException e) { }
            }
            if (!waitSyncList.isEmpty()) {
                Log.i(TAG, "Missing broadcast: " + waitSyncList);
            }
            return waitSyncList.isEmpty();
        }
    }

    private boolean waitForBroadcasts(int waitSingleSync) {
        return waitForBroadcasts(
                new LinkedList<Integer>(Arrays.asList(waitSingleSync)));
    }

    private boolean waitForServiceResponse(MyResponse waitResponse) {
        synchronized (waitResponse) {
            long timeout = System.currentTimeMillis() + TIMEOUT_MSEC;
            while (System.currentTimeMillis() < timeout) {
                try {
                    waitResponse.wait(WAIT_MSEC);
                } catch (InterruptedException e) { }

                if (waitResponse.valid) {
                    return true;
                }
            }
            return false;
        }
    }

    // Return true if location is enabled.
    private boolean isLocationEnabled() {
        return Settings.Secure.getInt(getContext().getContentResolver(),
                Settings.Secure.LOCATION_MODE, Settings.Secure.LOCATION_MODE_OFF)
                != Settings.Secure.LOCATION_MODE_OFF;
    }

    // Returns true if the device has location feature.
    private boolean hasLocationFeature() {
        return getContext().getPackageManager().hasSystemFeature(PackageManager.FEATURE_LOCATION);
    }

    private void resetResponse(MyResponse responseObj) {
        synchronized (responseObj) {
            responseObj.valid = false;
            responseObj.networkInfo = null;
            responseObj.p2pInfo = null;
            responseObj.deviceName = null;
            responseObj.persistentGroups = null;
            responseObj.group = null;
        }
    }

    /*
     * Enables Wifi and block until connection is established.
     */
    private void enableWifi() throws InterruptedException {
        if (!mWifiManager.isWifiEnabled()) {
            SystemUtil.runShellCommand("svc wifi enable");
        }

        ConnectivityManager cm =
            (ConnectivityManager) getContext().getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkRequest request =
            new NetworkRequest.Builder().addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                                        .addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                                        .build();
        final CountDownLatch latch = new CountDownLatch(1);
        NetworkCallback networkCallback = new NetworkCallback() {
            @Override
            public void onAvailable(Network network) {
                latch.countDown();
            }
        };
        cm.registerNetworkCallback(request, networkCallback);
        latch.await(DURATION, TimeUnit.MILLISECONDS);

        cm.unregisterNetworkCallback(networkCallback);
    }

    private boolean setupWifiP2p() {
        // Cannot support p2p alone
        if (!WifiFeature.isWifiSupported(getContext())) {
            assertTrue(!WifiFeature.isP2pSupported(getContext()));
            return false;
        }

        if (!WifiFeature.isP2pSupported(getContext())) {
            // skip the test if p2p is not supported
            return false;
        }

        if (!hasLocationFeature()) {
            Log.d(TAG, "Skipping test as location is not supported");
            return false;
        }
        if (!isLocationEnabled()) {
            fail("Please enable location for this test - since P-release WiFi Direct"
                    + " needs Location enabled.");
        }

        mWifiP2pManager =
                (WifiP2pManager) getContext().getSystemService(Context.WIFI_P2P_SERVICE);
        mWifiP2pChannel = mWifiP2pManager.initialize(
                getContext(), getContext().getMainLooper(), null);

        assertNotNull(mWifiP2pManager);
        assertNotNull(mWifiP2pChannel);

        long timeout = System.currentTimeMillis() + TIMEOUT_MSEC;
        while (!mWifiManager.isWifiEnabled() && System.currentTimeMillis() < timeout) {
            try {
                enableWifi();
            } catch (InterruptedException e) { }
        }

        assertTrue(mWifiManager.isWifiEnabled());

        assertTrue(waitForBroadcasts(
                new LinkedList<Integer>(
                Arrays.asList(MySync.WIFI_STATE, MySync.P2P_STATE))));

        assertEquals(WifiManager.WIFI_STATE_ENABLED, mMySync.expectedWifiState);
        assertEquals(WifiP2pManager.WIFI_P2P_STATE_ENABLED, mMySync.expectedP2pState);

        assertTrue(waitForBroadcasts(MySync.NETWORK_INFO));
        // wait for changing to EnabledState
        assertNotNull(mMySync.expectedNetworkInfo);

        return true;
    }

    public void testConcurrency() {
        if (!setupWifiP2p()) {
            return;
        }

        resetResponse(mMyResponse);
        mWifiP2pManager.requestP2pState(mWifiP2pChannel, new WifiP2pManager.P2pStateListener() {
            @Override
            public void onP2pStateAvailable(int state) {
                synchronized (mMyResponse) {
                    mMyResponse.valid = true;
                    mMyResponse.p2pState = state;
                    mMyResponse.notify();
                }
            }
        });
        assertTrue(waitForServiceResponse(mMyResponse));
        assertEquals(WifiP2pManager.WIFI_P2P_STATE_ENABLED, mMyResponse.p2pState);
    }

    public void testRequestDiscoveryState() {
        if (!setupWifiP2p()) {
            return;
        }

        resetResponse(mMyResponse);
        mWifiP2pManager.requestDiscoveryState(
                mWifiP2pChannel, new WifiP2pManager.DiscoveryStateListener() {
                    @Override
                    public void onDiscoveryStateAvailable(int state) {
                        synchronized (mMyResponse) {
                            mMyResponse.valid = true;
                            mMyResponse.discoveryState = state;
                            mMyResponse.notify();
                        }
                    }
                });
        assertTrue(waitForServiceResponse(mMyResponse));
        assertEquals(WifiP2pManager.WIFI_P2P_DISCOVERY_STOPPED, mMyResponse.discoveryState);

        // If there is any saved network and this device is connecting to this saved network,
        // p2p discovery might be blocked during DHCP provision.
        int retryCount = 3;
        while (retryCount > 0) {
            resetResponse(mMyResponse);
            mWifiP2pManager.discoverPeers(mWifiP2pChannel, mActionListener);
            assertTrue(waitForServiceResponse(mMyResponse));
            if (mMyResponse.success
                    || mMyResponse.failureReason != WifiP2pManager.BUSY) {
                break;
            }
            Log.w(TAG, "Discovery is blocked, try again!");
            try {
                Thread.sleep(500);
            } catch (InterruptedException ex) {}
            retryCount--;
        }
        assertTrue(mMyResponse.success);
        assertTrue(waitForBroadcasts(MySync.DISCOVERY_STATE));

        resetResponse(mMyResponse);
        mWifiP2pManager.requestDiscoveryState(mWifiP2pChannel,
                new WifiP2pManager.DiscoveryStateListener() {
                    @Override
                    public void onDiscoveryStateAvailable(int state) {
                        synchronized (mMyResponse) {
                            mMyResponse.valid = true;
                            mMyResponse.discoveryState = state;
                            mMyResponse.notify();
                        }
                    }
                });
        assertTrue(waitForServiceResponse(mMyResponse));
        assertEquals(WifiP2pManager.WIFI_P2P_DISCOVERY_STARTED, mMyResponse.discoveryState);

        mWifiP2pManager.stopPeerDiscovery(mWifiP2pChannel, null);
    }

    public void testRequestNetworkInfo() {
        if (!setupWifiP2p()) {
            return;
        }

        resetResponse(mMyResponse);
        mWifiP2pManager.requestNetworkInfo(mWifiP2pChannel,
                new WifiP2pManager.NetworkInfoListener() {
                    @Override
                    public void onNetworkInfoAvailable(NetworkInfo info) {
                        synchronized (mMyResponse) {
                            mMyResponse.valid = true;
                            mMyResponse.networkInfo = info;
                            mMyResponse.notify();
                        }
                    }
                });
        assertTrue(waitForServiceResponse(mMyResponse));
        assertNotNull(mMyResponse.networkInfo);
        // The state might be IDLE, DISCONNECTED, FAILED before a connection establishment.
        // Just ensure the state is NOT CONNECTED.
        assertNotEquals(NetworkInfo.DetailedState.CONNECTED,
                mMySync.expectedNetworkInfo.getDetailedState());

        resetResponse(mMyResponse);
        mWifiP2pManager.createGroup(mWifiP2pChannel, mActionListener);
        assertTrue(waitForServiceResponse(mMyResponse));
        assertTrue(mMyResponse.success);
        assertTrue(waitForBroadcasts(MySync.NETWORK_INFO));
        assertNotNull(mMySync.expectedNetworkInfo);
        assertEquals(NetworkInfo.DetailedState.CONNECTED,
                mMySync.expectedNetworkInfo.getDetailedState());

        resetResponse(mMyResponse);
        mWifiP2pManager.requestNetworkInfo(mWifiP2pChannel,
                new WifiP2pManager.NetworkInfoListener() {
                    @Override
                    public void onNetworkInfoAvailable(NetworkInfo info) {
                        synchronized (mMyResponse) {
                            mMyResponse.valid = true;
                            mMyResponse.networkInfo = info;
                            mMyResponse.notify();
                        }
                    }
                });
        assertTrue(waitForServiceResponse(mMyResponse));
        assertNotNull(mMyResponse.networkInfo);
        assertEquals(NetworkInfo.DetailedState.CONNECTED,
                mMyResponse.networkInfo.getDetailedState());

        resetResponse(mMyResponse);
        mWifiP2pManager.requestConnectionInfo(mWifiP2pChannel,
                new WifiP2pManager.ConnectionInfoListener() {
                    @Override
                    public void onConnectionInfoAvailable(WifiP2pInfo info) {
                        synchronized (mMyResponse) {
                            mMyResponse.valid = true;
                            mMyResponse.p2pInfo = new WifiP2pInfo(info);
                            mMyResponse.notify();
                        }
                    }
                });
        assertTrue(waitForServiceResponse(mMyResponse));
        assertNotNull(mMyResponse.p2pInfo);
        assertTrue(mMyResponse.p2pInfo.groupFormed);
        assertTrue(mMyResponse.p2pInfo.isGroupOwner);

        resetResponse(mMyResponse);
        mWifiP2pManager.requestGroupInfo(mWifiP2pChannel,
                new WifiP2pManager.GroupInfoListener() {
                    @Override
                    public void onGroupInfoAvailable(WifiP2pGroup group) {
                        synchronized (mMyResponse) {
                            mMyResponse.group = new WifiP2pGroup(group);
                            mMyResponse.valid = true;
                            mMyResponse.notify();
                        }
                    }
                });
        assertTrue(waitForServiceResponse(mMyResponse));
        assertNotNull(mMyResponse.group);
        assertNotEquals(0, mMyResponse.group.getFrequency());
        assertTrue(mMyResponse.group.getNetworkId() >= 0);

        resetResponse(mMyResponse);
        mWifiP2pManager.removeGroup(mWifiP2pChannel, mActionListener);
        assertTrue(waitForServiceResponse(mMyResponse));
        assertTrue(mMyResponse.success);
        assertTrue(waitForBroadcasts(MySync.NETWORK_INFO));
        assertNotNull(mMySync.expectedNetworkInfo);
        assertEquals(NetworkInfo.DetailedState.DISCONNECTED,
                mMySync.expectedNetworkInfo.getDetailedState());
    }

    private String getDeviceName() {
        resetResponse(mMyResponse);
        mWifiP2pManager.requestDeviceInfo(mWifiP2pChannel,
                new WifiP2pManager.DeviceInfoListener() {
                    @Override
                    public void onDeviceInfoAvailable(WifiP2pDevice wifiP2pDevice) {
                        synchronized (mMyResponse) {
                            mMyResponse.deviceName = wifiP2pDevice.deviceName;
                            mMyResponse.valid = true;
                            mMyResponse.notify();
                        }
                    }
                });
        assertTrue(waitForServiceResponse(mMyResponse));
        return mMyResponse.deviceName;
    }

    public void testSetDeviceName() {
        if (!setupWifiP2p()) {
            return;
        }

        String testDeviceName = "test";
        String originalDeviceName = getDeviceName();
        assertNotNull(originalDeviceName);

        ShellIdentityUtils.invokeWithShellPermissions(() -> {
            mWifiP2pManager.setDeviceName(
                    mWifiP2pChannel, testDeviceName, mActionListener);
            assertTrue(waitForServiceResponse(mMyResponse));
            assertTrue(mMyResponse.success);
        });

        String currentDeviceName = getDeviceName();
        assertEquals(testDeviceName, currentDeviceName);

        // restore the device name at the end
        ShellIdentityUtils.invokeWithShellPermissions(() -> {
            mWifiP2pManager.setDeviceName(
                    mWifiP2pChannel, originalDeviceName, mActionListener);
            assertTrue(waitForServiceResponse(mMyResponse));
            assertTrue(mMyResponse.success);
        });
    }

    private WifiP2pGroupList getPersistentGroups() {
        resetResponse(mMyResponse);
        ShellIdentityUtils.invokeWithShellPermissions(() -> {
            mWifiP2pManager.requestPersistentGroupInfo(mWifiP2pChannel,
                    new WifiP2pManager.PersistentGroupInfoListener() {
                        @Override
                        public void onPersistentGroupInfoAvailable(WifiP2pGroupList groups) {
                            synchronized (mMyResponse) {
                                mMyResponse.persistentGroups = groups;
                                mMyResponse.valid = true;
                                mMyResponse.notify();
                            }
                        }
                    });
            assertTrue(waitForServiceResponse(mMyResponse));
        });
        return mMyResponse.persistentGroups;
    }

    public void testPersistentGroupOperation() {
        if (!setupWifiP2p()) {
            return;
        }

        resetResponse(mMyResponse);
        mWifiP2pManager.createGroup(mWifiP2pChannel, mActionListener);
        assertTrue(waitForServiceResponse(mMyResponse));
        assertTrue(mMyResponse.success);
        assertTrue(waitForBroadcasts(MySync.NETWORK_INFO));
        assertNotNull(mMySync.expectedNetworkInfo);
        assertEquals(NetworkInfo.DetailedState.CONNECTED,
                mMySync.expectedNetworkInfo.getDetailedState());

        resetResponse(mMyResponse);
        mWifiP2pManager.removeGroup(mWifiP2pChannel, mActionListener);
        assertTrue(waitForServiceResponse(mMyResponse));
        assertTrue(mMyResponse.success);
        assertTrue(waitForBroadcasts(MySync.NETWORK_INFO));
        assertNotNull(mMySync.expectedNetworkInfo);
        assertEquals(NetworkInfo.DetailedState.DISCONNECTED,
                mMySync.expectedNetworkInfo.getDetailedState());

        WifiP2pGroupList persistentGroups = getPersistentGroups();
        assertNotNull(persistentGroups);
        assertEquals(1, persistentGroups.getGroupList().size());

        resetResponse(mMyResponse);
        final int firstNetworkId = persistentGroups.getGroupList().get(0).getNetworkId();
        ShellIdentityUtils.invokeWithShellPermissions(() -> {
            mWifiP2pManager.deletePersistentGroup(mWifiP2pChannel,
                    firstNetworkId,
                    mActionListener);
            assertTrue(waitForServiceResponse(mMyResponse));
            assertTrue(mMyResponse.success);
        });

        persistentGroups = getPersistentGroups();
        assertNotNull(persistentGroups);
        assertEquals(0, persistentGroups.getGroupList().size());

        resetResponse(mMyResponse);
        mWifiP2pManager.createGroup(mWifiP2pChannel, mActionListener);
        assertTrue(waitForServiceResponse(mMyResponse));
        assertTrue(mMyResponse.success);
        assertTrue(waitForBroadcasts(MySync.NETWORK_INFO));
        assertNotNull(mMySync.expectedNetworkInfo);
        assertEquals(NetworkInfo.DetailedState.CONNECTED,
                mMySync.expectedNetworkInfo.getDetailedState());

        resetResponse(mMyResponse);
        mWifiP2pManager.removeGroup(mWifiP2pChannel, mActionListener);
        assertTrue(waitForServiceResponse(mMyResponse));
        assertTrue(mMyResponse.success);
        assertTrue(waitForBroadcasts(MySync.NETWORK_INFO));
        assertNotNull(mMySync.expectedNetworkInfo);
        assertEquals(NetworkInfo.DetailedState.DISCONNECTED,
                mMySync.expectedNetworkInfo.getDetailedState());

        resetResponse(mMyResponse);
        ShellIdentityUtils.invokeWithShellPermissions(() -> {
            mWifiP2pManager.factoryReset(mWifiP2pChannel, mActionListener);
            assertTrue(waitForServiceResponse(mMyResponse));
            assertTrue(mMyResponse.success);
        });

        persistentGroups = getPersistentGroups();
        assertNotNull(persistentGroups);
        assertEquals(0, persistentGroups.getGroupList().size());
    }

    public void testP2pListening() {
        if (!setupWifiP2p()) {
            return;
        }

        resetResponse(mMyResponse);
        ShellIdentityUtils.invokeWithShellPermissions(() -> {
            mWifiP2pManager.setWifiP2pChannels(mWifiP2pChannel, 6, 11, mActionListener);
            assertTrue(waitForServiceResponse(mMyResponse));
            assertTrue(mMyResponse.success);
        });

        resetResponse(mMyResponse);
        ShellIdentityUtils.invokeWithShellPermissions(() -> {
            mWifiP2pManager.startListening(mWifiP2pChannel, mActionListener);
            assertTrue(waitForServiceResponse(mMyResponse));
            assertTrue(mMyResponse.success);
        });

        resetResponse(mMyResponse);
        ShellIdentityUtils.invokeWithShellPermissions(() -> {
            mWifiP2pManager.stopListening(mWifiP2pChannel, mActionListener);
            assertTrue(waitForServiceResponse(mMyResponse));
            assertTrue(mMyResponse.success);
        });
    }

    public void testP2pService() {
        if (!setupWifiP2p()) {
            return;
        }

        // This only store the listener to the WifiP2pManager internal variable, nothing to fail.
        mWifiP2pManager.setServiceResponseListener(mWifiP2pChannel,
                new WifiP2pManager.ServiceResponseListener() {
                    @Override
                    public void onServiceAvailable(
                            int protocolType, byte[] responseData, WifiP2pDevice srcDevice) {
                    }
                });

        resetResponse(mMyResponse);
        List<String> services = new ArrayList<String>();
        services.add("urn:schemas-upnp-org:service:AVTransport:1");
        services.add("urn:schemas-upnp-org:service:ConnectionManager:1");
        WifiP2pServiceInfo rendererService = WifiP2pUpnpServiceInfo.newInstance(
                "6859dede-8574-59ab-9332-123456789011",
                "urn:schemas-upnp-org:device:MediaRenderer:1",
                services);
        mWifiP2pManager.addLocalService(mWifiP2pChannel,
                rendererService,
                mActionListener);
        assertTrue(waitForServiceResponse(mMyResponse));
        assertTrue(mMyResponse.success);

        resetResponse(mMyResponse);
        mWifiP2pManager.removeLocalService(mWifiP2pChannel,
                rendererService,
                mActionListener);
        assertTrue(waitForServiceResponse(mMyResponse));
        assertTrue(mMyResponse.success);

        resetResponse(mMyResponse);
        mWifiP2pManager.clearLocalServices(mWifiP2pChannel,
                mActionListener);
        assertTrue(waitForServiceResponse(mMyResponse));
        assertTrue(mMyResponse.success);
    }
}
