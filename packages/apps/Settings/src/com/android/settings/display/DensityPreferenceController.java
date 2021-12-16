/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
package com.android.settings.display;

import static android.provider.Settings.System.SCREEN_OFF_TIMEOUT;

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.Settings;
import android.util.Log;
import android.view.IWindowManager;

import androidx.preference.Preference;

import com.android.settings.R;
import com.android.settings.core.PreferenceControllerMixin;
import com.android.settingslib.RestrictedLockUtils;
import com.android.settingslib.RestrictedLockUtils.EnforcedAdmin;
import com.android.settingslib.RestrictedLockUtilsInternal;
import com.android.settingslib.core.AbstractPreferenceController;

public class DensityPreferenceController extends AbstractPreferenceController implements
        PreferenceControllerMixin, Preference.OnPreferenceChangeListener {

    private static final String TAG = "DensityPrefContr";

    private static final int DEFAULT_DISPLAY = 0;
    private static final int DEFAULT_DENSITY = 280;

    private IWindowManager mWindowManager;
    private final String mScreenDensityKey;

    public DensityPreferenceController(Context context, String key) {
        super(context);
        mWindowManager = IWindowManager.Stub.asInterface(
                ServiceManager.getService(Context.WINDOW_SERVICE));
        mScreenDensityKey = key;
    }

    @Override
    public boolean isAvailable() {
        return true;
    }

    @Override
    public String getPreferenceKey() {
        return mScreenDensityKey;
    }

    @Override
    public void updateState(Preference preference) {
        final DensityListPreference densityListPreference = (DensityListPreference) preference;

        int currentDensity = getDensity();
        Log.d(TAG, "updateState: current density is " + currentDensity);
        densityListPreference.setValue(String.valueOf(currentDensity));

        updateDensityPreferenceDescription(densityListPreference,
                Long.parseLong(densityListPreference.getValue()));
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        try {
            int value = Integer.parseInt((String) newValue);
            Log.d(TAG, "onPreferenceChange: set density is " + value);
            setDensity(value);
            updateDensityPreferenceDescription((DensityListPreference) preference, value);
        } catch (NumberFormatException e) {
            Log.e(TAG, "could not persist screen density setting", e);
        }
        return true;
    }

    public static CharSequence getDensityDescription(
            long currentDensity, CharSequence[] entries, CharSequence[] values) {
        if (currentDensity < 0 || entries == null || values == null
                || values.length != entries.length) {
            return null;
        }

        for (int i = 0; i < values.length; i++) {
            long density = Long.parseLong(values[i].toString());
            if (currentDensity == density) {
                return entries[i];
            }
        }
        return null;
    }

    private void updateDensityPreferenceDescription(DensityListPreference preference,
            long currentDensity) {
        final CharSequence[] entries = preference.getEntries();
        final CharSequence[] values = preference.getEntryValues();
        final String summary;
        if (preference.isDisabledByAdmin()) {
            summary = mContext.getString(com.android.settings.R.string.disabled_by_policy_title);
        } else {
            if (0 == currentDensity || Integer.MAX_VALUE == currentDensity) {
                summary = preference.getContext().getResources().getStringArray(R.array.screen_density_entries)[4];
            } else {
                final CharSequence densityDescription = getDensityDescription(
                        currentDensity, entries, values);
                if (densityDescription == null) {
                    summary = "";
                } else {
                    summary = mContext.getString(R.string.screen_density_summary, densityDescription);
                }
            }
        }
        preference.setSummary(summary);
    }

    private void setDensity(int density) {
        Log.d(TAG, "setDensity, density = " + density);

        try {
            mWindowManager.setForcedDisplayDensityForUser(DEFAULT_DISPLAY, density, UserHandle.USER_CURRENT);
        } catch (RemoteException e) {
            Log.e(TAG, "setDensity failed, density = " + density);
        }
    }
	
	private int getDensity() {
        try {
            int initialDensity = mWindowManager.getInitialDisplayDensity(DEFAULT_DISPLAY);
		    int baseDensity = mWindowManager.getBaseDisplayDensity(DEFAULT_DISPLAY);
		    Log.d(TAG, "getDensity, initialDensity = " + initialDensity + " baseDensity = " + baseDensity);

            if (initialDensity != baseDensity) {
                return baseDensity;
            } else {
                return initialDensity;
            }
        } catch (RemoteException e) {
            Log.e(TAG, "getDensity failed");
        }

        return DEFAULT_DENSITY;
	}
}
