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

package com.android.car.settings.system;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.provider.Settings;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceController;

/** Controls the visibility of the regulatory info preference. */
public class RegulatoryInfoPreferenceController extends PreferenceController<Preference> {

    private static final Intent INTENT_PROBE = new Intent(Settings.ACTION_SHOW_REGULATORY_INFO);

    private PackageManager mPm;

    public RegulatoryInfoPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
        mPm = context.getPackageManager();
    }

    @Override
    protected Class<Preference> getPreferenceType() {
        return Preference.class;
    }

    @Override
    protected int getAvailabilityStatus() {
        return mPm.queryIntentActivities(INTENT_PROBE, /* flags= */ 0).isEmpty()
                ? UNSUPPORTED_ON_DEVICE : AVAILABLE;
    }

    @VisibleForTesting
    void setPackageManager(PackageManager pm) {
        mPm = pm;
    }
}
