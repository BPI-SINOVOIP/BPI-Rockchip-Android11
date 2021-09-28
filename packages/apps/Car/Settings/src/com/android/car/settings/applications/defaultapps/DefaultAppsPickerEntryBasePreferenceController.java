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

package com.android.car.settings.applications.defaultapps;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.Nullable;

import com.android.car.settings.common.FragmentController;
import com.android.car.ui.preference.CarUiTwoActionIconPreference;
import com.android.settingslib.applications.DefaultAppInfo;

/**
 * Base preference which handles the logic to display the currently selected default app as well as
 * an option to navigate to the settings of the selected default app.
 */
public abstract class DefaultAppsPickerEntryBasePreferenceController extends
        DefaultAppEntryBasePreferenceController<CarUiTwoActionIconPreference> {

    public DefaultAppsPickerEntryBasePreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
    }

    @Override
    protected Class<CarUiTwoActionIconPreference> getPreferenceType() {
        return CarUiTwoActionIconPreference.class;
    }

    @Override
    protected void updateState(CarUiTwoActionIconPreference preference) {
        super.updateState(preference);

        // If activity does not exist, return. Otherwise allow intenting to the activity.
        Intent intent = getSettingIntent(getCurrentDefaultAppInfo());
        if (intent == null || intent.resolveActivityInfo(
                getContext().getPackageManager(), intent.getFlags()) == null) {
            preference.setSecondaryActionVisible(false);
            return;
        }

        // Use startActivityForResult because some apps need to check the identity of the caller.
        preference.setOnSecondaryActionClickListener(() -> {
            getContext().startActivityForResult(
                    getContext().getBasePackageName(),
                    intent,
                    /* requestCode= */ 0,
                    /* options= */ null);
        });
        preference.setSecondaryActionVisible(true);
    }

    /**
     * Returns an optional intent that will be launched when clicking the secondary action icon.
     */
    @Nullable
    protected Intent getSettingIntent(@Nullable DefaultAppInfo info) {
        return null;
    }
}
