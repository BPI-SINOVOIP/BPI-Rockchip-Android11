/*
 * Copyright (C) 2018 The Android Open Source Project
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

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.UserHandle;

import androidx.annotation.XmlRes;

import com.android.car.settings.R;
import com.android.car.settings.common.Logger;
import com.android.car.settings.common.SettingsFragment;
import com.android.settingslib.applications.ApplicationsState;

/**
 * Shows details about an application.
 */
public class ApplicationDetailsFragment extends SettingsFragment {
    private static final Logger LOG = new Logger(ApplicationDetailsFragment.class);
    public static final String EXTRA_PACKAGE_NAME = "extra_package_name";

    private PackageManager mPm;

    private String mPackageName;
    private PackageInfo mPackageInfo;
    private ApplicationsState mAppState;
    private ApplicationsState.AppEntry mAppEntry;

    /** Creates an instance of this fragment, passing packageName as an argument. */
    public static ApplicationDetailsFragment getInstance(String packageName) {
        ApplicationDetailsFragment applicationDetailFragment = new ApplicationDetailsFragment();
        Bundle bundle = new Bundle();
        bundle.putString(EXTRA_PACKAGE_NAME, packageName);
        applicationDetailFragment.setArguments(bundle);
        return applicationDetailFragment;
    }

    @Override
    @XmlRes
    protected int getPreferenceScreenResId() {
        return R.xml.application_details_fragment;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mPm = context.getPackageManager();

        // These should be loaded before onCreate() so that the controller operates as expected.
        mPackageName = getArguments().getString(EXTRA_PACKAGE_NAME);

        mAppState = ApplicationsState.getInstance(requireActivity().getApplication());

        retrieveAppEntry();

        use(ApplicationPreferenceController.class,
                R.string.pk_application_details_app)
                .setAppEntry(mAppEntry).setAppState(mAppState);
        use(ApplicationActionButtonsPreferenceController.class,
                R.string.pk_application_details_action_buttons)
                .setAppEntry(mAppEntry).setAppState(mAppState).setPackageName(mPackageName);
        use(NotificationsPreferenceController.class,
                R.string.pk_application_details_notifications).setPackageInfo(mPackageInfo);
        use(PermissionsPreferenceController.class,
                R.string.pk_application_details_permissions).setPackageName(mPackageName);
        use(StoragePreferenceController.class,
                R.string.pk_application_details_storage)
                .setAppEntry(mAppEntry).setPackageName(mPackageName);
        use(VersionPreferenceController.class,
                R.string.pk_application_details_version).setPackageInfo(mPackageInfo);
    }

    private void retrieveAppEntry() {
        mAppEntry = mAppState.getEntry(mPackageName, UserHandle.myUserId());
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
}
