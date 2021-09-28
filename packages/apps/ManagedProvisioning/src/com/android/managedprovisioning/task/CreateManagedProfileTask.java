/*
 * Copyright (C) 2016 The Android Open Source Project
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

import static com.android.internal.logging.nano.MetricsProto.MetricsEvent.PROVISIONING_CREATE_PROFILE_TASK_MS;
import static com.android.internal.util.Preconditions.checkNotNull;

import android.Manifest;
import android.app.AppOpsManager;
import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.pm.CrossProfileApps;
import android.content.pm.PackageManager;
import android.content.pm.UserInfo;
import android.os.UserManager;

import com.android.internal.annotations.VisibleForTesting;
import com.android.managedprovisioning.R;
import com.android.managedprovisioning.analytics.MetricsWriterFactory;
import com.android.managedprovisioning.analytics.ProvisioningAnalyticsTracker;
import com.android.managedprovisioning.common.ManagedProvisioningSharedPreferences;
import com.android.managedprovisioning.common.SettingsFacade;
import com.android.managedprovisioning.model.ProvisioningParams;
import com.android.managedprovisioning.task.interactacrossprofiles.CrossProfileAppsSnapshot;
import com.android.managedprovisioning.task.nonrequiredapps.NonRequiredAppsLogic;

import java.util.Set;
import java.util.stream.Collectors;

/**
 * Task to create a managed profile.
 */
public class CreateManagedProfileTask extends AbstractProvisioningTask {

    private int mProfileUserId;
    private final NonRequiredAppsLogic mNonRequiredAppsLogic;
    private final CrossProfileAppsSnapshot mCrossProfileAppsSnapshot;
    private final UserManager mUserManager;
    private final CrossProfileApps mCrossProfileApps;
    private final DevicePolicyManager mDevicePolicyManager;
    private final PackageManager mPackageManager;
    private final AppOpsManager mAppOpsManager;

    public CreateManagedProfileTask(Context context, ProvisioningParams params, Callback callback) {
        this(
                context,
                params,
                callback,
                context.getSystemService(UserManager.class),
                new NonRequiredAppsLogic(context, true, params),
                new CrossProfileAppsSnapshot(context),
                new ProvisioningAnalyticsTracker(
                        MetricsWriterFactory.getMetricsWriter(context, new SettingsFacade()),
                        new ManagedProvisioningSharedPreferences(context)));
    }

    @VisibleForTesting
    CreateManagedProfileTask(
            Context context,
            ProvisioningParams params,
            Callback callback,
            UserManager userManager,
            NonRequiredAppsLogic logic,
            CrossProfileAppsSnapshot crossProfileAppsSnapshot,
            ProvisioningAnalyticsTracker provisioningAnalyticsTracker) {
        super(context, params, callback, provisioningAnalyticsTracker);
        mNonRequiredAppsLogic = checkNotNull(logic);
        mUserManager = checkNotNull(userManager);
        mCrossProfileAppsSnapshot = checkNotNull(crossProfileAppsSnapshot);
        mCrossProfileApps = context.getSystemService(CrossProfileApps.class);
        mDevicePolicyManager = context.getSystemService(DevicePolicyManager.class);
        mPackageManager = context.getPackageManager();
        mAppOpsManager = context.getSystemService(AppOpsManager.class);
    }

    @Override
    public void run(int userId) {
        startTaskTimer();
        final Set<String> nonRequiredApps = mNonRequiredAppsLogic.getSystemAppsToRemove(userId);
        UserInfo userInfo = mUserManager.createProfileForUserEvenWhenDisallowed(
                mContext.getString(R.string.default_managed_profile_name),
                UserManager.USER_TYPE_PROFILE_MANAGED, UserInfo.FLAG_DISABLED,
                userId, nonRequiredApps.toArray(new String[nonRequiredApps.size()]));
        if (userInfo == null) {
            error(0);
            return;
        }
        mProfileUserId = userInfo.id;
        mNonRequiredAppsLogic.maybeTakeSystemAppsSnapshot(userInfo.id);
        mCrossProfileAppsSnapshot.takeNewSnapshot(mContext.getUserId());
        resetInteractAcrossProfilesAppOps();
        stopTaskTimer();
        success();
    }

    private void resetInteractAcrossProfilesAppOps() {
        mCrossProfileApps.clearInteractAcrossProfilesAppOps();
        pregrantDefaultInteractAcrossProfilesAppOps();
    }

    private void pregrantDefaultInteractAcrossProfilesAppOps() {
        final String op =
                AppOpsManager.permissionToOp(Manifest.permission.INTERACT_ACROSS_PROFILES);
        for (String packageName : getConfigurableDefaultCrossProfilePackages()) {
            if (appOpIsChangedFromDefault(op, packageName)) {
                continue;
            }
            mCrossProfileApps.setInteractAcrossProfilesAppOp(packageName, MODE_ALLOWED);
        }
    }

    private Set<String> getConfigurableDefaultCrossProfilePackages() {
        Set<String> defaultPackages = mDevicePolicyManager.getDefaultCrossProfilePackages();
        return defaultPackages.stream().filter(
                mCrossProfileApps::canConfigureInteractAcrossProfiles).collect(Collectors.toSet());
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
        return R.string.progress_initialize;
    }

    @Override
    protected int getMetricsCategory() {
        return PROVISIONING_CREATE_PROFILE_TASK_MS;
    }

    public int getProfileUserId() {
        return mProfileUserId;
    }
}
