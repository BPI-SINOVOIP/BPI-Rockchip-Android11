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

package com.android.cts.devicepolicy;

import android.platform.test.annotations.FlakyTest;

import org.junit.Test;

/**
 * Set of tests for use cases that apply to profile and device owner with DPC
 * targeting API level 25.
 */
public abstract class DeviceAndProfileOwnerTestApi25 extends BaseDevicePolicyTest {

    protected static final String DEVICE_ADMIN_PKG = "com.android.cts.deviceandprofileowner";
    protected static final String DEVICE_ADMIN_APK = "CtsDeviceAndProfileOwnerApp25.apk";

    private static final String TEST_APP_APK = "CtsSimpleApp.apk";
    private static final String TEST_APP_PKG = "com.android.cts.launcherapps.simpleapp";
    private static final String SIMPLE_PRE_M_APP_APK = "CtsSimplePreMApp.apk";

    protected static final String ADMIN_RECEIVER_TEST_CLASS
            = ".BaseDeviceAdminTest$BasicAdminReceiver";

    protected int mUserId;

    @Override
    public void tearDown() throws Exception {
        if (mHasFeature) {
            getDevice().uninstallPackage(DEVICE_ADMIN_PKG);
            getDevice().uninstallPackage(TEST_APP_PKG);

            // Clear device lock in case test fails (testUnlockFbe in particular)
            getDevice().executeShellCommand("cmd lock_settings clear --old 12345");
            // Press the HOME key to close any alart dialog that may be shown.
            getDevice().executeShellCommand("input keyevent 3");
        }
        super.tearDown();
    }

    @Test
    public void testPermissionGrantPreMApp() throws Exception {
        if (!mHasFeature) {
            return;
        }
        installAppAsUser(SIMPLE_PRE_M_APP_APK, mUserId);
        executeDeviceTestMethod(".PermissionsTest", "testPermissionGrantStateAppPreMDeviceAdminPreQ");
    }

    @Test
    public void testPasswordRequirementsApi() throws Exception {
        if (!mHasFeature) {
            return;
        }

        executeDeviceTestMethod(".PasswordRequirementsTest",
                "testPasswordConstraintsDoesntThrowAndPreservesValuesPreR");
    }

    @Test
    public void testResetPasswordDeprecated() throws Exception {
        if (!mHasFeature || !mHasSecureLockScreen) {
            return;
        }
        executeDeviceTestMethod(".ResetPasswordTest", "testResetPasswordDeprecated");
    }

    protected void executeDeviceTestClass(String className) throws Exception {
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, className, mUserId);
    }

    protected void executeDeviceTestMethod(String className, String testName) throws Exception {
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, className, testName, mUserId);
    }

    /**
     * Start SimpleActivity synchronously in a particular user.
     */
    protected void startSimpleActivityAsUser(int userId) throws Exception {
        installAppAsUser(TEST_APP_APK, userId);
        String command = "am start -W --user " + userId + " " + TEST_APP_PKG + "/"
                + TEST_APP_PKG + ".SimpleActivity";
        getDevice().executeShellCommand(command);
    }
}
