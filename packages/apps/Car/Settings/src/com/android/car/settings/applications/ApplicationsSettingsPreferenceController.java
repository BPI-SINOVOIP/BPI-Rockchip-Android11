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
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.provider.Settings;

import androidx.preference.Preference;
import androidx.preference.PreferenceGroup;

import com.android.car.settings.common.CarSettingActivities;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceController;
import com.android.car.ui.preference.CarUiPreference;
import com.android.settingslib.applications.ApplicationsState;

import java.util.ArrayList;

/** Business logic which populates the applications in this setting. */
public class ApplicationsSettingsPreferenceController extends
        PreferenceController<PreferenceGroup> implements
        ApplicationListItemManager.AppListItemListener {

    public ApplicationsSettingsPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
    }

    @Override
    protected Class<PreferenceGroup> getPreferenceType() {
        return PreferenceGroup.class;
    }

    @Override
    public void onDataLoaded(ArrayList<ApplicationsState.AppEntry> apps) {
        getPreference().removeAll();
        for (ApplicationsState.AppEntry appEntry : apps) {
            getPreference().addPreference(
                    createPreference(appEntry.label, appEntry.sizeStr, appEntry.icon,
                            appEntry.info.packageName));
        }
    }

    private Preference createPreference(String title, String summary, Drawable icon,
            String packageName) {
        CarUiPreference preference = new CarUiPreference(getContext());
        preference.setTitle(title);
        preference.setSummary(summary);
        preference.setIcon(icon);
        preference.setKey(packageName);
        preference.setOnPreferenceClickListener(p -> {
            Intent intent = new Intent(getContext(),
                    CarSettingActivities.ApplicationsDetailsActivity.class);
            intent.putExtra(Settings.EXTRA_APP_PACKAGE, packageName);
            getContext().startActivity(intent);
            return true;
        });
        return preference;
    }
}
