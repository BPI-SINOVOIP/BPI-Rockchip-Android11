/*
 * Copyright (C) 2019 The Android Open Source Project
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

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.UserHandle;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.XmlRes;
import androidx.loader.app.LoaderManager;

import com.android.car.settings.R;
import com.android.car.settings.common.Logger;
import com.android.car.settings.common.SettingsFragment;
import com.android.settingslib.applications.ApplicationsState;
import com.android.settingslib.applications.StorageStatsSource;

import java.util.Arrays;
import java.util.List;

/**
 * Fragment to display the applications storage information.
 */
public class AppStorageSettingsDetailsFragment extends SettingsFragment {
    private static final Logger LOG = new Logger(AppStorageSettingsDetailsFragment.class);

    public static final String EXTRA_PACKAGE_NAME = "extra_package_name";

    // Package information
    protected PackageManager mPackageManager;
    private String mPackageName;

    // Application state info
    private ApplicationsState.AppEntry mAppEntry;
    private ApplicationsState mAppState;
    private AppsStorageStatsManager mAppsStorageStatsManager;

    // User info
    private int mUserId;

    /** Creates an instance of this fragment, passing packageName as an argument. */
    public static AppStorageSettingsDetailsFragment getInstance(String packageName) {
        AppStorageSettingsDetailsFragment applicationDetailFragment =
                new AppStorageSettingsDetailsFragment();
        Bundle bundle = new Bundle();
        bundle.putString(EXTRA_PACKAGE_NAME, packageName);
        applicationDetailFragment.setArguments(bundle);
        return applicationDetailFragment;
    }

    @Override
    @XmlRes
    protected int getPreferenceScreenResId() {
        return R.xml.app_storage_settings_details_fragment;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mUserId = UserHandle.myUserId();
        mPackageName = getArguments().getString(EXTRA_PACKAGE_NAME);
        mAppState = ApplicationsState.getInstance(requireActivity().getApplication());
        mAppEntry = mAppState.getEntry(mPackageName, mUserId);
        StorageStatsSource storageStatsSource = new StorageStatsSource(context);
        StorageStatsSource.AppStorageStats stats = null;
        mPackageManager = context.getPackageManager();
        try {
            stats = storageStatsSource.getStatsForPackage(/* volumeUuid= */ null, mPackageName,
                    UserHandle.of(mUserId));
        } catch (Exception e) {
            // This may happen if the package was removed during our calculation.
            LOG.w("App unexpectedly not found", e);
        }
        mAppsStorageStatsManager = new AppsStorageStatsManager(context);
        use(StorageApplicationPreferenceController.class,
                R.string.pk_storage_application_details)
                .setAppEntry(mAppEntry)
                .setAppState(mAppState);
        use(StorageApplicationActionButtonsPreferenceController.class,
                R.string.pk_storage_application_action_buttons)
                .setAppEntry(mAppEntry)
                .setPackageName(mPackageName)
                .setAppsStorageStatsManager(mAppsStorageStatsManager)
                .setLoaderManager(LoaderManager.getInstance(this));

        List<? extends StorageSizeBasePreferenceController> preferenceControllers = Arrays.asList(
                use(StorageApplicationSizePreferenceController.class,
                        R.string.pk_storage_application_size),
                use(StorageApplicationTotalSizePreferenceController.class,
                        R.string.pk_storage_application_total_size),
                use(StorageApplicationUserDataPreferenceController.class,
                        R.string.pk_storage_application_data_size),
                use(StorageApplicationCacheSizePreferenceController.class,
                        R.string.pk_storage_application_cache_size)
        );

        for (StorageSizeBasePreferenceController pc : preferenceControllers) {
            pc.setAppsStorageStatsManager(mAppsStorageStatsManager);
            pc.setAppStorageStats(stats);
        }
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        enableRotaryScroll();
    }
}
