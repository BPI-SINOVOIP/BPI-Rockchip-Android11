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
import android.content.pm.PackageInfo;

import androidx.preference.Preference;

import com.android.car.settings.R;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceController;

/** Business logic for the Version field in the application details page. */
public class VersionPreferenceController extends PreferenceController<Preference> {

    private PackageInfo mPackageInfo;

    public VersionPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
    }

    @Override
    protected Class<Preference> getPreferenceType() {
        return Preference.class;
    }

    /** Set the package info which is used to get the version name. */
    public void setPackageInfo(PackageInfo packageInfo) {
        mPackageInfo = packageInfo;
    }

    @Override
    protected void checkInitialized() {
        if (mPackageInfo == null) {
            throw new IllegalStateException(
                    "PackageInfo should be set before calling this function");
        }
    }

    @Override
    protected void updateState(Preference preference) {
        preference.setTitle(getContext().getString(
                R.string.application_version_label, mPackageInfo.versionName));
    }

    @Override
    protected int getAvailabilityStatus() {
        return AVAILABLE_FOR_VIEWING;
    }
}
