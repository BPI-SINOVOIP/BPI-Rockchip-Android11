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

import android.Manifest;
import android.app.AppOpsManager;
import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.pm.CrossProfileApps;
import android.content.pm.PackageManager;
import android.content.pm.UserInfo;
import android.os.UserHandle;
import android.os.UserManager;

import com.android.internal.annotations.VisibleForTesting;
import com.android.managedprovisioning.task.interactacrossprofiles.CrossProfileAppsSnapshot;

import java.util.Set;
import java.util.stream.Collectors;


/**
 * Controller for granting the INTERACT_ACROSS_PROFILES appop at boot time.
 */
public class CrossProfileAppsPregrantController {

    private final Context mContext;
    private final UserManager mUserManager;
    private final PackageManager mPackageManager;
    private final DevicePolicyManager mDevicePolicyManager;
    private final CrossProfileApps mCrossProfileApps;
    private final CrossProfileAppsSnapshot mCrossProfileAppsSnapshot;
    private final AppOpsManager mAppOpsManager;

    public CrossProfileAppsPregrantController(Context context) {
        this(context,
                context.getSystemService(UserManager.class),
                context.getPackageManager(),
                context.getSystemService(DevicePolicyManager.class),
                context.getSystemService(CrossProfileApps.class),
                context.getSystemService(AppOpsManager.class));
    }

    @VisibleForTesting
    CrossProfileAppsPregrantController(Context context,
            UserManager userManager,
            PackageManager packageManager,
            DevicePolicyManager devicePolicyManager,
            CrossProfileApps crossProfileApps,
            AppOpsManager appOpsManager) {
        mContext = context;
        mUserManager = userManager;
        mPackageManager = packageManager;
        mDevicePolicyManager = devicePolicyManager;
        mCrossProfileApps = crossProfileApps;
        mAppOpsManager = appOpsManager;
        mCrossProfileAppsSnapshot = new CrossProfileAppsSnapshot(context);
    }

    public void checkCrossProfileAppsPermissions() {
        if (!isParentProfileOfManagedProfile()) {
            return;
        }

        Set<String> crossProfilePackages =
                mCrossProfileAppsSnapshot.getSnapshot(mContext.getUserId());

        String op = AppOpsManager.permissionToOp(Manifest.permission.INTERACT_ACROSS_PROFILES);
        for (String crossProfilePackageName : getConfigurableDefaultCrossProfilePackages()) {
            if (crossProfilePackages.contains(crossProfilePackageName) &&
                    !appOpIsChangedFromDefault(op, crossProfilePackageName)) {
                mCrossProfileApps.setInteractAcrossProfilesAppOp(crossProfilePackageName,
                        AppOpsManager.MODE_ALLOWED);
            }
        }
    }

    private boolean appOpIsChangedFromDefault(String op, String packageName) {
        try {
            int uid = mPackageManager.getPackageUid(packageName, /* flags= */ 0);
            return mAppOpsManager.unsafeCheckOpNoThrow(op, uid, packageName)
                    != AppOpsManager.MODE_DEFAULT;
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    private Set<String> getConfigurableDefaultCrossProfilePackages() {
        Set<String> defaultPackages = mDevicePolicyManager.getDefaultCrossProfilePackages();
        return defaultPackages.stream().filter(
                mCrossProfileApps::canConfigureInteractAcrossProfiles).collect(Collectors.toSet());
    }

    private boolean isParentProfileOfManagedProfile() {
        int currentUserId = android.os.Process.myUserHandle().getIdentifier();
        for (UserInfo userInfo : mUserManager.getProfiles(currentUserId)) {
            UserHandle userHandle = userInfo.getUserHandle();
            if (userInfo.isManagedProfile() &&
                    mUserManager.getProfileParent(userHandle).getIdentifier() == currentUserId) {
                return true;
            }
        }
        return false;
    }
}
