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

package com.android.car.settings.applications;

import static android.app.Activity.RESULT_OK;

import static com.android.car.settings.applications.ApplicationsUtils.isKeepEnabledPackage;
import static com.android.car.settings.applications.ApplicationsUtils.isProfileOrDeviceOwner;
import static com.android.car.settings.common.ActionButtonsPreference.ActionButtons;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.admin.DevicePolicyManager;
import android.car.drivingstate.CarUxRestrictions;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Bundle;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.ArraySet;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.android.car.settings.R;
import com.android.car.settings.common.ActionButtonInfo;
import com.android.car.settings.common.ActionButtonsPreference;
import com.android.car.settings.common.ActivityResultCallback;
import com.android.car.settings.common.ConfirmationDialogFragment;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.Logger;
import com.android.car.settings.common.PreferenceController;
import com.android.car.settings.users.UserHelper;
import com.android.settingslib.Utils;
import com.android.settingslib.applications.ApplicationsState;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * Shows actions associated with an application, like uninstall and forceStop.
 *
 * <p>To uninstall an app, it must <i>not</i> be:
 * <ul>
 * <li>a system bundled app
 * <li>system signed
 * <li>managed by an active admin from a device policy
 * <li>a device or profile owner
 * <li>the only home app
 * <li>the default home app
 * <li>for a user with the {@link UserManager#DISALLOW_APPS_CONTROL} restriction
 * <li>for a user with the {@link UserManager#DISALLOW_UNINSTALL_APPS} restriction
 * </ul>
 *
 * <p>For apps that cannot be uninstalled, a disable option is shown instead (or enable if the app
 * is already disabled).
 */
public class ApplicationActionButtonsPreferenceController extends
        PreferenceController<ActionButtonsPreference> implements ActivityResultCallback {
    private static final Logger LOG = new Logger(
            ApplicationActionButtonsPreferenceController.class);

    @VisibleForTesting
    static final String DISABLE_CONFIRM_DIALOG_TAG =
            "com.android.car.settings.applications.DisableConfirmDialog";
    @VisibleForTesting
    static final String FORCE_STOP_CONFIRM_DIALOG_TAG =
            "com.android.car.settings.applications.ForceStopConfirmDialog";
    @VisibleForTesting
    static final int UNINSTALL_REQUEST_CODE = 10;

    private DevicePolicyManager mDpm;
    private PackageManager mPm;
    private UserManager mUserManager;
    private UserHelper mUserHelper;
    private ApplicationsState.Session mSession;

    private ApplicationsState.AppEntry mAppEntry;
    private ApplicationsState mApplicationsState;
    private String mPackageName;
    private PackageInfo mPackageInfo;

    @VisibleForTesting
    final ConfirmationDialogFragment.ConfirmListener mForceStopConfirmListener =
            new ConfirmationDialogFragment.ConfirmListener() {
                @Override
                public void onConfirm(@Nullable Bundle arguments) {
                    LOG.d("Stopping package " + mPackageName);
                    getContext().getSystemService(ActivityManager.class)
                            .forceStopPackage(mPackageName);
                    int userId = UserHandle.getUserId(mAppEntry.info.uid);
                    mApplicationsState.invalidatePackage(mPackageName, userId);
                }
            };

    private final View.OnClickListener mForceStopClickListener = i -> {
        ConfirmationDialogFragment dialogFragment =
                new ConfirmationDialogFragment.Builder(getContext())
                        .setTitle(R.string.force_stop_dialog_title)
                        .setMessage(R.string.force_stop_dialog_text)
                        .setPositiveButton(android.R.string.ok,
                                mForceStopConfirmListener)
                        .setNegativeButton(android.R.string.cancel, /* rejectListener= */ null)
                        .build();
        getFragmentController().showDialog(dialogFragment, FORCE_STOP_CONFIRM_DIALOG_TAG);
    };

    @VisibleForTesting
    final BroadcastReceiver mCheckKillProcessesReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            boolean enabled = getResultCode() != Activity.RESULT_CANCELED;
            LOG.d("Got broadcast response: Restart status for " + mPackageName + " " + enabled);
            updateForceStopButtonInner(enabled);
        }
    };

    @VisibleForTesting
    final ConfirmationDialogFragment.ConfirmListener mDisableConfirmListener =
            new ConfirmationDialogFragment.ConfirmListener() {
                @Override
                public void onConfirm(@Nullable Bundle arguments) {
                    mPm.setApplicationEnabledSetting(mPackageName,
                            PackageManager.COMPONENT_ENABLED_STATE_DISABLED_USER, /* flags= */ 0);
                    updateUninstallButtonInner(false);
                }
            };

    private final View.OnClickListener mDisableClickListener = i -> {
        ConfirmationDialogFragment dialogFragment =
                new ConfirmationDialogFragment.Builder(getContext())
                        .setMessage(getContext().getString(R.string.app_disable_dialog_text))
                        .setPositiveButton(R.string.app_disable_dialog_positive,
                                mDisableConfirmListener)
                        .setNegativeButton(android.R.string.cancel, /* rejectListener= */ null)
                        .build();
        getFragmentController().showDialog(dialogFragment, DISABLE_CONFIRM_DIALOG_TAG);
    };

    private final View.OnClickListener mEnableClickListener = i -> {
        mPm.setApplicationEnabledSetting(mPackageName,
                PackageManager.COMPONENT_ENABLED_STATE_DEFAULT, /* flags= */ 0);
        updateUninstallButtonInner(true);
    };

    private final View.OnClickListener mUninstallClickListener = i -> {
        Uri packageUri = Uri.parse("package:" + mPackageName);
        Intent uninstallIntent = new Intent(Intent.ACTION_UNINSTALL_PACKAGE, packageUri);
        uninstallIntent.putExtra(Intent.EXTRA_RETURN_RESULT, true);
        getFragmentController().startActivityForResult(uninstallIntent, UNINSTALL_REQUEST_CODE,
                /* callback= */ this);
    };

    private final ApplicationsState.Callbacks mApplicationStateCallbacks =
            new ApplicationsState.Callbacks() {
                @Override
                public void onRunningStateChanged(boolean running) {
                }

                @Override
                public void onPackageListChanged() {
                    refreshUi();
                }

                @Override
                public void onRebuildComplete(ArrayList<ApplicationsState.AppEntry> apps) {
                }

                @Override
                public void onPackageIconChanged() {
                }

                @Override
                public void onPackageSizeChanged(String packageName) {
                }

                @Override
                public void onAllSizesComputed() {
                }

                @Override
                public void onLauncherInfoChanged() {
                }

                @Override
                public void onLoadEntriesCompleted() {
                }
            };

    public ApplicationActionButtonsPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
        mDpm = context.getSystemService(DevicePolicyManager.class);
        mPm = context.getPackageManager();
        mUserManager = UserManager.get(context);
        mUserHelper = UserHelper.getInstance(context);
    }

    @Override
    protected Class<ActionButtonsPreference> getPreferenceType() {
        return ActionButtonsPreference.class;
    }

    /** Sets the {@link ApplicationsState.AppEntry} which is used to load the app name and icon. */
    public ApplicationActionButtonsPreferenceController setAppEntry(
            ApplicationsState.AppEntry appEntry) {
        mAppEntry = appEntry;
        return this;
    }

    /** Sets the {@link ApplicationsState} which is used to load the app name and icon. */
    public ApplicationActionButtonsPreferenceController setAppState(
            ApplicationsState applicationsState) {
        mApplicationsState = applicationsState;
        return this;
    }

    /**
     * Set the packageName, which is used to perform actions on a particular package.
     */
    public ApplicationActionButtonsPreferenceController setPackageName(String packageName) {
        mPackageName = packageName;
        return this;
    }

    @Override
    protected void checkInitialized() {
        if (mAppEntry == null || mApplicationsState == null || mPackageName == null) {
            throw new IllegalStateException(
                    "AppEntry, AppState, and PackageName should be set before calling this "
                            + "function");
        }
    }

    @Override
    protected void onCreateInternal() {
        ConfirmationDialogFragment.resetListeners(
                (ConfirmationDialogFragment) getFragmentController().findDialogByTag(
                        DISABLE_CONFIRM_DIALOG_TAG),
                mDisableConfirmListener,
                /* rejectListener= */ null,
                /* neutralListener= */ null);
        ConfirmationDialogFragment.resetListeners(
                (ConfirmationDialogFragment) getFragmentController().findDialogByTag(
                        FORCE_STOP_CONFIRM_DIALOG_TAG),
                mForceStopConfirmListener,
                /* rejectListener= */ null,
                /* neutralListener= */ null);
        getPreference().getButton(ActionButtons.BUTTON2)
                .setText(R.string.force_stop)
                .setIcon(R.drawable.ic_warning)
                .setOnClickListener(mForceStopClickListener)
                .setEnabled(false);
        mSession = mApplicationsState.newSession(mApplicationStateCallbacks);
    }

    @Override
    protected void onStartInternal() {
        mSession.onResume();
    }

    @Override
    protected void onStopInternal() {
        mSession.onPause();
    }

    @Override
    protected void updateState(ActionButtonsPreference preference) {
        refreshAppEntry();
        if (mAppEntry == null) {
            getFragmentController().goBack();
            return;
        }
        updateForceStopButton();
        updateUninstallButton();
    }

    private void refreshAppEntry() {
        mAppEntry = mApplicationsState.getEntry(mPackageName, UserHandle.myUserId());
        if (mAppEntry != null) {
            try {
                mPackageInfo = mPm.getPackageInfo(mPackageName,
                        PackageManager.MATCH_DISABLED_COMPONENTS | PackageManager.MATCH_ANY_USER
                                | PackageManager.GET_SIGNATURES | PackageManager.GET_PERMISSIONS);
            } catch (PackageManager.NameNotFoundException e) {
                LOG.e("Exception when retrieving package:" + mPackageName, e);
                mPackageInfo = null;
            }
        } else {
            mPackageInfo = null;
        }
    }

    private void updateForceStopButton() {
        if (mDpm.packageHasActiveAdmins(mPackageName)) {
            updateForceStopButtonInner(/* enabled= */ false);
        } else if ((mAppEntry.info.flags & ApplicationInfo.FLAG_STOPPED) == 0) {
            // If the app isn't explicitly stopped, then always show the force stop button.
            updateForceStopButtonInner(/* enabled= */ true);
        } else {
            Intent intent = new Intent(Intent.ACTION_QUERY_PACKAGE_RESTART,
                    Uri.fromParts("package", mPackageName, /* fragment= */ null));
            intent.putExtra(Intent.EXTRA_PACKAGES, new String[]{mPackageName});
            intent.putExtra(Intent.EXTRA_UID, mAppEntry.info.uid);
            intent.putExtra(Intent.EXTRA_USER_HANDLE,
                    UserHandle.getUserId(mAppEntry.info.uid));
            LOG.d("Sending broadcast to query restart status for " + mPackageName);
            getContext().sendOrderedBroadcastAsUser(intent,
                    UserHandle.CURRENT,
                    /* receiverPermission= */ null,
                    mCheckKillProcessesReceiver,
                    /* scheduler= */ null,
                    Activity.RESULT_CANCELED,
                    /* initialData= */ null,
                    /* initialExtras= */ null);
        }
    }

    private void updateForceStopButtonInner(boolean enabled) {
        getPreference().getButton(ActionButtons.BUTTON2).setEnabled(
                enabled && !mUserManager.hasUserRestriction(UserManager.DISALLOW_APPS_CONTROL));
    }

    private void updateUninstallButtonInner(boolean isAppEnabled) {
        ActionButtonInfo uninstallButton = getPreference().getButton(ActionButtons.BUTTON1);
        if (isBundledApp()) {
            if (isAppEnabled) {
                uninstallButton.setText(R.string.disable_text).setIcon(
                        R.drawable.ic_block).setOnClickListener(mDisableClickListener);
            } else {
                uninstallButton.setText(R.string.enable_text).setIcon(
                        R.drawable.ic_check_circle).setOnClickListener(mEnableClickListener);
            }
        } else {
            uninstallButton.setText(R.string.uninstall_text).setIcon(
                    R.drawable.ic_delete).setOnClickListener(mUninstallClickListener);
        }

        uninstallButton.setEnabled(!shouldDisableUninstallButton());
    }

    private void updateUninstallButton() {
        updateUninstallButtonInner(isAppEnabled());
    }

    private boolean shouldDisableUninstallButton() {
        if (shouldDisableUninstallForHomeApp()) {
            LOG.d("Uninstall disabled for home app");
            return true;
        }

        if (isAppEnabled() && isKeepEnabledPackage(getContext(), mPackageName)) {
            LOG.d("Disable button disabled for keep enabled package");
            return true;
        }

        if (Utils.isSystemPackage(getContext().getResources(), mPm, mPackageInfo)) {
            LOG.d("Uninstall disabled for system package");
            return true;
        }

        if (mDpm.packageHasActiveAdmins(mPackageName)) {
            LOG.d("Uninstall disabled because package has active admins");
            return true;
        }

        // We don't allow uninstalling profile/device owner on any user because if it's a system
        // app, "uninstall" is actually "downgrade to the system version + disable", and
        // "downgrade" will clear data on all users.
        if (isProfileOrDeviceOwner(mPackageName, mDpm, mUserHelper)) {
            LOG.d("Uninstall disabled because package is profile or device owner");
            return true;
        }

        if (mDpm.isUninstallInQueue(mPackageName)) {
            LOG.d("Uninstall disabled because intent is already queued");
            return true;
        }

        if (mUserManager.hasUserRestriction(UserManager.DISALLOW_APPS_CONTROL)) {
            LOG.d("Uninstall disabled because user has DISALLOW_APPS_CONTROL restriction");
            return true;
        }

        if (mUserManager.hasUserRestriction(UserManager.DISALLOW_UNINSTALL_APPS)) {
            LOG.d("Uninstall disabled because user has DISALLOW_UNINSTALL_APPS restriction");
            return true;
        }

        return false;
    }

    /**
     * Returns {@code true} if the package is a Home app that should not be uninstalled. We don't
     * risk downgrading bundled home apps because that can interfere with home-key resolution. We
     * can't allow removal of the only home app, and we don't want to allow removal of an
     * explicitly preferred home app. The user can go to Home settings and pick a different app,
     * after which we'll permit removal of the now-not-default app.
     */
    private boolean shouldDisableUninstallForHomeApp() {
        Set<String> homePackages = new ArraySet<>();
        // Get list of "home" apps and trace through any meta-data references.
        List<ResolveInfo> homeActivities = new ArrayList<>();
        ComponentName currentDefaultHome = mPm.getHomeActivities(homeActivities);
        for (int i = 0; i < homeActivities.size(); i++) {
            ResolveInfo ri = homeActivities.get(i);
            String activityPkg = ri.activityInfo.packageName;
            homePackages.add(activityPkg);

            // Also make sure to include anything proxying for the home app.
            Bundle metadata = ri.activityInfo.metaData;
            if (metadata != null) {
                String metaPkg = metadata.getString(ActivityManager.META_HOME_ALTERNATE);
                if (signaturesMatch(metaPkg, activityPkg)) {
                    homePackages.add(metaPkg);
                }
            }
        }

        if (homePackages.contains(mPackageName)) {
            if (isBundledApp()) {
                // Don't risk a downgrade.
                return true;
            } else if (currentDefaultHome == null) {
                // No preferred default. Permit uninstall only when there is more than one
                // candidate.
                return (homePackages.size() == 1);
            } else {
                // Explicit default home app. Forbid uninstall of that one, but permit it for
                // installed-but-inactive ones.
                return mPackageName.equals(currentDefaultHome.getPackageName());
            }
        } else {
            // Not a home app.
            return false;
        }
    }

    private boolean signaturesMatch(String pkg1, String pkg2) {
        if (pkg1 != null && pkg2 != null) {
            try {
                int match = mPm.checkSignatures(pkg1, pkg2);
                if (match >= PackageManager.SIGNATURE_MATCH) {
                    return true;
                }
            } catch (Exception e) {
                // e.g. package not found during lookup. Possibly bad input.
                // Just return false as this isn't a reason to crash given the use case.
            }
        }
        return false;
    }

    private boolean isBundledApp() {
        return (mAppEntry.info.flags & ApplicationInfo.FLAG_SYSTEM) != 0;
    }

    private boolean isAppEnabled() {
        return mAppEntry.info.enabled && !(mAppEntry.info.enabledSetting
                == PackageManager.COMPONENT_ENABLED_STATE_DISABLED_UNTIL_USED);
    }

    @Override
    public void processActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        if (requestCode == UNINSTALL_REQUEST_CODE) {
            if (resultCode == RESULT_OK) {
                getFragmentController().goBack();
            } else {
                LOG.e("Uninstall failed with result " + resultCode);
            }
        }
    }
}
