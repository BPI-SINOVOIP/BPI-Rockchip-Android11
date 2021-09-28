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

package android.jobscheduler.cts;


import android.annotation.TargetApi;
import android.app.job.JobInfo;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.SystemClock;
import android.provider.Settings;
import android.util.Log;

import com.android.compatibility.common.util.SystemUtil;

/**
 * Schedules jobs with the {@link android.app.job.JobScheduler} that have battery constraints.
 */
@TargetApi(26)
public class BatteryConstraintTest extends BaseJobSchedulerTest {
    private static final String TAG = "BatteryConstraintTest";

    private String FEATURE_WATCH = "android.hardware.type.watch";
    private String TWM_HARDWARE_FEATURE = "com.google.clockwork.hardware.traditional_watch_mode";

    /** Unique identifier for the job scheduled by this suite of tests. */
    public static final int BATTERY_JOB_ID = BatteryConstraintTest.class.hashCode();

    private JobInfo.Builder mBuilder;
    /**
     * Record of the previous state of power save mode trigger level to reset it after the test
     * finishes.
     */
    private int mPreviousLowPowerTriggerLevel;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        // Disable power save mode as some devices may turn off Android when power save mode is
        // enabled, causing the test to fail.
        mPreviousLowPowerTriggerLevel = Settings.Global.getInt(getContext().getContentResolver(),
                Settings.Global.LOW_POWER_MODE_TRIGGER_LEVEL, -1);
        Settings.Global.putInt(getContext().getContentResolver(),
                Settings.Global.LOW_POWER_MODE_TRIGGER_LEVEL, 0);

        mBuilder = new JobInfo.Builder(BATTERY_JOB_ID, kJobServiceComponent);
        SystemUtil.runShellCommand(getInstrumentation(), "cmd jobscheduler monitor-battery on");
    }

    @Override
    public void tearDown() throws Exception {
        mJobScheduler.cancel(BATTERY_JOB_ID);
        // Put battery service back in to normal operation.
        SystemUtil.runShellCommand(getInstrumentation(), "cmd jobscheduler monitor-battery off");
        SystemUtil.runShellCommand(getInstrumentation(), "cmd battery reset");

        // Reset power save mode to its previous state.
        if (mPreviousLowPowerTriggerLevel == -1) {
            Settings.Global.putString(getContext().getContentResolver(),
                    Settings.Global.LOW_POWER_MODE_TRIGGER_LEVEL, null);
        } else {
            Settings.Global.putInt(getContext().getContentResolver(),
                    Settings.Global.LOW_POWER_MODE_TRIGGER_LEVEL, mPreviousLowPowerTriggerLevel);
        }

        super.tearDown();
    }

    boolean hasBattery() throws Exception {
        Intent batteryInfo = getContext().registerReceiver(
                null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        boolean present = batteryInfo.getBooleanExtra(BatteryManager.EXTRA_PRESENT, true);
        if (!present) {
            Log.i(TAG, "Device doesn't have a battery.");
        }
        return present;
    }

    void setBatteryState(boolean plugged, int level) throws Exception {
        if (plugged) {
            SystemUtil.runShellCommand(getInstrumentation(), "cmd battery set ac 1");
        } else {
            SystemUtil.runShellCommand(getInstrumentation(), "cmd battery unplug");
        }
        int seq = Integer.parseInt(SystemUtil.runShellCommand(getInstrumentation(),
                "cmd battery set -f level " + level).trim());
        long startTime = SystemClock.elapsedRealtime();

        // Wait for the battery update to be processed by job scheduler before proceeding.
        int curSeq;
        boolean curCharging;
        do {
            Thread.sleep(50);
            curSeq = Integer.parseInt(SystemUtil.runShellCommand(getInstrumentation(),
                    "cmd jobscheduler get-battery-seq").trim());
            curCharging = Boolean.parseBoolean(SystemUtil.runShellCommand(getInstrumentation(),
                    "cmd jobscheduler get-battery-charging").trim());
            if (curSeq >= seq && curCharging == plugged) {
                return;
            }
        } while ((SystemClock.elapsedRealtime() - startTime) < 5000);

        fail("Timed out waiting for job scheduler: expected seq=" + seq + ", cur=" + curSeq
                + ", expected plugged=" + plugged + " curCharging=" + curCharging);
    }

    void verifyChargingState(boolean charging) throws Exception {
        boolean curCharging = Boolean.parseBoolean(SystemUtil.runShellCommand(getInstrumentation(),
                "cmd jobscheduler get-battery-charging").trim());
        assertEquals(charging, curCharging);
    }

    void verifyBatteryNotLowState(boolean notLow) throws Exception {
        boolean curNotLow = Boolean.parseBoolean(SystemUtil.runShellCommand(getInstrumentation(),
                "cmd jobscheduler get-battery-not-low").trim());
        assertEquals(notLow, curNotLow);
        IntentFilter filter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        Intent batteryState = getContext().registerReceiver(null, filter);
        assertEquals(notLow,
                !batteryState.getBooleanExtra(BatteryManager.EXTRA_BATTERY_LOW, notLow));
    }

    String getJobState() throws Exception {
        return getJobState(BATTERY_JOB_ID);
    }

    void assertJobReady() throws Exception {
        assertJobReady(BATTERY_JOB_ID);
    }

    void assertJobWaiting() throws Exception {
        assertJobWaiting(BATTERY_JOB_ID);
    }

    void assertJobNotReady() throws Exception {
        assertJobNotReady(BATTERY_JOB_ID);
    }

    // --------------------------------------------------------------------------------------------
    // Positives - schedule jobs under conditions that require them to pass.
    // --------------------------------------------------------------------------------------------

    /**
     * Schedule a job that requires the device is charging, when the battery reports it is
     * plugged in.
     */
    public void testChargingConstraintExecutes() throws Exception {
        setBatteryState(true, 100);
        verifyChargingState(true);

        kTestEnvironment.setExpectedExecutions(1);
        kTestEnvironment.setExpectedWaitForRun();
        mJobScheduler.schedule(mBuilder.setRequiresCharging(true).build());
        assertJobReady();
        kTestEnvironment.readyToRun();

        assertTrue("Job with charging constraint did not fire on power.",
                kTestEnvironment.awaitExecution());
    }

    /**
     * Schedule a job that requires the device is not critical, when the battery reports it is
     * plugged in.
     */
    public void testBatteryNotLowConstraintExecutes_withPower() throws Exception {
        setBatteryState(true, 100);
        Thread.sleep(2_000);
        verifyChargingState(true);
        verifyBatteryNotLowState(true);

        kTestEnvironment.setExpectedExecutions(1);
        kTestEnvironment.setExpectedWaitForRun();
        mJobScheduler.schedule(mBuilder.setRequiresBatteryNotLow(true).build());
        assertJobReady();
        kTestEnvironment.readyToRun();

        assertTrue("Job with battery not low constraint did not fire on power.",
                kTestEnvironment.awaitExecution());
    }

    /**
     * Schedule a job that requires the device is not critical, when the battery reports it is
     * not plugged in but has sufficient power.
     */
    public void testBatteryNotLowConstraintExecutes_withoutPower() throws Exception {
        // "Without power" test case is valid only for devices with a battery.
        if (!hasBattery()) {
            return;
        }

        setBatteryState(false, 100);
        Thread.sleep(2_000);
        verifyChargingState(false);
        verifyBatteryNotLowState(true);

        kTestEnvironment.setExpectedExecutions(1);
        kTestEnvironment.setExpectedWaitForRun();
        mJobScheduler.schedule(mBuilder.setRequiresBatteryNotLow(true).build());
        assertJobReady();
        kTestEnvironment.readyToRun();

        assertTrue("Job with battery not low constraint did not fire on power.",
                kTestEnvironment.awaitExecution());
    }

    // --------------------------------------------------------------------------------------------
    // Negatives - schedule jobs under conditions that require that they fail.
    // --------------------------------------------------------------------------------------------

    /**
     * Schedule a job that requires the device is charging, and assert if failed when
     * the device is not on power.
     */
    public void testChargingConstraintFails() throws Exception {
        // "Without power" test case is valid only for devices with a battery.
        if (!hasBattery()) {
            return;
        }

        setBatteryState(false, 100);
        verifyChargingState(false);

        kTestEnvironment.setExpectedExecutions(0);
        kTestEnvironment.setExpectedWaitForRun();
        mJobScheduler.schedule(mBuilder.setRequiresCharging(true).build());
        assertJobWaiting();
        assertJobNotReady();
        kTestEnvironment.readyToRun();

        assertFalse("Job with charging constraint fired while not on power.",
                kTestEnvironment.awaitExecution(250));
        assertJobWaiting();
        assertJobNotReady();

        // Ensure the job runs once the device is plugged in.
        kTestEnvironment.setExpectedExecutions(1);
        kTestEnvironment.setExpectedWaitForRun();
        kTestEnvironment.setContinueAfterStart();
        setBatteryState(true, 100);
        verifyChargingState(true);
        kTestEnvironment.setExpectedStopped();
        assertJobReady();
        kTestEnvironment.readyToRun();
        assertTrue("Job with charging constraint did not fire on power.",
                kTestEnvironment.awaitExecution());

        // And check that the job is stopped if the device is unplugged while it is running.
        setBatteryState(false, 100);
        verifyChargingState(false);
        assertTrue("Job with charging constraint did not stop when power removed.",
                kTestEnvironment.awaitStopped());
    }

    /**
     * Schedule a job that requires the device is not critical, and assert it failed when
     * the battery level is critical and not on power.
     */
    public void testBatteryNotLowConstraintFails_withoutPower() throws Exception {
        // "Without power" test case is valid only for devices with a battery.
        if (!hasBattery()) {
            return;
        }
        if (getInstrumentation().getContext().getPackageManager().hasSystemFeature(FEATURE_WATCH)
                && getInstrumentation().getContext().getPackageManager().hasSystemFeature(
                TWM_HARDWARE_FEATURE)) {
            return;
        }

        setBatteryState(false, 5);
        // setBatteryState() waited for the charging/not-charging state to formally settle,
        // but battery level reporting lags behind that.  wait a moment to let that happen
        // before proceeding.
        Thread.sleep(2_000);
        verifyChargingState(false);
        verifyBatteryNotLowState(false);

        kTestEnvironment.setExpectedExecutions(0);
        kTestEnvironment.setExpectedWaitForRun();
        mJobScheduler.schedule(mBuilder.setRequiresBatteryNotLow(true).build());
        assertJobWaiting();
        assertJobNotReady();
        kTestEnvironment.readyToRun();

        assertFalse("Job with battery not low constraint fired while level critical.",
                kTestEnvironment.awaitExecution(250));
        assertJobWaiting();
        assertJobNotReady();

        // Ensure the job runs once the device's battery level is not low.
        kTestEnvironment.setExpectedExecutions(1);
        kTestEnvironment.setExpectedWaitForRun();
        kTestEnvironment.setContinueAfterStart();
        setBatteryState(false, 50);
        Thread.sleep(2_000);
        verifyChargingState(false);
        verifyBatteryNotLowState(true);
        kTestEnvironment.setExpectedStopped();
        assertJobReady();
        kTestEnvironment.readyToRun();
        assertTrue("Job with not low constraint did not fire when charge increased.",
                kTestEnvironment.awaitExecution());

        // And check that the job is stopped if battery goes low again.
        setBatteryState(false, 5);
        setBatteryState(false, 4);
        Thread.sleep(2_000);
        verifyChargingState(false);
        verifyBatteryNotLowState(false);
        assertTrue("Job with not low constraint did not stop when battery went low.",
                kTestEnvironment.awaitStopped());
    }
}
