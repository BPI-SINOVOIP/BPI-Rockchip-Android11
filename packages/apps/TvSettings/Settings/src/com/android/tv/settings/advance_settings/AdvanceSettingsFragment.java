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
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.location.LocationManager;
import android.os.BatteryManager;
import android.os.Bundle;
import android.os.UserManager;
import android.os.SystemProperties;

import android.provider.Settings;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceGroup;
import androidx.preference.PreferenceScreen;
import androidx.preference.SwitchPreference;
import android.text.TextUtils;
import android.util.Log;

import com.android.internal.logging.nano.MetricsProto;
import com.android.settingslib.location.RecentLocationApps;
import com.android.tv.settings.R;
import com.android.tv.settings.SettingsPreferenceFragment;
import com.android.tv.settings.device.apps.AppManagementFragment;
import com.android.tv.settings.display.DrmDisplaySetting;
import com.android.tv.settings.util.ReflectUtils;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import com.android.tv.settings.advance_settings.performance.PerformanceService;
import android.app.ActivityManager;
import android.app.ActivityManager.RunningServiceInfo;

/**
 * The location settings screen in TV settings.
 */
public class AdvanceSettingsFragment extends SettingsPreferenceFragment implements
        Preference.OnPreferenceChangeListener {

    private static final String TAG = "AdvanceSettingsFragment";

    private static final String SERVICE_NAME = "com.android.tv.settings.advance_settings.performance.PerformanceService";
    private static final String LOCATION_MODE_WIFI = "wifi";
    private static final String LOCATION_MODE_OFF = "off";

    private static final String KEY_LOCATION_MODE = "locationMode";
    private static final String KEY_POWER_KEY = "power_key";
    private static final String KEY_DEFAULT_LAUNCHER = "default_launcher";
    private static final String KEY_PERFORMANCE = "performance_float";
    private static final String DORMANCY = "0";
    private static final String POWER_OFF = "1";
    private static final String REBOOT = "2";
    private static final String PROPERTY_POWER_KEY = "persist.sys.power_key";

    private PreferenceCategory displayPC;
    private ListPreference powerKeyPref;
    private SwitchPreference mPerformancePref;

    private BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Received location mode change intent: " + intent);
            }
            
        }
    };

    public static AdvanceSettingsFragment newInstance() {
        return new AdvanceSettingsFragment();
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.advance_settings_prefs, null);

        powerKeyPref = (ListPreference) findPreference(KEY_POWER_KEY);
        initPowerKey();
        //Default Launcher
        Preference defaultLauncher = (Preference) findPreference(KEY_DEFAULT_LAUNCHER);
        defaultLauncher.setSummary(getCurrentLauncherString());

        mPerformancePref = (SwitchPreference) findPreference(KEY_PERFORMANCE);
        updatePerformancePref();
    }

    // When selecting the location preference, LeanbackPreferenceFragment
    // creates an inner view with the selection options; that's when we want to
    // register our receiver, bacause from now on user can change the location
    // providers.
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        //getActivity().registerReceiver(mReceiver, new IntentFilter(LocationManager.MODE_CHANGED_ACTION));
    }

    @Override
    public void onResume() {
        super.onResume();

        Preference defaultLauncher = (Preference) findPreference(KEY_DEFAULT_LAUNCHER);
        defaultLauncher.setSummary(getCurrentLauncherString());
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        //getActivity().unregisterReceiver(mReceiver);
    }

    private void addPreferencesSorted(List<Preference> prefs, PreferenceGroup container) {
        // If there's some items to display, sort the items and add them to the container.
        prefs.sort(Comparator.comparing(lhs -> lhs.getTitle().toString()));
        for (Preference entry : prefs) {
            container.addPreference(entry);
        }
    }
    private void updatePerformancePref(){
       if(isServiceRunning()){
           mPerformancePref.setChecked(true);
       } else {
           mPerformancePref.setChecked(false);

       }
    }

    private boolean isServiceRunning() {
          ActivityManager manager = (ActivityManager) getContext().getSystemService(Context.ACTIVITY_SERVICE);
          for (RunningServiceInfo service : manager.getRunningServices(Integer.MAX_VALUE)) {
                  if (SERVICE_NAME.equals(service.service.getClassName())) {
                        return true;
                  }
          }
          return false;
    }

     @Override
    public boolean onPreferenceTreeClick(Preference preference) {
          if (preference == mPerformancePref){
             Log.d("ROCKCHIP","performance float click!");
             Intent intent = new Intent(getContext(), PerformanceService.class);
             if (mPerformancePref.isChecked()) {
               getContext().startService(intent);
             } else {
               if (isServiceRunning()) {
                    getContext().stopService(intent);
               }
             }
             return true;
          }
          return super.onPreferenceTreeClick(preference);
    }
    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        /*if (TextUtils.equals(preference.getKey(), KEY_LOCATION_MODE)) {
            int mode = Settings.Secure.LOCATION_MODE_OFF;
            if (TextUtils.equals((CharSequence) newValue, LOCATION_MODE_WIFI)) {
                mode = Settings.Secure.LOCATION_MODE_HIGH_ACCURACY;
            } else if (TextUtils.equals((CharSequence) newValue, LOCATION_MODE_OFF)) {
                mode = Settings.Secure.LOCATION_MODE_OFF;
            } else {
                Log.wtf(TAG, "Tried to set unknown location mode!");
            }
        }*/
        Log.i("ROCKCHIP", "newValue = " + newValue);
        if (TextUtils.equals(preference.getKey(), KEY_POWER_KEY)) {
            String powerKey = (String) newValue;
            switch (powerKey) {
            case POWER_OFF:
                SystemProperties.set(PROPERTY_POWER_KEY, POWER_OFF);
                break;
            case REBOOT:
                SystemProperties.set(PROPERTY_POWER_KEY, REBOOT);
                break;
            default:
                SystemProperties.set(PROPERTY_POWER_KEY, DORMANCY);
                break;
            }
        }
        return true;
    }
	
	
    @Override
    public int getMetricsCategory() {
        return MetricsProto.MetricsEvent.LOCATION;
    }

    private void initPowerKey() {
        String powerKey = SystemProperties.get(PROPERTY_POWER_KEY, "0");
        Log.i("ROCKCHIP", "powerKey = " + powerKey);
        switch (powerKey) {
        case POWER_OFF:
            powerKeyPref.setValue(POWER_OFF);
            break;
        case REBOOT:
            powerKeyPref.setValue(REBOOT);
            break;
        default:
            powerKeyPref.setValue(DORMANCY);
            break;
        }
        powerKeyPref.setOnPreferenceChangeListener(this);
    }

    private String getCurrentLauncherString() {
        PackageManager pm = getActivity().getPackageManager();
        Intent intent = new Intent(Intent.ACTION_MAIN, null);
        intent.addCategory(Intent.CATEGORY_HOME);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        ResolveInfo currentLauncher =  pm.resolveActivity(intent, 0);
        if (currentLauncher != null && !TextUtils.isEmpty(currentLauncher.activityInfo.packageName) && !TextUtils.equals(currentLauncher.activityInfo.packageName, "android")) {
            return currentLauncher.loadLabel(pm).toString();
        } else {
            return "android";
        }
    }
}
