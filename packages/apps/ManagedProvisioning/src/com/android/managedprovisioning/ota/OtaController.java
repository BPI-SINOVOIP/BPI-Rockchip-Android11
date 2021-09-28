/*
 * Copyright 2016, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.managedprovisioning.ota;

import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_DEVICE;
import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_PROFILE;
import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_USER;

import static com.android.internal.util.Preconditions.checkNotNull;

import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.UserInfo;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.ArraySet;
import android.view.inputmethod.InputMethod;

import com.android.internal.annotations.VisibleForTesting;
import com.android.managedprovisioning.analytics.MetricsWriterFactory;
import com.android.managedprovisioning.analytics.ProvisioningAnalyticsTracker;
import com.android.managedprovisioning.common.ManagedProvisioningSharedPreferences;
import com.android.managedprovisioning.common.ProvisionLogger;
import com.android.managedprovisioning.common.SettingsFacade;
import com.android.managedprovisioning.model.ProvisioningParams;
import com.android.managedprovisioning.task.CrossProfileIntentFiltersSetter;
import com.android.managedprovisioning.task.DeleteNonRequiredAppsTask;
import com.android.managedprovisioning.task.DisableInstallShortcutListenersTask;
import com.android.managedprovisioning.task.DisallowAddUserTask;
import com.android.managedprovisioning.task.InstallExistingPackageTask;
import com.android.managedprovisioning.task.MigrateSystemAppsSnapshotTask;
import com.android.managedprovisioning.task.UpdateInteractAcrossProfilesAppOpTask;

import java.util.List;
import java.util.function.IntFunction;

/**
 * After a system update, this class resets the cross-profile intent filters and performs any
 * tasks necessary to bring the system up to date.
 */
public class OtaController {

    private static final String TELECOM_PACKAGE = "com.android.server.telecom";

    private final Context mContext;
    private final TaskExecutor mTaskExecutor;
    private final CrossProfileIntentFiltersSetter mCrossProfileIntentFiltersSetter;

    private final UserManager mUserManager;
    private final DevicePolicyManager mDevicePolicyManager;

    private final IntFunction<ArraySet<String>> mMissingSystemImeProvider;
    private final ProvisioningAnalyticsTracker mProvisioningAnalyticsTracker;

    public OtaController(Context context) {
        this(context, new TaskExecutor(), new CrossProfileIntentFiltersSetter(context),
                userId -> getMissingSystemImePackages(context, UserHandle.of(userId)),
                new ProvisioningAnalyticsTracker(
                        MetricsWriterFactory.getMetricsWriter(context, new SettingsFacade()),
                        new ManagedProvisioningSharedPreferences(context)));
    }

    @VisibleForTesting
    OtaController(Context context, TaskExecutor taskExecutor,
            CrossProfileIntentFiltersSetter crossProfileIntentFiltersSetter,
            IntFunction<ArraySet<String>> missingSystemImeProvider,
            ProvisioningAnalyticsTracker provisioningAnalyticsTracker) {
        mContext = checkNotNull(context);
        mTaskExecutor = checkNotNull(taskExecutor);
        mCrossProfileIntentFiltersSetter = checkNotNull(crossProfileIntentFiltersSetter);
        mProvisioningAnalyticsTracker = checkNotNull(provisioningAnalyticsTracker);

        mUserManager = (UserManager) context.getSystemService(Context.USER_SERVICE);
        mDevicePolicyManager = (DevicePolicyManager) context.getSystemService(
                Context.DEVICE_POLICY_SERVICE);

        mMissingSystemImeProvider = missingSystemImeProvider;
    }

    public void run() {
        if (mContext.getUserId() != UserHandle.USER_SYSTEM) {
            return;
        }
        // Migrate snapshot files to use user serial number as file name.
        mTaskExecutor.execute(
                UserHandle.USER_SYSTEM, new MigrateSystemAppsSnapshotTask(
                        mContext, mTaskExecutor, mProvisioningAnalyticsTracker));

        // Check for device owner.
        final int deviceOwnerUserId = mDevicePolicyManager.getDeviceOwnerUserId();
        if (deviceOwnerUserId != UserHandle.USER_NULL) {
            addDeviceOwnerTasks(deviceOwnerUserId, mContext);
        }

        for (UserInfo userInfo : mUserManager.getUsers()) {
            if (userInfo.isManagedProfile()) {
                addManagedProfileTasks(userInfo.id, mContext);
            } else if (mDevicePolicyManager.getProfileOwnerAsUser(userInfo.id) != null) {
                addManagedUserTasks(userInfo.id, mContext);
            } else {
                // if this user has managed profiles, reset the cross-profile intent filters between
                // this user and its managed profiles.
                mCrossProfileIntentFiltersSetter.resetFilters(userInfo.id);
            }
        }

        mTaskExecutor.execute(mContext.getUserId(), new UpdateInteractAcrossProfilesAppOpTask(
                mContext,
                /* params= */ null,
                mTaskExecutor,
                mProvisioningAnalyticsTracker
        ));
    }

    void addDeviceOwnerTasks(final int userId, Context context) {
        ComponentName deviceOwner = mDevicePolicyManager.getDeviceOwnerComponentOnAnyUser();
        if (deviceOwner == null) {
            // Shouldn't happen
            ProvisionLogger.loge("No device owner found.");
            return;
        }

        // Build a set of fake params to be able to run the tasks
        ProvisioningParams fakeParams = new ProvisioningParams.Builder()
                .setDeviceAdminComponentName(deviceOwner)
                .setProvisioningAction(ACTION_PROVISION_MANAGED_DEVICE)
                .build();

        mTaskExecutor.execute(userId,
                new DeleteNonRequiredAppsTask(false, context, fakeParams, mTaskExecutor,
                        mProvisioningAnalyticsTracker));
        mTaskExecutor.execute(userId,
                new DisallowAddUserTask(UserManager.isSplitSystemUser(), context, fakeParams,
                        mTaskExecutor, mProvisioningAnalyticsTracker));
    }

    void addManagedProfileTasks(final int userId, Context context) {
        mUserManager.setUserRestriction(UserManager.DISALLOW_WALLPAPER, true,
                UserHandle.of(userId));
        // Enabling telecom package as it supports managed profiles from N.
        mTaskExecutor.execute(userId,
                new InstallExistingPackageTask(TELECOM_PACKAGE, context, null, mTaskExecutor,
                        mProvisioningAnalyticsTracker));

        ComponentName profileOwner = mDevicePolicyManager.getProfileOwnerAsUser(userId);
        if (profileOwner == null) {
            // Shouldn't happen.
            ProvisionLogger.loge("No profile owner on managed profile " + userId);
            return;
        }

        // Build a set of fake params to be able to run the tasks
        ProvisioningParams fakeParams = new ProvisioningParams.Builder()
                .setDeviceAdminComponentName(profileOwner)
                .setProvisioningAction(ACTION_PROVISION_MANAGED_PROFILE)
                .build();
        mTaskExecutor.execute(userId,
                new DisableInstallShortcutListenersTask(context, fakeParams, mTaskExecutor,
                        mProvisioningAnalyticsTracker));
        mTaskExecutor.execute(userId,
                new DeleteNonRequiredAppsTask(false, context, fakeParams, mTaskExecutor,
                        mProvisioningAnalyticsTracker));

        // Copying missing system IMEs if necessary.
        mMissingSystemImeProvider.apply(userId).forEach(packageName -> mTaskExecutor.execute(userId,
                new InstallExistingPackageTask(packageName, context, fakeParams, mTaskExecutor,
                        mProvisioningAnalyticsTracker)));
    }

    void addManagedUserTasks(final int userId, Context context) {
        ComponentName profileOwner = mDevicePolicyManager.getProfileOwnerAsUser(userId);
        if (profileOwner == null) {
            // Shouldn't happen.
            ProvisionLogger.loge("No profile owner on managed user " + userId);
            return;
        }

        // Build a set of fake params to be able to run the tasks
        ProvisioningParams fakeParams = new ProvisioningParams.Builder()
                .setDeviceAdminComponentName(profileOwner)
                .setProvisioningAction(ACTION_PROVISION_MANAGED_USER)
                .build();
        mTaskExecutor.execute(userId,
                new DeleteNonRequiredAppsTask(false, context, fakeParams, mTaskExecutor,
                        mProvisioningAnalyticsTracker));
    }

    /**
     * Returns IME packages that can be installed from the profile parent user.
     *
     * @param context {@link Context} of the caller.
     * @param userHandle {@link UserHandle} that specifies the user.
     * @return A set of IME package names that can be installed from the profile parent user.
     */
    private static ArraySet<String> getMissingSystemImePackages(Context context,
            UserHandle userHandle) {
        ArraySet<String> profileParentSystemImes = getInstalledSystemImePackages(context,
                context.getSystemService(UserManager.class).getProfileParent(userHandle));
        ArraySet<String> installedSystemImes = getInstalledSystemImePackages(context, userHandle);
        profileParentSystemImes.removeAll(installedSystemImes);
        return profileParentSystemImes;
    }

    /**
     * Returns a set of the installed IME package names for the given user.
     *
     * @param context {@link Context} of the caller.
     * @param userHandle {@link UserHandle} that specifies the user.
     * @return A set of IME package names.
     */
    private static ArraySet<String> getInstalledSystemImePackages(Context context,
            UserHandle userHandle) {
        PackageManager packageManager;
        try {
            packageManager = context
                    .createPackageContextAsUser("android", 0, userHandle)
                    .getPackageManager();
        } catch (PackageManager.NameNotFoundException e) {
            return new ArraySet<>();
        }
        List<ResolveInfo> resolveInfoList = packageManager.queryIntentServices(
                new Intent(InputMethod.SERVICE_INTERFACE),
                PackageManager.MATCH_SYSTEM_ONLY | PackageManager.MATCH_DISABLED_COMPONENTS);
        ArraySet<String> result = new ArraySet<>();
        for (ResolveInfo resolveInfo : resolveInfoList) {
            result.add(resolveInfo.serviceInfo.packageName);
        }
        return result;
    }
}
