/*
 * Copyright (C) 2015 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.tv.settings.advance_settings;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.location.LocationManager;
import android.os.BatteryManager;
import android.os.Bundle;
import android.os.UserManager;
import android.provider.Settings;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceGroup;
import androidx.preference.PreferenceScreen;
import android.text.TextUtils;
import android.util.Log;

import com.android.internal.logging.nano.MetricsProto;
import com.android.settingslib.location.RecentLocationApps;
import com.android.tv.settings.R;
import com.android.tv.settings.SettingsPreferenceFragment;
import com.android.tv.settings.device.apps.AppManagementFragment;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

/**
 * The location settings screen in TV settings.
 */
public class ScreenResolutionFragment extends SettingsPreferenceFragment
        implements Preference.OnPreferenceChangeListener {

    private static final String TAG = "ScreenResolutionFragment";

    private static final String LOCATION_MODE_WIFI = "wifi";
    private static final String LOCATION_MODE_OFF = "off";

    private static final String KEY_LOCATION_MODE = "locationMode";


    private BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Received location mode change intent: " + intent);
            }

        }
    };

    public static ScreenResolutionFragment newInstance() {
        return new ScreenResolutionFragment();
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.screen_resolution_prefs, null);
    }

    // When selecting the location preference, LeanbackPreferenceFragment
    // creates an inner view with the selection options; that's when we want to
    // register our receiver, bacause from now on user can change the location
    // providers.
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // getActivity().registerReceiver(mReceiver, new
        // IntentFilter(LocationManager.MODE_CHANGED_ACTION));
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        // getActivity().unregisterReceiver(mReceiver);
    }

    private void addPreferencesSorted(List<Preference> prefs, PreferenceGroup container) {
        // If there's some items to display, sort the items and add them to the
        // container.
        prefs.sort(Comparator.comparing(lhs -> lhs.getTitle().toString()));
        for (Preference entry : prefs) {
            container.addPreference(entry);
        }
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        /*
         * if (TextUtils.equals(preference.getKey(), KEY_LOCATION_MODE)) { int mode =
         * Settings.Secure.LOCATION_MODE_OFF; if (TextUtils.equals((CharSequence)
         * newValue, LOCATION_MODE_WIFI)) { mode =
         * Settings.Secure.LOCATION_MODE_HIGH_ACCURACY; } else if
         * (TextUtils.equals((CharSequence) newValue, LOCATION_MODE_OFF)) { mode =
         * Settings.Secure.LOCATION_MODE_OFF; } else { Log.wtf(TAG,
         * "Tried to set unknown location mode!"); } }
         */
        return true;
    }

    @Override
    public int getMetricsCategory() {
        return MetricsProto.MetricsEvent.LOCATION;
    }
}
