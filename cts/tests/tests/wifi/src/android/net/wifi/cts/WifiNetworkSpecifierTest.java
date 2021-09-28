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

package android.net.wifi.cts;

import static android.net.NetworkCapabilitiesProto.TRANSPORT_WIFI;
import static android.os.Process.myUid;

import static com.google.common.truth.Truth.assertThat;

import static junit.framework.TestCase.assertFalse;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.app.UiAutomation;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.MacAddress;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiEnterpriseConfig;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiManager.NetworkRequestMatchCallback;
import android.net.wifi.WifiNetworkSpecifier;
import android.os.PatternMatcher;
import android.os.WorkSource;
import android.platform.test.annotations.AppModeFull;
import android.support.test.uiautomator.UiDevice;
import android.text.TextUtils;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.PollingCheck;
import com.android.compatibility.common.util.ShellIdentityUtils;
import com.android.compatibility.common.util.SystemUtil;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;
import java.util.concurrent.Executors;

/**
 * Tests the entire connection flow using {@link WifiNetworkSpecifier} embedded in a
 * {@link NetworkRequest} & passed into {@link ConnectivityManager#requestNetwork(NetworkRequest,
 * ConnectivityManager.NetworkCallback)}.
 *
 * Assumes that all the saved networks is either open/WPA1/WPA2/WPA3 authenticated network.
 * TODO(b/150716005): Use assumeTrue for wifi support check.
 */
@AppModeFull(reason = "Cannot get WifiManager in instant app mode")
@SmallTest
@RunWith(AndroidJUnit4.class)
public class WifiNetworkSpecifierTest extends WifiJUnit4TestBase {
    private static final String TAG = "WifiNetworkSpecifierTest";

    private static boolean sWasVerboseLoggingEnabled;
    private static boolean sWasScanThrottleEnabled;

    private Context mContext;
    private WifiManager mWifiManager;
    private ConnectivityManager mConnectivityManager;
    private UiDevice mUiDevice;
    private final Object mLock = new Object();
    private final Object mUiLock = new Object();
    private WifiConfiguration mTestNetwork;
    private TestNetworkCallback mNetworkCallback;

    private static final int DURATION = 10_000;
    private static final int DURATION_UI_INTERACTION = 25_000;
    private static final int DURATION_NETWORK_CONNECTION = 60_000;
    private static final int DURATION_SCREEN_TOGGLE = 2000;
    private static final int SCAN_RETRY_CNT_TO_FIND_MATCHING_BSSID = 3;

    @BeforeClass
    public static void setUpClass() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation().getContext();
        // skip the test if WiFi is not supported
        assumeTrue(WifiFeature.isWifiSupported(context));

        WifiManager wifiManager = (WifiManager) context.getSystemService(Context.WIFI_SERVICE);
        assertNotNull(wifiManager);

        // turn on verbose logging for tests
        sWasVerboseLoggingEnabled = ShellIdentityUtils.invokeWithShellPermissions(
                () -> wifiManager.isVerboseLoggingEnabled());
        ShellIdentityUtils.invokeWithShellPermissions(
                () -> wifiManager.setVerboseLoggingEnabled(true));
        // Disable scan throttling for tests.
        sWasScanThrottleEnabled = ShellIdentityUtils.invokeWithShellPermissions(
                () -> wifiManager.isScanThrottleEnabled());
        ShellIdentityUtils.invokeWithShellPermissions(
                () -> wifiManager.setScanThrottleEnabled(false));

        // enable Wifi
        if (!wifiManager.isWifiEnabled()) setWifiEnabled(true);
        PollingCheck.check("Wifi not enabled", DURATION, () -> wifiManager.isWifiEnabled());

        // check we have >= 1 saved network
        List<WifiConfiguration> savedNetworks = ShellIdentityUtils.invokeWithShellPermissions(
                () -> wifiManager.getPrivilegedConfiguredNetworks());
        assertFalse("Need at least one saved network", savedNetworks.isEmpty());

        // Disconnect & disable auto-join on the saved network to prevent auto-connect from
        // interfering with the test.
        ShellIdentityUtils.invokeWithShellPermissions(
                () -> {
                    for (WifiConfiguration savedNetwork : savedNetworks) {
                        wifiManager.disableNetwork(savedNetwork.networkId);
                    }
                });
    }

    @AfterClass
    public static void tearDownClass() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation().getContext();
        if (!WifiFeature.isWifiSupported(context)) return;

        WifiManager wifiManager = (WifiManager) context.getSystemService(Context.WIFI_SERVICE);
        assertNotNull(wifiManager);

        if (!wifiManager.isWifiEnabled()) setWifiEnabled(true);

        // Re-enable networks.
        ShellIdentityUtils.invokeWithShellPermissions(
                () -> {
                    for (WifiConfiguration savedNetwork : wifiManager.getConfiguredNetworks()) {
                        wifiManager.enableNetwork(savedNetwork.networkId, false);
                    }
                });
        ShellIdentityUtils.invokeWithShellPermissions(
                () -> wifiManager.setScanThrottleEnabled(sWasScanThrottleEnabled));
        ShellIdentityUtils.invokeWithShellPermissions(
                () -> wifiManager.setVerboseLoggingEnabled(sWasVerboseLoggingEnabled));
    }

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getInstrumentation().getContext();
        mWifiManager = (WifiManager) mContext.getSystemService(Context.WIFI_SERVICE);
        mConnectivityManager = mContext.getSystemService(ConnectivityManager.class);
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());

        // turn screen on
        turnScreenOn();

        List<WifiConfiguration> savedNetworks = ShellIdentityUtils.invokeWithShellPermissions(
                () -> mWifiManager.getPrivilegedConfiguredNetworks());
        // Pick any one of the saved networks on the device (assumes that it is in range)
        mTestNetwork = savedNetworks.get(0);

        // Wait for Wifi to be disconnected.
        PollingCheck.check(
                "Wifi not disconnected",
                20000,
                () -> mWifiManager.getConnectionInfo().getNetworkId() == -1);
    }

    @After
    public void tearDown() throws Exception {
        // If there is failure, ensure we unregister the previous request.
        if (mNetworkCallback != null) {
            mConnectivityManager.unregisterNetworkCallback(mNetworkCallback);
        }
        turnScreenOff();
    }

    private static void setWifiEnabled(boolean enable) throws Exception {
        // now trigger the change using shell commands.
        SystemUtil.runShellCommand("svc wifi " + (enable ? "enable" : "disable"));
    }

    private void turnScreenOn() throws Exception {
        mUiDevice.executeShellCommand("input keyevent KEYCODE_WAKEUP");
        mUiDevice.executeShellCommand("wm dismiss-keyguard");
        // Since the screen on/off intent is ordered, they will not be sent right now.
        Thread.sleep(DURATION_SCREEN_TOGGLE);
    }

    private void turnScreenOff() throws Exception {
        mUiDevice.executeShellCommand("input keyevent KEYCODE_SLEEP");
        // Since the screen on/off intent is ordered, they will not be sent right now.
        Thread.sleep(DURATION_SCREEN_TOGGLE);
    }

    private static class TestNetworkCallback extends ConnectivityManager.NetworkCallback {
        private final Object mLock;
        public boolean onAvailableCalled = false;
        public boolean onUnavailableCalled = false;
        public NetworkCapabilities networkCapabilities;

        TestNetworkCallback(Object lock) {
            mLock = lock;
        }

        @Override
        public void onAvailable(Network network, NetworkCapabilities networkCapabilities,
                LinkProperties linkProperties, boolean blocked) {
            synchronized (mLock) {
                onAvailableCalled = true;
                this.networkCapabilities = networkCapabilities;
                mLock.notify();
            }
        }

        @Override
        public void onUnavailable() {
            synchronized (mLock) {
                onUnavailableCalled = true;
                mLock.notify();
            }
        }
    }

    private static class TestNetworkRequestMatchCallback implements NetworkRequestMatchCallback {
        private final Object mLock;

        public boolean onRegistrationCalled = false;
        public boolean onAbortCalled = false;
        public boolean onMatchCalled = false;
        public boolean onConnectSuccessCalled = false;
        public boolean onConnectFailureCalled = false;
        public WifiManager.NetworkRequestUserSelectionCallback userSelectionCallback = null;
        public List<ScanResult> matchedScanResults = null;

        TestNetworkRequestMatchCallback(Object lock) {
            mLock = lock;
        }

        @Override
        public void onUserSelectionCallbackRegistration(
                WifiManager.NetworkRequestUserSelectionCallback userSelectionCallback) {
            synchronized (mLock) {
                onRegistrationCalled = true;
                this.userSelectionCallback = userSelectionCallback;
                mLock.notify();
            }
        }

        @Override
        public void onAbort() {
            synchronized (mLock) {
                onAbortCalled = true;
                mLock.notify();
            }
        }

        @Override
        public void onMatch(List<ScanResult> scanResults) {
            synchronized (mLock) {
                // This can be invoked multiple times. So, ignore after the first one to avoid
                // disturbing the rest of the test sequence.
                if (onMatchCalled) return;
                onMatchCalled = true;
                matchedScanResults = scanResults;
                mLock.notify();
            }
        }

        @Override
        public void onUserSelectionConnectSuccess(WifiConfiguration config) {
            synchronized (mLock) {
                onConnectSuccessCalled = true;
                mLock.notify();
            }
        }

        @Override
        public void onUserSelectionConnectFailure(WifiConfiguration config) {
            synchronized (mLock) {
                onConnectFailureCalled = true;
                mLock.notify();
            }
        }
    }

    private void handleUiInteractions(boolean shouldUserReject) {
        UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        TestNetworkRequestMatchCallback networkRequestMatchCallback =
                new TestNetworkRequestMatchCallback(mUiLock);
        try {
            uiAutomation.adoptShellPermissionIdentity();

            // 1. Wait for registration callback.
            synchronized (mUiLock) {
                try {
                    mWifiManager.registerNetworkRequestMatchCallback(
                            Executors.newSingleThreadExecutor(), networkRequestMatchCallback);
                    mUiLock.wait(DURATION_UI_INTERACTION);
                } catch (InterruptedException e) {
                }
            }
            assertTrue(networkRequestMatchCallback.onRegistrationCalled);
            assertNotNull(networkRequestMatchCallback.userSelectionCallback);

            // 2. Wait for matching scan results
            synchronized (mUiLock) {
                try {
                    mUiLock.wait(DURATION_UI_INTERACTION);
                } catch (InterruptedException e) {
                }
            }
            assertTrue(networkRequestMatchCallback.onMatchCalled);
            assertNotNull(networkRequestMatchCallback.matchedScanResults);
            assertThat(networkRequestMatchCallback.matchedScanResults.size()).isAtLeast(1);

            // 3. Trigger connection to one of the matched networks or reject the request.
            if (shouldUserReject) {
                networkRequestMatchCallback.userSelectionCallback.reject();
            } else {
                networkRequestMatchCallback.userSelectionCallback.select(mTestNetwork);
            }

            // 4. Wait for connection success or abort.
            synchronized (mUiLock) {
                try {
                    mUiLock.wait(DURATION_UI_INTERACTION);
                } catch (InterruptedException e) {
                }
            }
            if (shouldUserReject) {
                assertTrue(networkRequestMatchCallback.onAbortCalled);
            } else {
                assertTrue(networkRequestMatchCallback.onConnectSuccessCalled);
            }
        } finally {
            mWifiManager.unregisterNetworkRequestMatchCallback(networkRequestMatchCallback);
            uiAutomation.dropShellPermissionIdentity();
        }
    }

    /**
     * Tests the entire connection flow using the provided specifier.
     *
     * @param specifier Specifier to use for network request.
     * @param shouldUserReject Whether to simulate user rejection or not.
     */
    private void testConnectionFlowWithSpecifier(
            WifiNetworkSpecifier specifier, boolean shouldUserReject) {
        // Fork a thread to handle the UI interactions.
        Thread uiThread = new Thread(() -> handleUiInteractions(shouldUserReject));

        // File the network request & wait for the callback.
        mNetworkCallback = new TestNetworkCallback(mLock);
        synchronized (mLock) {
            try {
                // File a request for wifi network.
                mConnectivityManager.requestNetwork(
                        new NetworkRequest.Builder()
                                .addTransportType(TRANSPORT_WIFI)
                                .setNetworkSpecifier(specifier)
                                .build(),
                        mNetworkCallback);
                // Wait for the request to reach the wifi stack before kick-starting the UI
                // interactions.
                Thread.sleep(100);
                // Start the UI interactions.
                uiThread.run();
                // now wait for callback
                mLock.wait(DURATION_NETWORK_CONNECTION);
            } catch (InterruptedException e) {
            }
        }
        if (shouldUserReject) {
            assertTrue(mNetworkCallback.onUnavailableCalled);
        } else {
            assertTrue(mNetworkCallback.onAvailableCalled);
        }

        try {
            // Ensure that the UI interaction thread has completed.
            uiThread.join(DURATION_UI_INTERACTION);
        } catch (InterruptedException e) {
            fail("UI interaction interrupted");
        }

        // Release the request after the test.
        mConnectivityManager.unregisterNetworkCallback(mNetworkCallback);
        mNetworkCallback = null;
    }

    private void testSuccessfulConnectionWithSpecifier(WifiNetworkSpecifier specifier) {
        testConnectionFlowWithSpecifier(specifier, false);
    }

    private void testUserRejectionWithSpecifier(WifiNetworkSpecifier specifier) {
        testConnectionFlowWithSpecifier(specifier, true);
    }

    private static String removeDoubleQuotes(String string) {
        return WifiInfo.sanitizeSsid(string);
    }

    private WifiNetworkSpecifier.Builder createSpecifierBuilderWithCredentialFromSavedNetwork() {
        WifiNetworkSpecifier.Builder specifierBuilder = new WifiNetworkSpecifier.Builder();
        if (mTestNetwork.preSharedKey != null) {
            if (mTestNetwork.allowedKeyManagement.get(WifiConfiguration.KeyMgmt.WPA_PSK)) {
                specifierBuilder.setWpa2Passphrase(removeDoubleQuotes(mTestNetwork.preSharedKey));
            } else if (mTestNetwork.allowedKeyManagement.get(WifiConfiguration.KeyMgmt.SAE)) {
                specifierBuilder.setWpa3Passphrase(removeDoubleQuotes(mTestNetwork.preSharedKey));
            } else {
                fail("Unsupported security type found in saved networks");
            }
        } else if (!mTestNetwork.allowedKeyManagement.get(WifiConfiguration.KeyMgmt.OWE)) {
            specifierBuilder.setIsEnhancedOpen(false);
        } else if (!mTestNetwork.allowedKeyManagement.get(WifiConfiguration.KeyMgmt.NONE)) {
            fail("Unsupported security type found in saved networks");
        }
        specifierBuilder.setIsHiddenSsid(mTestNetwork.hiddenSSID);
        return specifierBuilder;
    }

    /**
     * Tests the entire connection flow using a specific SSID in the specifier.
     */
    @Test
    public void testConnectionWithSpecificSsid() {
        WifiNetworkSpecifier specifier = createSpecifierBuilderWithCredentialFromSavedNetwork()
                .setSsid(removeDoubleQuotes(mTestNetwork.SSID))
                .build();
        testSuccessfulConnectionWithSpecifier(specifier);
    }

    /**
     * Tests the entire connection flow using a SSID pattern in the specifier.
     */
    @Test
    public void testConnectionWithSsidPattern() {
        // Creates a ssid pattern by dropping the last char in the saved network & pass that
        // as a prefix match pattern in the request.
        String ssidUnquoted = removeDoubleQuotes(mTestNetwork.SSID);
        assertThat(ssidUnquoted.length()).isAtLeast(2);
        String ssidPrefix = ssidUnquoted.substring(0, ssidUnquoted.length() - 1);
        // Note: The match may return more than 1 network in this case since we use a prefix match,
        // But, we will still ensure that the UI interactions in the test still selects the
        // saved network for connection.
        WifiNetworkSpecifier specifier = createSpecifierBuilderWithCredentialFromSavedNetwork()
                .setSsidPattern(new PatternMatcher(ssidPrefix, PatternMatcher.PATTERN_PREFIX))
                .build();
        testSuccessfulConnectionWithSpecifier(specifier);
    }

    private static class TestScanResultsCallback extends WifiManager.ScanResultsCallback {
        private final Object mLock;
        public boolean onAvailableCalled = false;

        TestScanResultsCallback(Object lock) {
            mLock = lock;
        }

        @Override
        public void onScanResultsAvailable() {
            synchronized (mLock) {
                onAvailableCalled = true;
                mLock.notify();
            }
        }
    }

    /**
     * Loops through all available scan results and finds the first match for the saved network.
     *
     * Note:
     * a) If there are more than 2 networks with the same SSID, but different credential type, then
     * this matching may pick the wrong one.
     */
    private ScanResult findScanResultMatchingSavedNetwork() {
        for (int i = 0; i < SCAN_RETRY_CNT_TO_FIND_MATCHING_BSSID; i++) {
            // Trigger a scan to get fresh scan results.
            TestScanResultsCallback scanResultsCallback = new TestScanResultsCallback(mLock);
            synchronized (mLock) {
                try {
                    mWifiManager.registerScanResultsCallback(
                            Executors.newSingleThreadExecutor(), scanResultsCallback);
                    mWifiManager.startScan(new WorkSource(myUid()));
                    // now wait for callback
                    mLock.wait(DURATION_NETWORK_CONNECTION);
                } catch (InterruptedException e) {
                } finally {
                    mWifiManager.unregisterScanResultsCallback(scanResultsCallback);
                }
            }
            List<ScanResult> scanResults = mWifiManager.getScanResults();
            if (scanResults == null || scanResults.isEmpty()) fail("No scan results available");
            for (ScanResult scanResult : scanResults) {
                if (TextUtils.equals(scanResult.SSID, removeDoubleQuotes(mTestNetwork.SSID))) {
                    return scanResult;
                }
            }
        }
        fail("No matching scan results found");
        return null;
    }

    /**
     * Tests the entire connection flow using a specific BSSID in the specifier.
     */
    @Test
    public void testConnectionWithSpecificBssid() {
        ScanResult scanResult = findScanResultMatchingSavedNetwork();
        WifiNetworkSpecifier specifier = createSpecifierBuilderWithCredentialFromSavedNetwork()
                .setBssid(MacAddress.fromString(scanResult.BSSID))
                .build();
        testSuccessfulConnectionWithSpecifier(specifier);
    }

    /**
     * Tests the entire connection flow using a BSSID pattern in the specifier.
     */
    @Test
    public void testConnectionWithBssidPattern() {
        ScanResult scanResult = findScanResultMatchingSavedNetwork();
        // Note: The match may return more than 1 network in this case since we use a prefix match,
        // But, we will still ensure that the UI interactions in the test still selects the
        // saved network for connection.
        WifiNetworkSpecifier specifier = createSpecifierBuilderWithCredentialFromSavedNetwork()
                .setBssidPattern(MacAddress.fromString(scanResult.BSSID),
                        MacAddress.fromString("ff:ff:ff:00:00:00"))
                .build();
        testSuccessfulConnectionWithSpecifier(specifier);
    }

    /**
     * Tests the entire connection flow using a BSSID pattern in the specifier.
     */
    @Test
    public void testUserRejectionWithSpecificSsid() {
        WifiNetworkSpecifier specifier = createSpecifierBuilderWithCredentialFromSavedNetwork()
                .setSsid(removeDoubleQuotes(mTestNetwork.SSID))
                .build();
        testUserRejectionWithSpecifier(specifier);
    }

    /**
     * Tests the builder for WPA2 enterprise networks.
     * Note: Can't do end to end tests for such networks in CTS environment.
     */
    @Test
    public void testBuilderForWpa2Enterprise() {
        WifiNetworkSpecifier specifier1 = new WifiNetworkSpecifier.Builder()
                .setSsid(removeDoubleQuotes(mTestNetwork.SSID))
                .setWpa2EnterpriseConfig(new WifiEnterpriseConfig())
                .build();
        WifiNetworkSpecifier specifier2 = new WifiNetworkSpecifier.Builder()
                .setSsid(removeDoubleQuotes(mTestNetwork.SSID))
                .setWpa2EnterpriseConfig(new WifiEnterpriseConfig())
                .build();
        assertThat(specifier1.canBeSatisfiedBy(specifier2)).isTrue();
    }

    /**
     * Tests the builder for WPA3 enterprise networks.
     * Note: Can't do end to end tests for such networks in CTS environment.
     */
    @Test
    public void testBuilderForWpa3Enterprise() {
        WifiNetworkSpecifier specifier1 = new WifiNetworkSpecifier.Builder()
                .setSsid(removeDoubleQuotes(mTestNetwork.SSID))
                .setWpa3EnterpriseConfig(new WifiEnterpriseConfig())
                .build();
        WifiNetworkSpecifier specifier2 = new WifiNetworkSpecifier.Builder()
                .setSsid(removeDoubleQuotes(mTestNetwork.SSID))
                .setWpa3EnterpriseConfig(new WifiEnterpriseConfig())
                .build();
        assertThat(specifier1.canBeSatisfiedBy(specifier2)).isTrue();
    }
}
