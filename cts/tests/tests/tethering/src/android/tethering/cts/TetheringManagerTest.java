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
package android.tethering.test;

import static android.content.pm.PackageManager.FEATURE_TELEPHONY;
import static android.net.NetworkCapabilities.NET_CAPABILITY_DUN;
import static android.net.NetworkCapabilities.NET_CAPABILITY_INTERNET;
import static android.net.NetworkCapabilities.TRANSPORT_CELLULAR;
import static android.net.NetworkCapabilities.TRANSPORT_ETHERNET;
import static android.net.TetheringManager.TETHERING_USB;
import static android.net.TetheringManager.TETHERING_WIFI;
import static android.net.TetheringManager.TETHERING_WIFI_P2P;
import static android.net.TetheringManager.TETHER_ERROR_ENTITLEMENT_UNKNOWN;
import static android.net.TetheringManager.TETHER_ERROR_NO_CHANGE_TETHERING_PERMISSION;
import static android.net.TetheringManager.TETHER_ERROR_NO_ERROR;
import static android.net.TetheringManager.TETHER_HARDWARE_OFFLOAD_FAILED;
import static android.net.TetheringManager.TETHER_HARDWARE_OFFLOAD_STARTED;
import static android.net.TetheringManager.TETHER_HARDWARE_OFFLOAD_STOPPED;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeTrue;

import android.app.UiAutomation;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.LinkAddress;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.TetheredClient;
import android.net.TetheringManager;
import android.net.TetheringManager.OnTetheringEntitlementResultListener;
import android.net.TetheringManager.TetheringEventCallback;
import android.net.TetheringManager.TetheringInterfaceRegexps;
import android.net.TetheringManager.TetheringRequest;
import android.net.cts.util.CtsNetUtils;
import android.net.cts.util.CtsNetUtils.TestNetworkCallback;
import android.net.wifi.WifiClient;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiManager.SoftApCallback;
import android.os.Bundle;
import android.os.ConditionVariable;
import android.os.PersistableBundle;
import android.os.ResultReceiver;
import android.telephony.CarrierConfigManager;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;

import androidx.annotation.NonNull;
import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.testutils.ArrayTrackRecord;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;

@RunWith(AndroidJUnit4.class)
public class TetheringManagerTest {

    private Context mContext;

    private ConnectivityManager mCm;
    private TetheringManager mTM;
    private WifiManager mWm;
    private PackageManager mPm;

    private TetherChangeReceiver mTetherChangeReceiver;
    private CtsNetUtils mCtsNetUtils;

    private static final int DEFAULT_TIMEOUT_MS = 60_000;

    private void adoptShellPermissionIdentity() {
        final UiAutomation uiAutomation =
                InstrumentationRegistry.getInstrumentation().getUiAutomation();
        uiAutomation.adoptShellPermissionIdentity();
    }

    private void dropShellPermissionIdentity() {
        final UiAutomation uiAutomation =
                InstrumentationRegistry.getInstrumentation().getUiAutomation();
        uiAutomation.dropShellPermissionIdentity();
    }

    @Before
    public void setUp() throws Exception {
        adoptShellPermissionIdentity();
        mContext = InstrumentationRegistry.getContext();
        mCm = (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        mTM = (TetheringManager) mContext.getSystemService(Context.TETHERING_SERVICE);
        mWm = (WifiManager) mContext.getSystemService(Context.WIFI_SERVICE);
        mPm = mContext.getPackageManager();
        mCtsNetUtils = new CtsNetUtils(mContext);
        mTetherChangeReceiver = new TetherChangeReceiver();
        final IntentFilter filter = new IntentFilter(
                TetheringManager.ACTION_TETHER_STATE_CHANGED);
        final Intent intent = mContext.registerReceiver(mTetherChangeReceiver, filter);
        if (intent != null) mTetherChangeReceiver.onReceive(null, intent);
    }

    @After
    public void tearDown() throws Exception {
        mTM.stopAllTethering();
        mContext.unregisterReceiver(mTetherChangeReceiver);
        dropShellPermissionIdentity();
    }

    private static class StopSoftApCallback implements SoftApCallback {
        private final ConditionVariable mWaiting = new ConditionVariable();
        @Override
        public void onStateChanged(int state, int failureReason) {
            if (state == WifiManager.WIFI_AP_STATE_DISABLED) mWaiting.open();
        }

        @Override
        public void onConnectedClientsChanged(List<WifiClient> clients) { }

        public void waitForSoftApStopped() {
            if (!mWaiting.block(DEFAULT_TIMEOUT_MS)) {
                fail("stopSoftAp Timeout");
            }
        }
    }

    // Wait for softAp to be disabled. This is necessary on devices where stopping softAp
    // deletes the interface. On these devices, tethering immediately stops when the softAp
    // interface is removed, but softAp is not yet fully disabled. Wait for softAp to be
    // fully disabled, because otherwise the next test might fail because it attempts to
    // start softAp before it's fully stopped.
    private void expectSoftApDisabled() {
        final StopSoftApCallback callback = new StopSoftApCallback();
        try {
            mWm.registerSoftApCallback(c -> c.run(), callback);
            // registerSoftApCallback will immediately call the callback with the current state, so
            // this callback will fire even if softAp is already disabled.
            callback.waitForSoftApStopped();
        } finally {
            mWm.unregisterSoftApCallback(callback);
        }
    }

    private class TetherChangeReceiver extends BroadcastReceiver {
        private class TetherState {
            final ArrayList<String> mAvailable;
            final ArrayList<String> mActive;
            final ArrayList<String> mErrored;

            TetherState(Intent intent) {
                mAvailable = intent.getStringArrayListExtra(
                        TetheringManager.EXTRA_AVAILABLE_TETHER);
                mActive = intent.getStringArrayListExtra(
                        TetheringManager.EXTRA_ACTIVE_TETHER);
                mErrored = intent.getStringArrayListExtra(
                        TetheringManager.EXTRA_ERRORED_TETHER);
            }
        }

        @Override
        public void onReceive(Context content, Intent intent) {
            String action = intent.getAction();
            if (action.equals(TetheringManager.ACTION_TETHER_STATE_CHANGED)) {
                mResult.add(new TetherState(intent));
            }
        }

        public final LinkedBlockingQueue<TetherState> mResult = new LinkedBlockingQueue<>();

        // Expects that tethering reaches the desired state.
        // - If active is true, expects that tethering is enabled on at least one interface
        //   matching ifaceRegexs.
        // - If active is false, expects that tethering is disabled on all the interfaces matching
        //   ifaceRegexs.
        // Fails if any interface matching ifaceRegexs becomes errored.
        public void expectTethering(final boolean active, final String[] ifaceRegexs) {
            while (true) {
                final TetherState state = pollAndAssertNoError(DEFAULT_TIMEOUT_MS, ifaceRegexs);
                assertNotNull("Did not receive expected state change, active: " + active, state);

                if (isIfaceActive(ifaceRegexs, state) == active) return;
            }
        }

        private TetherState pollAndAssertNoError(final int timeout, final String[] ifaceRegexs) {
            final TetherState state = pollTetherState(timeout);
            assertNoErroredIfaces(state, ifaceRegexs);
            return state;
        }

        private TetherState pollTetherState(final int timeout) {
            try {
                return mResult.poll(timeout, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                fail("No result after " + timeout + " ms");
                return null;
            }
        }

        private boolean isIfaceActive(final String[] ifaceRegexs, final TetherState state) {
            return isIfaceMatch(ifaceRegexs, state.mActive);
        }

        private void assertNoErroredIfaces(final TetherState state, final String[] ifaceRegexs) {
            if (state == null || state.mErrored == null) return;

            if (isIfaceMatch(ifaceRegexs, state.mErrored)) {
                fail("Found failed tethering interfaces: " + Arrays.toString(state.mErrored.toArray()));
            }
        }
    }

    private static class StartTetheringCallback implements TetheringManager.StartTetheringCallback {
        private static int TIMEOUT_MS = 30_000;
        public static class CallbackValue {
            public final int error;

            private CallbackValue(final int e) {
                error = e;
            }

            public static class OnTetheringStarted extends CallbackValue {
                OnTetheringStarted() { super(TETHER_ERROR_NO_ERROR); }
            }

            public static class OnTetheringFailed extends CallbackValue {
                OnTetheringFailed(final int error) { super(error); }
            }

            @Override
            public String toString() {
                return String.format("%s(%d)", getClass().getSimpleName(), error);
            }
        }

        private final ArrayTrackRecord<CallbackValue>.ReadHead mHistory =
                new ArrayTrackRecord<CallbackValue>().newReadHead();

        @Override
        public void onTetheringStarted() {
            mHistory.add(new CallbackValue.OnTetheringStarted());
        }

        @Override
        public void onTetheringFailed(final int error) {
            mHistory.add(new CallbackValue.OnTetheringFailed(error));
        }

        public void verifyTetheringStarted() {
            final CallbackValue cv = mHistory.poll(TIMEOUT_MS, c -> true);
            assertNotNull("No onTetheringStarted after " + TIMEOUT_MS + " ms", cv);
            assertTrue("Fail start tethering:" + cv,
                    cv instanceof CallbackValue.OnTetheringStarted);
        }

        public void expectTetheringFailed(final int expected) throws InterruptedException {
            final CallbackValue cv = mHistory.poll(TIMEOUT_MS, c -> true);
            assertNotNull("No onTetheringFailed after " + TIMEOUT_MS + " ms", cv);
            assertTrue("Expect fail with error code " + expected + ", but received: " + cv,
                (cv instanceof CallbackValue.OnTetheringFailed) && (cv.error == expected));
        }
    }

    private static boolean isIfaceMatch(final List<String> ifaceRegexs,
            final List<String> ifaces) {
        return isIfaceMatch(ifaceRegexs.toArray(new String[0]), ifaces);
    }

    private static boolean isIfaceMatch(final String[] ifaceRegexs, final List<String> ifaces) {
        if (ifaceRegexs == null) fail("ifaceRegexs should not be null");

        if (ifaces == null) return false;

        for (String s : ifaces) {
            for (String regex : ifaceRegexs) {
                if (s.matches(regex)) {
                    return true;
                }
            }
        }
        return false;
    }

    @Test
    public void testStartTetheringWithStateChangeBroadcast() throws Exception {
        if (!mTM.isTetheringSupported()) return;

        final String[] wifiRegexs = mTM.getTetherableWifiRegexs();
        if (wifiRegexs.length == 0) return;

        final String[] tetheredIfaces = mTM.getTetheredIfaces();
        assertTrue(tetheredIfaces.length == 0);

        final StartTetheringCallback startTetheringCallback = new StartTetheringCallback();
        final TetheringRequest request = new TetheringRequest.Builder(TETHERING_WIFI)
                .setShouldShowEntitlementUi(false).build();
        mTM.startTethering(request, c -> c.run() /* executor */, startTetheringCallback);
        startTetheringCallback.verifyTetheringStarted();

        mTetherChangeReceiver.expectTethering(true /* active */, wifiRegexs);

        mTM.stopTethering(TETHERING_WIFI);
        expectSoftApDisabled();
        mTetherChangeReceiver.expectTethering(false /* active */, wifiRegexs);
    }

    @Test
    public void testTetheringRequest() {
        final TetheringRequest tr = new TetheringRequest.Builder(TETHERING_WIFI).build();
        assertEquals(TETHERING_WIFI, tr.getTetheringType());
        assertNull(tr.getLocalIpv4Address());
        assertNull(tr.getClientStaticIpv4Address());
        assertFalse(tr.isExemptFromEntitlementCheck());
        assertTrue(tr.getShouldShowEntitlementUi());

        final LinkAddress localAddr = new LinkAddress("192.168.24.5/24");
        final LinkAddress clientAddr = new LinkAddress("192.168.24.100/24");
        final TetheringRequest tr2 = new TetheringRequest.Builder(TETHERING_USB)
                .setStaticIpv4Addresses(localAddr, clientAddr)
                .setExemptFromEntitlementCheck(true)
                .setShouldShowEntitlementUi(false).build();

        assertEquals(localAddr, tr2.getLocalIpv4Address());
        assertEquals(clientAddr, tr2.getClientStaticIpv4Address());
        assertEquals(TETHERING_USB, tr2.getTetheringType());
        assertTrue(tr2.isExemptFromEntitlementCheck());
        assertFalse(tr2.getShouldShowEntitlementUi());
    }

    // Must poll the callback before looking at the member.
    private static class TestTetheringEventCallback implements TetheringEventCallback {
        private static final int TIMEOUT_MS = 30_000;

        public enum CallbackType {
            ON_SUPPORTED,
            ON_UPSTREAM,
            ON_TETHERABLE_REGEX,
            ON_TETHERABLE_IFACES,
            ON_TETHERED_IFACES,
            ON_ERROR,
            ON_CLIENTS,
            ON_OFFLOAD_STATUS,
        };

        public static class CallbackValue {
            public final CallbackType callbackType;
            public final Object callbackParam;
            public final int callbackParam2;

            private CallbackValue(final CallbackType type, final Object param, final int param2) {
                this.callbackType = type;
                this.callbackParam = param;
                this.callbackParam2 = param2;
            }
        }

        private final ArrayTrackRecord<CallbackValue> mHistory =
                new ArrayTrackRecord<CallbackValue>();

        private final ArrayTrackRecord<CallbackValue>.ReadHead mCurrent =
                mHistory.newReadHead();

        private TetheringInterfaceRegexps mTetherableRegex;
        private List<String> mTetherableIfaces;
        private List<String> mTetheredIfaces;

        @Override
        public void onTetheringSupported(boolean supported) {
            mHistory.add(new CallbackValue(CallbackType.ON_SUPPORTED, null, (supported ? 1 : 0)));
        }

        @Override
        public void onUpstreamChanged(Network network) {
            mHistory.add(new CallbackValue(CallbackType.ON_UPSTREAM, network, 0));
        }

        @Override
        public void onTetherableInterfaceRegexpsChanged(TetheringInterfaceRegexps reg) {
            mTetherableRegex = reg;
            mHistory.add(new CallbackValue(CallbackType.ON_TETHERABLE_REGEX, reg, 0));
        }

        @Override
        public void onTetherableInterfacesChanged(List<String> interfaces) {
            mTetherableIfaces = interfaces;
            mHistory.add(new CallbackValue(CallbackType.ON_TETHERABLE_IFACES, interfaces, 0));
        }

        @Override
        public void onTetheredInterfacesChanged(List<String> interfaces) {
            mTetheredIfaces = interfaces;
            mHistory.add(new CallbackValue(CallbackType.ON_TETHERED_IFACES, interfaces, 0));
        }

        @Override
        public void onError(String ifName, int error) {
            mHistory.add(new CallbackValue(CallbackType.ON_ERROR, ifName, error));
        }

        @Override
        public void onClientsChanged(Collection<TetheredClient> clients) {
            mHistory.add(new CallbackValue(CallbackType.ON_CLIENTS, clients, 0));
        }

        @Override
        public void onOffloadStatusChanged(int status) {
            mHistory.add(new CallbackValue(CallbackType.ON_OFFLOAD_STATUS, status, 0));
        }

        public void expectTetherableInterfacesChanged(@NonNull List<String> regexs) {
            assertNotNull("No expected tetherable ifaces callback", mCurrent.poll(TIMEOUT_MS,
                (cv) -> {
                    if (cv.callbackType != CallbackType.ON_TETHERABLE_IFACES) return false;
                    final List<String> interfaces = (List<String>) cv.callbackParam;
                    return isIfaceMatch(regexs, interfaces);
                }));
        }

        public void expectTetheredInterfacesChanged(@NonNull List<String> regexs) {
            assertNotNull("No expected tethered ifaces callback", mCurrent.poll(TIMEOUT_MS,
                (cv) -> {
                    if (cv.callbackType != CallbackType.ON_TETHERED_IFACES) return false;

                    final List<String> interfaces = (List<String>) cv.callbackParam;

                    // Null regexs means no active tethering.
                    if (regexs == null) return interfaces.isEmpty();

                    return isIfaceMatch(regexs, interfaces);
                }));
        }

        public void expectCallbackStarted() {
            int receivedBitMap = 0;
            // The each bit represent a type from CallbackType.ON_*.
            // Expect all of callbacks except for ON_ERROR.
            final int expectedBitMap = 0xff ^ (1 << CallbackType.ON_ERROR.ordinal());
            // Receive ON_ERROR on started callback is not matter. It just means tethering is
            // failed last time, should able to continue the test this time.
            while ((receivedBitMap & expectedBitMap) != expectedBitMap) {
                final CallbackValue cv = mCurrent.poll(TIMEOUT_MS, c -> true);
                if (cv == null) {
                    fail("No expected callbacks, " + "expected bitmap: "
                            + expectedBitMap + ", actual: " + receivedBitMap);
                }

                receivedBitMap |= (1 << cv.callbackType.ordinal());
            }
        }

        public void expectOneOfOffloadStatusChanged(int... offloadStatuses) {
            assertNotNull("No offload status changed", mCurrent.poll(TIMEOUT_MS, (cv) -> {
                if (cv.callbackType != CallbackType.ON_OFFLOAD_STATUS) return false;

                final int status = (int) cv.callbackParam;
                for (int offloadStatus : offloadStatuses) if (offloadStatus == status) return true;

                return false;
            }));
        }

        public void expectErrorOrTethered(final String iface) {
            assertNotNull("No expected callback", mCurrent.poll(TIMEOUT_MS, (cv) -> {
                if (cv.callbackType == CallbackType.ON_ERROR
                        && iface.equals((String) cv.callbackParam)) {
                    return true;
                }
                if (cv.callbackType == CallbackType.ON_TETHERED_IFACES
                        && ((List<String>) cv.callbackParam).contains(iface)) {
                    return true;
                }

                return false;
            }));
        }

        public Network getCurrentValidUpstream() {
            final CallbackValue result = mCurrent.poll(TIMEOUT_MS, (cv) -> {
                return (cv.callbackType == CallbackType.ON_UPSTREAM)
                        && cv.callbackParam != null;
            });

            assertNotNull("No valid upstream", result);
            return (Network) result.callbackParam;
        }

        public void assumeTetheringSupported() {
            final ArrayTrackRecord<CallbackValue>.ReadHead history =
                    mHistory.newReadHead();
            assertNotNull("No onSupported callback", history.poll(TIMEOUT_MS, (cv) -> {
                if (cv.callbackType != CallbackType.ON_SUPPORTED) return false;

                assumeTrue(cv.callbackParam2 == 1 /* supported */);
                return true;
            }));
        }

        public TetheringInterfaceRegexps getTetheringInterfaceRegexps() {
            return mTetherableRegex;
        }

        public List<String> getTetherableInterfaces() {
            return mTetherableIfaces;
        }

        public List<String> getTetheredInterfaces() {
            return mTetheredIfaces;
        }
    }

    private TestTetheringEventCallback registerTetheringEventCallback() {
        final TestTetheringEventCallback tetherEventCallback =
                new TestTetheringEventCallback();

        mTM.registerTetheringEventCallback(c -> c.run() /* executor */, tetherEventCallback);
        tetherEventCallback.expectCallbackStarted();

        return tetherEventCallback;
    }

    private void unregisterTetheringEventCallback(final TestTetheringEventCallback callback) {
        mTM.unregisterTetheringEventCallback(callback);
    }

    private List<String> getWifiTetherableInterfaceRegexps(
            final TestTetheringEventCallback callback) {
        return callback.getTetheringInterfaceRegexps().getTetherableWifiRegexs();
    }

    private boolean isWifiTetheringSupported(final TestTetheringEventCallback callback) {
        return !getWifiTetherableInterfaceRegexps(callback).isEmpty();
    }

    private void startWifiTethering(final TestTetheringEventCallback callback)
            throws InterruptedException {
        final List<String> wifiRegexs = getWifiTetherableInterfaceRegexps(callback);
        assertFalse(isIfaceMatch(wifiRegexs, callback.getTetheredInterfaces()));

        final StartTetheringCallback startTetheringCallback = new StartTetheringCallback();
        final TetheringRequest request = new TetheringRequest.Builder(TETHERING_WIFI)
                .setShouldShowEntitlementUi(false).build();
        mTM.startTethering(request, c -> c.run() /* executor */, startTetheringCallback);
        startTetheringCallback.verifyTetheringStarted();

        callback.expectTetheredInterfacesChanged(wifiRegexs);

        callback.expectOneOfOffloadStatusChanged(
                TETHER_HARDWARE_OFFLOAD_STARTED,
                TETHER_HARDWARE_OFFLOAD_FAILED);
    }

    private void stopWifiTethering(final TestTetheringEventCallback callback) {
        mTM.stopTethering(TETHERING_WIFI);
        expectSoftApDisabled();
        callback.expectTetheredInterfacesChanged(null);
        callback.expectOneOfOffloadStatusChanged(TETHER_HARDWARE_OFFLOAD_STOPPED);
    }

    @Test
    public void testRegisterTetheringEventCallback() throws Exception {
        final TestTetheringEventCallback tetherEventCallback = registerTetheringEventCallback();
        tetherEventCallback.assumeTetheringSupported();

        if (!isWifiTetheringSupported(tetherEventCallback)) {
            unregisterTetheringEventCallback(tetherEventCallback);
            return;
        }

        startWifiTethering(tetherEventCallback);

        final List<String> tetheredIfaces = tetherEventCallback.getTetheredInterfaces();
        assertEquals(1, tetheredIfaces.size());
        final String wifiTetheringIface = tetheredIfaces.get(0);

        stopWifiTethering(tetherEventCallback);

        try {
            final int ret = mTM.tether(wifiTetheringIface);

            // There is no guarantee that the wifi interface will be available after disabling
            // the hotspot, so don't fail the test if the call to tether() fails.
            assumeTrue(ret == TETHER_ERROR_NO_ERROR);

            // If calling #tether successful, there is a callback to tell the result of tethering
            // setup.
            tetherEventCallback.expectErrorOrTethered(wifiTetheringIface);
        } finally {
            mTM.untether(wifiTetheringIface);
            unregisterTetheringEventCallback(tetherEventCallback);
        }
    }

    @Test
    public void testGetTetherableInterfaceRegexps() {
        final TestTetheringEventCallback tetherEventCallback = registerTetheringEventCallback();
        tetherEventCallback.assumeTetheringSupported();

        final TetheringInterfaceRegexps tetherableRegexs =
                tetherEventCallback.getTetheringInterfaceRegexps();
        final List<String> wifiRegexs = tetherableRegexs.getTetherableWifiRegexs();
        final List<String> usbRegexs = tetherableRegexs.getTetherableUsbRegexs();
        final List<String> btRegexs = tetherableRegexs.getTetherableBluetoothRegexs();

        assertEquals(wifiRegexs, Arrays.asList(mTM.getTetherableWifiRegexs()));
        assertEquals(usbRegexs, Arrays.asList(mTM.getTetherableUsbRegexs()));
        assertEquals(btRegexs, Arrays.asList(mTM.getTetherableBluetoothRegexs()));

        //Verify that any regex name should only contain in one array.
        wifiRegexs.forEach(s -> assertFalse(usbRegexs.contains(s)));
        wifiRegexs.forEach(s -> assertFalse(btRegexs.contains(s)));
        usbRegexs.forEach(s -> assertFalse(btRegexs.contains(s)));

        unregisterTetheringEventCallback(tetherEventCallback);
    }

    @Test
    public void testStopAllTethering() throws Exception {
        final TestTetheringEventCallback tetherEventCallback = registerTetheringEventCallback();
        tetherEventCallback.assumeTetheringSupported();

        try {
            if (!isWifiTetheringSupported(tetherEventCallback)) return;

            // TODO: start ethernet tethering here when TetheringManagerTest is moved to
            // TetheringIntegrationTest.

            startWifiTethering(tetherEventCallback);

            mTM.stopAllTethering();
            tetherEventCallback.expectTetheredInterfacesChanged(null);
        } finally {
            unregisterTetheringEventCallback(tetherEventCallback);
        }
    }

    @Test
    public void testEnableTetheringPermission() throws Exception {
        dropShellPermissionIdentity();
        final StartTetheringCallback startTetheringCallback = new StartTetheringCallback();
        mTM.startTethering(new TetheringRequest.Builder(TETHERING_WIFI).build(),
                c -> c.run() /* executor */, startTetheringCallback);
        startTetheringCallback.expectTetheringFailed(TETHER_ERROR_NO_CHANGE_TETHERING_PERMISSION);
    }

    private class EntitlementResultListener implements OnTetheringEntitlementResultListener {
        private final CompletableFuture<Integer> future = new CompletableFuture<>();

        @Override
        public void onTetheringEntitlementResult(int result) {
            future.complete(result);
        }

        public int get(long timeout, TimeUnit unit) throws Exception {
            return future.get(timeout, unit);
        }

    }

    private void assertEntitlementResult(final Consumer<EntitlementResultListener> functor,
            final int expect) throws Exception {
        final EntitlementResultListener listener = new EntitlementResultListener();
        functor.accept(listener);

        assertEquals(expect, listener.get(DEFAULT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    @Test
    public void testRequestLatestEntitlementResult() throws Exception {
        assumeTrue(mTM.isTetheringSupported());
        // Verify that requestLatestTetheringEntitlementResult() can get entitlement
        // result(TETHER_ERROR_ENTITLEMENT_UNKNOWN due to invalid downstream type) via listener.
        assertEntitlementResult(listener -> mTM.requestLatestTetheringEntitlementResult(
                TETHERING_WIFI_P2P, false, c -> c.run(), listener),
                TETHER_ERROR_ENTITLEMENT_UNKNOWN);

        // Verify that requestLatestTetheringEntitlementResult() can get entitlement
        // result(TETHER_ERROR_ENTITLEMENT_UNKNOWN due to invalid downstream type) via receiver.
        assertEntitlementResult(listener -> mTM.requestLatestTetheringEntitlementResult(
                TETHERING_WIFI_P2P,
                new ResultReceiver(null /* handler */) {
                    @Override
                    public void onReceiveResult(int resultCode, Bundle resultData) {
                        listener.onTetheringEntitlementResult(resultCode);
                    }
                }, false),
                TETHER_ERROR_ENTITLEMENT_UNKNOWN);

        // Do not request TETHERING_WIFI entitlement result if TETHERING_WIFI is not available.
        assumeTrue(mTM.getTetherableWifiRegexs().length > 0);

        // Verify that null listener will cause IllegalArgumentException.
        try {
            mTM.requestLatestTetheringEntitlementResult(
                    TETHERING_WIFI, false, c -> c.run(), null);
        } catch (IllegalArgumentException expect) { }

        // Override carrier config to ignore entitlement check.
        final PersistableBundle bundle = new PersistableBundle();
        bundle.putBoolean(CarrierConfigManager.KEY_REQUIRE_ENTITLEMENT_CHECKS_BOOL, false);
        overrideCarrierConfig(bundle);

        // Verify that requestLatestTetheringEntitlementResult() can get entitlement
        // result TETHER_ERROR_NO_ERROR due to provisioning bypassed.
        assertEntitlementResult(listener -> mTM.requestLatestTetheringEntitlementResult(
                TETHERING_WIFI, false, c -> c.run(), listener), TETHER_ERROR_NO_ERROR);

        // Reset carrier config.
        overrideCarrierConfig(null);
    }

    private void overrideCarrierConfig(PersistableBundle bundle) {
        final CarrierConfigManager configManager = (CarrierConfigManager) mContext
                .getSystemService(Context.CARRIER_CONFIG_SERVICE);
        final int subId = SubscriptionManager.getDefaultSubscriptionId();
        configManager.overrideConfig(subId, bundle);
    }

    @Test
    public void testTetheringUpstream() throws Exception {
        assumeTrue(mPm.hasSystemFeature(FEATURE_TELEPHONY));
        final TestTetheringEventCallback tetherEventCallback = registerTetheringEventCallback();
        tetherEventCallback.assumeTetheringSupported();
        final boolean previousWifiEnabledState = mWm.isWifiEnabled();

        try {
            if (!isWifiTetheringSupported(tetherEventCallback)) return;

            if (previousWifiEnabledState) {
                mCtsNetUtils.disconnectFromWifi(null);
            }

            final TestNetworkCallback networkCallback = new TestNetworkCallback();
            Network activeNetwork = null;
            try {
                mCm.registerDefaultNetworkCallback(networkCallback);
                activeNetwork = networkCallback.waitForAvailable();
            } finally {
                mCm.unregisterNetworkCallback(networkCallback);
            }

            assertNotNull("No active network. Please ensure the device has working mobile data.",
                    activeNetwork);
            final NetworkCapabilities activeNetCap = mCm.getNetworkCapabilities(activeNetwork);

            // If active nework is ETHERNET, tethering may not use cell network as upstream.
            assumeFalse(activeNetCap.hasTransport(TRANSPORT_ETHERNET));

            assertTrue(activeNetCap.hasTransport(TRANSPORT_CELLULAR));

            startWifiTethering(tetherEventCallback);

            final TelephonyManager telephonyManager = (TelephonyManager) mContext.getSystemService(
                    Context.TELEPHONY_SERVICE);
            final boolean dunRequired = telephonyManager.isTetheringApnRequired();
            final int expectedCap = dunRequired ? NET_CAPABILITY_DUN : NET_CAPABILITY_INTERNET;
            final Network network = tetherEventCallback.getCurrentValidUpstream();
            final NetworkCapabilities netCap = mCm.getNetworkCapabilities(network);
            assertTrue(netCap.hasTransport(TRANSPORT_CELLULAR));
            assertTrue(netCap.hasCapability(expectedCap));

            stopWifiTethering(tetherEventCallback);
        } finally {
            unregisterTetheringEventCallback(tetherEventCallback);
            if (previousWifiEnabledState) {
                mCtsNetUtils.connectToWifi();
            }
        }
    }
}
