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

package com.android.car.settings.storage;

import static com.android.car.settings.common.ActionButtonsPreference.ActionButtons;

import android.app.ActivityManager;
import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.IPackageDataObserver;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.os.Message;
import android.os.UserHandle;
import android.os.UserManager;

import androidx.annotation.VisibleForTesting;
import androidx.loader.app.LoaderManager;

import com.android.car.settings.R;
import com.android.car.settings.common.ActionButtonInfo;
import com.android.car.settings.common.ActionButtonsPreference;
import com.android.car.settings.common.ConfirmationDialogFragment;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.Logger;
import com.android.car.settings.common.PreferenceController;
import com.android.settingslib.RestrictedLockUtils;
import com.android.settingslib.RestrictedLockUtilsInternal;
import com.android.settingslib.applications.ApplicationsState;
import com.android.settingslib.applications.StorageStatsSource;

/**
 * Displays the action buttons to clear an applications cache and user data.
 */
public class StorageApplicationActionButtonsPreferenceController extends
        PreferenceController<ActionButtonsPreference> implements
        AppsStorageStatsManager.Callback {
    private static final Logger LOG = new Logger(
            StorageApplicationActionButtonsPreferenceController.class);

    @VisibleForTesting
    static final String CONFIRM_CLEAR_STORAGE_DIALOG_TAG =
            "com.android.car.settings.storage.ConfirmClearStorageDialog";

    @VisibleForTesting
    static final String CONFIRM_CANNOT_CLEAR_STORAGE_DIALOG_TAG =
            "com.android.car.settings.storage.ConfirmCannotClearStorageDialog";

    public static final String EXTRA_PACKAGE_NAME = "extra_package_name";
    // Result code identifiers
    public static final int REQUEST_MANAGE_SPACE = 2;

    // Internal constants used in Handler
    private static final int OP_SUCCESSFUL = 1;
    private static final int OP_FAILED = 2;

    // Constant used in handler to determine when the user data is cleared.
    private static final int MSG_CLEAR_USER_DATA = 1;
    // Constant used in handler to determine when the cache is cleared.
    private static final int MSG_CLEAR_CACHE = 2;

    private ActionButtonInfo mClearStorageButton;
    private ActionButtonInfo mClearCacheButton;

    private ApplicationsState.AppEntry mAppEntry;
    private String mPackageName;
    private ApplicationInfo mInfo;
    private AppsStorageStatsManager mAppsStorageStatsManager;
    private LoaderManager mLoaderManager;

    //  An observer callback to get notified when the cache file deletion is complete.
    private ClearCacheObserver mClearCacheObserver;
    //  An observer callback to get notified when the user data deletion is complete.
    private ClearUserDataObserver mClearDataObserver;

    private PackageManager mPackageManager;
    private RestrictedLockUtils.EnforcedAdmin mAppsControlDisallowedAdmin;
    private boolean mAppsControlDisallowedBySystem;
    private int mUserId;

    private boolean mCacheCleared;
    private boolean mDataCleared;

    private final ConfirmationDialogFragment.ConfirmListener mConfirmClearStorageDialog =
            arguments -> initiateClearUserData();

    private final ConfirmationDialogFragment.ConfirmListener mConfirmCannotClearStorageDialog =
            arguments -> mClearStorageButton.setEnabled(false);

    public StorageApplicationActionButtonsPreferenceController(Context context,
            String preferenceKey, FragmentController fragmentController,
            CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
        mUserId = UserHandle.myUserId();
        mPackageManager = context.getPackageManager();
    }

    @Override
    protected Class<ActionButtonsPreference> getPreferenceType() {
        return ActionButtonsPreference.class;
    }

    /** Sets the {@link ApplicationsState.AppEntry} which is used to load the app name and icon. */
    public StorageApplicationActionButtonsPreferenceController setAppEntry(
            ApplicationsState.AppEntry appEntry) {
        mAppEntry = appEntry;
        return this;
    }

    /**
     * Set the packageName, which is used to perform actions on a particular package.
     */
    public StorageApplicationActionButtonsPreferenceController setPackageName(String packageName) {
        mPackageName = packageName;
        return this;
    }

    /**
     * Sets the {@link AppsStorageStatsManager} which will be used to register the controller to the
     * Listener {@link AppsStorageStatsManager.Callback}.
     */
    public StorageApplicationActionButtonsPreferenceController setAppsStorageStatsManager(
            AppsStorageStatsManager appsStorageStatsManager) {
        mAppsStorageStatsManager = appsStorageStatsManager;
        return this;
    }

    /**
     * Sets the {@link LoaderManager} used to load app storage stats.
     */
    public StorageApplicationActionButtonsPreferenceController setLoaderManager(
            LoaderManager loaderManager) {
        mLoaderManager = loaderManager;
        return this;
    }

    @Override
    protected void checkInitialized() {
        if (mAppEntry == null || mPackageName == null || mAppsStorageStatsManager == null
                || mLoaderManager == null) {
            throw new IllegalStateException(
                    "AppEntry, PackageName, AppStorageStatsManager, and LoaderManager should be "
                            + "set before calling this function");
        }
    }

    @Override
    protected void onCreateInternal() {
        mAppsStorageStatsManager.registerListener(this);

        mClearStorageButton = getPreference().getButton(ActionButtons.BUTTON1);
        mClearCacheButton = getPreference().getButton(ActionButtons.BUTTON2);

        ConfirmationDialogFragment.resetListeners(
                (ConfirmationDialogFragment) getFragmentController().findDialogByTag(
                        CONFIRM_CLEAR_STORAGE_DIALOG_TAG),
                mConfirmClearStorageDialog,
                /* rejectListener= */ null,
                /* neutralListener= */ null);
        ConfirmationDialogFragment.resetListeners(
                (ConfirmationDialogFragment) getFragmentController().findDialogByTag(
                        CONFIRM_CANNOT_CLEAR_STORAGE_DIALOG_TAG),
                mConfirmCannotClearStorageDialog,
                /* rejectListener= */ null,
                /* neutralListener= */ null);

        mClearStorageButton
                .setText(R.string.storage_clear_user_data_text)
                .setIcon(R.drawable.ic_delete)
                .setOnClickListener(i -> handleClearDataClick())
                .setEnabled(false);
        mClearCacheButton
                .setText(R.string.storage_clear_cache_btn_text)
                .setIcon(R.drawable.ic_delete)
                .setOnClickListener(i -> handleClearCacheClick())
                .setEnabled(false);
    }

    @Override
    protected void onStartInternal() {
        mAppsControlDisallowedAdmin = RestrictedLockUtilsInternal.checkIfRestrictionEnforced(
                getContext(), UserManager.DISALLOW_APPS_CONTROL, mUserId);
        mAppsControlDisallowedBySystem = RestrictedLockUtilsInternal.hasBaseUserRestriction(
                getContext(), UserManager.DISALLOW_APPS_CONTROL, mUserId);
    }

    @Override
    protected void updateState(ActionButtonsPreference preference) {
        try {
            mInfo = mPackageManager.getApplicationInfo(mPackageName, 0);
        } catch (PackageManager.NameNotFoundException e) {
            LOG.e("Could not find package", e);
        }
        if (mInfo == null) {
            return;
        }
        mAppsStorageStatsManager.startLoading(mLoaderManager, mInfo, mUserId, mCacheCleared,
                mDataCleared);
    }

    @Override
    public void onDataLoaded(StorageStatsSource.AppStorageStats data, boolean cacheCleared,
            boolean dataCleared) {
        if (data == null || mAppsControlDisallowedBySystem) {
            mClearStorageButton.setEnabled(false);
            mClearCacheButton.setEnabled(false);
        } else {
            long cacheSize = data.getCacheBytes();
            long dataSize = data.getDataBytes() - cacheSize;

            mClearStorageButton.setEnabled(dataSize > 0 && !mDataCleared);
            mClearCacheButton.setEnabled(cacheSize > 0 && !mCacheCleared);
        }
    }

    @VisibleForTesting
    void setPackageManager(PackageManager packageManager) {
        mPackageManager = packageManager;
    }

    @VisibleForTesting
    void setAppsControlDisallowedAdmin(RestrictedLockUtils.EnforcedAdmin admin) {
        mAppsControlDisallowedAdmin = admin;
    }

    @VisibleForTesting
    void setAppsControlDisallowedBySystem(boolean disallowed) {
        mAppsControlDisallowedBySystem = disallowed;
    }

    private void handleClearCacheClick() {
        if (mAppsControlDisallowedAdmin != null && !mAppsControlDisallowedBySystem) {
            RestrictedLockUtils.sendShowAdminSupportDetailsIntent(
                    getContext(), mAppsControlDisallowedAdmin);
            return;
        }
        // Lazy initialization of observer.
        if (mClearCacheObserver == null) {
            mClearCacheObserver = new ClearCacheObserver();
        }
        mPackageManager.deleteApplicationCacheFiles(mPackageName, mClearCacheObserver);
    }

    private void handleClearDataClick() {
        if (mAppsControlDisallowedAdmin != null && !mAppsControlDisallowedBySystem) {
            RestrictedLockUtils.sendShowAdminSupportDetailsIntent(
                    getContext(), mAppsControlDisallowedAdmin);
        } else {
            Intent intent = new Intent(Intent.ACTION_DEFAULT);
            boolean isManageSpaceActivityAvailable = false;
            if (mAppEntry.info.manageSpaceActivityName != null) {
                intent.setClassName(mAppEntry.info.packageName,
                        mAppEntry.info.manageSpaceActivityName);
                isManageSpaceActivityAvailable = mPackageManager.resolveActivity(
                        intent, /* flags= */ 0) != null;
            }

            if (isManageSpaceActivityAvailable) {
                getFragmentController().startActivityForResult(intent,
                        REQUEST_MANAGE_SPACE, /* callback= */ null);
            } else {
                showClearDataDialog();
            }
        }
    }

    /*
     * Private method to initiate clearing user data when the user clicks the clear data
     * button for a system package
     */
    private void initiateClearUserData() {
        mClearStorageButton.setEnabled(false);
        // Invoke uninstall or clear user data based on sysPackage
        String packageName = mAppEntry.info.packageName;
        LOG.i("Clearing user data for package : " + packageName);
        if (mClearDataObserver == null) {
            mClearDataObserver = new ClearUserDataObserver();
        }
        ActivityManager am = (ActivityManager)
                getContext().getSystemService(Context.ACTIVITY_SERVICE);
        boolean res = am.clearApplicationUserData(packageName, mClearDataObserver);
        if (!res) {
            // Clearing data failed for some obscure reason. Just log error for now
            LOG.i("Couldn't clear application user data for package:" + packageName);
            showCannotClearDataDialog();
        }
    }

    /*
     * Private method to handle clear message notification from observer when
     * the async operation from PackageManager is complete
     */
    private void processClearMsg(Message msg) {
        int result = msg.arg1;
        String packageName = mAppEntry.info.packageName;
        if (result == OP_SUCCESSFUL) {
            LOG.i("Cleared user data for package : " + packageName);
            refreshUi();
        } else {
            mClearStorageButton.setEnabled(true);
        }
    }

    private void showClearDataDialog() {
        ConfirmationDialogFragment confirmClearStorageDialog =
                new ConfirmationDialogFragment.Builder(getContext())
                        .setTitle(R.string.storage_clear_user_data_text)
                        .setMessage(getContext().getString(R.string.storage_clear_data_dlg_text))
                        .setPositiveButton(R.string.okay, mConfirmClearStorageDialog)
                        .setNegativeButton(android.R.string.cancel, /* rejectListener= */ null)
                        .build();
        getFragmentController().showDialog(confirmClearStorageDialog,
                CONFIRM_CLEAR_STORAGE_DIALOG_TAG);
    }

    private void showCannotClearDataDialog() {
        ConfirmationDialogFragment dialogFragment =
                new ConfirmationDialogFragment.Builder(getContext())
                        .setTitle(R.string.storage_clear_data_dlg_title)
                        .setMessage(getContext().getString(R.string.storage_clear_failed_dlg_text))
                        .setPositiveButton(R.string.okay, mConfirmCannotClearStorageDialog)
                        .build();
        getFragmentController().showDialog(dialogFragment, CONFIRM_CANNOT_CLEAR_STORAGE_DIALOG_TAG);
    }

    private final Handler mHandler = new Handler() {
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_CLEAR_USER_DATA:
                    mDataCleared = true;
                    mCacheCleared = true;
                    processClearMsg(msg);
                    break;
                case MSG_CLEAR_CACHE:
                    mCacheCleared = true;
                    // Refresh info
                    refreshUi();
                    break;
            }
        }
    };

    class ClearCacheObserver extends IPackageDataObserver.Stub {
        public void onRemoveCompleted(final String packageName, final boolean succeeded) {
            Message msg = mHandler.obtainMessage(MSG_CLEAR_CACHE);
            msg.arg1 = succeeded ? OP_SUCCESSFUL : OP_FAILED;
            mHandler.sendMessage(msg);
        }
    }

    class ClearUserDataObserver extends IPackageDataObserver.Stub {
        public void onRemoveCompleted(final String packageName, final boolean succeeded) {
            Message msg = mHandler.obtainMessage(MSG_CLEAR_USER_DATA);
            msg.arg1 = succeeded ? OP_SUCCESSFUL : OP_FAILED;
            mHandler.sendMessage(msg);
        }
    }
}
