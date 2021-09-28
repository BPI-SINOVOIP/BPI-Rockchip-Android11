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

package com.android.cts.devicepolicy;

import android.platform.test.annotations.SecurityTest;

import org.junit.Test;

import com.android.tradefed.device.DeviceNotAvailableException;

/**
 * Host side tests for separate profile challenge permissions.
 * Run the CtsSeparateProfileChallengeApp device side test.
 */

public class SeparateProfileChallengeTest extends BaseDevicePolicyTest {
    private static final String SEPARATE_PROFILE_PKG = "com.android.cts.separateprofilechallenge";
    private static final String SEPARATE_PROFILE_APK = "CtsSeparateProfileChallengeApp.apk";
    private static final String SEPARATE_PROFILE_TEST_CLASS =
        ".SeparateProfileChallengePermissionsTest";
    private String mPreviousHiddenApiPolicy = "0";

    @Override
    public void setUp() throws Exception {
        super.setUp();
        setHiddenApiPolicyOn();
    }

    @Override
    public void tearDown() throws Exception {
        removeTestUsers();
        getDevice().uninstallPackage(SEPARATE_PROFILE_PKG);
        setHiddenApiPolicyPreviousOrOff();
        super.tearDown();
    }

    @SecurityTest
    @Test
    public void testSeparateProfileChallengePermissions() throws Exception {
        if (!mHasFeature || !mSupportsMultiUser) {
            return;
        }

        // Create managed profile.
        final int profileUserId = createManagedProfile(mPrimaryUserId);
        // createManagedProfile doesn't start the user automatically.
        startUser(profileUserId);
        installAppAsUser(SEPARATE_PROFILE_APK, profileUserId);
        executeSeparateProfileChallengeTest(profileUserId);
    }

    protected void setHiddenApiPolicyOn() throws Exception {
        mPreviousHiddenApiPolicy = getDevice().executeShellCommand(
                "settings get global hidden_api_policy_p_apps");
        executeShellCommand("settings put global hidden_api_policy_p_apps 1");
    }

    protected void setHiddenApiPolicyPreviousOrOff() throws Exception {
        executeShellCommand("settings put global hidden_api_policy_p_apps "
            + mPreviousHiddenApiPolicy);
    }

    private void executeSeparateProfileChallengeTest(int userId) throws Exception {
        runDeviceTestsAsUser(SEPARATE_PROFILE_PKG, SEPARATE_PROFILE_TEST_CLASS, userId);
    }
}
