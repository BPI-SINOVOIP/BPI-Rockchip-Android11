/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.settings.wifi;

import android.content.Context;
import android.provider.Settings;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import android.util.Log;
import android.widget.Toast;

import com.android.settings.R;
import com.android.settings.Utils;
import com.android.settings.core.PreferenceControllerMixin;
import com.android.settingslib.core.AbstractPreferenceController;

import static com.android.internal.os.MemoryPowerCalculator.TAG;

public class WifiSleepPolicyPreferenceController extends AbstractPreferenceController
	implements PreferenceControllerMixin, Preference.OnPreferenceChangeListener {

    private static final String KEY_SLEEP_POLICY = "sleep_policy";

    public WifiSleepPolicyPreferenceController(Context context) {
        super(context);
    }

    @Override
    public boolean isAvailable() {
        return true;
    }

    @Override
    public String getPreferenceKey() {
        return KEY_SLEEP_POLICY;
    }

    @Override
    public void updateState(Preference preference) {
        ListPreference sleepPolicyPref = (ListPreference) preference;
        if (sleepPolicyPref != null) {
            if (Utils.isWifiOnly(mContext)) {
                sleepPolicyPref.setEntries(R.array.wifi_sleep_policy_entries_wifi_only);
            }
            int value = Settings.Global.getInt(mContext.getContentResolver(),
                    Settings.Global.WIFI_SLEEP_POLICY,
                    Settings.Global.WIFI_SLEEP_POLICY_NEVER);
            String stringValue = String.valueOf(value);
            sleepPolicyPref.setValue(stringValue);
            updateSleepPolicySummary(sleepPolicyPref, stringValue);
        }
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        try {
            String stringValue = (String) newValue;
            Settings.Global.putInt(mContext.getContentResolver(), Settings.Global.WIFI_SLEEP_POLICY,
                    Integer.parseInt(stringValue));
            updateSleepPolicySummary(preference, stringValue);
        } catch (NumberFormatException e) {
            Toast.makeText(mContext, R.string.wifi_setting_sleep_policy_error,
                    Toast.LENGTH_SHORT).show();
            return false;
        }
        return true;
    }

    private void updateSleepPolicySummary(Preference sleepPolicyPref, String value) {
        if (value != null) {
            String[] values = mContext.getResources().getStringArray(R.array
                    .wifi_sleep_policy_values);
            final int summaryArrayResId = Utils.isWifiOnly(mContext)
                    ? R.array.wifi_sleep_policy_entries_wifi_only
                    : R.array.wifi_sleep_policy_entries;
            String[] summaries = mContext.getResources().getStringArray(summaryArrayResId);
            for (int i = 0; i < values.length; i++) {
                if (value.equals(values[i])) {
                    if (i < summaries.length) {
                        sleepPolicyPref.setSummary(summaries[i]);
                        return;
                    }
                }
            }
        }

        sleepPolicyPref.setSummary("");
        Log.e(TAG, "Invalid sleep policy value: " + value);
    }

}
