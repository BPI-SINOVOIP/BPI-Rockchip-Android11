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
import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_PROFILE;

import static com.google.common.truth.Truth.assertThat;

import static org.robolectric.Shadows.shadowOf;

import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.ContextWrapper;
import android.content.pm.ApplicationInfo;
import android.content.pm.CrossProfileApps;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;

import androidx.test.core.app.ApplicationProvider;

import com.android.managedprovisioning.common.ManagedProvisioningSharedPreferences;
import com.android.managedprovisioning.model.ProvisioningParams;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;

/** Robolectric unit tests for {@link CreateManagedProfileTask}. */
@RunWith(RobolectricTestRunner.class)
public class CreateManagedProfileTaskRoboTest {
    private static final ProvisioningParams TEST_PARAMS =
            new ProvisioningParams.Builder()
                    .setDeviceAdminComponentName(
                            new ComponentName("com.example.testdeviceadmin", "TestDeviceAdmin"))
                    .setProvisioningAction(ACTION_PROVISION_MANAGED_PROFILE)
                    .build();
    private static final int TEST_USER_ID = 0;

    private final ContextWrapper mContext = ApplicationProvider.getApplicationContext();
    private final PackageManager mPackageManager = mContext.getPackageManager();
    private final CrossProfileApps mCrossProfileApps =
            mContext.getSystemService(CrossProfileApps.class);
    private final DevicePolicyManager mDevicePolicyManager =
            mContext.getSystemService(DevicePolicyManager.class);

    private @Mock AbstractProvisioningTask.Callback mCallback;

    @Before
    public void initializeMocks() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void run_clearsNonDefaultInteractAcrossProfilesAppOps() {
        final String testPackage1 = "com.example.testapp1";
        final String testPackage2 = "com.example.testapp2";
        shadowOf(mPackageManager).installPackage(buildTestPackageInfo(testPackage1));
        shadowOf(mPackageManager).installPackage(buildTestPackageInfo(testPackage2));
        mCrossProfileApps.setInteractAcrossProfilesAppOp(testPackage1, MODE_DEFAULT);
        mCrossProfileApps.setInteractAcrossProfilesAppOp(testPackage2, MODE_ALLOWED);
        shadowOf(mDevicePolicyManager).setDefaultCrossProfilePackages(new HashSet<>());

        new CreateManagedProfileTask(mContext, TEST_PARAMS, mCallback).run(TEST_USER_ID);

        assertThat(shadowOf(mCrossProfileApps).getInteractAcrossProfilesAppOp(testPackage1))
                .isEqualTo(MODE_DEFAULT);
        assertThat(shadowOf(mCrossProfileApps).getInteractAcrossProfilesAppOp(testPackage2))
                .isEqualTo(MODE_DEFAULT);
    }

    @Test
    public void run_grantsDefaultConfigurableInteractAcrossProfilesAppOps() {
        final String crossProfilePackage = "com.example.testapp1";
        Set<String> packages = new HashSet<>();
        packages.add(crossProfilePackage);
        shadowOf(mPackageManager).installPackage(buildTestPackageInfo(crossProfilePackage));
        mCrossProfileApps.setInteractAcrossProfilesAppOp(crossProfilePackage, MODE_DEFAULT);
        shadowOf(mCrossProfileApps).addCrossProfilePackage(crossProfilePackage);
        shadowOf(mDevicePolicyManager).setDefaultCrossProfilePackages(packages);

        new CreateManagedProfileTask(mContext, TEST_PARAMS, mCallback).run(TEST_USER_ID);

        assertThat(shadowOf(mCrossProfileApps).getInteractAcrossProfilesAppOp(crossProfilePackage))
                .isEqualTo(MODE_ALLOWED);
    }

    @Test
    public void run_clearsDefaultNotConfigurableInteractAcrossProfilesAppOps() {
        final String nonCrossProfilePackage = "com.example.testapp2";
        Set<String> packages = new HashSet<>();
        packages.add(nonCrossProfilePackage);
        shadowOf(mPackageManager).installPackage(buildTestPackageInfo(nonCrossProfilePackage));
        mCrossProfileApps.setInteractAcrossProfilesAppOp(nonCrossProfilePackage, MODE_ALLOWED);
        shadowOf(mDevicePolicyManager).setDefaultCrossProfilePackages(packages);

        new CreateManagedProfileTask(mContext, TEST_PARAMS, mCallback).run(TEST_USER_ID);

        assertThat(shadowOf(mCrossProfileApps)
                .getInteractAcrossProfilesAppOp(nonCrossProfilePackage))
                .isEqualTo(MODE_DEFAULT);
    }

    private PackageInfo buildTestPackageInfo(String packageName) {
        final PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = packageName;
        packageInfo.applicationInfo = new ApplicationInfo();
        packageInfo.applicationInfo.packageName = packageName;
        packageInfo.applicationInfo.name = "appName";
        return packageInfo;
    }
}
