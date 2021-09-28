/*
 * Copyright (C) 2014 The Android Open Source Project
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
package android.jobscheduler.cts;

import android.annotation.TargetApi;
import android.app.job.JobInfo;
import android.content.Context;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiManager;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.UserHandle;
import android.platform.test.annotations.RequiresDevice;
import android.util.Log;

import com.android.compatibility.common.util.ShellIdentityUtils;
import com.android.compatibility.common.util.SystemUtil;

import junit.framework.AssertionFailedError;

import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Schedules jobs with the {@link android.app.job.JobScheduler} that have network connectivity
 * constraints.
 * Requires manipulating the {@link android.net.wifi.WifiManager} to ensure an unmetered network.
 * Similarly, requires that the phone be connected to a wifi hotspot, or else the test will fail.
 */
@TargetApi(21)
@RequiresDevice // Emulators don't always have access to wifi/network
public class ConnectivityConstraintTest extends BaseJobSchedulerTest {
    private static final String TAG = "ConnectivityConstraintTest";
    private static final String RESTRICT_BACKGROUND_GET_CMD =
            "cmd netpolicy get restrict-background";
    private static final String RESTRICT_BACKGROUND_ON_CMD =
            "cmd netpolicy set restrict-background true";
    private static final String RESTRICT_BACKGROUND_OFF_CMD =
            "cmd netpolicy set restrict-background false";

    /** Unique identifier for the job scheduled by this suite of tests. */
    public static final int CONNECTIVITY_JOB_ID = ConnectivityConstraintTest.class.hashCode();

    private WifiManager mWifiManager;
    private ConnectivityManager mCm;

    /** Whether the device running these tests supports WiFi. */
    private boolean mHasWifi;
    /** Whether the device running these tests supports telephony. */
    private boolean mHasTelephony;
    /** Track whether WiFi was enabled in case we turn it off. */
    private boolean mInitialWiFiState;
    /** Track whether restrict background policy was enabled in case we turn it off. */
    private boolean mInitialRestrictBackground;

    private JobInfo.Builder mBuilder;

    private TestAppInterface mTestAppInterface;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mWifiManager = (WifiManager) getContext().getSystemService(Context.WIFI_SERVICE);
        mCm = (ConnectivityManager) getContext().getSystemService(Context.CONNECTIVITY_SERVICE);

        PackageManager packageManager = mContext.getPackageManager();
        mHasWifi = packageManager.hasSystemFeature(PackageManager.FEATURE_WIFI);
        mHasTelephony = packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY);
        mBuilder = new JobInfo.Builder(CONNECTIVITY_JOB_ID, kJobServiceComponent);

        if (mHasWifi) {
            mInitialWiFiState = mWifiManager.isWifiEnabled();
            ensureSavedWifiNetwork(mWifiManager);
        }
        mInitialRestrictBackground = SystemUtil
                .runShellCommand(getInstrumentation(), RESTRICT_BACKGROUND_GET_CMD)
                .contains("enabled");
        setDataSaverEnabled(false);
    }

    @Override
    public void tearDown() throws Exception {
        if (mTestAppInterface != null) {
            mTestAppInterface.cleanup();
        }
        mJobScheduler.cancel(CONNECTIVITY_JOB_ID);

        // Restore initial restrict background data usage policy
        setDataSaverEnabled(mInitialRestrictBackground);

        // Ensure that we leave WiFi in its previous state.
        if (mHasWifi && mWifiManager.isWifiEnabled() != mInitialWiFiState) {
            try {
                setWifiState(mInitialWiFiState, mCm, mWifiManager);
            } catch (AssertionFailedError e) {
                // Don't fail the test just because wifi state wasn't set in tearDown.
                Log.e(TAG, "Failed to return wifi state to " + mInitialWiFiState, e);
            }
        }

        super.tearDown();
    }

    // --------------------------------------------------------------------------------------------
    // Positives - schedule jobs under conditions that require them to pass.
    // --------------------------------------------------------------------------------------------

    /**
     * Schedule a job that requires a WiFi connection, and assert that it executes when the device
     * is connected to WiFi. This will fail if a wifi connection is unavailable.
     */
    public void testUnmeteredConstraintExecutes_withWifi() throws Exception {
        if (!mHasWifi) {
            Log.d(TAG, "Skipping test that requires the device be WiFi enabled.");
            return;
        }
        connectToWifi();

        kTestEnvironment.setExpectedExecutions(1);
        mJobScheduler.schedule(
                mBuilder.setRequiredNetworkType(JobInfo.NETWORK_TYPE_UNMETERED)
                        .build());

        runJob();

        assertTrue("Job with unmetered constraint did not fire on WiFi.",
                kTestEnvironment.awaitExecution());
    }

    /**
     * Schedule a job with a connectivity constraint, and ensure that it executes on WiFi.
     */
    public void testConnectivityConstraintExecutes_withWifi() throws Exception {
        if (!mHasWifi) {
            Log.d(TAG, "Skipping test that requires the device be WiFi enabled.");
            return;
        }
        connectToWifi();

        kTestEnvironment.setExpectedExecutions(1);
        mJobScheduler.schedule(
                mBuilder.setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                        .build());

        runJob();

        assertTrue("Job with connectivity constraint did not fire on WiFi.",
                kTestEnvironment.awaitExecution());
    }

    /**
     * Schedule a job with a generic connectivity constraint, and ensure that it executes on WiFi,
     * even with Data Saver on.
     */
    public void testConnectivityConstraintExecutes_withWifi_DataSaverOn() throws Exception {
        if (!mHasWifi) {
            Log.d(TAG, "Skipping test that requires the device be WiFi enabled.");
            return;
        }
        connectToWifi();
        setDataSaverEnabled(true);

        kTestEnvironment.setExpectedExecutions(1);
        mJobScheduler.schedule(
                mBuilder.setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                        .build());

        runJob();

        assertTrue("Job with connectivity constraint did not fire on WiFi.",
                kTestEnvironment.awaitExecution());
    }

    /**
     * Schedule a job with a generic connectivity constraint, and ensure that it executes
     * on a cellular data connection.
     */
    public void testConnectivityConstraintExecutes_withMobile() throws Exception {
        if (!checkDeviceSupportsMobileData()) {
            return;
        }
        disconnectWifiToConnectToMobile();

        kTestEnvironment.setExpectedExecutions(1);
        mJobScheduler.schedule(
                mBuilder.setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                        .build());

        runJob();

        assertTrue("Job with connectivity constraint did not fire on mobile.",
                kTestEnvironment.awaitExecution());
    }

    /**
     * Schedule a job with a generic connectivity constraint, and ensure that it isn't stopped when
     * the device transitions to WiFi.
     */
    public void testConnectivityConstraintExecutes_transitionNetworks() throws Exception {
        if (!mHasWifi) {
            Log.d(TAG, "Skipping test that requires the device be WiFi enabled.");
            return;
        }
        if (!checkDeviceSupportsMobileData()) {
            return;
        }
        disconnectWifiToConnectToMobile();

        kTestEnvironment.setExpectedExecutions(1);
        kTestEnvironment.setExpectedStopped();
        mJobScheduler.schedule(
                mBuilder.setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                        .build());

        runJob();

        assertTrue("Job with connectivity constraint did not fire on mobile.",
                kTestEnvironment.awaitExecution());

        connectToWifi();
        assertFalse(
                "Job with connectivity constraint was stopped when network transitioned to WiFi.",
                kTestEnvironment.awaitStopped());
    }

    /**
     * Schedule a job with a metered connectivity constraint, and ensure that it executes
     * on a mobile data connection.
     */
    public void testConnectivityConstraintExecutes_metered() throws Exception {
        if (!checkDeviceSupportsMobileData()) {
            return;
        }
        disconnectWifiToConnectToMobile();

        kTestEnvironment.setExpectedExecutions(1);
        mJobScheduler.schedule(
                mBuilder.setRequiredNetworkType(JobInfo.NETWORK_TYPE_METERED)
                        .build());

        runJob();
        assertTrue("Job with metered connectivity constraint did not fire on mobile.",
                kTestEnvironment.awaitExecution());
    }

    /**
     * Schedule a job with a cellular connectivity constraint, and ensure that it executes
     * on a mobile data connection and is not stopped when Data Saver is turned on because the app
     * is in the foreground.
     */
    public void testCellularConstraintExecutedAndStopped_Foreground() throws Exception {
        if (!checkDeviceSupportsMobileData()) {
            return;
        }
        disconnectWifiToConnectToMobile();
        mTestAppInterface = new TestAppInterface(mContext, CONNECTIVITY_JOB_ID);
        mTestAppInterface.startAndKeepTestActivity();

        mTestAppInterface.scheduleJob(false, true);

        runJob();
        assertTrue("Job with metered connectivity constraint did not fire on mobile.",
                mTestAppInterface.awaitJobStart(30_000));

        setDataSaverEnabled(true);
        assertFalse(
                "Job with metered connectivity constraint for foreground app was stopped when"
                        + " Data Saver was turned on.",
                mTestAppInterface.awaitJobStop(30_000));
    }

    // --------------------------------------------------------------------------------------------
    // Positives & Negatives - schedule jobs under conditions that require that pass initially and
    // then fail with a constraint change.
    // --------------------------------------------------------------------------------------------

    /**
     * Schedule a job with a cellular connectivity constraint, and ensure that it executes
     * on a mobile data connection and is stopped when Data Saver is turned on.
     */
    public void testCellularConstraintExecutedAndStopped() throws Exception {
        if (!checkDeviceSupportsMobileData()) {
            return;
        }
        disconnectWifiToConnectToMobile();

        kTestEnvironment.setExpectedExecutions(1);
        kTestEnvironment.setContinueAfterStart();
        kTestEnvironment.setExpectedStopped();
        mJobScheduler.schedule(
                mBuilder.setRequiredNetworkType(JobInfo.NETWORK_TYPE_CELLULAR)
                        .build());

        runJob();
        assertTrue("Job with metered connectivity constraint did not fire on mobile.",
                kTestEnvironment.awaitExecution());

        setDataSaverEnabled(true);
        assertTrue(
                "Job with metered connectivity constraint was not stopped when Data Saver was "
                        + "turned on.",
                kTestEnvironment.awaitStopped());
    }

    // --------------------------------------------------------------------------------------------
    // Negatives - schedule jobs under conditions that require that they fail.
    // --------------------------------------------------------------------------------------------

    /**
     * Schedule a job that requires a WiFi connection, and assert that it fails when the device is
     * connected to a cellular provider.
     * This test assumes that if the device supports a mobile data connection, then this connection
     * will be available.
     */
    public void testUnmeteredConstraintFails_withMobile() throws Exception {
        if (!checkDeviceSupportsMobileData()) {
            return;
        }
        disconnectWifiToConnectToMobile();

        kTestEnvironment.setExpectedExecutions(0);
        mJobScheduler.schedule(
                mBuilder.setRequiredNetworkType(JobInfo.NETWORK_TYPE_UNMETERED)
                        .build());
        runJob();

        assertTrue("Job requiring unmetered connectivity still executed on mobile.",
                kTestEnvironment.awaitTimeout());
    }

    /**
     * Schedule a job that requires a metered connection, and verify that it does not run when
     * the device is not connected to WiFi and Data Saver is on.
     */
    public void testMeteredConstraintFails_withMobile_DataSaverOn() throws Exception {
        if (!checkDeviceSupportsMobileData()) {
            Log.d(TAG, "Skipping test that requires the device be mobile data enabled.");
            return;
        }
        disconnectWifiToConnectToMobile();
        setDataSaverEnabled(true);

        kTestEnvironment.setExpectedExecutions(0);
        mJobScheduler.schedule(
                mBuilder.setRequiredNetworkType(JobInfo.NETWORK_TYPE_CELLULAR)
                        .build());
        runJob();

        assertTrue("Job requiring metered connectivity still executed on WiFi.",
                kTestEnvironment.awaitTimeout());
    }

    /**
     * Schedule a job that requires a metered connection, and verify that it does not run when
     * the device is connected to an unmetered WiFi provider.
     * This test assumes that if the device supports a mobile data connection, then this connection
     * will be available.
     */
    public void testMeteredConstraintFails_withWiFi() throws Exception {
        if (!mHasWifi) {
            Log.d(TAG, "Skipping test that requires the device be WiFi enabled.");
            return;
        }
        if (!checkDeviceSupportsMobileData()) {
            Log.d(TAG, "Skipping test that requires the device be mobile data enabled.");
            return;
        }
        connectToWifi();

        kTestEnvironment.setExpectedExecutions(0);
        mJobScheduler.schedule(
                mBuilder.setRequiredNetworkType(JobInfo.NETWORK_TYPE_METERED)
                        .build());
        runJob();

        assertTrue("Job requiring metered connectivity still executed on WiFi.",
                kTestEnvironment.awaitTimeout());
    }

    /**
     * Schedule a job that requires a cellular connection, and verify that it does not run when
     * the device is connected to a WiFi provider.
     */
    public void testCellularConstraintFails_withWiFi() throws Exception {
        if (!mHasWifi) {
            Log.d(TAG, "Skipping test that requires the device be WiFi enabled.");
            return;
        }
        if (!checkDeviceSupportsMobileData()) {
            Log.d(TAG, "Skipping test that requires the device be mobile data enabled.");
            return;
        }
        connectToWifi();

        kTestEnvironment.setExpectedExecutions(0);
        mJobScheduler.schedule(
                mBuilder.setRequiredNetworkType(JobInfo.NETWORK_TYPE_CELLULAR).build());
        runJob();

        assertTrue("Job requiring cellular connectivity still executed on WiFi.",
                kTestEnvironment.awaitTimeout());
    }

    // --------------------------------------------------------------------------------------------
    // Utility methods
    // --------------------------------------------------------------------------------------------

    /** Asks (not forces) JobScheduler to run the job if functional constraints are met. */
    private void runJob() throws Exception {
        // Since connectivity is a functional constraint, calling the "run" command without force
        // will only get the job to run if the constraint is satisfied.
        SystemUtil.runShellCommand(getInstrumentation(), "cmd jobscheduler run"
                + " -u " + UserHandle.myUserId()
                + " " + kJobServiceComponent.getPackageName() + " " + CONNECTIVITY_JOB_ID);
    }

    /**
     * Determine whether the device running these CTS tests should be subject to tests involving
     * mobile data.
     * @return True if this device will support a mobile data connection.
     */
    private boolean checkDeviceSupportsMobileData() {
        if (!mHasTelephony) {
            Log.d(TAG, "Skipping test that requires telephony features, not supported by this" +
                    " device");
            return false;
        }
        Network[] networks = mCm.getAllNetworks();
        for (Network network : networks) {
            if (mCm.getNetworkCapabilities(network)
                    .hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)) {
                return true;
            }
        }
        Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
        return false;
    }

    /**
     * Ensure WiFi is enabled, and block until we've verified that we are in fact connected.
     */
    private void connectToWifi()
            throws InterruptedException {
        setWifiState(true, mCm, mWifiManager);
    }

    /**
     * Ensure WiFi is disabled, and block until we've verified that we are in fact disconnected.
     */
    private void disconnectFromWifi()
            throws InterruptedException {
        setWifiState(false, mCm, mWifiManager);
    }

    /** Ensures that the device has a wifi network saved. */
    static void ensureSavedWifiNetwork(WifiManager wifiManager) {
        final List<WifiConfiguration> savedNetworks =
                ShellIdentityUtils.invokeMethodWithShellPermissions(
                        wifiManager, WifiManager::getConfiguredNetworks);
        assertFalse("Need at least one saved wifi network", savedNetworks.isEmpty());
    }

    /**
     * Set Wifi connection to specific state , and block until we've verified
     * that we are in the state.
     * Taken from {@link android.net.http.cts.ApacheHttpClientTest}.
     */
    static void setWifiState(final boolean enable,
            final ConnectivityManager cm, final WifiManager wm) throws InterruptedException {
        if (enable != isWiFiConnected(cm, wm)) {
            NetworkRequest nr = new NetworkRequest.Builder().addCapability(
                    NetworkCapabilities.NET_CAPABILITY_NOT_METERED).build();
            NetworkTracker tracker = new NetworkTracker(false, enable, cm);
            cm.registerNetworkCallback(nr, tracker);

            if (enable) {
                SystemUtil.runShellCommand("svc wifi enable");
                //noinspection deprecation
                SystemUtil.runWithShellPermissionIdentity(wm::reconnect,
                        android.Manifest.permission.NETWORK_SETTINGS);
            } else {
                SystemUtil.runShellCommand("svc wifi disable");
            }

            tracker.waitForStateChange();

            assertTrue("Wifi must be " + (enable ? "connected to" : "disconnected from")
                            + " an access point for this test.",
                    enable == isWiFiConnected(cm, wm));

            cm.unregisterNetworkCallback(tracker);
        }
    }

    private static boolean isWiFiConnected(final ConnectivityManager cm, final WifiManager wm) {
        return wm.isWifiEnabled() && cm.getActiveNetwork() != null && !cm.isActiveNetworkMetered();
    }

    /**
     * Disconnect from WiFi in an attempt to connect to cellular data. Worth noting that this is
     * best effort - there are no public APIs to force connecting to cell data. We disable WiFi
     * and wait for a broadcast that we're connected to cell.
     * We will not call into this function if the device doesn't support telephony.
     * @see #mHasTelephony
     * @see #checkDeviceSupportsMobileData()
     */
    private void disconnectWifiToConnectToMobile() throws InterruptedException {
        if (mHasWifi && mWifiManager.isWifiEnabled()) {
            NetworkRequest nr = new NetworkRequest.Builder().build();
            NetworkTracker tracker = new NetworkTracker(true, true, mCm);
            mCm.registerNetworkCallback(nr, tracker);

            disconnectFromWifi();

            assertTrue("Device must have access to a metered network for this test.",
                    tracker.waitForStateChange());

            mCm.unregisterNetworkCallback(tracker);
        }
    }

    /**
     * Ensures that restrict background data usage policy is turned off.
     * If the policy is on, it interferes with tests that relies on metered connection.
     */
    private void setDataSaverEnabled(boolean enabled) throws Exception {
        SystemUtil.runShellCommand(getInstrumentation(),
                enabled ? RESTRICT_BACKGROUND_ON_CMD : RESTRICT_BACKGROUND_OFF_CMD);
    }

    private static class NetworkTracker extends ConnectivityManager.NetworkCallback {
        private static final int MSG_CHECK_ACTIVE_NETWORK = 1;
        private final ConnectivityManager mCm;

        private final CountDownLatch mReceiveLatch = new CountDownLatch(1);

        private final boolean mNetworkMetered;

        private final boolean mExpectedConnected;

        private final Handler mHandler = new Handler(Looper.getMainLooper()) {
            @Override
            public void handleMessage(Message msg) {
                if (msg.what == MSG_CHECK_ACTIVE_NETWORK) {
                    checkActiveNetwork();
                }
            }
        };

        private NetworkTracker(boolean networkMetered, boolean expectedConnected,
                ConnectivityManager cm) {
            mNetworkMetered = networkMetered;
            mExpectedConnected = expectedConnected;
            mCm = cm;
        }

        @Override
        public void onAvailable(Network network, NetworkCapabilities networkCapabilities,
                LinkProperties linkProperties, boolean blocked) {
            // Available doesn't mean it's the active network. We need to check that separately.
            checkActiveNetwork();
        }

        @Override
        public void onLost(Network network) {
            checkActiveNetwork();
        }

        boolean waitForStateChange() throws InterruptedException {
            checkActiveNetwork();
            return mReceiveLatch.await(60, TimeUnit.SECONDS);
        }

        private void checkActiveNetwork() {
            if (mReceiveLatch.getCount() == 0) {
                return;
            }

            if (mExpectedConnected) {
                if (mCm.getActiveNetwork() != null
                        && mNetworkMetered == mCm.isActiveNetworkMetered()) {
                    mReceiveLatch.countDown();
                } else {
                    mHandler.sendEmptyMessageDelayed(MSG_CHECK_ACTIVE_NETWORK, 5000);
                }
            } else {
                if (mCm.getActiveNetwork() != null
                        && mNetworkMetered != mCm.isActiveNetworkMetered()) {
                    mReceiveLatch.countDown();
                } else {
                    mHandler.sendEmptyMessageDelayed(MSG_CHECK_ACTIVE_NETWORK, 5000);
                }
            }
        }
    }
}
