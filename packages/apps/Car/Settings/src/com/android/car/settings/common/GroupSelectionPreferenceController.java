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

package com.android.car.settings.common;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.preference.PreferenceGroup;
import androidx.preference.TwoStatePreference;

import java.util.List;

/**
 * A preference controller which ensure that only one of many {@link TwoStatePreference}
 * instances can be checked at any given point in time.
 *
 * <p>The behavior of this class is not well defined if multiple preferences have the same key.
 */
public abstract class GroupSelectionPreferenceController extends
        PreferenceController<PreferenceGroup> {

    public GroupSelectionPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
    }

    @Override
    protected Class<PreferenceGroup> getPreferenceType() {
        return PreferenceGroup.class;
    }

    @Override
    protected final void updateState(PreferenceGroup preferenceGroup) {
        List<TwoStatePreference> items = getGroupPreferences();
        preferenceGroup.removeAll();
        for (TwoStatePreference preference : items) {

            preference.setOnPreferenceClickListener(pref -> {
                if (handleGroupItemSelected(preference)) {
                    notifyCheckedKeyChanged();
                }
                return true;
            });
            // All changes to the checked state will happen programmatically if using this
            // class.
            preference.setOnPreferenceChangeListener((pref, o) -> false);
            preference.setPersistent(false);
            preferenceGroup.addPreference(preference);
        }

        notifyCheckedKeyChanged();
    }

    /** Returns the value of the currently checked key. */
    protected abstract String getCurrentCheckedKey();

    /** Returns the list of preferences that should be available to select in this group. */
    @NonNull
    protected abstract List<TwoStatePreference> getGroupPreferences();

    /**
     * Intercepts a click action on an item in the group. Refreshes the UI immediately if returns
     * {@code true}. Otherwise {@link #notifyCheckedKeyChanged()} can be used when the selected key
     * has changed.
     */
    protected boolean handleGroupItemSelected(TwoStatePreference preference) {
        return true;
    }

    /** Refreshes the checked state of the preferences in the {@link PreferenceGroup}. */
    protected void notifyCheckedKeyChanged() {
        for (int i = 0; i < getPreference().getPreferenceCount(); i++) {
            TwoStatePreference preference = (TwoStatePreference) getPreference().getPreference(i);
            preference.setChecked(TextUtils.equals(preference.getKey(), getCurrentCheckedKey()));
        }
    }
}
