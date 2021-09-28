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

package android.jobscheduler.cts;

import static com.android.compatibility.common.util.TestUtils.waitUntil;

import android.annotation.TargetApi;
import android.app.UiModeManager;
import android.app.job.JobInfo;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.PowerManager;
import android.os.UserHandle;
import android.support.test.uiautomator.UiDevice;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.BatteryUtils;

/**
 * Make sure the state of {@link android.app.job.JobScheduler} is correct.
 */
public class IdleConstraintTest extends BaseJobSchedulerTest {
    /** Unique identifier for the job scheduled by this suite of tests. */
    private static final int STATE_JOB_ID = IdleConstraintTest.class.hashCode();
    private static final String TAG = "IdleConstraintTest";

    private PowerManager mPowerManager;
    private JobInfo.Builder mBuilder;
    private UiDevice mUiDevice;

    private String mInitialDisplayTimeout;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mBuilder = new JobInfo.Builder(STATE_JOB_ID, kJobServiceComponent);
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        mPowerManager = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);

        // Make sure the screen doesn't turn off when the test turns it on.
        mInitialDisplayTimeout = mUiDevice.executeShellCommand(
                "settings get system screen_off_timeout");
        mUiDevice.executeShellCommand("settings put system screen_off_timeout 300000");
    }

    @Override
    public void tearDown() throws Exception {
        mJobScheduler.cancel(STATE_JOB_ID);
        // Put device back in to normal operation.
        toggleScreenOn(true);
        if (isCarModeSupported()) {
            setCarMode(false);
        }

        mUiDevice.executeShellCommand(
                "settings put system screen_off_timeout " + mInitialDisplayTimeout);

        super.tearDown();
    }

    void assertJobReady() throws Exception {
        assertJobReady(STATE_JOB_ID);
    }

    void assertJobWaiting() throws Exception {
        assertJobWaiting(STATE_JOB_ID);
    }

    void assertJobNotReady() throws Exception {
        assertJobNotReady(STATE_JOB_ID);
    }

    /**
     * Toggle device is dock idle or dock active.
     */
    private void toggleFakeDeviceDockState(final boolean idle) throws Exception {
        mUiDevice.executeShellCommand("cmd jobscheduler trigger-dock-state "
                + (idle ? "idle" : "active"));
        // Wait a moment to let that happen before proceeding.
        Thread.sleep(2_000);
    }

    /**
     * Set the screen state.
     */
    private void toggleScreenOn(final boolean screenon) throws Exception {
        BatteryUtils.turnOnScreen(screenon);
        // Wait a little bit for the broadcasts to be processed.
        Thread.sleep(2_000);
    }

    /**
     * Simulated for idle, and then perform idle maintenance now.
     */
    private void triggerIdleMaintenance() throws Exception {
        mUiDevice.executeShellCommand("cmd activity idle-maintenance");
        // Wait a moment to let that happen before proceeding.
        Thread.sleep(2_000);
    }

    /**
     * Schedule a job that requires the device is idle, and assert it fired to make
     * sure the device is idle.
     */
    void verifyIdleState() throws Exception {
        kTestEnvironment.setExpectedExecutions(1);
        kTestEnvironment.setExpectedWaitForRun();
        mJobScheduler.schedule(mBuilder.setRequiresDeviceIdle(true).build());
        assertJobReady();
        kTestEnvironment.readyToRun();
        runSatisfiedJob();

        assertTrue("Job with idle constraint did not fire on idle",
                kTestEnvironment.awaitExecution());
    }

    /**
     * Schedule a job that requires the device is idle, and assert it failed to make
     * sure the device is active.
     */
    void verifyActiveState() throws Exception {
        kTestEnvironment.setExpectedExecutions(0);
        kTestEnvironment.setExpectedWaitForRun();
        mJobScheduler.schedule(mBuilder.setRequiresDeviceIdle(true).build());
        assertJobWaiting();
        assertJobNotReady();
        kTestEnvironment.readyToRun();
        runSatisfiedJob();

        assertFalse("Job with idle constraint fired while not on idle.",
                kTestEnvironment.awaitExecution(250));
    }

    /**
     * Ensure that device can switch state normally.
     */
    public void testDeviceChangeIdleActiveState() throws Exception {
        toggleScreenOn(true);
        verifyActiveState();

        // Assert device is idle when screen is off for a while.
        toggleScreenOn(false);
        triggerIdleMaintenance();
        verifyIdleState();

        // Assert device is back to active when screen is on.
        toggleScreenOn(true);
        verifyActiveState();
    }

    private boolean isCarModeSupported() {
        // TVs don't support car mode.
        return !getContext().getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_LEANBACK_ONLY)
                && !getContext().getSystemService(UiModeManager.class).isUiModeLocked();
    }

    /**
     * Check if dock state is supported.
     */
    private boolean isDockStateSupported() {
        // Car does not support dock state.
        return !getContext().getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_AUTOMOTIVE);
    }

    /**
     * Ensure that device can switch state on dock normally.
     */
    @TargetApi(28)
    public void testScreenOnDeviceOnDockChangeState() throws Exception {
        if (!isDockStateSupported()) {
            return;
        }
        toggleScreenOn(true);
        verifyActiveState();

        // Assert device go to idle if user doesn't interact with device for a while.
        toggleFakeDeviceDockState(true /* idle */);
        triggerIdleMaintenance();
        verifyIdleState();

        // Assert device go back to active if user interacts with device.
        toggleFakeDeviceDockState(false /* active */);
        verifyActiveState();
    }

    /**
     *  Ensure that the tracker ignores this dock intent during screen off.
     */
    @TargetApi(28)
    public void testScreenOffDeviceOnDockNoChangeState() throws Exception {
        if (!isDockStateSupported()) {
            return;
        }
        toggleScreenOn(false);
        triggerIdleMaintenance();
        verifyIdleState();

        toggleFakeDeviceDockState(false /* active */);
        verifyIdleState();
    }

    private void setCarMode(boolean on) throws Exception {
        UiModeManager uiModeManager = getContext().getSystemService(UiModeManager.class);
        final boolean wasScreenOn = mPowerManager.isInteractive();
        if (on) {
            uiModeManager.enableCarMode(0);
            waitUntil("UI mode didn't change to " + Configuration.UI_MODE_TYPE_CAR,
                    () -> Configuration.UI_MODE_TYPE_CAR ==
                            (getContext().getResources().getConfiguration().uiMode
                                    & Configuration.UI_MODE_TYPE_MASK));
        } else {
            uiModeManager.disableCarMode(0);
            waitUntil("UI mode didn't change from " + Configuration.UI_MODE_TYPE_CAR,
                    () -> Configuration.UI_MODE_TYPE_CAR !=
                            (getContext().getResources().getConfiguration().uiMode
                                    & Configuration.UI_MODE_TYPE_MASK));
        }
        Thread.sleep(2_000);
        if (mPowerManager.isInteractive() != wasScreenOn) {
            // Apparently setting the car mode can change the screen state >.<
            Log.d(TAG, "Screen state changed");
            toggleScreenOn(wasScreenOn);
        }
    }

    /**
     * Ensure car mode is considered active.
     */
    public void testCarModePreventsIdle() throws Exception {
        if (!isCarModeSupported()) {
            return;
        }

        toggleScreenOn(false);

        setCarMode(true);
        triggerIdleMaintenance();
        verifyActiveState();

        setCarMode(false);
        triggerIdleMaintenance();
        verifyIdleState();
    }

    private void runIdleJobStartsOnlyWhenIdle() throws Exception {
        if (!isCarModeSupported()) {
            return;
        }

        toggleScreenOn(true);

        kTestEnvironment.setExpectedExecutions(0);
        kTestEnvironment.setExpectedWaitForRun();
        mJobScheduler.schedule(mBuilder.setRequiresDeviceIdle(true).build());
        triggerIdleMaintenance();
        assertJobWaiting();
        assertJobNotReady();
        kTestEnvironment.readyToRun();
        runSatisfiedJob();
        assertFalse("Job fired when the device was active.", kTestEnvironment.awaitExecution(500));

        kTestEnvironment.setExpectedExecutions(0);
        kTestEnvironment.setExpectedWaitForRun();
        setCarMode(true);
        toggleScreenOn(false);
        triggerIdleMaintenance();
        assertJobWaiting();
        assertJobNotReady();
        kTestEnvironment.readyToRun();
        runSatisfiedJob();
        assertFalse("Job fired when the device was active.", kTestEnvironment.awaitExecution(500));

        kTestEnvironment.setExpectedExecutions(1);
        kTestEnvironment.setExpectedWaitForRun();
        kTestEnvironment.setContinueAfterStart();
        kTestEnvironment.setExpectedStopped();
        setCarMode(false);
        triggerIdleMaintenance();
        assertJobReady();
        kTestEnvironment.readyToRun();
        runSatisfiedJob();
        assertTrue("Job didn't fire when the device became idle.",
                kTestEnvironment.awaitExecution());
    }

    public void testIdleJobStartsOnlyWhenIdle_carEndsIdle() throws Exception {
        if (!isCarModeSupported()) {
            return;
        }
        runIdleJobStartsOnlyWhenIdle();

        setCarMode(true);
        assertTrue("Job didn't stop when the device became active.",
                kTestEnvironment.awaitStopped());
    }

    public void testIdleJobStartsOnlyWhenIdle_screenEndsIdle() throws Exception {
        if (!isCarModeSupported()) {
            return;
        }
        runIdleJobStartsOnlyWhenIdle();

        toggleScreenOn(true);
        assertTrue("Job didn't stop when the device became active.",
                kTestEnvironment.awaitStopped());
    }

    /** Asks (not forces) JobScheduler to run the job if constraints are met. */
    private void runSatisfiedJob() throws Exception {
        mUiDevice.executeShellCommand("cmd jobscheduler run -s"
                + " -u " + UserHandle.myUserId()
                + " " + kJobServiceComponent.getPackageName()
                + " " + STATE_JOB_ID);
    }
}
