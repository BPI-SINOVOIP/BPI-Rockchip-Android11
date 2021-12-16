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

public class RotationPreferenceController extends AbstractPreferenceController implements
        PreferenceControllerMixin, Preference.OnPreferenceChangeListener {

    private static final String TAG = "RotationPrefContr";

    private static final String KEY_ROTATION = "screen_rotation";
    private static final int ROTATION_FREE = -1;

    private IWindowManager mWindowManager;

    public RotationPreferenceController(Context context) {
        super(context);
        mWindowManager = IWindowManager.Stub.asInterface(
                ServiceManager.getService(Context.WINDOW_SERVICE));
    }

    @Override
    public boolean isAvailable() {
        return true;
    }

    @Override
    public String getPreferenceKey() {
        return KEY_ROTATION;
    }

    @Override
    public void updateState(Preference preference) {
        final RotationListPreference rotationListPreference = (RotationListPreference) preference;

        int currentRotation = getRotation();
        Log.d(TAG, "updateState: current rotation is " + currentRotation);
        rotationListPreference.setValue(String.valueOf(currentRotation));

        updateRotationPreferenceDescription(rotationListPreference,
                Long.parseLong(rotationListPreference.getValue()));
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        try {
            int value = Integer.parseInt((String) newValue);
            Log.d(TAG, "onPreferenceChange: set rotation is " + value);
            setRotation(value);
            updateRotationPreferenceDescription((RotationListPreference) preference, value);
        } catch (NumberFormatException e) {
            Log.e(TAG, "could not persist screen rotation setting", e);
        }
        return true;
    }

    public static CharSequence getRotationDescription(
            long currentRotation, CharSequence[] entries, CharSequence[] values) {
        if (currentRotation < 0 || entries == null || values == null
                || values.length != entries.length) {
            return null;
        }

        for (int i = 0; i < values.length; i++) {
            long rotation = Long.parseLong(values[i].toString());
            if (currentRotation == rotation) {
                return entries[i];
            }
        }
        return null;
    }

    private void updateRotationPreferenceDescription(RotationListPreference preference,
            long currentRotation) {
        final CharSequence[] entries = preference.getEntries();
        final CharSequence[] values = preference.getEntryValues();
        final String summary;
        if (preference.isDisabledByAdmin()) {
            summary = mContext.getString(com.android.settings.R.string.disabled_by_policy_title);
        } else {
            if (-1 == currentRotation || Integer.MAX_VALUE == currentRotation) {
                summary = preference.getContext().getResources().getStringArray(R.array.screen_rotation_entries)[0];
            } else {
                final CharSequence rotationDescription = getRotationDescription(
                        currentRotation, entries, values);
                if (rotationDescription == null) {
                    summary = "";
                } else {
                    summary = mContext.getString(R.string.screen_rotation_summary, rotationDescription);
                }
            }
        }
        preference.setSummary(summary);
    }

    private void setRotation(int rotation) {
        Log.d(TAG, "setRotation, rotation = " + rotation);

        try {
            if (rotation == ROTATION_FREE) {
                mWindowManager.thawRotation();
            } else {
                mWindowManager.freezeRotation(rotation);
            }
        } catch (RemoteException e) {
            Log.e(TAG, "setRotation failed, rotation = " + rotation);
        }
    }
	
    private int getRotation() {
        int rotation = 1;

        try {
            if (mWindowManager.isRotationFrozen()) {
                rotation = mWindowManager.getDefaultDisplayRotation();
            } else {
                rotation = ROTATION_FREE;
            }

            Log.d(TAG, "getRotation, rotation = " + rotation);

        } catch (RemoteException e) {
            Log.e(TAG, "getRotation failed");
        }

        return rotation;
    }
}
