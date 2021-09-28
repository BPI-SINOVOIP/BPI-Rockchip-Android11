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

package com.android.tv.twopanelsettingsoverlay;

import android.app.Fragment;

import androidx.annotation.NonNull;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragment;
import androidx.preference.PreferenceScreen;

import com.android.tv.settings.system.LeanbackPickerDialogFragment;
import com.android.tv.settings.system.LeanbackPickerDialogPreference;
import com.android.tv.twopanelsettings.TwoPanelSettingsFragment;

/**
 * Base class for settings fragments. Handles launching fragments and dialogs in a reasonably
 * generic way. Subclasses should only override onPreferenceStartInitialScreen.
 */

public abstract class TwoPanelBaseSettingsFragment extends TwoPanelSettingsFragment {

    @Override
    public final boolean onPreferenceStartScreen(PreferenceFragment caller, PreferenceScreen pref) {
        return false;
    }

    @Override
    public Fragment onCreatePreviewFragment(@NonNull Fragment caller, Preference pref) {
        Fragment f;
        if (pref instanceof LeanbackPickerDialogPreference) {
            final LeanbackPickerDialogPreference dialogPreference = (LeanbackPickerDialogPreference)
                    pref;
            f = dialogPreference.getType().equals("date")
                    ? LeanbackPickerDialogFragment.newDatePickerInstance(pref.getKey())
                    : LeanbackPickerDialogFragment.newTimePickerInstance(pref.getKey());
            f.setTargetFragment(caller, 0);
            return f;
        } else {
            return super.onCreatePreviewFragment(caller, pref);
        }
    }
}
