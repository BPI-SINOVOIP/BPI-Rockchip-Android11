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

package com.android.car.settings.applications.assist;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.net.Uri;
import android.provider.Settings;

import androidx.preference.TwoStatePreference;

import com.android.car.settings.common.FragmentController;

import java.util.Collections;
import java.util.List;

/** Toggles the assistant's ability to use the text on the screen for context. */
public class TextContextPreferenceController extends AssistConfigBasePreferenceController {

    public TextContextPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
    }

    @Override
    protected void updateState(TwoStatePreference preference) {
        preference.setChecked(isAssistContextEnabled());
    }

    @Override
    protected boolean handlePreferenceChanged(TwoStatePreference preference, Object newValue) {
        Settings.Secure.putInt(getContext().getContentResolver(),
                Settings.Secure.ASSIST_STRUCTURE_ENABLED, (boolean) newValue ? 1 : 0);
        return true;
    }

    @Override
    protected List<Uri> getSettingUris() {
        return Collections.singletonList(
                Settings.Secure.getUriFor(Settings.Secure.ASSIST_STRUCTURE_ENABLED));
    }

    private boolean isAssistContextEnabled() {
        return Settings.Secure.getInt(getContext().getContentResolver(),
                Settings.Secure.ASSIST_STRUCTURE_ENABLED, 1) != 0;
    }
}
