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

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;

import androidx.preference.Preference;

import com.android.car.settings.R;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceController;
import com.android.car.settings.storage.AppStorageSettingsDetailsFragment;
import com.android.settingslib.applications.ApplicationsState;

/** Business logic for the storage entry in the application details settings. */
public class StoragePreferenceController extends PreferenceController<Preference> {

    private String mPackageName;
    private ApplicationsState.AppEntry mAppEntry;

    public StoragePreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
    }

    @Override
    protected Class<Preference> getPreferenceType() {
        return Preference.class;
    }

    /** Sets the {@link ApplicationsState.AppEntry} which is used to load the app size. */
    public StoragePreferenceController setAppEntry(ApplicationsState.AppEntry appEntry) {
        mAppEntry = appEntry;
        return this;
    }
    /**
     * Set the packageName, which is used to open the AppStorageSettingsDetailsFragment
     */
    public StoragePreferenceController setPackageName(String packageName) {
        mPackageName = packageName;
        return this;
    }

    @Override
    protected void checkInitialized() {
        if (mAppEntry == null || mPackageName == null) {
            throw new IllegalStateException(
                    "AppEntry and PackageName should be set before calling this function");
        }
    }

    @Override
    protected void updateState(Preference preference) {
        preference.setSummary(
                getContext().getString(R.string.storage_type_internal, mAppEntry.sizeStr));
    }

    @Override
    protected boolean handlePreferenceClicked(Preference preference) {
        getFragmentController().launchFragment(
                AppStorageSettingsDetailsFragment.getInstance(mPackageName));
        return true;
    }
}
