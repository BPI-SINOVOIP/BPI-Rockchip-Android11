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

package com.android.tv.settings.device.display.daydream;

import static android.provider.Settings.Secure.ATTENTIVE_TIMEOUT;
import static android.provider.Settings.Secure.SLEEP_TIMEOUT;

import static com.android.tv.settings.util.InstrumentationUtils.logEntrySelected;

import android.app.tvsettings.TvSettingsEnums;
import android.os.Bundle;
import android.provider.Settings;
import android.text.format.DateUtils;

import androidx.annotation.Keep;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.SwitchPreference;

import com.android.internal.logging.nano.MetricsProto;
import com.android.tv.settings.R;
import com.android.tv.settings.SettingsPreferenceFragment;

/**
 * The energy saver screen in TV settings.
 */
@Keep
public class EnergySaverFragment extends SettingsPreferenceFragment implements
        Preference.OnPreferenceChangeListener {
    private static final String TAG = "EnergySaverFragment";
    private static final String KEY_SLEEP_TIME = "sleepTime";
    private static final String KEY_ALLOW_TURN_SCREEN_OFF = "allowTurnScreenOff";
    private static final int DEFAULT_SLEEP_TIME_MS = (int) (24 * DateUtils.HOUR_IN_MILLIS);
    private SwitchPreference mAllowTurnScreenOffWithWakeLockPref;
    private ListPreference mSleepTimePref;

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        setPreferencesFromResource(R.xml.energy_saver, null);
        mAllowTurnScreenOffWithWakeLockPref = findPreference(KEY_ALLOW_TURN_SCREEN_OFF);
        mAllowTurnScreenOffWithWakeLockPref.setOnPreferenceChangeListener(this);
        mAllowTurnScreenOffWithWakeLockPref.setVisible(showStandbyTimeout());
        updateAllowTurnScreenOffWithWakeLockPref();
        mSleepTimePref = findPreference(KEY_SLEEP_TIME);
        if (allowTurnOffWithWakeLock()) {
            int validatedAttentiveSleepTime = getValidatedTimeout(getAttentiveSleepTime());
            mSleepTimePref.setValue(String.valueOf(validatedAttentiveSleepTime));
            if (getAttentiveSleepTime() != validatedAttentiveSleepTime) {
                setAttentiveSleepTime(validatedAttentiveSleepTime);
            }
        } else {
            int validatedSleepTime = getValidatedTimeout(getSleepTime());
            mSleepTimePref.setValue(String.valueOf(validatedSleepTime));
            if (getSleepTime() != validatedSleepTime) {
                setSleepTime(validatedSleepTime);
            }
        }
        mSleepTimePref.setOnPreferenceChangeListener(this);
        mSleepTimePref.setOnPreferenceClickListener(
                preference -> {
                    logEntrySelected(TvSettingsEnums.SYSTEM_ENERGYSAVER_START_DELAY);
                    return false;
                });
    }

    private boolean showStandbyTimeout() {
        return getResources().getBoolean(R.bool.config_show_standby_timeout);
    }

    private boolean allowTurnOffWithWakeLock() {
        return showStandbyTimeout() && mAllowTurnScreenOffWithWakeLockPref.isChecked();
    }

    private void updateAllowTurnScreenOffWithWakeLockPref() {
        if (!mAllowTurnScreenOffWithWakeLockPref.isVisible()) {
            return;
        }
        if (getSleepTime() == -1) {
            mAllowTurnScreenOffWithWakeLockPref.setChecked(false);
            mAllowTurnScreenOffWithWakeLockPref.setEnabled(false);
        } else if (getAttentiveSleepTime() == -1) {
            mAllowTurnScreenOffWithWakeLockPref.setChecked(false);
            mAllowTurnScreenOffWithWakeLockPref.setEnabled(true);
        } else {
            mAllowTurnScreenOffWithWakeLockPref.setChecked(true);
            mAllowTurnScreenOffWithWakeLockPref.setEnabled(true);
        }
    }

    @Override
    public int getMetricsCategory() {
        return MetricsProto.MetricsEvent.DREAM;
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        switch (preference.getKey()) {
            case KEY_SLEEP_TIME:
                final int newSleepTime = Integer.parseInt((String) newValue);
                if (getSleepTimeEntryId(newSleepTime) != -1) {
                    logEntrySelected(getSleepTimeEntryId(newSleepTime));
                }
                updateTimeOut(allowTurnOffWithWakeLock(), newSleepTime);
                break;
            case KEY_ALLOW_TURN_SCREEN_OFF:
                updateTimeOut((boolean) newValue, Integer.parseInt(mSleepTimePref.getValue()));
                break;
        }
        return true;
    }

    private void updateTimeOut(boolean allowTurnScreenOffWithWakeLock, int value) {
        if (allowTurnScreenOffWithWakeLock) {
            setSleepTime(value);
            if (showStandbyTimeout()) {
                setAttentiveSleepTime(value);
            }
        } else {
            setSleepTime(value);
            if (showStandbyTimeout()) {
                setAttentiveSleepTime(-1);
            }
        }
        updateAllowTurnScreenOffWithWakeLockPref();
    }

    private int getSleepTime() {
        return Settings.Secure.getInt(getActivity().getContentResolver(), SLEEP_TIMEOUT,
                DEFAULT_SLEEP_TIME_MS);
    }

    private int getAttentiveSleepTime() {
        return Settings.Secure.getInt(getActivity().getContentResolver(), ATTENTIVE_TIMEOUT,
                DEFAULT_SLEEP_TIME_MS);
    }

    private void setSleepTime(int ms) {
        Settings.Secure.putInt(getActivity().getContentResolver(), SLEEP_TIMEOUT, ms);
    }

    private void setAttentiveSleepTime(int ms) {
        Settings.Secure.putInt(getActivity().getContentResolver(), ATTENTIVE_TIMEOUT, ms);
    }

    // The SLEEP_TIMEOUT and ATTENTIVE_TIMEOUT could be defined in overlay by OEMs. We validate the
    // value to make sure that we select from the predefined options. If the value from overlay is
    // not one of the predefined options, we round it to the closest predefined value, except -1.
    private int getValidatedTimeout(int purposedTimeout) {
        int validatedTimeout = DEFAULT_SLEEP_TIME_MS;
        if (purposedTimeout < 0) {
            return -1;
        }
        String[] optionsString = getResources().getStringArray(R.array.screen_off_timeout_values);
        // Find the value from the predefined values that is closest to the proposed value except -1
        int diff = Integer.MAX_VALUE;
        for (String option : optionsString) {
            if (Integer.parseInt(option) != -1) {
                int currentDiff = Math.abs(purposedTimeout - Integer.parseInt(option));
                if (currentDiff < diff) {
                    diff = currentDiff;
                    validatedTimeout = Integer.parseInt(option);
                }
            }
        }
        return validatedTimeout;
    }

    // TODO(b/158783050): update logging for new options 4H, 8H, 24H.
    // Map @array/screen_off_timeout_entries to defined log enum
    private int getSleepTimeEntryId(int sleepTimeValue) {
        switch(sleepTimeValue) {
            case -1:
                return TvSettingsEnums.SYSTEM_ENERGYSAVER_START_DELAY_NEVER;
            case 900000:
                return TvSettingsEnums.SYSTEM_ENERGYSAVER_START_DELAY_15M;
            case 1800000:
                return TvSettingsEnums.SYSTEM_ENERGYSAVER_START_DELAY_30M;
            case 3600000:
                return TvSettingsEnums.SYSTEM_ENERGYSAVER_START_DELAY_1H;
            case 43200000:
                return TvSettingsEnums.SYSTEM_ENERGYSAVER_START_DELAY_12H;
            default:
                return -1;
        }
    }

    @Override
    protected int getPageId() {
        return TvSettingsEnums.SYSTEM_ENERGYSAVER;
    }
}
