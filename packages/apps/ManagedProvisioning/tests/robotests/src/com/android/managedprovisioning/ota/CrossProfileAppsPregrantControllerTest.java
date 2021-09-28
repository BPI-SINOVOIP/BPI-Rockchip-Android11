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

package com.android.managedprovisioning.ota;

import static android.content.pm.UserInfo.FLAG_MANAGED_PROFILE;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static com.google.common.truth.Truth.assertThat;
import android.app.AppOpsManager;
import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.pm.CrossProfileApps;
import android.content.pm.PackageManager;
import android.os.UserHandle;
import android.os.UserManager;

import androidx.test.core.app.ApplicationProvider;

import com.android.managedprovisioning.task.interactacrossprofiles.CrossProfileAppsSnapshot;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.shadows.ShadowProcess;

import java.util.Collections;
import java.util.Set;

@RunWith(RobolectricTestRunner.class)
public class CrossProfileAppsPregrantControllerTest {

    private static final int MY_USER_ID = 0;
    private static final int OTHER_USER_ID = 5;
    private static final int OTHER_USER_UID = UserHandle.PER_USER_RANGE * OTHER_USER_ID;

    private final Context mContext = ApplicationProvider.getApplicationContext();
    private final UserManager mUserManager = mContext.getSystemService(UserManager.class);
    private final CrossProfileApps mCrossProfileApps =
            mContext.getSystemService(CrossProfileApps.class);
    private final DevicePolicyManager mDevicePolicyManager =
            mContext.getSystemService(DevicePolicyManager.class);
    private final CrossProfileAppsSnapshot mCrossProfileAppsSnapshot =
            new CrossProfileAppsSnapshot(mContext);

    // TODO(158280372): Remove mocks once suitable shadows are added
    private final PackageManager mPackageManager = mock(PackageManager.class);
    private final AppOpsManager mAppOpsManager = mock(AppOpsManager.class);

    private final CrossProfileAppsPregrantController mPregrantController =
            new CrossProfileAppsPregrantController(
                    mContext,
                    mUserManager,
                    mPackageManager,
                    mDevicePolicyManager,
                    mCrossProfileApps,
                    mAppOpsManager);

    private static final String TEST_PACKAGE = "test.package";

    @Before
    public void setup() {
        // This is needed because the cross profile apps shadow doesn't actually set the appop
        // so we copy the value of the app op into the mock AppOpsManager
        when(mAppOpsManager.unsafeCheckOpNoThrow(anyString(), anyInt(), anyString()))
                .thenAnswer(
                        invocation ->
                                shadowOf(mCrossProfileApps).getInteractAcrossProfilesAppOp(
                                        invocation.getArgument(2)));
    }

    @Test
    public void onPrimaryProfile_noManagedProfile_doesNotPregrant() {
        // We default to no managed profile
        setWhitelistedPackages(Collections.singleton(TEST_PACKAGE));
        setConfigurablePackages(Collections.singleton(TEST_PACKAGE));
        mCrossProfileAppsSnapshot.takeNewSnapshot(mContext.getUserId());
        mCrossProfileApps.setInteractAcrossProfilesAppOp(TEST_PACKAGE, AppOpsManager.MODE_DEFAULT);

        mPregrantController.checkCrossProfileAppsPermissions();

        assertThat(shadowOf(mCrossProfileApps)
                .getInteractAcrossProfilesAppOp(TEST_PACKAGE))
                .isEqualTo(AppOpsManager.MODE_DEFAULT);
    }

    @Test
    public void onManagedProfile_doesNotPregrant() {
        setRunningOnManagedProfile();
        setWhitelistedPackages(Collections.singleton(TEST_PACKAGE));
        setConfigurablePackages(Collections.singleton(TEST_PACKAGE));
        mCrossProfileAppsSnapshot.takeNewSnapshot(mContext.getUserId());
        mCrossProfileApps.setInteractAcrossProfilesAppOp(TEST_PACKAGE, AppOpsManager.MODE_DEFAULT);

        mPregrantController.checkCrossProfileAppsPermissions();

        assertThat(shadowOf(mCrossProfileApps)
                .getInteractAcrossProfilesAppOp(TEST_PACKAGE))
                .isEqualTo(AppOpsManager.MODE_DEFAULT);
    }

    @Test
    public void defaultConfigurablePackage_doesPregrant() {
        setRunningOnParentProfile();
        setWhitelistedPackages(Collections.singleton(TEST_PACKAGE));
        setConfigurablePackages(Collections.singleton(TEST_PACKAGE));
        mCrossProfileAppsSnapshot.takeNewSnapshot(mContext.getUserId());
        mCrossProfileApps.setInteractAcrossProfilesAppOp(TEST_PACKAGE, AppOpsManager.MODE_DEFAULT);

        mPregrantController.checkCrossProfileAppsPermissions();

        assertThat(shadowOf(mCrossProfileApps)
                .getInteractAcrossProfilesAppOp(TEST_PACKAGE))
                .isEqualTo(AppOpsManager.MODE_ALLOWED);
    }

    @Test
    public void nonDefaultConfigurablePackage_doesNotPregrant() {
        setRunningOnParentProfile();
        setWhitelistedPackages(Collections.singleton(TEST_PACKAGE));
        setConfigurablePackages(Collections.singleton(TEST_PACKAGE));
        mCrossProfileAppsSnapshot.takeNewSnapshot(mContext.getUserId());
        mCrossProfileApps.setInteractAcrossProfilesAppOp(TEST_PACKAGE, AppOpsManager.MODE_IGNORED);

        mPregrantController.checkCrossProfileAppsPermissions();

        assertThat(shadowOf(mCrossProfileApps)
                .getInteractAcrossProfilesAppOp(TEST_PACKAGE))
                .isEqualTo(AppOpsManager.MODE_IGNORED);
    }

    private void setRunningOnParentProfile() {
        shadowOf(mUserManager).addProfile(
                MY_USER_ID, OTHER_USER_ID, /* profileName= */"otherUser", FLAG_MANAGED_PROFILE);
    }

    private void setRunningOnManagedProfile() {
        shadowOf(mUserManager).addProfile(
                MY_USER_ID, OTHER_USER_ID, /* profileName= */"otherUser", FLAG_MANAGED_PROFILE);
        ShadowProcess.setUid(OTHER_USER_UID);
    }

    private void setWhitelistedPackages(Set<String> packages) {
        shadowOf(mDevicePolicyManager).setDefaultCrossProfilePackages(packages);
    }

    private void setConfigurablePackages(Set<String> packages) {
        packages.forEach(p -> shadowOf(mCrossProfileApps).addCrossProfilePackage(p));
    }
}
