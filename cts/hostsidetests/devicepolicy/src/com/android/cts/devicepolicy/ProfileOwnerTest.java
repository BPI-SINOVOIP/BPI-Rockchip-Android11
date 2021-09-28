/*
 * Copyright (C) 2016 The Android Open Source Project
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

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import org.junit.Test;

/**
 * Host side tests for profile owner.  Run the CtsProfileOwnerApp device side test.
 */
public class ProfileOwnerTest extends BaseDevicePolicyTest {
    private static final String PROFILE_OWNER_PKG = "com.android.cts.profileowner";
    private static final String PROFILE_OWNER_APK = "CtsProfileOwnerApp.apk";
    private static final String FEATURE_BACKUP = "android.software.backup";

    private static final String ADMIN_RECEIVER_TEST_CLASS =
            PROFILE_OWNER_PKG + ".BaseProfileOwnerTest$BasicAdminReceiver";

    private int mUserId = 0;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mUserId = getPrimaryUser();


        if (mHasFeature) {
            installAppAsUser(PROFILE_OWNER_APK, mUserId);
            if (!setProfileOwner(
                    PROFILE_OWNER_PKG + "/" + ADMIN_RECEIVER_TEST_CLASS, mUserId,
                    /* expectFailure */ false)) {
                removeAdmin(PROFILE_OWNER_PKG + "/" + ADMIN_RECEIVER_TEST_CLASS, mUserId);
                getDevice().uninstallPackage(PROFILE_OWNER_PKG);
                fail("Failed to set profile owner");
            }
        }
    }

    @Test
    public void testManagement() throws Exception {
        if (!mHasFeature) {
            return;
        }
        executeProfileOwnerTest("ManagementTest");
    }

    @Test
    public void testAdminActionBookkeeping() throws Exception {
        if (!mHasFeature) {
            return;
        }
        executeProfileOwnerTest("AdminActionBookkeepingTest");
    }

    @Test
    public void testAppUsageObserver() throws Exception {
        if (!mHasFeature) {
            return;
        }
        executeProfileOwnerTest("AppUsageObserverTest");
    }

    @Test
    public void testBackupServiceEnabling() throws Exception {
        final boolean hasBackupService = getDevice().hasFeature(FEATURE_BACKUP);
        // The backup service cannot be enabled if the backup feature is not supported.
        if (!mHasFeature || !hasBackupService) {
            return;
        }
        executeProfileOwnerTest("BackupServicePoliciesTest");
    }

    @Override
    public void tearDown() throws Exception {
        if (mHasFeature) {
            assertTrue("Failed to remove profile owner.",
                    removeAdmin(PROFILE_OWNER_PKG + "/" + ADMIN_RECEIVER_TEST_CLASS, mUserId));
            getDevice().uninstallPackage(PROFILE_OWNER_PKG);
        }

        super.tearDown();
    }

    private void executeProfileOwnerTest(String testClassName) throws Exception {
        if (!mHasFeature) {
            return;
        }
        String testClass = PROFILE_OWNER_PKG + "." + testClassName;
        runDeviceTestsAsUser(PROFILE_OWNER_PKG, testClass, mPrimaryUserId);
    }

    protected void executeProfileOwnerTestMethod(String className, String testName)
            throws Exception {
        runDeviceTestsAsUser(PROFILE_OWNER_PKG, className, testName, mUserId);
    }
}
