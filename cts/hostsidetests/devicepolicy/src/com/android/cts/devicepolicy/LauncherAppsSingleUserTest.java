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

package com.android.cts.devicepolicy;

import android.platform.test.annotations.FlakyTest;

import org.junit.Test;

import java.util.Collections;

/**
 * Set of tests for LauncherApps with managed profiles.
 */
public class LauncherAppsSingleUserTest extends BaseLauncherAppsTest {

    private boolean mHasLauncherApps;
    private String mSerialNumber;
    private int mCurrentUserId;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mHasLauncherApps = getDevice().getApiLevel() >= 21;

        if (mHasLauncherApps) {
            mCurrentUserId = getDevice().getCurrentUser();
            mSerialNumber = Integer.toString(getUserSerialNumber(mCurrentUserId));
            uninstallTestApps();
            installTestApps(mCurrentUserId);
        }
    }

    @Override
    public void tearDown() throws Exception {
        if (mHasLauncherApps) {
            uninstallTestApps();
        }
        super.tearDown();
    }

    @Test
    public void testInstallAppMainUser() throws Exception {
        if (!mHasLauncherApps) {
            return;
        }
        installAppAsUser(SIMPLE_APP_APK, mCurrentUserId);
        runDeviceTestsAsUser(LAUNCHER_TESTS_PKG,
                LAUNCHER_TESTS_CLASS, "testSimpleAppInstalledForUser",
                mCurrentUserId, Collections.singletonMap(PARAM_TEST_USER, mSerialNumber));
    }

    @FlakyTest
    @Test
    public void testLauncherCallbackPackageAddedMainUser() throws Exception {
        if (!mHasLauncherApps) {
            return;
        }
        startCallbackService(mCurrentUserId);
        installAppAsUser(SIMPLE_APP_APK, mCurrentUserId);

        runDeviceTestsAsUser(LAUNCHER_TESTS_PKG,
                LAUNCHER_TESTS_CLASS,
                "testPackageAddedCallbackForUser",
                mCurrentUserId, Collections.singletonMap(PARAM_TEST_USER, mSerialNumber));
    }

    @FlakyTest
    @Test
    public void testLauncherCallbackPackageRemovedMainUser() throws Exception {
        if (!mHasLauncherApps) {
            return;
        }
        installAppAsUser(SIMPLE_APP_APK, mCurrentUserId);
        startCallbackService(mCurrentUserId);
        getDevice().uninstallPackage(SIMPLE_APP_PKG);
        runDeviceTestsAsUser(LAUNCHER_TESTS_PKG,
                LAUNCHER_TESTS_CLASS,
                "testPackageRemovedCallbackForUser",
                mCurrentUserId, Collections.singletonMap(PARAM_TEST_USER, mSerialNumber));
    }

    @FlakyTest
    @Test
    public void testLauncherCallbackPackageChangedMainUser() throws Exception {
        if (!mHasLauncherApps) {
            return;
        }
        installAppAsUser(SIMPLE_APP_APK, mCurrentUserId);
        startCallbackService(mCurrentUserId);
        installAppAsUser(SIMPLE_APP_APK, mCurrentUserId);
        runDeviceTestsAsUser(LAUNCHER_TESTS_PKG,
                LAUNCHER_TESTS_CLASS,
                "testPackageChangedCallbackForUser",
                mCurrentUserId, Collections.singletonMap(PARAM_TEST_USER, mSerialNumber));
    }

    @Test
    public void testLauncherNonExportedAppFails() throws Exception {
        if (!mHasLauncherApps) {
            return;
        }
        installAppAsUser(SIMPLE_APP_APK, mCurrentUserId);
        runDeviceTestsAsUser(LAUNCHER_TESTS_PKG,
                LAUNCHER_TESTS_CLASS, "testLaunchNonExportActivityFails",
                mCurrentUserId, Collections.singletonMap(PARAM_TEST_USER, mSerialNumber));
    }

    @Test
    public void testLaunchNonExportActivityFails() throws Exception {
        if (!mHasLauncherApps) {
            return;
        }
        installAppAsUser(SIMPLE_APP_APK, mCurrentUserId);
        runDeviceTestsAsUser(LAUNCHER_TESTS_PKG,
                LAUNCHER_TESTS_CLASS, "testLaunchNonExportLauncherFails",
                mCurrentUserId, Collections.singletonMap(PARAM_TEST_USER, mSerialNumber));
    }

    @Test
    public void testLaunchMainActivity() throws Exception {
        if (!mHasLauncherApps) {
            return;
        }
        installAppAsUser(SIMPLE_APP_APK, mCurrentUserId);
        runDeviceTestsAsUser(LAUNCHER_TESTS_PKG,
                LAUNCHER_TESTS_CLASS, "testLaunchMainActivity",
                mCurrentUserId, Collections.singletonMap(PARAM_TEST_USER, mSerialNumber));
    }
}
