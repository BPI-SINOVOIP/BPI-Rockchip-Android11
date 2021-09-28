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

import android.Manifest;
import android.app.AppOpsManager;
import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.pm.CrossProfileApps;
import android.content.pm.PackageManager;
import android.util.ArraySet;

import com.android.managedprovisioning.analytics.ProvisioningAnalyticsTracker;
import com.android.managedprovisioning.model.ProvisioningParams;
import com.android.managedprovisioning.task.interactacrossprofiles.CrossProfileAppsSnapshot;

import java.util.HashSet;
import java.util.Set;

/**
 * Task which resets the {@code INTERACT_ACROSS_USERS} app op when the OEM whitelist is changed.
 */
public class UpdateInteractAcrossProfilesAppOpTask extends AbstractProvisioningTask {

    private final CrossProfileAppsSnapshot mCrossProfileAppsSnapshot;
    private final CrossProfileApps mCrossProfileApps;
    private final DevicePolicyManager mDevicePolicyManager;
    private final AppOpsManager mAppOpsManager;
    private final PackageManager mPackageManager;

    public UpdateInteractAcrossProfilesAppOpTask(Context context,
            ProvisioningParams provisioningParams,
            Callback callback,
            ProvisioningAnalyticsTracker provisioningAnalyticsTracker) {
        super(context, provisioningParams, callback, provisioningAnalyticsTracker);
        mCrossProfileAppsSnapshot = new CrossProfileAppsSnapshot(context);
        mCrossProfileApps = context.getSystemService(CrossProfileApps.class);
        mDevicePolicyManager = context.getSystemService(DevicePolicyManager.class);
        mAppOpsManager = context.getSystemService(AppOpsManager.class);
        mPackageManager = context.getPackageManager();
    }

    @Override
    public void run(int userId) {
        Set<String> previousCrossProfileApps =
                mCrossProfileAppsSnapshot.hasSnapshot(userId) ?
                        mCrossProfileAppsSnapshot.getSnapshot(userId) :
                        new ArraySet<>();
        mCrossProfileAppsSnapshot.takeNewSnapshot(userId);
        Set<String> currentCrossProfileApps = mCrossProfileAppsSnapshot.getSnapshot(userId);

        updateAfterOtaChanges(previousCrossProfileApps, currentCrossProfileApps);
    }

    private void updateAfterOtaChanges(
            Set<String> previousCrossProfilePackages, Set<String> currentCrossProfilePackages) {
        mCrossProfileApps.resetInteractAcrossProfilesAppOps(
                previousCrossProfilePackages, currentCrossProfilePackages);
        Set<String> newCrossProfilePackages = new HashSet<>(currentCrossProfilePackages);
        newCrossProfilePackages.removeAll(previousCrossProfilePackages);

        grantNewConfigurableDefaultCrossProfilePackages(newCrossProfilePackages);
    }

    private void grantNewConfigurableDefaultCrossProfilePackages(
            Set<String> newCrossProfilePackages) {
        final String op =
                AppOpsManager.permissionToOp(Manifest.permission.INTERACT_ACROSS_PROFILES);
        for (String crossProfilePackageName : newCrossProfilePackages) {
            if (!mCrossProfileApps.canConfigureInteractAcrossProfiles(crossProfilePackageName)) {
                continue;
            }
            if (appOpIsChangedFromDefault(op, crossProfilePackageName)) {
                continue;
            }

            mCrossProfileApps.setInteractAcrossProfilesAppOp(crossProfilePackageName,
                    AppOpsManager.MODE_ALLOWED);
        }
    }

    private boolean appOpIsChangedFromDefault(String op, String packageName) {
        try {
            final int uid = mPackageManager.getPackageUid(packageName, /* flags= */ 0);
            return mAppOpsManager.unsafeCheckOpNoThrow(op, uid, packageName)
                    != AppOpsManager.MODE_DEFAULT;
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    @Override
    public int getStatusMsgId() {
        return 0;
    }
}
