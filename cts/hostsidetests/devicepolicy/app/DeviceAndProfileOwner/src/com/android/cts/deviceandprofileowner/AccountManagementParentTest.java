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

package com.android.cts.deviceandprofileowner;

import static com.google.common.truth.Truth.assertThat;
import static com.android.cts.deviceandprofileowner.BaseDeviceAdminTest.ADMIN_RECEIVER_COMPONENT;

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.test.InstrumentationTestCase;

public class AccountManagementParentTest extends InstrumentationTestCase {
    private static final String SOME_ACCOUNT_TYPE = "com.example.account.type";

    private DevicePolicyManager mDevicePolicyManager;
    private Context mContext;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getContext();

        mDevicePolicyManager = mContext.getSystemService(DevicePolicyManager.class);
        assertNotNull(mDevicePolicyManager);
    }

    public void testSetAccountManagementDisabledOnParent() {
        DevicePolicyManager parentDevicePolicyManager = getParentInstance();

        parentDevicePolicyManager.setAccountManagementDisabled(ADMIN_RECEIVER_COMPONENT,
                SOME_ACCOUNT_TYPE, true);
        assertThat(
                parentDevicePolicyManager.getAccountTypesWithManagementDisabled()).asList()
                .containsExactly(SOME_ACCOUNT_TYPE);
        // Ensure that account management is not restricted on the managed profile itself.
        assertThat(mDevicePolicyManager.getAccountTypesWithManagementDisabled()).isEmpty();
    }

    public void testAccountManagementDisabled() {
        assertThat(
                mDevicePolicyManager.getAccountTypesWithManagementDisabled()).asList()
                .containsExactly(SOME_ACCOUNT_TYPE);
    }

    public void testEnableAccountManagement() {
        DevicePolicyManager parentDevicePolicyManager = getParentInstance();

        parentDevicePolicyManager.setAccountManagementDisabled(ADMIN_RECEIVER_COMPONENT,
                SOME_ACCOUNT_TYPE, false);
        assertThat(parentDevicePolicyManager.getAccountTypesWithManagementDisabled()).isEmpty();
    }

    private DevicePolicyManager getParentInstance() {
        assertThat(mDevicePolicyManager.isAdminActive(ADMIN_RECEIVER_COMPONENT)).isTrue();
        assertThat(mDevicePolicyManager.isProfileOwnerApp(
                ADMIN_RECEIVER_COMPONENT.getPackageName())).isTrue();
        assertThat(mDevicePolicyManager.isOrganizationOwnedDeviceWithManagedProfile()).isTrue();

        DevicePolicyManager parentDevicePolicyManager =
                mDevicePolicyManager.getParentProfileInstance(ADMIN_RECEIVER_COMPONENT);
        assertThat(parentDevicePolicyManager).isNotNull();

        return parentDevicePolicyManager;
    }
}
