/*
 * Copyright (C) 2009 The Android Open Source Project
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

package android.net.cts;

import static android.Manifest.permission.CONNECTIVITY_USE_RESTRICTED_NETWORKS;
import static android.content.pm.PackageManager.FEATURE_ETHERNET;
import static android.content.pm.PackageManager.FEATURE_TELEPHONY;
import static android.content.pm.PackageManager.FEATURE_USB_HOST;
import static android.content.pm.PackageManager.FEATURE_WIFI;
import static android.content.pm.PackageManager.GET_PERMISSIONS;
import static android.content.pm.PackageManager.PERMISSION_GRANTED;
import static android.net.NetworkCapabilities.NET_CAPABILITY_IMS;
import static android.net.NetworkCapabilities.NET_CAPABILITY_INTERNET;
import static android.net.NetworkCapabilities.NET_CAPABILITY_NOT_METERED;
import static android.net.NetworkCapabilities.NET_CAPABILITY_NOT_RESTRICTED;
import static android.net.NetworkCapabilities.TRANSPORT_WIFI;
import static android.net.cts.util.CtsNetUtils.ConnectivityActionReceiver;
import static android.net.cts.util.CtsNetUtils.HTTP_PORT;
import static android.net.cts.util.CtsNetUtils.NETWORK_CALLBACK_ACTION;
import static android.net.cts.util.CtsNetUtils.TEST_HOST;
import static android.net.cts.util.CtsNetUtils.TestNetworkCallback;
import static android.os.MessageQueue.OnFileDescriptorEventListener.EVENT_INPUT;
import static android.provider.Settings.Global.NETWORK_METERED_MULTIPATH_PREFERENCE;
import static android.system.OsConstants.AF_INET;
import static android.system.OsConstants.AF_INET6;
import static android.system.OsConstants.AF_UNSPEC;

import static com.android.compatibility.common.util.SystemUtil.runShellCommand;
import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.annotation.NonNull;
import android.app.Instrumentation;
import android.app.PendingIntent;
import android.app.UiAutomation;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.net.ConnectivityManager;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.IpSecManager;
import android.net.IpSecManager.UdpEncapsulationSocket;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkConfig;
import android.net.NetworkInfo;
import android.net.NetworkInfo.DetailedState;
import android.net.NetworkInfo.State;
import android.net.NetworkRequest;
import android.net.NetworkUtils;
import android.net.SocketKeepalive;
import android.net.cts.util.CtsNetUtils;
import android.net.util.KeepaliveUtils;
import android.net.wifi.WifiManager;
import android.os.Binder;
import android.os.Build;
import android.os.Looper;
import android.os.MessageQueue;
import android.os.SystemClock;
import android.os.SystemProperties;
import android.os.VintfRuntimeInfo;
import android.platform.test.annotations.AppModeFull;
import android.provider.Settings;
import android.text.TextUtils;
import android.util.Log;
import android.util.Pair;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.internal.util.ArrayUtils;
import com.android.testutils.SkipPresubmit;

import libcore.io.Streams;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.Inet4Address;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.URL;
import java.net.UnknownHostException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

@RunWith(AndroidJUnit4.class)
public class ConnectivityManagerTest {

    private static final String TAG = ConnectivityManagerTest.class.getSimpleName();

    public static final int TYPE_MOBILE = ConnectivityManager.TYPE_MOBILE;
    public static final int TYPE_WIFI = ConnectivityManager.TYPE_WIFI;

    private static final int HOST_ADDRESS = 0x7f000001;// represent ip 127.0.0.1
    private static final int KEEPALIVE_CALLBACK_TIMEOUT_MS = 2000;
    private static final int INTERVAL_KEEPALIVE_RETRY_MS = 500;
    private static final int MAX_KEEPALIVE_RETRY_COUNT = 3;
    private static final int MIN_KEEPALIVE_INTERVAL = 10;

    // Changing meteredness on wifi involves reconnecting, which can take several seconds (involves
    // re-associating, DHCP...)
    private static final int NETWORK_CHANGE_METEREDNESS_TIMEOUT = 30_000;
    private static final int NUM_TRIES_MULTIPATH_PREF_CHECK = 20;
    private static final long INTERVAL_MULTIPATH_PREF_CHECK_MS = 500;
    // device could have only one interface: data, wifi.
    private static final int MIN_NUM_NETWORK_TYPES = 1;

    // Minimum supported keepalive counts for wifi and cellular.
    public static final int MIN_SUPPORTED_CELLULAR_KEEPALIVE_COUNT = 1;
    public static final int MIN_SUPPORTED_WIFI_KEEPALIVE_COUNT = 3;

    private static final String NETWORK_METERED_MULTIPATH_PREFERENCE_RES_NAME =
            "config_networkMeteredMultipathPreference";
    private static final String KEEPALIVE_ALLOWED_UNPRIVILEGED_RES_NAME =
            "config_allowedUnprivilegedKeepalivePerUid";
    private static final String KEEPALIVE_RESERVED_PER_SLOT_RES_NAME =
            "config_reservedPrivilegedKeepaliveSlots";

    private Context mContext;
    private Instrumentation mInstrumentation;
    private ConnectivityManager mCm;
    private WifiManager mWifiManager;
    private PackageManager mPackageManager;
    private final HashMap<Integer, NetworkConfig> mNetworks =
            new HashMap<Integer, NetworkConfig>();
    boolean mWifiWasDisabled;
    private UiAutomation mUiAutomation;
    private CtsNetUtils mCtsNetUtils;

    @Before
    public void setUp() throws Exception {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mContext = mInstrumentation.getContext();
        mCm = (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        mWifiManager = (WifiManager) mContext.getSystemService(Context.WIFI_SERVICE);
        mPackageManager = mContext.getPackageManager();
        mCtsNetUtils = new CtsNetUtils(mContext);
        mWifiWasDisabled = false;

        // Get com.android.internal.R.array.networkAttributes
        int resId = mContext.getResources().getIdentifier("networkAttributes", "array", "android");
        String[] naStrings = mContext.getResources().getStringArray(resId);
        //TODO: What is the "correct" way to determine if this is a wifi only device?
        boolean wifiOnly = SystemProperties.getBoolean("ro.radio.noril", false);
        for (String naString : naStrings) {
            try {
                NetworkConfig n = new NetworkConfig(naString);
                if (wifiOnly && ConnectivityManager.isNetworkTypeMobile(n.type)) {
                    continue;
                }
                mNetworks.put(n.type, n);
            } catch (Exception e) {}
        }
        mUiAutomation = mInstrumentation.getUiAutomation();
    }

    @After
    public void tearDown() throws Exception {
        // Return WiFi to its original disabled state after tests that explicitly connect.
        if (mWifiWasDisabled) {
            mCtsNetUtils.disconnectFromWifi(null);
        }
        if (mCtsNetUtils.cellConnectAttempted()) {
            mCtsNetUtils.disconnectFromCell();
        }
    }

    /**
     * Make sure WiFi is connected to an access point if it is not already. If
     * WiFi is enabled as a result of this function, it will be disabled
     * automatically in tearDown().
     */
    private Network ensureWifiConnected() {
        mWifiWasDisabled = !mWifiManager.isWifiEnabled();
        // Even if wifi is enabled, the network may not be connected or ready yet
        return mCtsNetUtils.connectToWifi();
    }

    @Test
    public void testIsNetworkTypeValid() {
        assertTrue(ConnectivityManager.isNetworkTypeValid(ConnectivityManager.TYPE_MOBILE));
        assertTrue(ConnectivityManager.isNetworkTypeValid(ConnectivityManager.TYPE_WIFI));
        assertTrue(ConnectivityManager.isNetworkTypeValid(ConnectivityManager.TYPE_MOBILE_MMS));
        assertTrue(ConnectivityManager.isNetworkTypeValid(ConnectivityManager.TYPE_MOBILE_SUPL));
        assertTrue(ConnectivityManager.isNetworkTypeValid(ConnectivityManager.TYPE_MOBILE_DUN));
        assertTrue(ConnectivityManager.isNetworkTypeValid(ConnectivityManager.TYPE_MOBILE_HIPRI));
        assertTrue(ConnectivityManager.isNetworkTypeValid(ConnectivityManager.TYPE_WIMAX));
        assertTrue(ConnectivityManager.isNetworkTypeValid(ConnectivityManager.TYPE_BLUETOOTH));
        assertTrue(ConnectivityManager.isNetworkTypeValid(ConnectivityManager.TYPE_DUMMY));
        assertTrue(ConnectivityManager.isNetworkTypeValid(ConnectivityManager.TYPE_ETHERNET));
        assertTrue(mCm.isNetworkTypeValid(ConnectivityManager.TYPE_MOBILE_FOTA));
        assertTrue(mCm.isNetworkTypeValid(ConnectivityManager.TYPE_MOBILE_IMS));
        assertTrue(mCm.isNetworkTypeValid(ConnectivityManager.TYPE_MOBILE_CBS));
        assertTrue(mCm.isNetworkTypeValid(ConnectivityManager.TYPE_WIFI_P2P));
        assertTrue(mCm.isNetworkTypeValid(ConnectivityManager.TYPE_MOBILE_IA));
        assertFalse(mCm.isNetworkTypeValid(-1));
        assertTrue(mCm.isNetworkTypeValid(0));
        assertTrue(mCm.isNetworkTypeValid(ConnectivityManager.MAX_NETWORK_TYPE));
        assertFalse(ConnectivityManager.isNetworkTypeValid(ConnectivityManager.MAX_NETWORK_TYPE+1));

        NetworkInfo[] ni = mCm.getAllNetworkInfo();

        for (NetworkInfo n: ni) {
            assertTrue(ConnectivityManager.isNetworkTypeValid(n.getType()));
        }

    }

    @Test
    public void testSetNetworkPreference() {
        // getNetworkPreference() and setNetworkPreference() are both deprecated so they do
        // not preform any action.  Verify they are at least still callable.
        mCm.setNetworkPreference(mCm.getNetworkPreference());
    }

    @Test
    public void testGetActiveNetworkInfo() {
        NetworkInfo ni = mCm.getActiveNetworkInfo();

        assertNotNull("You must have an active network connection to complete CTS", ni);
        assertTrue(ConnectivityManager.isNetworkTypeValid(ni.getType()));
        assertTrue(ni.getState() == State.CONNECTED);
    }

    @Test
    public void testGetActiveNetwork() {
        Network network = mCm.getActiveNetwork();
        assertNotNull("You must have an active network connection to complete CTS", network);

        NetworkInfo ni = mCm.getNetworkInfo(network);
        assertNotNull("Network returned from getActiveNetwork was invalid", ni);

        // Similar to testGetActiveNetworkInfo above.
        assertTrue(ConnectivityManager.isNetworkTypeValid(ni.getType()));
        assertTrue(ni.getState() == State.CONNECTED);
    }

    @Test
    public void testGetNetworkInfo() {
        for (int type = -1; type <= ConnectivityManager.MAX_NETWORK_TYPE+1; type++) {
            if (shouldBeSupported(type)) {
                NetworkInfo ni = mCm.getNetworkInfo(type);
                assertTrue("Info shouldn't be null for " + type, ni != null);
                State state = ni.getState();
                assertTrue("Bad state for " + type, State.UNKNOWN.ordinal() >= state.ordinal()
                           && state.ordinal() >= State.CONNECTING.ordinal());
                DetailedState ds = ni.getDetailedState();
                assertTrue("Bad detailed state for " + type,
                           DetailedState.FAILED.ordinal() >= ds.ordinal()
                           && ds.ordinal() >= DetailedState.IDLE.ordinal());
            } else {
                assertNull("Info should be null for " + type, mCm.getNetworkInfo(type));
            }
        }
    }

    @Test
    public void testGetAllNetworkInfo() {
        NetworkInfo[] ni = mCm.getAllNetworkInfo();
        assertTrue(ni.length >= MIN_NUM_NETWORK_TYPES);
        for (int type = 0; type <= ConnectivityManager.MAX_NETWORK_TYPE; type++) {
            int desiredFoundCount = (shouldBeSupported(type) ? 1 : 0);
            int foundCount = 0;
            for (NetworkInfo i : ni) {
                if (i.getType() == type) foundCount++;
            }
            if (foundCount != desiredFoundCount) {
                Log.e(TAG, "failure in testGetAllNetworkInfo.  Dump of returned NetworkInfos:");
                for (NetworkInfo networkInfo : ni) Log.e(TAG, "  " + networkInfo);
            }
            assertTrue("Unexpected foundCount of " + foundCount + " for type " + type,
                    foundCount == desiredFoundCount);
        }
    }

    /**
     * Tests that connections can be opened on WiFi and cellphone networks,
     * and that they are made from different IP addresses.
     */
    @AppModeFull(reason = "Cannot get WifiManager in instant app mode")
    @Test
    @SkipPresubmit(reason = "Virtual devices use a single internet connection for all networks")
    public void testOpenConnection() throws Exception {
        boolean canRunTest = mPackageManager.hasSystemFeature(FEATURE_WIFI)
                && mPackageManager.hasSystemFeature(FEATURE_TELEPHONY);
        if (!canRunTest) {
            Log.i(TAG,"testOpenConnection cannot execute unless device supports both WiFi "
                    + "and a cellular connection");
            return;
        }

        Network wifiNetwork = mCtsNetUtils.connectToWifi();
        Network cellNetwork = mCtsNetUtils.connectToCell();
        // This server returns the requestor's IP address as the response body.
        URL url = new URL("http://google-ipv6test.appspot.com/ip.js?fmt=text");
        String wifiAddressString = httpGet(wifiNetwork, url);
        String cellAddressString = httpGet(cellNetwork, url);

        assertFalse(String.format("Same address '%s' on two different networks (%s, %s)",
                wifiAddressString, wifiNetwork, cellNetwork),
                wifiAddressString.equals(cellAddressString));

        // Sanity check that the IP addresses that the requests appeared to come from
        // are actually on the respective networks.
        assertOnNetwork(wifiAddressString, wifiNetwork);
        assertOnNetwork(cellAddressString, cellNetwork);

        assertFalse("Unexpectedly equal: " + wifiNetwork, wifiNetwork.equals(cellNetwork));
    }

    /**
     * Performs a HTTP GET to the specified URL on the specified Network, and returns
     * the response body decoded as UTF-8.
     */
    private static String httpGet(Network network, URL httpUrl) throws IOException {
        HttpURLConnection connection = (HttpURLConnection) network.openConnection(httpUrl);
        try {
            InputStream inputStream = connection.getInputStream();
            return Streams.readFully(new InputStreamReader(inputStream, StandardCharsets.UTF_8));
        } finally {
            connection.disconnect();
        }
    }

    private void assertOnNetwork(String adressString, Network network) throws UnknownHostException {
        InetAddress address = InetAddress.getByName(adressString);
        LinkProperties linkProperties = mCm.getLinkProperties(network);
        // To make sure that the request went out on the right network, check that
        // the IP address seen by the server is assigned to the expected network.
        // We can only do this for IPv6 addresses, because in IPv4 we will likely
        // have a private IPv4 address, and that won't match what the server sees.
        if (address instanceof Inet6Address) {
            assertContains(linkProperties.getAddresses(), address);
        }
    }

    private static<T> void assertContains(Collection<T> collection, T element) {
        assertTrue(element + " not found in " + collection, collection.contains(element));
    }

    private void assertStartUsingNetworkFeatureUnsupported(int networkType, String feature) {
        try {
            mCm.startUsingNetworkFeature(networkType, feature);
            fail("startUsingNetworkFeature is no longer supported in the current API version");
        } catch (UnsupportedOperationException expected) {}
    }

    private void assertStopUsingNetworkFeatureUnsupported(int networkType, String feature) {
        try {
            mCm.startUsingNetworkFeature(networkType, feature);
            fail("stopUsingNetworkFeature is no longer supported in the current API version");
        } catch (UnsupportedOperationException expected) {}
    }

    private void assertRequestRouteToHostUnsupported(int networkType, int hostAddress) {
        try {
            mCm.requestRouteToHost(networkType, hostAddress);
            fail("requestRouteToHost is no longer supported in the current API version");
        } catch (UnsupportedOperationException expected) {}
    }

    @Test
    public void testStartUsingNetworkFeature() {

        final String invalidateFeature = "invalidateFeature";
        final String mmsFeature = "enableMMS";

        assertStartUsingNetworkFeatureUnsupported(TYPE_MOBILE, invalidateFeature);
        assertStopUsingNetworkFeatureUnsupported(TYPE_MOBILE, invalidateFeature);
        assertStartUsingNetworkFeatureUnsupported(TYPE_WIFI, mmsFeature);
    }

    private boolean shouldEthernetBeSupported() {
        // Instant mode apps aren't allowed to query the Ethernet service due to selinux policies.
        // When in instant mode, don't fail if the Ethernet service is available. Instead, rely on
        // the fact that Ethernet should be supported if the device has a hardware Ethernet port, or
        // if the device can be a USB host and thus can use USB Ethernet adapters.
        //
        // Note that this test this will still fail in instant mode if a device supports Ethernet
        // via other hardware means. We are not currently aware of any such device.
        return (mContext.getSystemService(Context.ETHERNET_SERVICE) != null) ||
            mPackageManager.hasSystemFeature(FEATURE_ETHERNET) ||
            mPackageManager.hasSystemFeature(FEATURE_USB_HOST);
    }

    private boolean shouldBeSupported(int networkType) {
        return mNetworks.containsKey(networkType) ||
               (networkType == ConnectivityManager.TYPE_VPN) ||
               (networkType == ConnectivityManager.TYPE_ETHERNET && shouldEthernetBeSupported());
    }

    @Test
    public void testIsNetworkSupported() {
        for (int type = -1; type <= ConnectivityManager.MAX_NETWORK_TYPE; type++) {
            boolean supported = mCm.isNetworkSupported(type);
            if (shouldBeSupported(type)) {
                assertTrue("Network type " + type + " should be supported", supported);
            } else {
                assertFalse("Network type " + type + " should not be supported", supported);
            }
        }
    }

    @Test
    public void testRequestRouteToHost() {
        for (int type = -1 ; type <= ConnectivityManager.MAX_NETWORK_TYPE; type++) {
            assertRequestRouteToHostUnsupported(type, HOST_ADDRESS);
        }
    }

    @Test
    public void testTest() {
        mCm.getBackgroundDataSetting();
    }

    private NetworkRequest makeWifiNetworkRequest() {
        return new NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                .build();
    }

    /**
     * Exercises both registerNetworkCallback and unregisterNetworkCallback. This checks to
     * see if we get a callback for the TRANSPORT_WIFI transport type being available.
     *
     * <p>In order to test that a NetworkCallback occurs, we need some change in the network
     * state (either a transport or capability is now available). The most straightforward is
     * WiFi. We could add a version that uses the telephony data connection but it's not clear
     * that it would increase test coverage by much (how many devices have 3G radio but not Wifi?).
     */
    @AppModeFull(reason = "Cannot get WifiManager in instant app mode")
    @Test
    public void testRegisterNetworkCallback() {
        if (!mPackageManager.hasSystemFeature(FEATURE_WIFI)) {
            Log.i(TAG, "testRegisterNetworkCallback cannot execute unless device supports WiFi");
            return;
        }

        // We will register for a WIFI network being available or lost.
        final TestNetworkCallback callback = new TestNetworkCallback();
        mCm.registerNetworkCallback(makeWifiNetworkRequest(), callback);

        final TestNetworkCallback defaultTrackingCallback = new TestNetworkCallback();
        mCm.registerDefaultNetworkCallback(defaultTrackingCallback);

        Network wifiNetwork = null;

        try {
            ensureWifiConnected();

            // Now we should expect to get a network callback about availability of the wifi
            // network even if it was already connected as a state-based action when the callback
            // is registered.
            wifiNetwork = callback.waitForAvailable();
            assertNotNull("Did not receive NetworkCallback.onAvailable for TRANSPORT_WIFI",
                    wifiNetwork);

            assertNotNull("Did not receive NetworkCallback.onAvailable for any default network",
                    defaultTrackingCallback.waitForAvailable());
        } catch (InterruptedException e) {
            fail("Broadcast receiver or NetworkCallback wait was interrupted.");
        } finally {
            mCm.unregisterNetworkCallback(callback);
            mCm.unregisterNetworkCallback(defaultTrackingCallback);
        }
    }

    /**
     * Tests both registerNetworkCallback and unregisterNetworkCallback similarly to
     * {@link #testRegisterNetworkCallback} except that a {@code PendingIntent} is used instead
     * of a {@code NetworkCallback}.
     */
    @AppModeFull(reason = "Cannot get WifiManager in instant app mode")
    @Test
    public void testRegisterNetworkCallback_withPendingIntent() {
        if (!mPackageManager.hasSystemFeature(FEATURE_WIFI)) {
            Log.i(TAG, "testRegisterNetworkCallback cannot execute unless device supports WiFi");
            return;
        }

        // Create a ConnectivityActionReceiver that has an IntentFilter for our locally defined
        // action, NETWORK_CALLBACK_ACTION.
        IntentFilter filter = new IntentFilter();
        filter.addAction(NETWORK_CALLBACK_ACTION);

        ConnectivityActionReceiver receiver = new ConnectivityActionReceiver(
                mCm, ConnectivityManager.TYPE_WIFI, NetworkInfo.State.CONNECTED);
        mContext.registerReceiver(receiver, filter);

        // Create a broadcast PendingIntent for NETWORK_CALLBACK_ACTION.
        Intent intent = new Intent(NETWORK_CALLBACK_ACTION);
        PendingIntent pendingIntent = PendingIntent.getBroadcast(
                mContext, 0, intent, PendingIntent.FLAG_CANCEL_CURRENT);

        // We will register for a WIFI network being available or lost.
        mCm.registerNetworkCallback(makeWifiNetworkRequest(), pendingIntent);

        try {
            ensureWifiConnected();

            // Now we expect to get the Intent delivered notifying of the availability of the wifi
            // network even if it was already connected as a state-based action when the callback
            // is registered.
            assertTrue("Did not receive expected Intent " + intent + " for TRANSPORT_WIFI",
                    receiver.waitForState());
        } catch (InterruptedException e) {
            fail("Broadcast receiver or NetworkCallback wait was interrupted.");
        } finally {
            mCm.unregisterNetworkCallback(pendingIntent);
            pendingIntent.cancel();
            mContext.unregisterReceiver(receiver);
        }
    }

    /**
     * Exercises the requestNetwork with NetworkCallback API. This checks to
     * see if we get a callback for an INTERNET request.
     */
    @AppModeFull(reason = "CHANGE_NETWORK_STATE permission can't be granted to instant apps")
    @Test
    public void testRequestNetworkCallback() {
        final TestNetworkCallback callback = new TestNetworkCallback();
        mCm.requestNetwork(new NetworkRequest.Builder()
                .addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                .build(), callback);

        try {
            // Wait to get callback for availability of internet
            Network internetNetwork = callback.waitForAvailable();
            assertNotNull("Did not receive NetworkCallback#onAvailable for INTERNET",
                    internetNetwork);
        } catch (InterruptedException e) {
            fail("NetworkCallback wait was interrupted.");
        } finally {
            mCm.unregisterNetworkCallback(callback);
        }
    }

    /**
     * Exercises the requestNetwork with NetworkCallback API with timeout - expected to
     * fail. Use WIFI and switch Wi-Fi off.
     */
    @AppModeFull(reason = "Cannot get WifiManager in instant app mode")
    @Test
    public void testRequestNetworkCallback_onUnavailable() {
        final boolean previousWifiEnabledState = mWifiManager.isWifiEnabled();
        if (previousWifiEnabledState) {
            mCtsNetUtils.disconnectFromWifi(null);
        }

        final TestNetworkCallback callback = new TestNetworkCallback();
        mCm.requestNetwork(new NetworkRequest.Builder()
                .addTransportType(TRANSPORT_WIFI)
                .build(), callback, 100);

        try {
            // Wait to get callback for unavailability of requested network
            assertTrue("Did not receive NetworkCallback#onUnavailable",
                    callback.waitForUnavailable());
        } catch (InterruptedException e) {
            fail("NetworkCallback wait was interrupted.");
        } finally {
            mCm.unregisterNetworkCallback(callback);
            if (previousWifiEnabledState) {
                mCtsNetUtils.connectToWifi();
            }
        }
    }

    private InetAddress getFirstV4Address(Network network) {
        LinkProperties linkProperties = mCm.getLinkProperties(network);
        for (InetAddress address : linkProperties.getAddresses()) {
            if (address instanceof Inet4Address) {
                return address;
            }
        }
        return null;
    }

    /** Verify restricted networks cannot be requested. */
    @AppModeFull(reason = "CHANGE_NETWORK_STATE permission can't be granted to instant apps")
    @Test
    public void testRestrictedNetworks() {
        // Verify we can request unrestricted networks:
        NetworkRequest request = new NetworkRequest.Builder()
                .addCapability(NET_CAPABILITY_INTERNET).build();
        NetworkCallback callback = new NetworkCallback();
        mCm.requestNetwork(request, callback);
        mCm.unregisterNetworkCallback(callback);
        // Verify we cannot request restricted networks:
        request = new NetworkRequest.Builder().addCapability(NET_CAPABILITY_IMS).build();
        callback = new NetworkCallback();
        try {
            mCm.requestNetwork(request, callback);
            fail("No exception thrown when restricted network requested.");
        } catch (SecurityException expected) {}
    }

    // Returns "true", "false" or "none"
    private String getWifiMeteredStatus(String ssid) throws Exception {
        // Interestingly giving the SSID as an argument to list wifi-networks
        // only works iff the network in question has the "false" policy.
        // Also unfortunately runShellCommand does not pass the command to the interpreter
        // so it's not possible to | grep the ssid.
        final String command = "cmd netpolicy list wifi-networks";
        final String policyString = runShellCommand(mInstrumentation, command);

        final Matcher m = Pattern.compile("^" + ssid + ";(true|false|none)$",
                Pattern.MULTILINE | Pattern.UNIX_LINES).matcher(policyString);
        if (!m.find()) {
            fail("Unexpected format from cmd netpolicy");
        }
        return m.group(1);
    }

    // metered should be "true", "false" or "none"
    private void setWifiMeteredStatus(String ssid, String metered) throws Exception {
        final String setCommand = "cmd netpolicy set metered-network " + ssid + " " + metered;
        runShellCommand(mInstrumentation, setCommand);
        assertEquals(getWifiMeteredStatus(ssid), metered);
    }

    private String unquoteSSID(String ssid) {
        // SSID is returned surrounded by quotes if it can be decoded as UTF-8.
        // Otherwise it's guaranteed not to start with a quote.
        if (ssid.charAt(0) == '"') {
            return ssid.substring(1, ssid.length() - 1);
        } else {
            return ssid;
        }
    }

    private void waitForActiveNetworkMetered(int targetTransportType, boolean requestedMeteredness)
            throws Exception {
        final CountDownLatch latch = new CountDownLatch(1);
        final NetworkCallback networkCallback = new NetworkCallback() {
            @Override
            public void onCapabilitiesChanged(Network network, NetworkCapabilities nc) {
                if (!nc.hasTransport(targetTransportType)) return;

                final boolean metered = !nc.hasCapability(NET_CAPABILITY_NOT_METERED);
                if (metered == requestedMeteredness) {
                    latch.countDown();
                }
            }
        };
        // Registering a callback here guarantees onCapabilitiesChanged is called immediately
        // with the current setting. Therefore, if the setting has already been changed,
        // this method will return right away, and if not it will wait for the setting to change.
        mCm.registerDefaultNetworkCallback(networkCallback);
        if (!latch.await(NETWORK_CHANGE_METEREDNESS_TIMEOUT, TimeUnit.MILLISECONDS)) {
            fail("Timed out waiting for active network metered status to change to "
                 + requestedMeteredness + " ; network = " + mCm.getActiveNetwork());
        }
        mCm.unregisterNetworkCallback(networkCallback);
    }

    private void assertMultipathPreferenceIsEventually(Network network, int oldValue,
            int expectedValue) {
        // Sanity check : if oldValue == expectedValue, there is no way to guarantee the test
        // is not flaky.
        assertNotSame(oldValue, expectedValue);

        for (int i = 0; i < NUM_TRIES_MULTIPATH_PREF_CHECK; ++i) {
            final int actualValue = mCm.getMultipathPreference(network);
            if (actualValue == expectedValue) {
                return;
            }
            if (actualValue != oldValue) {
                fail("Multipath preference is neither previous (" + oldValue
                        + ") nor expected (" + expectedValue + ")");
            }
            SystemClock.sleep(INTERVAL_MULTIPATH_PREF_CHECK_MS);
        }
        fail("Timed out waiting for multipath preference to change. expected = "
                + expectedValue + " ; actual = " + mCm.getMultipathPreference(network));
    }

    private int getCurrentMeteredMultipathPreference(ContentResolver resolver) {
        final String rawMeteredPref = Settings.Global.getString(resolver,
                NETWORK_METERED_MULTIPATH_PREFERENCE);
        return TextUtils.isEmpty(rawMeteredPref)
            ? getIntResourceForName(NETWORK_METERED_MULTIPATH_PREFERENCE_RES_NAME)
            : Integer.parseInt(rawMeteredPref);
    }

    private int findNextPrefValue(ContentResolver resolver) {
        // A bit of a nuclear hammer, but race conditions in CTS are bad. To be able to
        // detect a correct setting value without race conditions, the next pref must
        // be a valid value (range 0..3) that is different from the old setting of the
        // metered preference and from the unmetered preference.
        final int meteredPref = getCurrentMeteredMultipathPreference(resolver);
        final int unmeteredPref = ConnectivityManager.MULTIPATH_PREFERENCE_UNMETERED;
        if (0 != meteredPref && 0 != unmeteredPref) return 0;
        if (1 != meteredPref && 1 != unmeteredPref) return 1;
        return 2;
    }

    /**
     * Verify that getMultipathPreference does return appropriate values
     * for metered and unmetered networks.
     */
    @AppModeFull(reason = "Cannot get WifiManager in instant app mode")
    @Test
    public void testGetMultipathPreference() throws Exception {
        final ContentResolver resolver = mContext.getContentResolver();
        ensureWifiConnected();
        final String ssid = unquoteSSID(mWifiManager.getConnectionInfo().getSSID());
        final String oldMeteredSetting = getWifiMeteredStatus(ssid);
        final String oldMeteredMultipathPreference = Settings.Global.getString(
                resolver, NETWORK_METERED_MULTIPATH_PREFERENCE);
        try {
            final int initialMeteredPreference = getCurrentMeteredMultipathPreference(resolver);
            int newMeteredPreference = findNextPrefValue(resolver);
            Settings.Global.putString(resolver, NETWORK_METERED_MULTIPATH_PREFERENCE,
                    Integer.toString(newMeteredPreference));
            setWifiMeteredStatus(ssid, "true");
            waitForActiveNetworkMetered(TRANSPORT_WIFI, true);
            // Wifi meterness changes from unmetered to metered will disconnect and reconnect since
            // R.
            final Network network = ensureWifiConnected();
            assertEquals(ssid, unquoteSSID(mWifiManager.getConnectionInfo().getSSID()));
            assertEquals(mCm.getNetworkCapabilities(network).hasCapability(
                    NET_CAPABILITY_NOT_METERED), false);
            assertMultipathPreferenceIsEventually(network, initialMeteredPreference,
                    newMeteredPreference);

            final int oldMeteredPreference = newMeteredPreference;
            newMeteredPreference = findNextPrefValue(resolver);
            Settings.Global.putString(resolver, NETWORK_METERED_MULTIPATH_PREFERENCE,
                    Integer.toString(newMeteredPreference));
            assertEquals(mCm.getNetworkCapabilities(network).hasCapability(
                    NET_CAPABILITY_NOT_METERED), false);
            assertMultipathPreferenceIsEventually(network,
                    oldMeteredPreference, newMeteredPreference);

            setWifiMeteredStatus(ssid, "false");
            // No disconnect from unmetered to metered.
            waitForActiveNetworkMetered(TRANSPORT_WIFI, false);
            assertEquals(mCm.getNetworkCapabilities(network).hasCapability(
                    NET_CAPABILITY_NOT_METERED), true);
            assertMultipathPreferenceIsEventually(network, newMeteredPreference,
                    ConnectivityManager.MULTIPATH_PREFERENCE_UNMETERED);
        } finally {
            Settings.Global.putString(resolver, NETWORK_METERED_MULTIPATH_PREFERENCE,
                    oldMeteredMultipathPreference);
            setWifiMeteredStatus(ssid, oldMeteredSetting);
        }
    }

    // TODO: move the following socket keep alive test to dedicated test class.
    /**
     * Callback used in tcp keepalive offload that allows caller to wait callback fires.
     */
    private static class TestSocketKeepaliveCallback extends SocketKeepalive.Callback {
        public enum CallbackType { ON_STARTED, ON_STOPPED, ON_ERROR };

        public static class CallbackValue {
            public final CallbackType callbackType;
            public final int error;

            private CallbackValue(final CallbackType type, final int error) {
                this.callbackType = type;
                this.error = error;
            }

            public static class OnStartedCallback extends CallbackValue {
                OnStartedCallback() { super(CallbackType.ON_STARTED, 0); }
            }

            public static class OnStoppedCallback extends CallbackValue {
                OnStoppedCallback() { super(CallbackType.ON_STOPPED, 0); }
            }

            public static class OnErrorCallback extends CallbackValue {
                OnErrorCallback(final int error) { super(CallbackType.ON_ERROR, error); }
            }

            @Override
            public boolean equals(Object o) {
                return o.getClass() == this.getClass()
                        && this.callbackType == ((CallbackValue) o).callbackType
                        && this.error == ((CallbackValue) o).error;
            }

            @Override
            public String toString() {
                return String.format("%s(%s, %d)", getClass().getSimpleName(), callbackType, error);
            }
        }

        private final LinkedBlockingQueue<CallbackValue> mCallbacks = new LinkedBlockingQueue<>();

        @Override
        public void onStarted() {
            mCallbacks.add(new CallbackValue.OnStartedCallback());
        }

        @Override
        public void onStopped() {
            mCallbacks.add(new CallbackValue.OnStoppedCallback());
        }

        @Override
        public void onError(final int error) {
            mCallbacks.add(new CallbackValue.OnErrorCallback(error));
        }

        public CallbackValue pollCallback() {
            try {
                return mCallbacks.poll(KEEPALIVE_CALLBACK_TIMEOUT_MS,
                        TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                fail("Callback not seen after " + KEEPALIVE_CALLBACK_TIMEOUT_MS + " ms");
            }
            return null;
        }
        private void expectCallback(CallbackValue expectedCallback) {
            final CallbackValue actualCallback = pollCallback();
            assertEquals(expectedCallback, actualCallback);
        }

        public void expectStarted() {
            expectCallback(new CallbackValue.OnStartedCallback());
        }

        public void expectStopped() {
            expectCallback(new CallbackValue.OnStoppedCallback());
        }

        public void expectError(int error) {
            expectCallback(new CallbackValue.OnErrorCallback(error));
        }
    }

    private InetAddress getAddrByName(final String hostname, final int family) throws Exception {
        final InetAddress[] allAddrs = InetAddress.getAllByName(hostname);
        for (InetAddress addr : allAddrs) {
            if (family == AF_INET && addr instanceof Inet4Address) return addr;

            if (family == AF_INET6 && addr instanceof Inet6Address) return addr;

            if (family == AF_UNSPEC) return addr;
        }
        return null;
    }

    private Socket getConnectedSocket(final Network network, final String host, final int port,
            final int family) throws Exception {
        final Socket s = network.getSocketFactory().createSocket();
        try {
            final InetAddress addr = getAddrByName(host, family);
            if (addr == null) fail("Fail to get destination address for " + family);

            final InetSocketAddress sockAddr = new InetSocketAddress(addr, port);
            s.connect(sockAddr);
        } catch (Exception e) {
            s.close();
            throw e;
        }
        return s;
    }

    private int getSupportedKeepalivesForNet(@NonNull Network network) throws Exception {
        final NetworkCapabilities nc = mCm.getNetworkCapabilities(network);

        // Get number of supported concurrent keepalives for testing network.
        final int[] keepalivesPerTransport = KeepaliveUtils.getSupportedKeepalives(mContext);
        return KeepaliveUtils.getSupportedKeepalivesForNetworkCapabilities(
                keepalivesPerTransport, nc);
    }

    private static boolean isTcpKeepaliveSupportedByKernel() {
        final String kVersionString = VintfRuntimeInfo.getKernelVersion();
        return compareMajorMinorVersion(kVersionString, "4.8") >= 0;
    }

    private static Pair<Integer, Integer> getVersionFromString(String version) {
        // Only gets major and minor number of the version string.
        final Pattern versionPattern = Pattern.compile("^(\\d+)(\\.(\\d+))?.*");
        final Matcher m = versionPattern.matcher(version);
        if (m.matches()) {
            final int major = Integer.parseInt(m.group(1));
            final int minor = TextUtils.isEmpty(m.group(3)) ? 0 : Integer.parseInt(m.group(3));
            return new Pair<>(major, minor);
        } else {
            return new Pair<>(0, 0);
        }
    }

    // TODO: Move to util class.
    private static int compareMajorMinorVersion(final String s1, final String s2) {
        final Pair<Integer, Integer> v1 = getVersionFromString(s1);
        final Pair<Integer, Integer> v2 = getVersionFromString(s2);

        if (v1.first == v2.first) {
            return Integer.compare(v1.second, v2.second);
        } else {
            return Integer.compare(v1.first, v2.first);
        }
    }

    /**
     * Verifies that version string compare logic returns expected result for various cases.
     * Note that only major and minor number are compared.
     */
    @Test
    public void testMajorMinorVersionCompare() {
        assertEquals(0, compareMajorMinorVersion("4.8.1", "4.8"));
        assertEquals(1, compareMajorMinorVersion("4.9", "4.8.1"));
        assertEquals(1, compareMajorMinorVersion("5.0", "4.8"));
        assertEquals(1, compareMajorMinorVersion("5", "4.8"));
        assertEquals(0, compareMajorMinorVersion("5", "5.0"));
        assertEquals(1, compareMajorMinorVersion("5-beta1", "4.8"));
        assertEquals(0, compareMajorMinorVersion("4.8.0.0", "4.8"));
        assertEquals(0, compareMajorMinorVersion("4.8-RC1", "4.8"));
        assertEquals(0, compareMajorMinorVersion("4.8", "4.8"));
        assertEquals(-1, compareMajorMinorVersion("3.10", "4.8.0"));
        assertEquals(-1, compareMajorMinorVersion("4.7.10.10", "4.8"));
    }

    /**
     * Verifies that the keepalive API cannot create any keepalive when the maximum number of
     * keepalives is set to 0.
     */
    @AppModeFull(reason = "Cannot get WifiManager in instant app mode")
    @Test
    public void testKeepaliveWifiUnsupported() throws Exception {
        if (!mPackageManager.hasSystemFeature(FEATURE_WIFI)) {
            Log.i(TAG, "testKeepaliveUnsupported cannot execute unless device"
                    + " supports WiFi");
            return;
        }

        final Network network = ensureWifiConnected();
        if (getSupportedKeepalivesForNet(network) != 0) return;
        final InetAddress srcAddr = getFirstV4Address(network);
        assumeTrue("This test requires native IPv4", srcAddr != null);

        runWithShellPermissionIdentity(() -> {
            assertEquals(0, createConcurrentSocketKeepalives(network, srcAddr, 1, 0));
            assertEquals(0, createConcurrentSocketKeepalives(network, srcAddr, 0, 1));
        });
    }

    @AppModeFull(reason = "Cannot get WifiManager in instant app mode")
    @Test
    @SkipPresubmit(reason = "Keepalive is not supported on virtual hardware")
    public void testCreateTcpKeepalive() throws Exception {
        if (!mPackageManager.hasSystemFeature(FEATURE_WIFI)) {
            Log.i(TAG, "testCreateTcpKeepalive cannot execute unless device supports WiFi");
            return;
        }

        final Network network = ensureWifiConnected();
        if (getSupportedKeepalivesForNet(network) == 0) return;
        final InetAddress srcAddr = getFirstV4Address(network);
        assumeTrue("This test requires native IPv4", srcAddr != null);

        // If kernel < 4.8 then it doesn't support TCP keepalive, but it might still support
        // NAT-T keepalive. If keepalive limits from resource overlay is not zero, TCP keepalive
        // needs to be supported except if the kernel doesn't support it.
        if (!isTcpKeepaliveSupportedByKernel()) {
            // Sanity check to ensure the callback result is expected.
            runWithShellPermissionIdentity(() -> {
                assertEquals(0, createConcurrentSocketKeepalives(network, srcAddr, 0, 1));
            });
            Log.i(TAG, "testCreateTcpKeepalive is skipped for kernel "
                    + VintfRuntimeInfo.getKernelVersion());
            return;
        }

        final byte[] requestBytes = CtsNetUtils.HTTP_REQUEST.getBytes("UTF-8");
        // So far only ipv4 tcp keepalive offload is supported.
        // TODO: add test case for ipv6 tcp keepalive offload when it is supported.
        try (Socket s = getConnectedSocket(network, TEST_HOST, HTTP_PORT, AF_INET)) {

            // Should able to start keep alive offload when socket is idle.
            final Executor executor = mContext.getMainExecutor();
            final TestSocketKeepaliveCallback callback = new TestSocketKeepaliveCallback();

            mUiAutomation.adoptShellPermissionIdentity();
            try (SocketKeepalive sk = mCm.createSocketKeepalive(network, s, executor, callback)) {
                sk.start(MIN_KEEPALIVE_INTERVAL);
                callback.expectStarted();

                // App should not able to write during keepalive offload.
                final OutputStream out = s.getOutputStream();
                try {
                    out.write(requestBytes);
                    fail("Should not able to write");
                } catch (IOException e) { }
                // App should not able to read during keepalive offload.
                final InputStream in = s.getInputStream();
                byte[] responseBytes = new byte[4096];
                try {
                    in.read(responseBytes);
                    fail("Should not able to read");
                } catch (IOException e) { }

                // Stop.
                sk.stop();
                callback.expectStopped();
            } finally {
                mUiAutomation.dropShellPermissionIdentity();
            }

            // Ensure socket is still connected.
            assertTrue(s.isConnected());
            assertFalse(s.isClosed());

            // Let socket be not idle.
            try {
                final OutputStream out = s.getOutputStream();
                out.write(requestBytes);
            } catch (IOException e) {
                fail("Failed to write data " + e);
            }
            // Make sure response data arrives.
            final MessageQueue fdHandlerQueue = Looper.getMainLooper().getQueue();
            final FileDescriptor fd = s.getFileDescriptor$();
            final CountDownLatch mOnReceiveLatch = new CountDownLatch(1);
            fdHandlerQueue.addOnFileDescriptorEventListener(fd, EVENT_INPUT, (readyFd, events) -> {
                mOnReceiveLatch.countDown();
                return 0; // Unregister listener.
            });
            if (!mOnReceiveLatch.await(2, TimeUnit.SECONDS)) {
                fdHandlerQueue.removeOnFileDescriptorEventListener(fd);
                fail("Timeout: no response data");
            }

            // Should get ERROR_SOCKET_NOT_IDLE because there is still data in the receive queue
            // that has not been read.
            mUiAutomation.adoptShellPermissionIdentity();
            try (SocketKeepalive sk = mCm.createSocketKeepalive(network, s, executor, callback)) {
                sk.start(MIN_KEEPALIVE_INTERVAL);
                callback.expectError(SocketKeepalive.ERROR_SOCKET_NOT_IDLE);
            } finally {
                mUiAutomation.dropShellPermissionIdentity();
            }
        }
    }

    private ArrayList<SocketKeepalive> createConcurrentKeepalivesOfType(
            int requestCount, @NonNull TestSocketKeepaliveCallback callback,
            Supplier<SocketKeepalive> kaFactory) {
        final ArrayList<SocketKeepalive> kalist = new ArrayList<>();

        int remainingRetries = MAX_KEEPALIVE_RETRY_COUNT;

        // Test concurrent keepalives with the given supplier.
        while (kalist.size() < requestCount) {
            final SocketKeepalive ka = kaFactory.get();
            ka.start(MIN_KEEPALIVE_INTERVAL);
            TestSocketKeepaliveCallback.CallbackValue cv = callback.pollCallback();
            assertNotNull(cv);
            if (cv.callbackType == TestSocketKeepaliveCallback.CallbackType.ON_ERROR) {
                if (kalist.size() == 0 && cv.error == SocketKeepalive.ERROR_UNSUPPORTED) {
                    // Unsupported.
                    break;
                } else if (cv.error == SocketKeepalive.ERROR_INSUFFICIENT_RESOURCES) {
                    // Limit reached or temporary unavailable due to stopped slot is not yet
                    // released.
                    if (remainingRetries > 0) {
                        SystemClock.sleep(INTERVAL_KEEPALIVE_RETRY_MS);
                        remainingRetries--;
                        continue;
                    }
                    break;
                }
            }
            if (cv.callbackType == TestSocketKeepaliveCallback.CallbackType.ON_STARTED) {
                kalist.add(ka);
            } else {
                fail("Unexpected error when creating " + (kalist.size() + 1) + " "
                        + ka.getClass().getSimpleName() + ": " + cv);
            }
        }

        return kalist;
    }

    private @NonNull ArrayList<SocketKeepalive> createConcurrentNattSocketKeepalives(
            @NonNull Network network, @NonNull InetAddress srcAddr, int requestCount,
            @NonNull TestSocketKeepaliveCallback callback)  throws Exception {

        final Executor executor = mContext.getMainExecutor();

        // Initialize a real NaT-T socket.
        final IpSecManager mIpSec = (IpSecManager) mContext.getSystemService(Context.IPSEC_SERVICE);
        final UdpEncapsulationSocket nattSocket = mIpSec.openUdpEncapsulationSocket();
        final InetAddress dstAddr = getAddrByName(TEST_HOST, AF_INET);
        assertNotNull(srcAddr);
        assertNotNull(dstAddr);

        // Test concurrent Nat-T keepalives.
        final ArrayList<SocketKeepalive> result = createConcurrentKeepalivesOfType(requestCount,
                callback, () -> mCm.createSocketKeepalive(network, nattSocket,
                        srcAddr, dstAddr, executor, callback));

        nattSocket.close();
        return result;
    }

    private @NonNull ArrayList<SocketKeepalive> createConcurrentTcpSocketKeepalives(
            @NonNull Network network, int requestCount,
            @NonNull TestSocketKeepaliveCallback callback) {
        final Executor executor = mContext.getMainExecutor();

        // Create concurrent TCP keepalives.
        return createConcurrentKeepalivesOfType(requestCount, callback, () -> {
            // Assert that TCP connections can be established. The file descriptor of tcp
            // sockets will be duplicated and kept valid in service side if the keepalives are
            // successfully started.
            try (Socket tcpSocket = getConnectedSocket(network, TEST_HOST, HTTP_PORT,
                    AF_INET)) {
                return mCm.createSocketKeepalive(network, tcpSocket, executor, callback);
            } catch (Exception e) {
                fail("Unexpected error when creating TCP socket: " + e);
            }
            return null;
        });
    }

    /**
     * Creates concurrent keepalives until the specified counts of each type of keepalives are
     * reached or the expected error callbacks are received for each type of keepalives.
     *
     * @return the total number of keepalives created.
     */
    private int createConcurrentSocketKeepalives(
            @NonNull Network network, @NonNull InetAddress srcAddr, int nattCount, int tcpCount)
            throws Exception {
        final ArrayList<SocketKeepalive> kalist = new ArrayList<>();
        final TestSocketKeepaliveCallback callback = new TestSocketKeepaliveCallback();

        kalist.addAll(createConcurrentNattSocketKeepalives(network, srcAddr, nattCount, callback));
        kalist.addAll(createConcurrentTcpSocketKeepalives(network, tcpCount, callback));

        final int ret = kalist.size();

        // Clean up.
        for (final SocketKeepalive ka : kalist) {
            ka.stop();
            callback.expectStopped();
        }
        kalist.clear();

        return ret;
    }

    /**
     * Verifies that the concurrent keepalive slots meet the minimum requirement, and don't
     * get leaked after iterations.
     */
    @AppModeFull(reason = "Cannot get WifiManager in instant app mode")
    @Test
    @SkipPresubmit(reason = "Keepalive is not supported on virtual hardware")
    public void testSocketKeepaliveLimitWifi() throws Exception {
        if (!mPackageManager.hasSystemFeature(FEATURE_WIFI)) {
            Log.i(TAG, "testSocketKeepaliveLimitWifi cannot execute unless device"
                    + " supports WiFi");
            return;
        }

        final Network network = ensureWifiConnected();
        final int supported = getSupportedKeepalivesForNet(network);
        if (supported == 0) {
            return;
        }
        final InetAddress srcAddr = getFirstV4Address(network);
        assumeTrue("This test requires native IPv4", srcAddr != null);

        runWithShellPermissionIdentity(() -> {
            // Verifies that the supported keepalive slots meet MIN_SUPPORTED_KEEPALIVE_COUNT.
            assertGreaterOrEqual(supported, MIN_SUPPORTED_WIFI_KEEPALIVE_COUNT);

            // Verifies that Nat-T keepalives can be established.
            assertEquals(supported, createConcurrentSocketKeepalives(network, srcAddr,
                    supported + 1, 0));
            // Verifies that keepalives don't get leaked in second round.
            assertEquals(supported, createConcurrentSocketKeepalives(network, srcAddr, supported,
                    0));
        });

        // If kernel < 4.8 then it doesn't support TCP keepalive, but it might still support
        // NAT-T keepalive. Test below cases only if TCP keepalive is supported by kernel.
        if (!isTcpKeepaliveSupportedByKernel()) return;

        runWithShellPermissionIdentity(() -> {
            assertEquals(supported, createConcurrentSocketKeepalives(network, srcAddr, 0,
                    supported + 1));

            // Verifies that different types can be established at the same time.
            assertEquals(supported, createConcurrentSocketKeepalives(network, srcAddr,
                    supported / 2, supported - supported / 2));

            // Verifies that keepalives don't get leaked in second round.
            assertEquals(supported, createConcurrentSocketKeepalives(network, srcAddr, 0,
                    supported));
            assertEquals(supported, createConcurrentSocketKeepalives(network, srcAddr,
                    supported / 2, supported - supported / 2));
        });
    }

    /**
     * Verifies that the concurrent keepalive slots meet the minimum telephony requirement, and
     * don't get leaked after iterations.
     */
    @AppModeFull(reason = "Cannot request network in instant app mode")
    @Test
    @SkipPresubmit(reason = "Keepalive is not supported on virtual hardware")
    public void testSocketKeepaliveLimitTelephony() throws Exception {
        if (!mPackageManager.hasSystemFeature(FEATURE_TELEPHONY)) {
            Log.i(TAG, "testSocketKeepaliveLimitTelephony cannot execute unless device"
                    + " supports telephony");
            return;
        }

        final int firstSdk = Build.VERSION.FIRST_SDK_INT;
        if (firstSdk < Build.VERSION_CODES.Q) {
            Log.i(TAG, "testSocketKeepaliveLimitTelephony: skip test for devices launching"
                    + " before Q: " + firstSdk);
            return;
        }

        final Network network = mCtsNetUtils.connectToCell();
        final int supported = getSupportedKeepalivesForNet(network);
        final InetAddress srcAddr = getFirstV4Address(network);
        assumeTrue("This test requires native IPv4", srcAddr != null);

        runWithShellPermissionIdentity(() -> {
            // Verifies that the supported keepalive slots meet minimum requirement.
            assertGreaterOrEqual(supported, MIN_SUPPORTED_CELLULAR_KEEPALIVE_COUNT);
            // Verifies that Nat-T keepalives can be established.
            assertEquals(supported, createConcurrentSocketKeepalives(network, srcAddr,
                    supported + 1, 0));
            // Verifies that keepalives don't get leaked in second round.
            assertEquals(supported, createConcurrentSocketKeepalives(network, srcAddr, supported,
                    0));
        });
    }

    private int getIntResourceForName(@NonNull String resName) {
        final Resources r = mContext.getResources();
        final int resId = r.getIdentifier(resName, "integer", "android");
        return r.getInteger(resId);
    }

    /**
     * Verifies that the keepalive slots are limited as customized for unprivileged requests.
     */
    @AppModeFull(reason = "Cannot get WifiManager in instant app mode")
    @Test
    @SkipPresubmit(reason = "Keepalive is not supported on virtual hardware")
    public void testSocketKeepaliveUnprivileged() throws Exception {
        if (!mPackageManager.hasSystemFeature(FEATURE_WIFI)) {
            Log.i(TAG, "testSocketKeepaliveUnprivileged cannot execute unless device"
                    + " supports WiFi");
            return;
        }

        final Network network = ensureWifiConnected();
        final int supported = getSupportedKeepalivesForNet(network);
        if (supported == 0) {
            return;
        }
        final InetAddress srcAddr = getFirstV4Address(network);
        assumeTrue("This test requires native IPv4", srcAddr != null);

        // Resource ID might be shifted on devices that compiled with different symbols.
        // Thus, resolve ID at runtime is needed.
        final int allowedUnprivilegedPerUid =
                getIntResourceForName(KEEPALIVE_ALLOWED_UNPRIVILEGED_RES_NAME);
        final int reservedPrivilegedSlots =
                getIntResourceForName(KEEPALIVE_RESERVED_PER_SLOT_RES_NAME);
        // Verifies that unprivileged request per uid cannot exceed the limit customized in the
        // resource. Currently, unprivileged keepalive slots are limited to Nat-T only, this test
        // does not apply to TCP.
        assertGreaterOrEqual(supported, reservedPrivilegedSlots);
        assertGreaterOrEqual(supported, allowedUnprivilegedPerUid);
        final int expectedUnprivileged =
                Math.min(allowedUnprivilegedPerUid, supported - reservedPrivilegedSlots);
        assertEquals(expectedUnprivileged,
                createConcurrentSocketKeepalives(network, srcAddr, supported + 1, 0));
    }

    private static void assertGreaterOrEqual(long greater, long lesser) {
        assertTrue("" + greater + " expected to be greater than or equal to " + lesser,
                greater >= lesser);
    }

    /**
     * Verifies that apps are not allowed to access restricted networks even if they declare the
     * CONNECTIVITY_USE_RESTRICTED_NETWORKS permission in their manifests.
     * See. b/144679405.
     */
    @AppModeFull(reason = "Cannot get WifiManager in instant app mode")
    @Test
    public void testRestrictedNetworkPermission() throws Exception {
        // Ensure that CONNECTIVITY_USE_RESTRICTED_NETWORKS isn't granted to this package.
        final PackageInfo app = mPackageManager.getPackageInfo(mContext.getPackageName(),
                GET_PERMISSIONS);
        final int index = ArrayUtils.indexOf(
                app.requestedPermissions, CONNECTIVITY_USE_RESTRICTED_NETWORKS);
        assertTrue(index >= 0);
        assertTrue(app.requestedPermissionsFlags[index] != PERMISSION_GRANTED);

        // Ensure that NetworkUtils.queryUserAccess always returns false since this package should
        // not have netd system permission to call this function.
        final Network wifiNetwork = ensureWifiConnected();
        assertFalse(NetworkUtils.queryUserAccess(Binder.getCallingUid(), wifiNetwork.netId));

        // Ensure that this package cannot bind to any restricted network that's currently
        // connected.
        Network[] networks = mCm.getAllNetworks();
        for (Network network : networks) {
            NetworkCapabilities nc = mCm.getNetworkCapabilities(network);
            if (nc != null && !nc.hasCapability(NET_CAPABILITY_NOT_RESTRICTED)) {
                try {
                    network.bindSocket(new Socket());
                    fail("Bind to restricted network " + network + " unexpectedly succeeded");
                } catch (IOException expected) {}
            }
        }
    }
}
