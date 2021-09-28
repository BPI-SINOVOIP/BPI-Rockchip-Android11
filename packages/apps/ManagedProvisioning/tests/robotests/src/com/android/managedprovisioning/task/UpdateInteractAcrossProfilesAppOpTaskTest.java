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

package com.android.managedprovisioning.task;

import static android.app.AppOpsManager.MODE_ALLOWED;
import static android.app.AppOpsManager.MODE_DEFAULT;

import static com.google.common.truth.Truth.assertThat;

import static org.robolectric.Shadows.shadowOf;

import android.Manifest;
import android.app.AppOpsManager;
import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.pm.CrossProfileApps;
import android.content.pm.PackageManager;
import android.os.UserManager;

import com.android.managedprovisioning.analytics.MetricsWriterFactory;
import com.android.managedprovisioning.analytics.ProvisioningAnalyticsTracker;
import com.android.managedprovisioning.common.ManagedProvisioningSharedPreferences;
import com.android.managedprovisioning.common.SettingsFacade;

import org.junit.Before;
import org.junit.Test;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import org.junit.runner.RunWith;

import java.util.HashSet;
import java.util.Set;

/**
 * Unit-tests for {@link UpdateInteractAcrossProfilesAppOpTask}.
 */
@RunWith(RobolectricTestRunner.class)
public class UpdateInteractAcrossProfilesAppOpTaskTest {
    private static final String TEST_PACKAGE_NAME_1 = "com.test.packagea";
    private static final String TEST_PACKAGE_NAME_2 = "com.test.packageb";
    private static final int TEST_USER_ID = 123;

    private final Context mContext = RuntimeEnvironment.application;
    private final UserManager mUserManager = mContext.getSystemService(UserManager.class);
    private final DevicePolicyManager mDevicePolicyManager =
            mContext.getSystemService(DevicePolicyManager.class);
    private final AppOpsManager mAppOpsManager = mContext.getSystemService(AppOpsManager.class);
    private final PackageManager mPackageManager = mContext.getPackageManager();
    private final TestTaskCallback mCallback = new TestTaskCallback();
    private final CrossProfileApps mCrossProfileApps =
            mContext.getSystemService(CrossProfileApps.class);
    private final ManagedProvisioningSharedPreferences mManagedProvisioningSharedPreferences
            = new ManagedProvisioningSharedPreferences(mContext);
    private final ProvisioningAnalyticsTracker mProvisioningAnalyticsTracker =
            new ProvisioningAnalyticsTracker(
                    MetricsWriterFactory.getMetricsWriter(mContext, new SettingsFacade()),
                    mManagedProvisioningSharedPreferences);
    private final UpdateInteractAcrossProfilesAppOpTask task =
            new UpdateInteractAcrossProfilesAppOpTask(
                    mContext, /* params= */ null, mCallback, mProvisioningAnalyticsTracker);

    @Before
    public void setUp() {
        shadowOf(mUserManager).addUser(TEST_USER_ID, "Username", /* flags= */ 0);
    }

    @Test
    public void run_firstRun_appOpIsNotReset() {
        setDefaultCrossProfilePackages(TEST_PACKAGE_NAME_1, TEST_PACKAGE_NAME_2);
        mCrossProfileApps.setInteractAcrossProfilesAppOp(TEST_PACKAGE_NAME_2, MODE_ALLOWED);

        task.run(TEST_USER_ID);

        assertThat(shadowOf(mCrossProfileApps).getInteractAcrossProfilesAppOp(TEST_PACKAGE_NAME_2))
                .isEqualTo(MODE_ALLOWED);
    }

    @Test
    public void run_packageIsRemovedButStillConfigurable_appOpIsNotReset() {
        setDefaultCrossProfilePackages(TEST_PACKAGE_NAME_1, TEST_PACKAGE_NAME_2);
        mCrossProfileApps.setInteractAcrossProfilesAppOp(TEST_PACKAGE_NAME_2, MODE_ALLOWED);
        shadowOf(mCrossProfileApps).addCrossProfilePackage(TEST_PACKAGE_NAME_2);
        task.run(TEST_USER_ID);

        setDefaultCrossProfilePackages(TEST_PACKAGE_NAME_1);
        task.run(TEST_USER_ID);

        assertThat(shadowOf(mCrossProfileApps).getInteractAcrossProfilesAppOp(TEST_PACKAGE_NAME_2))
                .isEqualTo(MODE_ALLOWED);
    }

    @Test
    public void run_packageIsNoLongerConfigurable_appOpIsReset() {
        setDefaultCrossProfilePackages(TEST_PACKAGE_NAME_1, TEST_PACKAGE_NAME_2);
        mCrossProfileApps.setInteractAcrossProfilesAppOp(TEST_PACKAGE_NAME_2, MODE_ALLOWED);
        // By default the configurable list is empty
        task.run(TEST_USER_ID);

        setDefaultCrossProfilePackages(TEST_PACKAGE_NAME_1);
        task.run(TEST_USER_ID);

        assertThat(shadowOf(mCrossProfileApps).getInteractAcrossProfilesAppOp(TEST_PACKAGE_NAME_2))
                .isEqualTo(MODE_DEFAULT);
    }

    @Test
    public void run_packageIsNotRemoved_appOpIsNotReset() {
        setDefaultCrossProfilePackages(TEST_PACKAGE_NAME_1, TEST_PACKAGE_NAME_2);
        mCrossProfileApps.setInteractAcrossProfilesAppOp(TEST_PACKAGE_NAME_2, MODE_ALLOWED);
        task.run(TEST_USER_ID);

        setDefaultCrossProfilePackages(TEST_PACKAGE_NAME_2);
        task.run(TEST_USER_ID);

        assertThat(shadowOf(mCrossProfileApps).getInteractAcrossProfilesAppOp(TEST_PACKAGE_NAME_2))
                .isEqualTo(MODE_ALLOWED);
    }

    private void setDefaultCrossProfilePackages(String... defaultCrossProfilePackages) {
        Set<String> packages = new HashSet<>();
        for (String packageName : defaultCrossProfilePackages) {
            packages.add(packageName);
        }

        shadowOf(mDevicePolicyManager).setDefaultCrossProfilePackages(packages);
    }
}
