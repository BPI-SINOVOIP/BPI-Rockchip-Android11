/*
 * Copyright (C) 2018 The Android Open Source Project
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

import org.junit.Test;

import java.util.Collections;

/**
 * Set of tests for the limit app icon hiding feature.
 */
public class LimitAppIconHidingTest extends BaseLauncherAppsTest {

    private static final String LAUNCHER_TESTS_HAS_LAUNCHER_ACTIVITY_APK =
            "CtsHasLauncherActivityApp.apk";
    private static final String LAUNCHER_TESTS_NO_LAUNCHER_ACTIVITY_APK =
            "CtsNoLauncherActivityApp.apk";
    private static final String LAUNCHER_TESTS_NO_PERMISSION_APK =
            "CtsNoPermissionApp.apk";

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

    @Override
    protected void installTestApps(int userId) throws Exception {
        super.installTestApps(mCurrentUserId);
        installAppAsUser(LAUNCHER_TESTS_HAS_LAUNCHER_ACTIVITY_APK, mCurrentUserId);
        installAppAsUser(LAUNCHER_TESTS_NO_LAUNCHER_ACTIVITY_APK, mCurrentUserId);
        installAppAsUser(LAUNCHER_TESTS_NO_PERMISSION_APK, mCurrentUserId);
    }

    @Override
    protected void uninstallTestApps() throws Exception {
        super.uninstallTestApps();
        getDevice().uninstallPackage(LAUNCHER_TESTS_NO_PERMISSION_APK);
        getDevice().uninstallPackage(LAUNCHER_TESTS_NO_LAUNCHER_ACTIVITY_APK);
        getDevice().uninstallPackage(LAUNCHER_TESTS_HAS_LAUNCHER_ACTIVITY_APK);
    }

    @Test
    public void testHasLauncherActivityAppHasAppDetailsActivityInjected() throws Exception {
        if (!mHasLauncherApps) {
            return;
        }
        runDeviceTestsAsUser(LAUNCHER_TESTS_PKG,
                LAUNCHER_TESTS_CLASS, "testHasLauncherActivityAppHasAppDetailsActivityInjected",
                mCurrentUserId, Collections.singletonMap(PARAM_TEST_USER, mSerialNumber));
    }

    @Test
    public void testNoSystemAppHasSyntheticAppDetailsActivityInjected() throws Exception {
        if (!mHasLauncherApps) {
            return;
        }
        runDeviceTestsAsUser(LAUNCHER_TESTS_PKG,
                LAUNCHER_TESTS_CLASS, "testNoSystemAppHasSyntheticAppDetailsActivityInjected",
                mCurrentUserId, Collections.singletonMap(PARAM_TEST_USER, mSerialNumber));
    }

    @Test
    public void testNoLauncherActivityAppNotInjected() throws Exception {
        if (!mHasLauncherApps) {
            return;
        }
        runDeviceTestsAsUser(LAUNCHER_TESTS_PKG,
                LAUNCHER_TESTS_CLASS, "testNoLauncherActivityAppNotInjected",
                mCurrentUserId, Collections.singletonMap(PARAM_TEST_USER, mSerialNumber));
    }

    @Test
    public void testNoPermissionAppNotInjected() throws Exception {
        if (!mHasLauncherApps) {
            return;
        }
        runDeviceTestsAsUser(LAUNCHER_TESTS_PKG,
                LAUNCHER_TESTS_CLASS, "testNoPermissionAppNotInjected",
                mCurrentUserId, Collections.singletonMap(PARAM_TEST_USER, mSerialNumber));
    }

    @Test
    public void testGetSetSyntheticAppDetailsActivityEnabled() throws Exception {
        if (!mHasLauncherApps) {
            return;
        }
        runDeviceTestsAsUser(LAUNCHER_TESTS_PKG,
                LAUNCHER_TESTS_CLASS, "testGetSetSyntheticAppDetailsActivityEnabled",
                mCurrentUserId, Collections.singletonMap(PARAM_TEST_USER, mSerialNumber));
    }
}
