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
 * limitations under the License
 */

package com.android.cts.devicepolicy;

import android.platform.test.annotations.FlakyTest;
import android.platform.test.annotations.LargeTest;

import com.android.tradefed.device.DeviceNotAvailableException;

import org.junit.Test;

import java.util.concurrent.TimeUnit;

public class ManagedProfileTimeoutTest extends BaseManagedProfileTest {
    // This should be sufficiently larger than ProfileTimeoutTestHelper.TIMEOUT_MS
    private static final int PROFILE_TIMEOUT_DELAY_MS = 60_000;

    /** Profile should get locked if it is not in foreground no matter what. */
    @FlakyTest
    @Test
    public void testWorkProfileTimeoutBackground() throws Exception {
        if (!mHasFeature || !mHasSecureLockScreen) {
            return;
        }
        setUpWorkProfileTimeout();

        startDummyActivity(mPrimaryUserId, true);
        simulateUserInteraction(PROFILE_TIMEOUT_DELAY_MS);

        verifyOnlyProfileLocked(true);
    }

    /** Profile should get locked if it is in foreground but with no user activity. */
    @LargeTest
    @Test
    public void testWorkProfileTimeoutIdleActivity() throws Exception {
        if (!mHasFeature || !mHasSecureLockScreen) {
            return;
        }
        setUpWorkProfileTimeout();

        startDummyActivity(mProfileUserId, false);
        Thread.sleep(PROFILE_TIMEOUT_DELAY_MS);

        verifyOnlyProfileLocked(true);
    }

    /** User activity in profile should prevent it from locking. */
    @FlakyTest
    @Test
    public void testWorkProfileTimeoutUserActivity() throws Exception {
        if (!mHasFeature || !mHasSecureLockScreen) {
            return;
        }
        setUpWorkProfileTimeout();

        startDummyActivity(mProfileUserId, false);
        simulateUserInteraction(PROFILE_TIMEOUT_DELAY_MS);

        verifyOnlyProfileLocked(false);
    }

    /** Keep screen on window flag in the profile should prevent it from locking. */
    @FlakyTest
    @Test
    public void testWorkProfileTimeoutKeepScreenOnWindow() throws Exception {
        if (!mHasFeature || !mHasSecureLockScreen) {
            return;
        }
        setUpWorkProfileTimeout();

        startDummyActivity(mProfileUserId, true);
        Thread.sleep(PROFILE_TIMEOUT_DELAY_MS);

        verifyOnlyProfileLocked(false);
    }

    private void setUpWorkProfileTimeout() throws DeviceNotAvailableException {
        // Set separate challenge.
        changeUserCredential(TEST_PASSWORD, null, mProfileUserId);

        // Make sure the profile is not prematurely locked.
        verifyUserCredential(TEST_PASSWORD, mProfileUserId);
        verifyOnlyProfileLocked(false);
        // Set profile timeout to 5 seconds.
        runProfileTimeoutTest("testSetWorkProfileTimeout", mProfileUserId);
    }

    private void verifyOnlyProfileLocked(boolean locked) throws DeviceNotAvailableException {
        final String expectedResultTest = locked ? "testDeviceLocked" : "testDeviceNotLocked";
        runProfileTimeoutTest(expectedResultTest, mProfileUserId);
        // Primary profile shouldn't be locked.
        runProfileTimeoutTest("testDeviceNotLocked", mPrimaryUserId);
    }

    private void simulateUserInteraction(int timeMs) throws Exception {
        final long endTime = System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(timeMs);
        final UserActivityEmulator helper = new UserActivityEmulator(getDevice());
        while (System.nanoTime() < endTime) {
            helper.tapScreenCenter();
            // Just in case to prevent busy loop.
            Thread.sleep(100);
        }
    }

    private void runProfileTimeoutTest(String method, int userId)
            throws DeviceNotAvailableException {
        runDeviceTestsAsUser(MANAGED_PROFILE_PKG, MANAGED_PROFILE_PKG + ".ProfileTimeoutTestHelper",
                method, userId);
    }

    private void startDummyActivity(int profileUserId, boolean keepScreenOn) throws Exception {
        getDevice().executeShellCommand(String.format(
                "am start-activity -W --user %d --ez keep_screen_on %s %s/.TimeoutActivity",
                profileUserId, keepScreenOn, MANAGED_PROFILE_PKG));
    }
}
