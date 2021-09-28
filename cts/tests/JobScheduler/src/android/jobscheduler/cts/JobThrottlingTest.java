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
 * limitations under the License
 */

package android.jobscheduler.cts;

import static android.jobscheduler.cts.ConnectivityConstraintTest.ensureSavedWifiNetwork;
import static android.jobscheduler.cts.ConnectivityConstraintTest.setWifiState;
import static android.jobscheduler.cts.TestAppInterface.TEST_APP_PACKAGE;
import static android.os.PowerManager.ACTION_DEVICE_IDLE_MODE_CHANGED;
import static android.os.PowerManager.ACTION_LIGHT_DEVICE_IDLE_MODE_CHANGED;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeTrue;

import android.app.AppOpsManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.jobscheduler.cts.jobtestapp.TestJobSchedulerReceiver;
import android.net.ConnectivityManager;
import android.net.wifi.WifiManager;
import android.os.PowerManager;
import android.os.SystemClock;
import android.os.Temperature;
import android.os.UserHandle;
import android.platform.test.annotations.RequiresDevice;
import android.provider.Settings;
import android.support.test.uiautomator.UiDevice;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.AppOpsUtils;
import com.android.compatibility.common.util.AppStandbyUtils;
import com.android.compatibility.common.util.BatteryUtils;
import com.android.compatibility.common.util.ThermalUtils;

import junit.framework.AssertionFailedError;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;

/**
 * Tests related to job throttling -- device idle, app standby and battery saver.
 */
@RunWith(AndroidJUnit4.class)
@LargeTest
public class JobThrottlingTest {
    private static final String TAG = JobThrottlingTest.class.getSimpleName();
    private static final long BACKGROUND_JOBS_EXPECTED_DELAY = 3_000;
    private static final long POLL_INTERVAL = 500;
    private static final long DEFAULT_WAIT_TIMEOUT = 2000;
    private static final long SHELL_TIMEOUT = 3_000;
    // TODO: mark Settings.System.SCREEN_OFF_TIMEOUT as @TestApi
    private static final String SCREEN_OFF_TIMEOUT = "screen_off_timeout";

    enum Bucket {
        ACTIVE,
        WORKING_SET,
        FREQUENT,
        RARE,
        RESTRICTED,
        NEVER
    }

    private Context mContext;
    private UiDevice mUiDevice;
    private PowerManager mPowerManager;
    private int mTestJobId;
    private int mTestPackageUid;
    private boolean mDeviceInDoze;
    private boolean mDeviceIdleEnabled;
    private boolean mAppStandbyEnabled;
    private WifiManager mWifiManager;
    private ConnectivityManager mCm;
    /** Whether the device running these tests supports WiFi. */
    private boolean mHasWifi;
    /** Track whether WiFi was enabled in case we turn it off. */
    private boolean mInitialWiFiState;
    private boolean mInitialAirplaneModeState;
    private String mInitialJobSchedulerConstants;
    private String mInitialDisplayTimeout;
    private String mInitialRestrictedBucketEnabled;
    private boolean mAutomotiveDevice;

    private TestAppInterface mTestAppInterface;

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            Log.d(TAG, "Received action " + intent.getAction());
            switch (intent.getAction()) {
                case ACTION_DEVICE_IDLE_MODE_CHANGED:
                case ACTION_LIGHT_DEVICE_IDLE_MODE_CHANGED:
                    synchronized (JobThrottlingTest.this) {
                        mDeviceInDoze = mPowerManager.isDeviceIdleMode();
                        Log.d(TAG, "mDeviceInDoze: " + mDeviceInDoze);
                    }
                    break;
            }
        }
    };

    private static boolean isDeviceIdleEnabled(UiDevice uiDevice) throws Exception {
        final String output = uiDevice.executeShellCommand("cmd deviceidle enabled deep").trim();
        return Integer.parseInt(output) != 0;
    }

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        mPowerManager = mContext.getSystemService(PowerManager.class);
        mDeviceInDoze = mPowerManager.isDeviceIdleMode();
        mTestPackageUid = mContext.getPackageManager().getPackageUid(TEST_APP_PACKAGE, 0);
        mTestJobId = (int) (SystemClock.uptimeMillis() / 1000);
        mTestAppInterface = new TestAppInterface(mContext, mTestJobId);
        final IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(ACTION_DEVICE_IDLE_MODE_CHANGED);
        intentFilter.addAction(ACTION_LIGHT_DEVICE_IDLE_MODE_CHANGED);
        mContext.registerReceiver(mReceiver, intentFilter);
        assertFalse("Test package already in temp whitelist", isTestAppTempWhitelisted());
        makeTestPackageIdle();
        mDeviceIdleEnabled = isDeviceIdleEnabled(mUiDevice);
        mAppStandbyEnabled = AppStandbyUtils.isAppStandbyEnabled();
        if (mAppStandbyEnabled) {
            setTestPackageStandbyBucket(Bucket.ACTIVE);
        } else {
            Log.w(TAG, "App standby not enabled on test device");
        }
        mWifiManager = mContext.getSystemService(WifiManager.class);
        mCm = mContext.getSystemService(ConnectivityManager.class);
        mHasWifi = mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_WIFI);
        mInitialWiFiState = mWifiManager.isWifiEnabled();
        mInitialAirplaneModeState = isAirplaneModeOn();
        mInitialJobSchedulerConstants = Settings.Global.getString(mContext.getContentResolver(),
                Settings.Global.JOB_SCHEDULER_CONSTANTS);
        mInitialRestrictedBucketEnabled = Settings.Global.getString(mContext.getContentResolver(),
                Settings.Global.ENABLE_RESTRICTED_BUCKET);
        // Make sure test jobs can run regardless of bucket.
        Settings.Global.putString(mContext.getContentResolver(),
                Settings.Global.JOB_SCHEDULER_CONSTANTS, "min_ready_non_active_jobs_count=0");
        // Make sure the screen doesn't turn off when the test turns it on.
        mInitialDisplayTimeout =
                Settings.System.getString(mContext.getContentResolver(), SCREEN_OFF_TIMEOUT);
        Settings.System.putString(mContext.getContentResolver(), SCREEN_OFF_TIMEOUT, "300000");

        // In automotive device, always-on screen and endless battery charging are assumed.
        mAutomotiveDevice =
                mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE);
        if (mAutomotiveDevice) {
            setScreenState(true);
            // TODO(b/159176758): make sure that initial power supply is on.
            BatteryUtils.runDumpsysBatterySetPluggedIn(true);
        }
    }

    @Test
    public void testAllowWhileIdleJobInTempwhitelist() throws Exception {
        assumeTrue("device idle not enabled", mDeviceIdleEnabled);

        toggleDeviceIdleState(true);
        Thread.sleep(DEFAULT_WAIT_TIMEOUT);
        sendScheduleJobBroadcast(true);
        assertFalse("Job started without being tempwhitelisted",
                mTestAppInterface.awaitJobStart(5_000));
        tempWhitelistTestApp(5_000);
        assertTrue("Job with allow_while_idle flag did not start when the app was tempwhitelisted",
                mTestAppInterface.awaitJobStart(5_000));
    }

    @Test
    public void testForegroundJobsStartImmediately() throws Exception {
        assumeTrue("device idle not enabled", mDeviceIdleEnabled);

        sendScheduleJobBroadcast(false);
        runJob();
        assertTrue("Job did not start after scheduling",
                mTestAppInterface.awaitJobStart(DEFAULT_WAIT_TIMEOUT));
        toggleDeviceIdleState(true);
        assertTrue("Job did not stop on entering doze",
                mTestAppInterface.awaitJobStop(DEFAULT_WAIT_TIMEOUT));
        Thread.sleep(TestJobSchedulerReceiver.JOB_INITIAL_BACKOFF);
        mTestAppInterface.startAndKeepTestActivity();
        toggleDeviceIdleState(false);
        assertTrue("Job for foreground app did not start immediately when device exited doze",
                mTestAppInterface.awaitJobStart(3_000));
    }

    @Test
    public void testBackgroundJobsDelayed() throws Exception {
        assumeTrue("device idle not enabled", mDeviceIdleEnabled);

        sendScheduleJobBroadcast(false);
        runJob();
        assertTrue("Job did not start after scheduling",
                mTestAppInterface.awaitJobStart(DEFAULT_WAIT_TIMEOUT));
        toggleDeviceIdleState(true);
        assertTrue("Job did not stop on entering doze",
                mTestAppInterface.awaitJobStop(DEFAULT_WAIT_TIMEOUT));
        Thread.sleep(TestJobSchedulerReceiver.JOB_INITIAL_BACKOFF);
        toggleDeviceIdleState(false);
        assertFalse("Job for background app started immediately when device exited doze",
                mTestAppInterface.awaitJobStart(DEFAULT_WAIT_TIMEOUT));
        Thread.sleep(BACKGROUND_JOBS_EXPECTED_DELAY - DEFAULT_WAIT_TIMEOUT);
        assertTrue("Job for background app did not start after the expected delay of "
                        + BACKGROUND_JOBS_EXPECTED_DELAY + "ms",
                mTestAppInterface.awaitJobStart(DEFAULT_WAIT_TIMEOUT));
    }

    @Test
    public void testJobStoppedWhenRestricted() throws Exception {
        sendScheduleJobBroadcast(false);
        runJob();
        assertTrue("Job did not start after scheduling",
                mTestAppInterface.awaitJobStart(DEFAULT_WAIT_TIMEOUT));
        setTestPackageRestricted(true);
        assertTrue("Job did not stop after test app was restricted",
                mTestAppInterface.awaitJobStop(DEFAULT_WAIT_TIMEOUT));
    }

    @Test
    public void testRestrictedJobStartedWhenUnrestricted() throws Exception {
        setTestPackageRestricted(true);
        sendScheduleJobBroadcast(false);
        assertFalse("Job started for restricted app",
                mTestAppInterface.awaitJobStart(DEFAULT_WAIT_TIMEOUT));
        setTestPackageRestricted(false);
        assertTrue("Job did not start when app was unrestricted",
                mTestAppInterface.awaitJobStart(DEFAULT_WAIT_TIMEOUT));
    }

    @Test
    public void testRestrictedJobAllowedWhenUidActive() throws Exception {
        setTestPackageRestricted(true);
        sendScheduleJobBroadcast(false);
        assertFalse("Job started for restricted app",
                mTestAppInterface.awaitJobStart(DEFAULT_WAIT_TIMEOUT));
        // Turn the screen on to ensure the app gets into the TOP state.
        setScreenState(true);
        mTestAppInterface.startAndKeepTestActivity(true);
        assertTrue("Job did not start when app had an activity",
                mTestAppInterface.awaitJobStart(DEFAULT_WAIT_TIMEOUT));
    }

    @RequiresDevice // Emulators don't always have access to wifi/network
    @Test
    public void testBackgroundConnectivityJobsThrottled() throws Exception {
        if (!mHasWifi) {
            Log.d(TAG, "Skipping test that requires the device be WiFi enabled.");
            return;
        }
        ensureSavedWifiNetwork(mWifiManager);
        setAirplaneMode(false);
        setWifiState(true, mCm, mWifiManager);
        assumeTrue("device idle not enabled", mDeviceIdleEnabled);
        mTestAppInterface.scheduleJob(false, true);
        runJob();
        assertTrue("Job did not start after scheduling",
                mTestAppInterface.awaitJobStart(DEFAULT_WAIT_TIMEOUT));
        ThermalUtils.overrideThermalStatus(Temperature.THROTTLING_CRITICAL);
        assertTrue("Job did not stop on thermal throttling",
                mTestAppInterface.awaitJobStop(DEFAULT_WAIT_TIMEOUT));
        Thread.sleep(TestJobSchedulerReceiver.JOB_INITIAL_BACKOFF);
        ThermalUtils.overrideThermalNotThrottling();
        runJob();
        assertTrue("Job did not start back from throttling",
                mTestAppInterface.awaitJobStart(DEFAULT_WAIT_TIMEOUT));
    }

    /** Tests that apps in the RESTRICTED bucket still get their one parole session per day. */
    @Test
    public void testJobsInRestrictedBucket_ParoleSession() throws Exception {
        assumeTrue("app standby not enabled", mAppStandbyEnabled);
        assumeFalse("not testable in automotive device", mAutomotiveDevice);

        setRestrictedBucketEnabled(true);

        // Disable coalescing
        Settings.Global.putString(mContext.getContentResolver(),
                Settings.Global.JOB_SCHEDULER_QUOTA_CONTROLLER_CONSTANTS,
                "timing_session_coalescing_duration_ms=0");

        setScreenState(true);

        BatteryUtils.runDumpsysBatteryUnplug();
        setTestPackageStandbyBucket(Bucket.RESTRICTED);
        Thread.sleep(DEFAULT_WAIT_TIMEOUT);
        sendScheduleJobBroadcast(false);
        runJob();
        assertTrue("Parole job didn't start in RESTRICTED bucket",
                mTestAppInterface.awaitJobStart(3_000));

        sendScheduleJobBroadcast(false);
        assertFalse("New job started in RESTRICTED bucket", mTestAppInterface.awaitJobStart(3_000));
    }

    @Test
    public void testJobsInRestrictedBucket_NoRequiredNetwork() throws Exception {
        assumeTrue("app standby not enabled", mAppStandbyEnabled);
        assumeFalse("not testable in automotive device", mAutomotiveDevice);

        setRestrictedBucketEnabled(true);

        // Disable coalescing and the parole session
        Settings.Global.putString(mContext.getContentResolver(),
                Settings.Global.JOB_SCHEDULER_QUOTA_CONTROLLER_CONSTANTS,
                "timing_session_coalescing_duration_ms=0,max_session_count_restricted=0");

        setAirplaneMode(true);
        setScreenState(true);

        BatteryUtils.runDumpsysBatteryUnplug();
        setTestPackageStandbyBucket(Bucket.RESTRICTED);
        Thread.sleep(DEFAULT_WAIT_TIMEOUT);
        sendScheduleJobBroadcast(false);
        assertFalse("New job started in RESTRICTED bucket", mTestAppInterface.awaitJobStart(3_000));

        // Slowly add back required bucket constraints.

        // Battery charging and high.
        BatteryUtils.runDumpsysBatterySetPluggedIn(true);
        assertFalse("New job started in RESTRICTED bucket", mTestAppInterface.awaitJobStart(3_000));
        BatteryUtils.runDumpsysBatterySetLevel(100);
        assertFalse("New job started in RESTRICTED bucket", mTestAppInterface.awaitJobStart(3_000));

        // Device is idle.
        setScreenState(false);
        assertFalse("New job started in RESTRICTED bucket", mTestAppInterface.awaitJobStart(3_000));
        triggerJobIdle();
        assertTrue("New job didn't start in RESTRICTED bucket",
                mTestAppInterface.awaitJobStart(3_000));
    }

    @RequiresDevice // Emulators don't always have access to wifi/network
    @Test
    public void testJobsInRestrictedBucket_WithRequiredNetwork() throws Exception {
        assumeTrue("app standby not enabled", mAppStandbyEnabled);
        assumeFalse("not testable in automotive device", mAutomotiveDevice);
        assumeTrue(mHasWifi);
        ensureSavedWifiNetwork(mWifiManager);

        setRestrictedBucketEnabled(true);

        // Disable coalescing and the parole session
        Settings.Global.putString(mContext.getContentResolver(),
                Settings.Global.JOB_SCHEDULER_QUOTA_CONTROLLER_CONSTANTS,
                "timing_session_coalescing_duration_ms=0,max_session_count_restricted=0");

        setAirplaneMode(true);
        setScreenState(true);

        BatteryUtils.runDumpsysBatteryUnplug();
        setTestPackageStandbyBucket(Bucket.RESTRICTED);
        Thread.sleep(DEFAULT_WAIT_TIMEOUT);
        mTestAppInterface.scheduleJob(false, true);
        runJob();
        assertFalse("New job started in RESTRICTED bucket", mTestAppInterface.awaitJobStart(3_000));

        // Slowly add back required bucket constraints.

        // Battery charging and high.
        BatteryUtils.runDumpsysBatterySetPluggedIn(true);
        runJob();
        assertFalse("New job started in RESTRICTED bucket", mTestAppInterface.awaitJobStart(3_000));
        BatteryUtils.runDumpsysBatterySetLevel(100);
        runJob();
        assertFalse("New job started in RESTRICTED bucket", mTestAppInterface.awaitJobStart(3_000));

        // Device is idle.
        setScreenState(false);
        runJob();
        assertFalse("New job started in RESTRICTED bucket", mTestAppInterface.awaitJobStart(3_000));
        triggerJobIdle();
        runJob();
        assertFalse("New job started in RESTRICTED bucket", mTestAppInterface.awaitJobStart(3_000));

        // Add network
        setAirplaneMode(false);
        setWifiState(true, mCm, mWifiManager);
        runJob();
        assertTrue("New job didn't start in RESTRICTED bucket",
                mTestAppInterface.awaitJobStart(5_000));
    }

    @Test
    public void testJobsInNeverApp() throws Exception {
        assumeTrue("app standby not enabled", mAppStandbyEnabled);
        assumeFalse("not testable in automotive device", mAutomotiveDevice);

        BatteryUtils.runDumpsysBatteryUnplug();
        setTestPackageStandbyBucket(Bucket.NEVER);
        Thread.sleep(DEFAULT_WAIT_TIMEOUT);
        sendScheduleJobBroadcast(false);
        assertFalse("New job started in NEVER bucket", mTestAppInterface.awaitJobStart(3_000));
    }

    @Test
    public void testUidActiveBypassesStandby() throws Exception {
        assumeFalse("not testable in automotive device", mAutomotiveDevice);

        BatteryUtils.runDumpsysBatteryUnplug();
        setTestPackageStandbyBucket(Bucket.NEVER);
        tempWhitelistTestApp(6_000);
        Thread.sleep(DEFAULT_WAIT_TIMEOUT);
        sendScheduleJobBroadcast(false);
        assertTrue("New job in uid-active app failed to start in NEVER standby",
                mTestAppInterface.awaitJobStart(4_000));
    }

    @Test
    public void testBatterySaverOff() throws Exception {
        assumeFalse("not testable in automotive device", mAutomotiveDevice);
        BatteryUtils.assumeBatterySaverFeature();

        BatteryUtils.runDumpsysBatteryUnplug();
        BatteryUtils.enableBatterySaver(false);
        sendScheduleJobBroadcast(false);
        assertTrue("New job failed to start with battery saver OFF",
                mTestAppInterface.awaitJobStart(3_000));
    }

    @Test
    public void testBatterySaverOn() throws Exception {
        assumeFalse("not testable in automotive device", mAutomotiveDevice);
        BatteryUtils.assumeBatterySaverFeature();

        BatteryUtils.runDumpsysBatteryUnplug();
        BatteryUtils.enableBatterySaver(true);
        sendScheduleJobBroadcast(false);
        assertFalse("New job started with battery saver ON",
                mTestAppInterface.awaitJobStart(3_000));
    }

    @Test
    public void testUidActiveBypassesBatterySaverOn() throws Exception {
        assumeFalse("not testable in automotive device", mAutomotiveDevice);
        BatteryUtils.assumeBatterySaverFeature();

        BatteryUtils.runDumpsysBatteryUnplug();
        BatteryUtils.enableBatterySaver(true);
        tempWhitelistTestApp(6_000);
        sendScheduleJobBroadcast(false);
        assertTrue("New job in uid-active app failed to start with battery saver OFF",
                mTestAppInterface.awaitJobStart(3_000));
    }

    @Test
    public void testBatterySaverOnThenUidActive() throws Exception {
        assumeFalse("not testable in automotive device", mAutomotiveDevice);
        BatteryUtils.assumeBatterySaverFeature();

        // Enable battery saver, and schedule a job. It shouldn't run.
        BatteryUtils.runDumpsysBatteryUnplug();
        BatteryUtils.enableBatterySaver(true);
        sendScheduleJobBroadcast(false);
        assertFalse("New job started with battery saver ON",
                mTestAppInterface.awaitJobStart(3_000));


        // Then make the UID active. Now the job should run.
        tempWhitelistTestApp(120_000);
        assertTrue("New job in uid-active app failed to start with battery saver OFF",
                mTestAppInterface.awaitJobStart(120_000));
    }

    @After
    public void tearDown() throws Exception {
        AppOpsUtils.reset(TEST_APP_PACKAGE);
        // Lock thermal service to not throttling
        ThermalUtils.overrideThermalNotThrottling();
        if (mDeviceIdleEnabled) {
            toggleDeviceIdleState(false);
        }
        mTestAppInterface.cleanup();
        BatteryUtils.runDumpsysBatteryReset();
        BatteryUtils.enableBatterySaver(false);
        removeTestAppFromTempWhitelist();

        // Ensure that we leave WiFi in its previous state.
        if (mWifiManager.isWifiEnabled() != mInitialWiFiState) {
            try {
                setWifiState(mInitialWiFiState, mCm, mWifiManager);
            } catch (AssertionFailedError e) {
                // Don't fail the test just because wifi state wasn't set in tearDown.
                Log.e(TAG, "Failed to return wifi state to " + mInitialWiFiState, e);
            }
        }
        Settings.Global.putString(mContext.getContentResolver(),
                Settings.Global.JOB_SCHEDULER_CONSTANTS, mInitialJobSchedulerConstants);
        Settings.Global.putString(mContext.getContentResolver(),
                Settings.Global.ENABLE_RESTRICTED_BUCKET, mInitialRestrictedBucketEnabled);
        if (isAirplaneModeOn() != mInitialAirplaneModeState) {
            setAirplaneMode(mInitialAirplaneModeState);
        }
        mUiDevice.executeShellCommand(
                "cmd jobscheduler reset-execution-quota -u " + UserHandle.myUserId()
                        + " " + TEST_APP_PACKAGE);

        Settings.System.putString(
                mContext.getContentResolver(), SCREEN_OFF_TIMEOUT, mInitialDisplayTimeout);
    }

    private void setTestPackageRestricted(boolean restricted) throws Exception {
        AppOpsUtils.setOpMode(TEST_APP_PACKAGE, "RUN_ANY_IN_BACKGROUND",
                restricted ? AppOpsManager.MODE_IGNORED : AppOpsManager.MODE_ALLOWED);
    }

    private void setRestrictedBucketEnabled(boolean enabled) {
        Settings.Global.putString(mContext.getContentResolver(),
                Settings.Global.ENABLE_RESTRICTED_BUCKET, enabled ? "1" : "0");
    }

    private boolean isTestAppTempWhitelisted() throws Exception {
        final String output = mUiDevice.executeShellCommand("cmd deviceidle tempwhitelist").trim();
        for (String line : output.split("\n")) {
            if (line.contains("UID=" + mTestPackageUid)) {
                return true;
            }
        }
        return false;
    }

    private void sendScheduleJobBroadcast(boolean allowWhileIdle) throws Exception {
        mTestAppInterface.scheduleJob(allowWhileIdle, false);
    }

    private void toggleDeviceIdleState(final boolean idle) throws Exception {
        mUiDevice.executeShellCommand("cmd deviceidle " + (idle ? "force-idle" : "unforce"));
        assertTrue("Could not change device idle state to " + idle,
                waitUntilTrue(SHELL_TIMEOUT, () -> {
                    synchronized (JobThrottlingTest.this) {
                        return mDeviceInDoze == idle;
                    }
                }));
    }

    private void tempWhitelistTestApp(long duration) throws Exception {
        mUiDevice.executeShellCommand("cmd deviceidle tempwhitelist -d " + duration
                + " " + TEST_APP_PACKAGE);
    }

    private void makeTestPackageIdle() throws Exception {
        mUiDevice.executeShellCommand("am make-uid-idle --user current " + TEST_APP_PACKAGE);
    }

    private void setTestPackageStandbyBucket(Bucket bucket) throws Exception {
        final String bucketName;
        switch (bucket) {
            case ACTIVE:
                bucketName = "active";
                break;
            case WORKING_SET:
                bucketName = "working";
                break;
            case FREQUENT:
                bucketName = "frequent";
                break;
            case RARE:
                bucketName = "rare";
                break;
            case RESTRICTED:
                bucketName = "restricted";
                break;
            case NEVER:
                bucketName = "never";
                break;
            default:
                throw new IllegalArgumentException("Requested unknown bucket " + bucket);
        }
        mUiDevice.executeShellCommand("am set-standby-bucket " + TEST_APP_PACKAGE
                + " " + bucketName);
    }

    private boolean removeTestAppFromTempWhitelist() throws Exception {
        mUiDevice.executeShellCommand("cmd deviceidle tempwhitelist -r " + TEST_APP_PACKAGE);
        return waitUntilTrue(SHELL_TIMEOUT, () -> !isTestAppTempWhitelisted());
    }

    /**
     * Set the screen state.
     */
    private void setScreenState(boolean on) throws Exception {
        if (on) {
            mUiDevice.executeShellCommand("input keyevent KEYCODE_WAKEUP");
        } else {
            mUiDevice.executeShellCommand("input keyevent KEYCODE_SLEEP");
        }
        // Wait a little bit to make sure the screen state has changed.
        Thread.sleep(2_000);
    }

    /**
     * Trigger job idle (not device idle);
     */
    private void triggerJobIdle() throws Exception {
        mUiDevice.executeShellCommand("cmd activity idle-maintenance");
        // Wait a moment to let that happen before proceeding.
        Thread.sleep(2_000);
    }

    /** Asks (not forces) JobScheduler to run the job if constraints are met. */
    private void runJob() throws Exception {
        // Since connectivity is a functional constraint, calling the "run" command without force
        // will only get the job to run if the constraint is satisfied.
        mUiDevice.executeShellCommand("cmd jobscheduler run -s"
                + " -u " + UserHandle.myUserId() + " " + TEST_APP_PACKAGE + " " + mTestJobId);
    }

    private boolean isAirplaneModeOn() throws IOException {
        final String output =
                mUiDevice.executeShellCommand("cmd connectivity airplane-mode").trim();
        return "enabled".equals(output);
    }

    private void setAirplaneMode(boolean on) throws IOException {
        if (on) {
            mUiDevice.executeShellCommand("cmd connectivity airplane-mode enable");
        } else {
            mUiDevice.executeShellCommand("cmd connectivity airplane-mode disable");
        }
    }

    private boolean waitUntilTrue(long maxWait, Condition condition) throws Exception {
        final long deadLine = SystemClock.uptimeMillis() + maxWait;
        do {
            Thread.sleep(POLL_INTERVAL);
        } while (!condition.isTrue() && SystemClock.uptimeMillis() < deadLine);
        return condition.isTrue();
    }

    private interface Condition {
        boolean isTrue() throws Exception;
    }
}
