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

package android.net.cts;

import static android.content.pm.PackageManager.FEATURE_TELEPHONY;
import static android.net.ConnectivityDiagnosticsManager.ConnectivityDiagnosticsCallback;
import static android.net.ConnectivityDiagnosticsManager.ConnectivityReport;
import static android.net.ConnectivityDiagnosticsManager.ConnectivityReport.KEY_NETWORK_PROBES_ATTEMPTED_BITMASK;
import static android.net.ConnectivityDiagnosticsManager.ConnectivityReport.KEY_NETWORK_PROBES_SUCCEEDED_BITMASK;
import static android.net.ConnectivityDiagnosticsManager.ConnectivityReport.KEY_NETWORK_VALIDATION_RESULT;
import static android.net.ConnectivityDiagnosticsManager.ConnectivityReport.NETWORK_VALIDATION_RESULT_VALID;
import static android.net.ConnectivityDiagnosticsManager.DataStallReport;
import static android.net.ConnectivityDiagnosticsManager.DataStallReport.DETECTION_METHOD_DNS_EVENTS;
import static android.net.ConnectivityDiagnosticsManager.DataStallReport.DETECTION_METHOD_TCP_METRICS;
import static android.net.ConnectivityDiagnosticsManager.DataStallReport.KEY_DNS_CONSECUTIVE_TIMEOUTS;
import static android.net.ConnectivityDiagnosticsManager.DataStallReport.KEY_TCP_METRICS_COLLECTION_PERIOD_MILLIS;
import static android.net.ConnectivityDiagnosticsManager.DataStallReport.KEY_TCP_PACKET_FAIL_RATE;
import static android.net.ConnectivityDiagnosticsManager.persistableBundleEquals;
import static android.net.NetworkCapabilities.NET_CAPABILITY_INTERNET;
import static android.net.NetworkCapabilities.NET_CAPABILITY_NOT_VPN;
import static android.net.NetworkCapabilities.NET_CAPABILITY_TRUSTED;
import static android.net.NetworkCapabilities.TRANSPORT_CELLULAR;
import static android.net.NetworkCapabilities.TRANSPORT_TEST;
import static android.net.cts.util.CtsNetUtils.TestNetworkCallback;

import static com.android.compatibility.common.util.SystemUtil.callWithShellPermissionIdentity;
import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.annotation.NonNull;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.ConnectivityDiagnosticsManager;
import android.net.ConnectivityManager;
import android.net.LinkAddress;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.TestNetworkInterface;
import android.net.TestNetworkManager;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.PersistableBundle;
import android.os.Process;
import android.platform.test.annotations.AppModeFull;
import android.telephony.CarrierConfigManager;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.util.Pair;

import androidx.test.InstrumentationRegistry;

import com.android.internal.telephony.uicc.IccUtils;
import com.android.internal.util.ArrayUtils;
import com.android.testutils.ArrayTrackRecord;
import com.android.testutils.DevSdkIgnoreRule.IgnoreUpTo;
import com.android.testutils.DevSdkIgnoreRunner;
import com.android.testutils.SkipPresubmit;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

@RunWith(DevSdkIgnoreRunner.class)
@IgnoreUpTo(Build.VERSION_CODES.Q) // ConnectivityDiagnosticsManager did not exist in Q
@AppModeFull(reason = "CHANGE_NETWORK_STATE, MANAGE_TEST_NETWORKS not grantable to instant apps")
public class ConnectivityDiagnosticsManagerTest {
    private static final int CALLBACK_TIMEOUT_MILLIS = 5000;
    private static final int NO_CALLBACK_INVOKED_TIMEOUT = 500;
    private static final long TIMESTAMP = 123456789L;
    private static final int DNS_CONSECUTIVE_TIMEOUTS = 5;
    private static final int COLLECTION_PERIOD_MILLIS = 5000;
    private static final int FAIL_RATE_PERCENTAGE = 100;
    private static final int UNKNOWN_DETECTION_METHOD = 4;
    private static final int FILTERED_UNKNOWN_DETECTION_METHOD = 0;
    private static final int CARRIER_CONFIG_CHANGED_BROADCAST_TIMEOUT = 5000;
    private static final int DELAY_FOR_ADMIN_UIDS_MILLIS = 2000;

    private static final Executor INLINE_EXECUTOR = x -> x.run();

    private static final NetworkRequest TEST_NETWORK_REQUEST =
            new NetworkRequest.Builder()
                    .addTransportType(TRANSPORT_TEST)
                    .removeCapability(NET_CAPABILITY_TRUSTED)
                    .removeCapability(NET_CAPABILITY_NOT_VPN)
                    .build();

    private static final String SHA_256 = "SHA-256";

    private static final NetworkRequest CELLULAR_NETWORK_REQUEST =
            new NetworkRequest.Builder()
                    .addTransportType(TRANSPORT_CELLULAR)
                    .addCapability(NET_CAPABILITY_INTERNET)
                    .build();

    private static final IBinder BINDER = new Binder();

    private Context mContext;
    private ConnectivityManager mConnectivityManager;
    private ConnectivityDiagnosticsManager mCdm;
    private CarrierConfigManager mCarrierConfigManager;
    private PackageManager mPackageManager;
    private TelephonyManager mTelephonyManager;

    // Callback used to keep TestNetworks up when there are no other outstanding NetworkRequests
    // for it.
    private TestNetworkCallback mTestNetworkCallback;
    private Network mTestNetwork;
    private ParcelFileDescriptor mTestNetworkFD;

    private List<TestConnectivityDiagnosticsCallback> mRegisteredCallbacks;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getContext();
        mConnectivityManager = mContext.getSystemService(ConnectivityManager.class);
        mCdm = mContext.getSystemService(ConnectivityDiagnosticsManager.class);
        mCarrierConfigManager = mContext.getSystemService(CarrierConfigManager.class);
        mPackageManager = mContext.getPackageManager();
        mTelephonyManager = mContext.getSystemService(TelephonyManager.class);

        mTestNetworkCallback = new TestNetworkCallback();
        mConnectivityManager.requestNetwork(TEST_NETWORK_REQUEST, mTestNetworkCallback);

        mRegisteredCallbacks = new ArrayList<>();
    }

    @After
    public void tearDown() throws Exception {
        mConnectivityManager.unregisterNetworkCallback(mTestNetworkCallback);
        if (mTestNetwork != null) {
            runWithShellPermissionIdentity(() -> {
                final TestNetworkManager tnm = mContext.getSystemService(TestNetworkManager.class);
                tnm.teardownTestNetwork(mTestNetwork);
            });
            mTestNetwork = null;
        }

        if (mTestNetworkFD != null) {
            mTestNetworkFD.close();
            mTestNetworkFD = null;
        }

        for (TestConnectivityDiagnosticsCallback cb : mRegisteredCallbacks) {
            mCdm.unregisterConnectivityDiagnosticsCallback(cb);
        }
    }

    @Test
    public void testRegisterConnectivityDiagnosticsCallback() throws Exception {
        mTestNetworkFD = setUpTestNetwork().getFileDescriptor();
        mTestNetwork = mTestNetworkCallback.waitForAvailable();

        final TestConnectivityDiagnosticsCallback cb =
                createAndRegisterConnectivityDiagnosticsCallback(TEST_NETWORK_REQUEST);

        final String interfaceName =
                mConnectivityManager.getLinkProperties(mTestNetwork).getInterfaceName();

        cb.expectOnConnectivityReportAvailable(mTestNetwork, interfaceName);
        cb.assertNoCallback();
    }

    @SkipPresubmit(reason = "Flaky: b/159718782; add to presubmit after fixing")
    @Test
    public void testRegisterCallbackWithCarrierPrivileges() throws Exception {
        assumeTrue(mPackageManager.hasSystemFeature(FEATURE_TELEPHONY));

        final int subId = SubscriptionManager.getDefaultSubscriptionId();
        if (subId == SubscriptionManager.INVALID_SUBSCRIPTION_ID) {
            fail("Need an active subscription. Please ensure that the device has working mobile"
                    + " data.");
        }

        final CarrierConfigReceiver carrierConfigReceiver = new CarrierConfigReceiver(subId);
        mContext.registerReceiver(
                carrierConfigReceiver,
                new IntentFilter(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED));

        final TestNetworkCallback testNetworkCallback = new TestNetworkCallback();

        try {
            doBroadcastCarrierConfigsAndVerifyOnConnectivityReportAvailable(
                    subId, carrierConfigReceiver, testNetworkCallback);
        } finally {
            runWithShellPermissionIdentity(
                    () -> mCarrierConfigManager.overrideConfig(subId, null),
                    android.Manifest.permission.MODIFY_PHONE_STATE);
            mConnectivityManager.unregisterNetworkCallback(testNetworkCallback);
            mContext.unregisterReceiver(carrierConfigReceiver);
        }
    }

    private String getCertHashForThisPackage() throws Exception {
        final PackageInfo pkgInfo =
                mPackageManager.getPackageInfo(
                        mContext.getOpPackageName(), PackageManager.GET_SIGNATURES);
        final MessageDigest md = MessageDigest.getInstance(SHA_256);
        final byte[] certHash = md.digest(pkgInfo.signatures[0].toByteArray());
        return IccUtils.bytesToHexString(certHash);
    }

    private void doBroadcastCarrierConfigsAndVerifyOnConnectivityReportAvailable(
            int subId,
            @NonNull CarrierConfigReceiver carrierConfigReceiver,
            @NonNull TestNetworkCallback testNetworkCallback)
            throws Exception {
        final PersistableBundle carrierConfigs = new PersistableBundle();
        carrierConfigs.putStringArray(
                CarrierConfigManager.KEY_CARRIER_CERTIFICATE_STRING_ARRAY,
                new String[] {getCertHashForThisPackage()});

        runWithShellPermissionIdentity(
                () -> {
                    mCarrierConfigManager.overrideConfig(subId, carrierConfigs);
                    mCarrierConfigManager.notifyConfigChangedForSubId(subId);
                },
                android.Manifest.permission.MODIFY_PHONE_STATE);

        // TODO(b/157779832): This should use android.permission.CHANGE_NETWORK_STATE. However, the
        // shell does not have CHANGE_NETWORK_STATE, so use CONNECTIVITY_INTERNAL until the shell
        // permissions are updated.
        runWithShellPermissionIdentity(
                () -> mConnectivityManager.requestNetwork(
                        CELLULAR_NETWORK_REQUEST, testNetworkCallback),
                android.Manifest.permission.CONNECTIVITY_INTERNAL);

        final Network network = testNetworkCallback.waitForAvailable();
        assertNotNull(network);

        assertTrue("Didn't receive broadcast for ACTION_CARRIER_CONFIG_CHANGED for subId=" + subId,
                carrierConfigReceiver.waitForCarrierConfigChanged());
        assertTrue("Don't have Carrier Privileges after adding cert for this package",
                mTelephonyManager.createForSubscriptionId(subId).hasCarrierPrivileges());

        // Wait for CarrierPrivilegesTracker to receive the ACTION_CARRIER_CONFIG_CHANGED
        // broadcast. CPT then needs to update the corresponding DataConnection, which then
        // updates ConnectivityService. Unfortunately, this update to the NetworkCapabilities in
        // CS does not trigger NetworkCallback#onCapabilitiesChanged as changing the
        // administratorUids is not a publicly visible change. In lieu of a better signal to
        // detministically wait for, use Thread#sleep here.
        // TODO(b/157949581): replace this Thread#sleep with a deterministic signal
        Thread.sleep(DELAY_FOR_ADMIN_UIDS_MILLIS);

        final TestConnectivityDiagnosticsCallback connDiagsCallback =
                createAndRegisterConnectivityDiagnosticsCallback(CELLULAR_NETWORK_REQUEST);

        final String interfaceName =
                mConnectivityManager.getLinkProperties(network).getInterfaceName();
        connDiagsCallback.expectOnConnectivityReportAvailable(
                network, interfaceName, TRANSPORT_CELLULAR);
        connDiagsCallback.assertNoCallback();
    }

    @Test
    public void testRegisterDuplicateConnectivityDiagnosticsCallback() {
        final TestConnectivityDiagnosticsCallback cb =
                createAndRegisterConnectivityDiagnosticsCallback(TEST_NETWORK_REQUEST);

        try {
            mCdm.registerConnectivityDiagnosticsCallback(TEST_NETWORK_REQUEST, INLINE_EXECUTOR, cb);
            fail("Registering the same callback twice should throw an IllegalArgumentException");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testUnregisterConnectivityDiagnosticsCallback() {
        final TestConnectivityDiagnosticsCallback cb = new TestConnectivityDiagnosticsCallback();
        mCdm.registerConnectivityDiagnosticsCallback(TEST_NETWORK_REQUEST, INLINE_EXECUTOR, cb);
        mCdm.unregisterConnectivityDiagnosticsCallback(cb);
    }

    @Test
    public void testUnregisterUnknownConnectivityDiagnosticsCallback() {
        // Expected to silently ignore the unregister() call
        mCdm.unregisterConnectivityDiagnosticsCallback(new TestConnectivityDiagnosticsCallback());
    }

    @Test
    public void testOnConnectivityReportAvailable() throws Exception {
        final TestConnectivityDiagnosticsCallback cb =
                createAndRegisterConnectivityDiagnosticsCallback(TEST_NETWORK_REQUEST);

        mTestNetworkFD = setUpTestNetwork().getFileDescriptor();
        mTestNetwork = mTestNetworkCallback.waitForAvailable();

        final String interfaceName =
                mConnectivityManager.getLinkProperties(mTestNetwork).getInterfaceName();

        cb.expectOnConnectivityReportAvailable(mTestNetwork, interfaceName);
        cb.assertNoCallback();
    }

    @Test
    public void testOnDataStallSuspected_DnsEvents() throws Exception {
        final PersistableBundle extras = new PersistableBundle();
        extras.putInt(KEY_DNS_CONSECUTIVE_TIMEOUTS, DNS_CONSECUTIVE_TIMEOUTS);

        verifyOnDataStallSuspected(DETECTION_METHOD_DNS_EVENTS, TIMESTAMP, extras);
    }

    @Test
    public void testOnDataStallSuspected_TcpMetrics() throws Exception {
        final PersistableBundle extras = new PersistableBundle();
        extras.putInt(KEY_TCP_METRICS_COLLECTION_PERIOD_MILLIS, COLLECTION_PERIOD_MILLIS);
        extras.putInt(KEY_TCP_PACKET_FAIL_RATE, FAIL_RATE_PERCENTAGE);

        verifyOnDataStallSuspected(DETECTION_METHOD_TCP_METRICS, TIMESTAMP, extras);
    }

    @Test
    public void testOnDataStallSuspected_UnknownDetectionMethod() throws Exception {
        verifyOnDataStallSuspected(
                UNKNOWN_DETECTION_METHOD,
                FILTERED_UNKNOWN_DETECTION_METHOD,
                TIMESTAMP,
                PersistableBundle.EMPTY);
    }

    private void verifyOnDataStallSuspected(
            int detectionMethod, long timestampMillis, @NonNull PersistableBundle extras)
            throws Exception {
        // Input detection method is expected to match received detection method
        verifyOnDataStallSuspected(detectionMethod, detectionMethod, timestampMillis, extras);
    }

    private void verifyOnDataStallSuspected(
            int inputDetectionMethod,
            int expectedDetectionMethod,
            long timestampMillis,
            @NonNull PersistableBundle extras)
            throws Exception {
        mTestNetworkFD = setUpTestNetwork().getFileDescriptor();
        mTestNetwork = mTestNetworkCallback.waitForAvailable();

        final TestConnectivityDiagnosticsCallback cb =
                createAndRegisterConnectivityDiagnosticsCallback(TEST_NETWORK_REQUEST);

        final String interfaceName =
                mConnectivityManager.getLinkProperties(mTestNetwork).getInterfaceName();

        cb.expectOnConnectivityReportAvailable(mTestNetwork, interfaceName);

        runWithShellPermissionIdentity(
                () -> mConnectivityManager.simulateDataStall(
                        inputDetectionMethod, timestampMillis, mTestNetwork, extras),
                android.Manifest.permission.MANAGE_TEST_NETWORKS);

        cb.expectOnDataStallSuspected(
                mTestNetwork, interfaceName, expectedDetectionMethod, timestampMillis, extras);
        cb.assertNoCallback();
    }

    @Test
    public void testOnNetworkConnectivityReportedTrue() throws Exception {
        verifyOnNetworkConnectivityReported(true /* hasConnectivity */);
    }

    @Test
    public void testOnNetworkConnectivityReportedFalse() throws Exception {
        verifyOnNetworkConnectivityReported(false /* hasConnectivity */);
    }

    private void verifyOnNetworkConnectivityReported(boolean hasConnectivity) throws Exception {
        mTestNetworkFD = setUpTestNetwork().getFileDescriptor();
        mTestNetwork = mTestNetworkCallback.waitForAvailable();

        final TestConnectivityDiagnosticsCallback cb =
                createAndRegisterConnectivityDiagnosticsCallback(TEST_NETWORK_REQUEST);

        // onConnectivityReportAvailable always invoked when the test network is established
        final String interfaceName =
                mConnectivityManager.getLinkProperties(mTestNetwork).getInterfaceName();
        cb.expectOnConnectivityReportAvailable(mTestNetwork, interfaceName);
        cb.assertNoCallback();

        mConnectivityManager.reportNetworkConnectivity(mTestNetwork, hasConnectivity);

        cb.expectOnNetworkConnectivityReported(mTestNetwork, hasConnectivity);

        // if hasConnectivity does not match the network's known connectivity, it will be
        // revalidated which will trigger another onConnectivityReportAvailable callback.
        if (!hasConnectivity) {
            cb.expectOnConnectivityReportAvailable(mTestNetwork, interfaceName);
        }

        cb.assertNoCallback();
    }

    private TestConnectivityDiagnosticsCallback createAndRegisterConnectivityDiagnosticsCallback(
            NetworkRequest request) {
        final TestConnectivityDiagnosticsCallback cb = new TestConnectivityDiagnosticsCallback();
        mCdm.registerConnectivityDiagnosticsCallback(request, INLINE_EXECUTOR, cb);
        mRegisteredCallbacks.add(cb);
        return cb;
    }

    /**
     * Registers a test NetworkAgent with ConnectivityService with limited capabilities, which leads
     * to the Network being validated.
     */
    @NonNull
    private TestNetworkInterface setUpTestNetwork() throws Exception {
        final int[] administratorUids = new int[] {Process.myUid()};
        return callWithShellPermissionIdentity(
                () -> {
                    final TestNetworkManager tnm =
                            mContext.getSystemService(TestNetworkManager.class);
                    final TestNetworkInterface tni = tnm.createTunInterface(new LinkAddress[0]);
                    tnm.setupTestNetwork(tni.getInterfaceName(), administratorUids, BINDER);
                    return tni;
                });
    }

    private static class TestConnectivityDiagnosticsCallback
            extends ConnectivityDiagnosticsCallback {
        private final ArrayTrackRecord<Object>.ReadHead mHistory =
                new ArrayTrackRecord<Object>().newReadHead();

        @Override
        public void onConnectivityReportAvailable(ConnectivityReport report) {
            mHistory.add(report);
        }

        @Override
        public void onDataStallSuspected(DataStallReport report) {
            mHistory.add(report);
        }

        @Override
        public void onNetworkConnectivityReported(Network network, boolean hasConnectivity) {
            mHistory.add(new Pair<Network, Boolean>(network, hasConnectivity));
        }

        public void expectOnConnectivityReportAvailable(
                @NonNull Network network, @NonNull String interfaceName) {
            expectOnConnectivityReportAvailable(network, interfaceName, TRANSPORT_TEST);
        }

        public void expectOnConnectivityReportAvailable(
                @NonNull Network network, @NonNull String interfaceName, int transportType) {
            final ConnectivityReport result =
                    (ConnectivityReport) mHistory.poll(CALLBACK_TIMEOUT_MILLIS, x -> true);
            assertEquals(network, result.getNetwork());

            final NetworkCapabilities nc = result.getNetworkCapabilities();
            assertNotNull(nc);
            assertTrue(nc.hasTransport(transportType));
            assertNotNull(result.getLinkProperties());
            assertEquals(interfaceName, result.getLinkProperties().getInterfaceName());

            final PersistableBundle extras = result.getAdditionalInfo();
            assertTrue(extras.containsKey(KEY_NETWORK_VALIDATION_RESULT));
            final int validationResult = extras.getInt(KEY_NETWORK_VALIDATION_RESULT);
            assertEquals("Network validation result is not 'valid'",
                    NETWORK_VALIDATION_RESULT_VALID, validationResult);

            assertTrue(extras.containsKey(KEY_NETWORK_PROBES_SUCCEEDED_BITMASK));
            final int probesSucceeded = extras.getInt(KEY_NETWORK_VALIDATION_RESULT);
            assertTrue("PROBES_SUCCEEDED mask not in expected range", probesSucceeded >= 0);

            assertTrue(extras.containsKey(KEY_NETWORK_PROBES_ATTEMPTED_BITMASK));
            final int probesAttempted = extras.getInt(KEY_NETWORK_PROBES_ATTEMPTED_BITMASK);
            assertTrue("PROBES_ATTEMPTED mask not in expected range", probesAttempted >= 0);
        }

        public void expectOnDataStallSuspected(
                @NonNull Network network,
                @NonNull String interfaceName,
                int detectionMethod,
                long timestampMillis,
                @NonNull PersistableBundle extras) {
            final DataStallReport result =
                    (DataStallReport) mHistory.poll(CALLBACK_TIMEOUT_MILLIS, x -> true);
            assertEquals(network, result.getNetwork());
            assertEquals(detectionMethod, result.getDetectionMethod());
            assertEquals(timestampMillis, result.getReportTimestamp());

            final NetworkCapabilities nc = result.getNetworkCapabilities();
            assertNotNull(nc);
            assertTrue(nc.hasTransport(TRANSPORT_TEST));
            assertNotNull(result.getLinkProperties());
            assertEquals(interfaceName, result.getLinkProperties().getInterfaceName());

            assertTrue(persistableBundleEquals(extras, result.getStallDetails()));
        }

        public void expectOnNetworkConnectivityReported(
                @NonNull Network network, boolean hasConnectivity) {
            final Pair<Network, Boolean> result =
                    (Pair<Network, Boolean>) mHistory.poll(CALLBACK_TIMEOUT_MILLIS, x -> true);
            assertEquals(network, result.first /* network */);
            assertEquals(hasConnectivity, result.second /* hasConnectivity */);
        }

        public void assertNoCallback() {
            // If no more callbacks exist, there should be nothing left in the ReadHead
            assertNull("Unexpected event in history",
                    mHistory.poll(NO_CALLBACK_INVOKED_TIMEOUT, x -> true));
        }
    }

    private class CarrierConfigReceiver extends BroadcastReceiver {
        private final CountDownLatch mLatch = new CountDownLatch(1);
        private final int mSubId;

        CarrierConfigReceiver(int subId) {
            mSubId = subId;
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            if (!CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED.equals(intent.getAction())) {
                return;
            }

            final int subId =
                    intent.getIntExtra(
                            CarrierConfigManager.EXTRA_SUBSCRIPTION_INDEX,
                            SubscriptionManager.INVALID_SUBSCRIPTION_ID);
            if (mSubId != subId) return;

            final PersistableBundle carrierConfigs = mCarrierConfigManager.getConfigForSubId(subId);
            if (!CarrierConfigManager.isConfigForIdentifiedCarrier(carrierConfigs)) return;

            final String[] certs =
                    carrierConfigs.getStringArray(
                            CarrierConfigManager.KEY_CARRIER_CERTIFICATE_STRING_ARRAY);
            try {
                if (ArrayUtils.contains(certs, getCertHashForThisPackage())) {
                    mLatch.countDown();
                }
            } catch (Exception e) {
            }
        }

        boolean waitForCarrierConfigChanged() throws Exception {
            return mLatch.await(CARRIER_CONFIG_CHANGED_BROADCAST_TIMEOUT, TimeUnit.MILLISECONDS);
        }
    }
}
