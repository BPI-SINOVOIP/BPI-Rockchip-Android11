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
 * limitations under the License.
 */

package com.android.cts.deviceandprofileowner;

import static com.android.cts.deviceandprofileowner.BaseDeviceAdminTest.ADMIN_RECEIVER_COMPONENT;

import static com.google.common.truth.Truth.assertThat;

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.hardware.camera2.CameraManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.UserManager;
import android.test.InstrumentationTestCase;
import android.util.Log;

import com.google.common.collect.ImmutableSet;

import java.util.concurrent.TimeUnit;
import java.util.Set;

public class UserRestrictionsParentTest extends InstrumentationTestCase {

    private static final String TAG = "UserRestrictionsParentTest";

    protected Context mContext;
    private DevicePolicyManager mDevicePolicyManager;
    private UserManager mUserManager;

    private CameraManager mCameraManager;

    private HandlerThread mBackgroundThread;

    /**
     * A {@link Handler} for running tasks in the background.
     */
    private Handler mBackgroundHandler;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getContext();

        mDevicePolicyManager = (DevicePolicyManager)
                mContext.getSystemService(Context.DEVICE_POLICY_SERVICE);
        assertNotNull(mDevicePolicyManager);

        mCameraManager = (CameraManager) mContext.getSystemService(Context.CAMERA_SERVICE);
        assertNotNull(mCameraManager);

        mUserManager = mContext.getSystemService(UserManager.class);
        assertNotNull(mUserManager);

        startBackgroundThread();
    }

    @Override
    protected void tearDown() throws Exception {
        stopBackgroundThread();
        super.tearDown();
    }

    public void testAddUserRestrictionDisallowConfigDateTime_onParent() {
        DevicePolicyManager parentDevicePolicyManager =
                mDevicePolicyManager.getParentProfileInstance(ADMIN_RECEIVER_COMPONENT);
        assertNotNull(parentDevicePolicyManager);

        parentDevicePolicyManager.addUserRestriction(ADMIN_RECEIVER_COMPONENT,
                UserManager.DISALLOW_CONFIG_DATE_TIME);
    }

    public void testHasUserRestrictionDisallowConfigDateTime() {
        assertThat(mUserManager.
                hasUserRestriction(UserManager.DISALLOW_CONFIG_DATE_TIME)).isTrue();
    }

    public void testUserRestrictionDisallowConfigDateTimeIsNotPersisted() throws Exception {
        final long deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(30);
        while (System.nanoTime() <= deadline) {
            if (!mUserManager.hasUserRestriction(UserManager.DISALLOW_CONFIG_DATE_TIME)) {
                return;
            }
            Thread.sleep(100);
        }
        fail("The restriction didn't go away.");
    }

    public void testAddUserRestrictionDisallowAddUser_onParent() {
        DevicePolicyManager parentDevicePolicyManager =
                mDevicePolicyManager.getParentProfileInstance(ADMIN_RECEIVER_COMPONENT);
        assertNotNull(parentDevicePolicyManager);

        parentDevicePolicyManager.addUserRestriction(ADMIN_RECEIVER_COMPONENT,
                UserManager.DISALLOW_ADD_USER);
    }

    public void testHasUserRestrictionDisallowAddUser() {
        assertThat(hasUserRestriction(UserManager.DISALLOW_ADD_USER)).isTrue();
    }

    public void testClearUserRestrictionDisallowAddUser() {
        DevicePolicyManager parentDevicePolicyManager =
                mDevicePolicyManager.getParentProfileInstance(ADMIN_RECEIVER_COMPONENT);

        parentDevicePolicyManager.clearUserRestriction(ADMIN_RECEIVER_COMPONENT,
                UserManager.DISALLOW_ADD_USER);
    }

    public void testAddUserRestrictionCameraDisabled_onParent() {
        DevicePolicyManager parentDevicePolicyManager =
                mDevicePolicyManager.getParentProfileInstance(ADMIN_RECEIVER_COMPONENT);
        parentDevicePolicyManager.setCameraDisabled(ADMIN_RECEIVER_COMPONENT, true);
        boolean actualDisabled =
                parentDevicePolicyManager.getCameraDisabled(ADMIN_RECEIVER_COMPONENT);

        assertThat(actualDisabled).isTrue();
    }

    public void testRemoveUserRestrictionCameraEnabled_onParent() {
        DevicePolicyManager parentDevicePolicyManager =
                mDevicePolicyManager.getParentProfileInstance(ADMIN_RECEIVER_COMPONENT);
        parentDevicePolicyManager.setCameraDisabled(ADMIN_RECEIVER_COMPONENT, false);
        boolean actualDisabled =
                parentDevicePolicyManager.getCameraDisabled(ADMIN_RECEIVER_COMPONENT);

        assertThat(actualDisabled).isFalse();
    }

    public void testCannotOpenCamera() throws Exception {
        checkCanOpenCamera(false);
    }

    public void testCanOpenCamera() throws Exception {
        checkCanOpenCamera(true);
    }

    private void checkCanOpenCamera(boolean canOpen) throws Exception {
        // If the device does not support a camera it will return an empty camera ID list.
        if (mCameraManager.getCameraIdList() == null
                || mCameraManager.getCameraIdList().length == 0) {
            return;
        }
        int retries = 10;
        boolean successToOpen = !canOpen;
        while (successToOpen != canOpen && retries > 0) {
            retries--;
            Thread.sleep(500);
            successToOpen = CameraUtils
                    .blockUntilOpenCamera(mCameraManager, mBackgroundHandler);
        }
        assertEquals(String.format("Timed out waiting the value to change to %b (actual=%b)",
                canOpen, successToOpen), canOpen, successToOpen);
    }

    private static final Set<String> PROFILE_OWNER_ORGANIZATION_OWNED_LOCAL_RESTRICTIONS =
            ImmutableSet.of(
                    UserManager.DISALLOW_BLUETOOTH,
                    UserManager.DISALLOW_BLUETOOTH_SHARING,
                    UserManager.DISALLOW_CONFIG_BLUETOOTH,
                    UserManager.DISALLOW_CONFIG_CELL_BROADCASTS,
                    UserManager.DISALLOW_CONFIG_LOCATION,
                    UserManager.DISALLOW_CONFIG_MOBILE_NETWORKS,
                    UserManager.DISALLOW_CONFIG_TETHERING,
                    UserManager.DISALLOW_CONFIG_WIFI,
                    UserManager.DISALLOW_CONTENT_CAPTURE,
                    UserManager.DISALLOW_CONTENT_SUGGESTIONS,
                    UserManager.DISALLOW_DATA_ROAMING,
                    UserManager.DISALLOW_SAFE_BOOT,
                    UserManager.DISALLOW_SHARE_LOCATION,
                    UserManager.DISALLOW_SMS,
                    UserManager.DISALLOW_USB_FILE_TRANSFER,
                    UserManager.DISALLOW_MOUNT_PHYSICAL_MEDIA,
                    UserManager.DISALLOW_OUTGOING_CALLS,
                    UserManager.DISALLOW_UNMUTE_MICROPHONE
                    // This restriction disables ADB, so is not used in test.
                    // UserManager.DISALLOW_DEBUGGING_FEATURES
            );

    public void testPerProfileUserRestriction_onParent() {
        DevicePolicyManager parentDevicePolicyManager =
                mDevicePolicyManager.getParentProfileInstance(ADMIN_RECEIVER_COMPONENT);
        assertNotNull(parentDevicePolicyManager);

        for (String restriction : PROFILE_OWNER_ORGANIZATION_OWNED_LOCAL_RESTRICTIONS) {
            try {
                boolean hasRestrictionOnManagedProfile = mUserManager.hasUserRestriction(
                        restriction);

                parentDevicePolicyManager.addUserRestriction(ADMIN_RECEIVER_COMPONENT, restriction);
                // Assert user restriction on personal profile has been added
                assertThat(hasUserRestriction(restriction)).isTrue();
                // Assert user restriction on managed profile has not changed
                assertThat(mUserManager.hasUserRestriction(restriction)).isEqualTo(
                        hasRestrictionOnManagedProfile);
            } finally {
                parentDevicePolicyManager.clearUserRestriction(ADMIN_RECEIVER_COMPONENT,
                        restriction);
                assertThat(hasUserRestriction(restriction)).isFalse();
            }
        }
    }

    private static final Set<String> PROFILE_OWNER_ORGANIZATION_OWNED_GLOBAL_RESTRICTIONS =
            ImmutableSet.of(
                    UserManager.DISALLOW_CONFIG_PRIVATE_DNS,
                    UserManager.DISALLOW_CONFIG_DATE_TIME,
                    UserManager.DISALLOW_AIRPLANE_MODE
            );

    public void testPerDeviceUserRestriction_onParent() {
        DevicePolicyManager parentDevicePolicyManager =
                mDevicePolicyManager.getParentProfileInstance(ADMIN_RECEIVER_COMPONENT);
        assertNotNull(parentDevicePolicyManager);

        for (String restriction : PROFILE_OWNER_ORGANIZATION_OWNED_GLOBAL_RESTRICTIONS) {
            try {
                parentDevicePolicyManager.addUserRestriction(ADMIN_RECEIVER_COMPONENT, restriction);
                // Assert user restriction on personal profile has been added
                assertThat(hasUserRestriction(restriction)).isTrue();
                // Assert user restriction on managed profile has been added
                assertThat(mUserManager.hasUserRestriction(restriction)).isTrue();
            } finally {
                parentDevicePolicyManager.clearUserRestriction(ADMIN_RECEIVER_COMPONENT,
                        restriction);
                assertThat(hasUserRestriction(restriction)).isFalse();
                assertThat(mUserManager.hasUserRestriction(restriction)).isFalse();
            }
        }
    }

    private boolean hasUserRestriction(String key) {
        DevicePolicyManager parentDevicePolicyManager =
                mDevicePolicyManager.getParentProfileInstance(ADMIN_RECEIVER_COMPONENT);
        Bundle userRestrictions =
                parentDevicePolicyManager.getUserRestrictions(ADMIN_RECEIVER_COMPONENT);
        return userRestrictions.getBoolean(key);
    }

    /**
     * Starts a background thread and its {@link Handler}.
     */
    private void startBackgroundThread() {
        mBackgroundThread = new HandlerThread("CameraBackground");
        mBackgroundThread.start();
        mBackgroundHandler = new Handler(mBackgroundThread.getLooper());
    }

    /**
     * Stops the background thread and its {@link Handler}.
     */
    private void stopBackgroundThread() {
        mBackgroundThread.quitSafely();
        try {
            mBackgroundThread.join();
            mBackgroundThread = null;
            mBackgroundHandler = null;
        } catch (InterruptedException e) {
            Log.e(TAG, "Interrupted exception thrown while stopping background thread.");
        }
    }

}
