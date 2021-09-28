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

import static org.junit.Assert.fail;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.log.LogUtil;

public abstract class BaseManagedProfileTest extends BaseDevicePolicyTest {
    protected static final String MANAGED_PROFILE_PKG = "com.android.cts.managedprofile";
    protected static final String INTENT_SENDER_PKG = "com.android.cts.intent.sender";
    protected static final String INTENT_RECEIVER_PKG = "com.android.cts.intent.receiver";
    protected static final String DUMMY_APP_1_PKG = "com.android.cts.dummyapps.dummyapp1";
    protected static final String DUMMY_APP_2_PKG = "com.android.cts.dummyapps.dummyapp2";
    protected static final String DUMMY_APP_3_PKG = "com.android.cts.dummyapps.dummyapp3";
    protected static final String DUMMY_APP_4_PKG = "com.android.cts.dummyapps.dummyapp4";
    protected static final String ADMIN_RECEIVER_TEST_CLASS =
            MANAGED_PROFILE_PKG + ".BaseManagedProfileTest$BasicAdminReceiver";
    protected static final String INTENT_SENDER_APK = "CtsIntentSenderApp.apk";
    protected static final String INTENT_RECEIVER_APK = "CtsIntentReceiverApp.apk";
    protected static final String SIMPLE_APP_APK = "CtsSimpleApp.apk";
    protected static final String DUMMY_APP_1_APK = "DummyApp1.apk";
    protected static final String DUMMY_APP_2_APK = "DummyApp2.apk";
    protected static final String DUMMY_APP_3_APK = "DummyApp3.apk";
    protected static final String DUMMY_APP_4_APK = "DummyApp4.apk";
    protected static final String SHARING_APP_1_APK = "SharingApp1.apk";
    protected static final String SHARING_APP_2_APK = "SharingApp2.apk";
    private static final String MANAGED_PROFILE_APK = "CtsManagedProfileApp.apk";
    private static final String NOTIFICATION_PKG =
            "com.android.cts.managedprofiletests.notificationsender";
    protected int mParentUserId;
    // ID of the profile we'll create. This will always be a profile of the parent.
    protected int mProfileUserId;
    protected boolean mHasNfcFeature;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        // We need multi user to be supported in order to create a profile of the user owner.
        mHasFeature = mHasFeature && hasDeviceFeature("android.software.managed_users");
        mHasNfcFeature = hasDeviceFeature("android.hardware.nfc")
                && hasDeviceFeature("android.sofware.nfc.beam");

        if (mHasFeature) {
            removeTestUsers();
            mParentUserId = mPrimaryUserId;
            mProfileUserId = createManagedProfile(mParentUserId);
            startUser(mProfileUserId);

            // Install the APK on both primary and profile user in one single transaction.
            // If they were installed separately, the second installation would become an app
            // update and result in the current running test process being killed.
            installAppAsUser(MANAGED_PROFILE_APK, USER_ALL);
            setProfileOwnerOrFail(MANAGED_PROFILE_PKG + "/" + ADMIN_RECEIVER_TEST_CLASS,
                    mProfileUserId);
            waitForUserUnlock(mProfileUserId);
        }
    }

    @Override
    public void tearDown() throws Exception {
        if (mHasFeature) {
            removeUser(mProfileUserId);
            getDevice().uninstallPackage(MANAGED_PROFILE_PKG);
            getDevice().uninstallPackage(INTENT_SENDER_PKG);
            getDevice().uninstallPackage(INTENT_RECEIVER_PKG);
            getDevice().uninstallPackage(NOTIFICATION_PKG);
            getDevice().uninstallPackage(DUMMY_APP_1_APK);
            getDevice().uninstallPackage(DUMMY_APP_2_APK);
            getDevice().uninstallPackage(DUMMY_APP_3_APK);
            getDevice().uninstallPackage(DUMMY_APP_4_APK);
            getDevice().uninstallPackage(SHARING_APP_1_APK);
            getDevice().uninstallPackage(SHARING_APP_2_APK);
        }
        super.tearDown();
    }

    protected void disableActivityForUser(String activityName, int userId)
            throws DeviceNotAvailableException {
        String command = "am start -W --user " + userId
                + " --es extra-package " + MANAGED_PROFILE_PKG
                + " --es extra-class-name " + MANAGED_PROFILE_PKG + "." + activityName
                + " " + MANAGED_PROFILE_PKG + "/.ComponentDisablingActivity ";
        LogUtil.CLog.d("Output for command " + command + ": "
                + getDevice().executeShellCommand(command));
    }
}
