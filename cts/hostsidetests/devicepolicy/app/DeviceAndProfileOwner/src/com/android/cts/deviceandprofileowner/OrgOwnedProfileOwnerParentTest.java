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

import static org.testng.Assert.assertThrows;

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.os.Bundle;
import android.os.UserManager;
import android.test.InstrumentationTestCase;

import com.google.common.collect.ImmutableSet;

import java.util.Set;

public class OrgOwnedProfileOwnerParentTest extends InstrumentationTestCase {

    protected Context mContext;
    private DevicePolicyManager mParentDevicePolicyManager;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getContext();

        DevicePolicyManager devicePolicyManager = (DevicePolicyManager)
                mContext.getSystemService(Context.DEVICE_POLICY_SERVICE);
        assertNotNull(devicePolicyManager);
        mParentDevicePolicyManager =
                devicePolicyManager.getParentProfileInstance(ADMIN_RECEIVER_COMPONENT);
        assertNotNull(mParentDevicePolicyManager);

        assertTrue(devicePolicyManager.isAdminActive(ADMIN_RECEIVER_COMPONENT));
        assertTrue(
                devicePolicyManager.isProfileOwnerApp(ADMIN_RECEIVER_COMPONENT.getPackageName()));
        assertTrue(devicePolicyManager.isManagedProfile(ADMIN_RECEIVER_COMPONENT));
    }

    private static final Set<String> PROFILE_OWNER_ORGANIZATION_OWNED_GLOBAL_RESTRICTIONS =
            ImmutableSet.of(
                    UserManager.DISALLOW_CONFIG_PRIVATE_DNS,
                    UserManager.DISALLOW_CONFIG_DATE_TIME,
                    UserManager.DISALLOW_AIRPLANE_MODE
            );

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

    public void testAddGetAndClearUserRestriction_onParent() {
        for (String restriction : PROFILE_OWNER_ORGANIZATION_OWNED_GLOBAL_RESTRICTIONS) {
            testAddGetAndClearUserRestriction_onParent(restriction);
        }
        for (String restriction : PROFILE_OWNER_ORGANIZATION_OWNED_LOCAL_RESTRICTIONS) {
            testAddGetAndClearUserRestriction_onParent(restriction);
        }
    }

    private void testAddGetAndClearUserRestriction_onParent(String restriction) {
        mParentDevicePolicyManager.addUserRestriction(ADMIN_RECEIVER_COMPONENT, restriction);

        Bundle restrictions = mParentDevicePolicyManager.getUserRestrictions(
                ADMIN_RECEIVER_COMPONENT);
        assertThat(restrictions.get(restriction)).isNotNull();

        mParentDevicePolicyManager.clearUserRestriction(ADMIN_RECEIVER_COMPONENT, restriction);

        restrictions = mParentDevicePolicyManager.getUserRestrictions(ADMIN_RECEIVER_COMPONENT);
        assertThat(restrictions.get(restriction)).isNull();
    }

    public void testUnableToAddAndClearBaseUserRestrictions_onParent() {
        testUnableToAddBaseUserRestriction(UserManager.DISALLOW_REMOVE_MANAGED_PROFILE);
        testUnableToClearBaseUserRestriction(UserManager.DISALLOW_REMOVE_MANAGED_PROFILE);
        testUnableToAddBaseUserRestriction(UserManager.DISALLOW_ADD_USER);
        testUnableToClearBaseUserRestriction(UserManager.DISALLOW_ADD_USER);
    }

    private void testUnableToAddBaseUserRestriction(String restriction) {
        assertThrows(SecurityException.class,
                () -> mParentDevicePolicyManager.addUserRestriction(ADMIN_RECEIVER_COMPONENT,
                        restriction));
    }

    private void testUnableToClearBaseUserRestriction(String restriction) {
        assertThrows(SecurityException.class,
                () -> mParentDevicePolicyManager.clearUserRestriction(ADMIN_RECEIVER_COMPONENT,
                        restriction));
    }

}