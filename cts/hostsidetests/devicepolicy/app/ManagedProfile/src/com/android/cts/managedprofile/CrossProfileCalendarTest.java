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
package com.android.cts.managedprofile;

import static com.android.cts.managedprofile.BaseManagedProfileTest.ADMIN_RECEIVER_COMPONENT;

import static com.google.common.truth.Truth.assertThat;

import android.app.admin.DevicePolicyManager;
import android.test.AndroidTestCase;
import android.util.ArraySet;

import java.util.Arrays;
import java.util.Collections;
import java.util.Set;

/**
 * This class contains tests for cross profile calendar related features. Most of the tests in
 * this class will need different setups, so the tests will be run separately.
 */
public class CrossProfileCalendarTest extends AndroidTestCase {

    private static final String MANAGED_PROFILE_PKG = "com.android.cts.managedprofile";

    private DevicePolicyManager mDevicePolicyManager;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mDevicePolicyManager = mContext.getSystemService(DevicePolicyManager.class);
    }

    public void testCrossProfileCalendarPackage() {
        requireRunningOnManagedProfile();

        Set<String> whitelist = mDevicePolicyManager.getCrossProfileCalendarPackages(
                ADMIN_RECEIVER_COMPONENT);
        assertThat(whitelist).isEmpty();

        mDevicePolicyManager.setCrossProfileCalendarPackages(
                ADMIN_RECEIVER_COMPONENT, new ArraySet<String>(Arrays.asList(MANAGED_PROFILE_PKG)));
        whitelist = mDevicePolicyManager.getCrossProfileCalendarPackages(
                ADMIN_RECEIVER_COMPONENT);
        assertThat(whitelist.size()).isEqualTo(1);
        assertThat(whitelist.contains(MANAGED_PROFILE_PKG)).isTrue();

        mDevicePolicyManager.setCrossProfileCalendarPackages(
                ADMIN_RECEIVER_COMPONENT, Collections.emptySet());
        whitelist = mDevicePolicyManager.getCrossProfileCalendarPackages(
                ADMIN_RECEIVER_COMPONENT);
        assertThat(whitelist).isEmpty();
    }

    // This method is to guard that particular tests are supposed to run on managed profile.
    private void requireRunningOnManagedProfile() {
        assertThat(isManagedProfile()).isTrue();
    }

    private boolean isManagedProfile() {
        String adminPackage = ADMIN_RECEIVER_COMPONENT.getPackageName();
        return mDevicePolicyManager.isProfileOwnerApp(adminPackage);
    }
}
