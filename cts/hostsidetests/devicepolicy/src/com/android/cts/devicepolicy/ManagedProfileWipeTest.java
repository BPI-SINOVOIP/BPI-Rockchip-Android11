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

import static com.android.cts.devicepolicy.metrics.DevicePolicyEventLogVerifier.assertMetricsLogged;
import static com.android.cts.devicepolicy.metrics.DevicePolicyEventLogVerifier.isStatsdEnabled;

import static org.junit.Assert.assertTrue;

import android.platform.test.annotations.FlakyTest;
import android.stats.devicepolicy.EventId;

import com.android.cts.devicepolicy.metrics.DevicePolicyEventWrapper;
import com.android.tradefed.device.DeviceNotAvailableException;

import org.junit.Test;

import java.util.HashMap;
import java.util.Map;

public class ManagedProfileWipeTest extends BaseManagedProfileTest {

    private static final String KEY_PROFILE_ID = "profileId";
    private final Map<String, String> mTestArgs = new HashMap<>();

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mTestArgs.put(KEY_PROFILE_ID, Integer.toString(mProfileUserId));
        configureNotificationListener();
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();
        mTestArgs.clear();
    }

    @FlakyTest
    @Test
    public void testWipeDataWithReason() throws Exception {
        if (!mHasFeature) {
            return;
        }
        assertTrue(listUsers().contains(mProfileUserId));

        // testWipeDataWithReason() removes the managed profile,
        // so it needs to separated from other tests.
        // Check and clear the notification is presented after work profile got removed, so profile
        // user no longer exists, verification should be run in primary user.
        // Both the profile wipe and notification verification are done on the device side test
        // because notifications are checked using a NotificationListenerService
        runDeviceTestsAsUser(
                MANAGED_PROFILE_PKG,
                ".WipeDataNotificationTest",
                "testWipeDataWithReasonVerification",
                mParentUserId,
                mTestArgs);
        // Note: the managed profile is removed by this test, which will make removeUserCommand in
        // tearDown() to complain, but that should be OK since its result is not asserted.
        waitUntilUserRemoved(mProfileUserId);
    }

    @FlakyTest
    @Test
    public void testWipeDataLogged() throws Exception {
        if (!mHasFeature || !isStatsdEnabled(getDevice())) {
            return;
        }
        assertTrue(listUsers().contains(mProfileUserId));

        // Both the profile wipe and notification verification are done on the device side test
        // because notifications are checked using a NotificationListenerService
        assertMetricsLogged(getDevice(), () -> {
            runDeviceTestsAsUser(
                    MANAGED_PROFILE_PKG,
                    ".WipeDataNotificationTest",
                    "testWipeDataWithReasonVerification",
                    mParentUserId,
                    mTestArgs);
        }, new DevicePolicyEventWrapper.Builder(EventId.WIPE_DATA_WITH_REASON_VALUE)
                .setAdminPackageName(MANAGED_PROFILE_PKG)
                .setInt(0)
                .setStrings("notCalledFromParent")
                .build());
        waitUntilUserRemoved(mProfileUserId);
    }

    @FlakyTest
    @Test
    public void testWipeDataWithoutReason() throws Exception {
        if (!mHasFeature) {
            return;
        }
        assertTrue(listUsers().contains(mProfileUserId));

        // testWipeDataWithoutReason() removes the managed profile,
        // so it needs to separated from other tests.
        // Check the notification is not presented after work profile got removed, so profile user
        // no longer exists, verification should be run in primary user.
        // Both the profile wipe and notification verification are done on the device side test
        // because notifications are checked using a NotificationListenerService
        runDeviceTestsAsUser(
                MANAGED_PROFILE_PKG,
                ".WipeDataNotificationTest",
                "testWipeDataWithoutReasonVerification",
                mParentUserId,
                mTestArgs);

        // Note: the managed profile is removed by this test, which will make removeUserCommand in
        // tearDown() to complain, but that should be OK since its result is not asserted.
        waitUntilUserRemoved(mProfileUserId);
    }

    /**
     * wipeData() test removes the managed profile, so it needs to be separated from other tests.
     */
    @Test
    public void testWipeData() throws Exception {
        if (!mHasFeature) {
            return;
        }
        assertTrue(listUsers().contains(mProfileUserId));

        runDeviceTestsAsUser(
                MANAGED_PROFILE_PKG,
                ".WipeDataNotificationTest",
                "testWipeDataWithEmptyReasonVerification",
                mParentUserId,
                mTestArgs);

        // Note: the managed profile is removed by this test, which will make removeUserCommand in
        // tearDown() to complain, but that should be OK since its result is not asserted.
        waitUntilUserRemoved(mProfileUserId);
    }

    private void configureNotificationListener() throws DeviceNotAvailableException {
        getDevice().executeShellCommand("cmd notification allow_listener "
                + "com.android.cts.managedprofile/.NotificationListener");
    }
}
