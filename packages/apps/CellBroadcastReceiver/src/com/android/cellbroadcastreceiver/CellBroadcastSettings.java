/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

package com.android.cellbroadcastreceiver;

import android.annotation.NonNull;
import android.app.ActionBar;
import android.app.Activity;
import android.app.Fragment;
import android.app.backup.BackupManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.os.Bundle;
import android.os.PersistableBundle;
import android.os.UserManager;
import android.telephony.CarrierConfigManager;
import android.telephony.SubscriptionManager;
import android.util.Log;
import android.view.MenuItem;

import androidx.localbroadcastmanager.content.LocalBroadcastManager;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceFragment;
import androidx.preference.PreferenceManager;
import androidx.preference.PreferenceScreen;
import androidx.preference.TwoStatePreference;

import com.android.internal.annotations.VisibleForTesting;

import java.util.HashMap;
import java.util.Map;

/**
 * Settings activity for the cell broadcast receiver.
 */
public class CellBroadcastSettings extends Activity {

    private static final String TAG = "CellBroadcastSettings";

    private static final boolean DBG = false;

    // Preference key for alert header (A text view, not clickable).
    public static final String KEY_ALERTS_HEADER = "alerts_header";

    // Preference key for a master toggle to enable/disable all alerts message (default enabled).
    public static final String KEY_ENABLE_ALERTS_MASTER_TOGGLE = "enable_alerts_master_toggle";

    // Preference key for whether to enable public safety messages (default enabled).
    public static final String KEY_ENABLE_PUBLIC_SAFETY_MESSAGES = "enable_public_safety_messages";

    // Preference key for whether to enable emergency alerts (default enabled).
    public static final String KEY_ENABLE_EMERGENCY_ALERTS = "enable_emergency_alerts";

    // Enable vibration on alert (unless master volume is silent).
    public static final String KEY_ENABLE_ALERT_VIBRATE = "enable_alert_vibrate";

    // Play alert sound in full volume regardless Do Not Disturb is on.
    public static final String KEY_OVERRIDE_DND = "override_dnd";

    public static final String KEY_OVERRIDE_DND_SETTINGS_CHANGED =
            "override_dnd_settings_changed";

    // Preference category for emergency alert and CMAS settings.
    public static final String KEY_CATEGORY_EMERGENCY_ALERTS = "category_emergency_alerts";

    // Preference category for alert preferences.
    public static final String KEY_CATEGORY_ALERT_PREFERENCES = "category_alert_preferences";

    // Show checkbox for Presidential alerts in settings
    // Whether to display CMAS presidential alert notifications (always enabled).
    public static final String KEY_ENABLE_CMAS_PRESIDENTIAL_ALERTS =
            "enable_cmas_presidential_alerts";

    // Whether to display CMAS extreme threat notifications (default is enabled).
    public static final String KEY_ENABLE_CMAS_EXTREME_THREAT_ALERTS =
            "enable_cmas_extreme_threat_alerts";

    // Whether to display CMAS severe threat notifications (default is enabled).
    public static final String KEY_ENABLE_CMAS_SEVERE_THREAT_ALERTS =
            "enable_cmas_severe_threat_alerts";

    // Whether to display CMAS amber alert messages (default is enabled).
    public static final String KEY_ENABLE_CMAS_AMBER_ALERTS = "enable_cmas_amber_alerts";

    // Whether to display monthly test messages (default is disabled).
    public static final String KEY_ENABLE_TEST_ALERTS = "enable_test_alerts";

    // Whether to display state/local test messages (default disabled).
    public static final String KEY_ENABLE_STATE_LOCAL_TEST_ALERTS =
            "enable_state_local_test_alerts";

    // Preference key for whether to enable area update information notifications
    // Enabled by default for phones sold in Brazil and India, otherwise this setting may be hidden.
    public static final String KEY_ENABLE_AREA_UPDATE_INFO_ALERTS =
            "enable_area_update_info_alerts";

    // Preference key for initial opt-in/opt-out dialog.
    public static final String KEY_SHOW_CMAS_OPT_OUT_DIALOG = "show_cmas_opt_out_dialog";

    // Alert reminder interval ("once" = single 2 minute reminder).
    public static final String KEY_ALERT_REMINDER_INTERVAL = "alert_reminder_interval";

    // Preference key for emergency alerts history
    public static final String KEY_EMERGENCY_ALERT_HISTORY = "emergency_alert_history";

    // For watch layout
    private static final String KEY_WATCH_ALERT_REMINDER = "watch_alert_reminder";

    // Resource cache
    private static final Map<Integer, Resources> sResourcesCache = new HashMap<>();

    // Test override for disabling the subId specific resources
    private static boolean sUseResourcesForSubId = true;

    // Whether to receive alert in second language code
    public static final String KEY_RECEIVE_CMAS_IN_SECOND_LANGUAGE =
            "receive_cmas_in_second_language";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        ActionBar actionBar = getActionBar();
        if (actionBar != null) {
            // android.R.id.home will be triggered in onOptionsItemSelected()
            actionBar.setDisplayHomeAsUpEnabled(true);
        }

        UserManager userManager = (UserManager) getSystemService(Context.USER_SERVICE);
        if (userManager.hasUserRestriction(UserManager.DISALLOW_CONFIG_CELL_BROADCASTS)) {
            setContentView(R.layout.cell_broadcast_disallowed_preference_screen);
            return;
        }

        // We only add new CellBroadcastSettingsFragment if no fragment is restored.
        Fragment fragment = getFragmentManager().findFragmentById(android.R.id.content);
        if (fragment == null) {
            fragment = new CellBroadcastSettingsFragment();
            getFragmentManager()
                    .beginTransaction()
                    .add(android.R.id.content, fragment)
                    .commit();
        }
    }

    @Override
    public void onStart() {
        super.onStart();
        getWindow().addSystemFlags(
                android.view.WindowManager.LayoutParams
                        .SYSTEM_FLAG_HIDE_NON_SYSTEM_OVERLAY_WINDOWS);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            // Respond to the action bar's Up/Home button
            case android.R.id.home:
                finish();
                return true;
        }
        return super.onOptionsItemSelected(item);
    }

    /**
     * New fragment-style implementation of preferences.
     */
    public static class CellBroadcastSettingsFragment extends PreferenceFragment {

        private TwoStatePreference mExtremeCheckBox;
        private TwoStatePreference mSevereCheckBox;
        private TwoStatePreference mAmberCheckBox;
        private TwoStatePreference mMasterToggle;
        private TwoStatePreference mPublicSafetyMessagesChannelCheckBox;
        private TwoStatePreference mEmergencyAlertsCheckBox;
        private ListPreference mReminderInterval;
        private TwoStatePreference mOverrideDndCheckBox;
        private TwoStatePreference mAreaUpdateInfoCheckBox;
        private TwoStatePreference mTestCheckBox;
        private TwoStatePreference mStateLocalTestCheckBox;
        private TwoStatePreference mEnableVibrateCheckBox;
        private Preference mAlertHistory;
        private Preference mAlertsHeader;
        private PreferenceCategory mAlertCategory;
        private PreferenceCategory mAlertPreferencesCategory;
        private boolean mDisableSevereWhenExtremeDisabled = true;

        // WATCH
        private TwoStatePreference mAlertReminder;

        // Show checkbox for Presidential alerts in settings
        private TwoStatePreference mPresidentialCheckBox;

        // on/off switch in settings for receiving alert in second language code
        private TwoStatePreference mReceiveCmasInSecondLanguageCheckBox;

        private final BroadcastReceiver mTestingModeChangedReeiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                switch (intent.getAction()) {
                    case CellBroadcastReceiver.ACTION_TESTING_MODE_CHANGED:
                        updatePreferenceVisibility();
                        break;
                }
            }
        };

        @Override
        public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {

            LocalBroadcastManager.getInstance(getContext())
                    .registerReceiver(mTestingModeChangedReeiver, new IntentFilter(
                            CellBroadcastReceiver.ACTION_TESTING_MODE_CHANGED));

            // Load the preferences from an XML resource
            PackageManager pm = getActivity().getPackageManager();
            if (pm.hasSystemFeature(PackageManager.FEATURE_WATCH)) {
                addPreferencesFromResource(R.xml.watch_preferences);
            } else {
                addPreferencesFromResource(R.xml.preferences);
            }

            PreferenceScreen preferenceScreen = getPreferenceScreen();

            mExtremeCheckBox = (TwoStatePreference)
                    findPreference(KEY_ENABLE_CMAS_EXTREME_THREAT_ALERTS);
            mSevereCheckBox = (TwoStatePreference)
                    findPreference(KEY_ENABLE_CMAS_SEVERE_THREAT_ALERTS);
            mAmberCheckBox = (TwoStatePreference)
                    findPreference(KEY_ENABLE_CMAS_AMBER_ALERTS);
            mMasterToggle = (TwoStatePreference)
                    findPreference(KEY_ENABLE_ALERTS_MASTER_TOGGLE);
            mPublicSafetyMessagesChannelCheckBox = (TwoStatePreference)
                    findPreference(KEY_ENABLE_PUBLIC_SAFETY_MESSAGES);
            mEmergencyAlertsCheckBox = (TwoStatePreference)
                    findPreference(KEY_ENABLE_EMERGENCY_ALERTS);
            mReminderInterval = (ListPreference)
                    findPreference(KEY_ALERT_REMINDER_INTERVAL);
            mOverrideDndCheckBox = (TwoStatePreference)
                    findPreference(KEY_OVERRIDE_DND);
            mAreaUpdateInfoCheckBox = (TwoStatePreference)
                    findPreference(KEY_ENABLE_AREA_UPDATE_INFO_ALERTS);
            mTestCheckBox = (TwoStatePreference)
                    findPreference(KEY_ENABLE_TEST_ALERTS);
            mStateLocalTestCheckBox = (TwoStatePreference)
                    findPreference(KEY_ENABLE_STATE_LOCAL_TEST_ALERTS);
            mAlertHistory = findPreference(KEY_EMERGENCY_ALERT_HISTORY);
            mAlertsHeader = findPreference(KEY_ALERTS_HEADER);
            mReceiveCmasInSecondLanguageCheckBox = (TwoStatePreference) findPreference
                    (KEY_RECEIVE_CMAS_IN_SECOND_LANGUAGE);
            mEnableVibrateCheckBox = findPreference(KEY_ENABLE_ALERT_VIBRATE);

            // Show checkbox for Presidential alerts in settings
            mPresidentialCheckBox = (TwoStatePreference)
                    findPreference(KEY_ENABLE_CMAS_PRESIDENTIAL_ALERTS);

            if (pm.hasSystemFeature(PackageManager.FEATURE_WATCH)) {
                mAlertReminder = (TwoStatePreference)
                        findPreference(KEY_WATCH_ALERT_REMINDER);
                if (Integer.valueOf(mReminderInterval.getValue()) == 0) {
                    mAlertReminder.setChecked(false);
                } else {
                    mAlertReminder.setChecked(true);
                }
                mAlertReminder.setOnPreferenceChangeListener((p, newVal) -> {
                    try {
                        mReminderInterval.setValueIndex((Boolean) newVal ? 1 : 3);
                    } catch (IndexOutOfBoundsException e) {
                        mReminderInterval.setValue(String.valueOf(0));
                        Log.w(TAG, "Setting default value");
                    }
                    return true;
                });
                PreferenceScreen watchScreen = (PreferenceScreen)
                        findPreference(KEY_CATEGORY_ALERT_PREFERENCES);
                watchScreen.removePreference(mReminderInterval);
            } else {
                mAlertPreferencesCategory = (PreferenceCategory)
                        findPreference(KEY_CATEGORY_ALERT_PREFERENCES);
                mAlertCategory = (PreferenceCategory)
                        findPreference(KEY_CATEGORY_EMERGENCY_ALERTS);
            }

            Resources res = CellBroadcastSettings.getResources(getContext(),
                    SubscriptionManager.DEFAULT_SUBSCRIPTION_ID);

            SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(getContext());
            mDisableSevereWhenExtremeDisabled = res.getBoolean(
                    R.bool.disable_severe_when_extreme_disabled);

            // Handler for settings that require us to reconfigure enabled channels in radio
            Preference.OnPreferenceChangeListener startConfigServiceListener =
                    new Preference.OnPreferenceChangeListener() {
                        @Override
                        public boolean onPreferenceChange(Preference pref, Object newValue) {
                            CellBroadcastReceiver.startConfigService(pref.getContext());

                            if (mDisableSevereWhenExtremeDisabled) {
                                if (pref.getKey().equals(KEY_ENABLE_CMAS_EXTREME_THREAT_ALERTS)) {
                                    boolean isExtremeAlertChecked = (Boolean) newValue;
                                    if (mSevereCheckBox != null) {
                                        mSevereCheckBox.setEnabled(isExtremeAlertChecked);
                                        mSevereCheckBox.setChecked(false);
                                    }
                                }
                            }

                            if (pref.getKey().equals(KEY_ENABLE_ALERTS_MASTER_TOGGLE)) {
                                boolean isEnableAlerts = (Boolean) newValue;
                                setAlertsEnabled(isEnableAlerts);
                            }

                            // Notify backup manager a backup pass is needed.
                            new BackupManager(getContext()).dataChanged();
                            return true;
                        }
                    };

            initReminderIntervalList();

            if (mMasterToggle != null) {
                mMasterToggle.setOnPreferenceChangeListener(startConfigServiceListener);
                // If allow alerts are disabled, we turn all sub-alerts off. If it's enabled, we
                // leave them as they are.
                if (!mMasterToggle.isChecked()) {
                    setAlertsEnabled(false);
                }
            }

            if (mAreaUpdateInfoCheckBox != null) {
                mAreaUpdateInfoCheckBox.setOnPreferenceChangeListener(startConfigServiceListener);
            }
            if (mExtremeCheckBox != null) {
                mExtremeCheckBox.setOnPreferenceChangeListener(startConfigServiceListener);
            }
            if (mPublicSafetyMessagesChannelCheckBox != null) {
                mPublicSafetyMessagesChannelCheckBox.setOnPreferenceChangeListener(
                        startConfigServiceListener);
            }
            if (mEmergencyAlertsCheckBox != null) {
                mEmergencyAlertsCheckBox.setOnPreferenceChangeListener(startConfigServiceListener);
            }
            if (mSevereCheckBox != null) {
                mSevereCheckBox.setOnPreferenceChangeListener(startConfigServiceListener);
                if (mDisableSevereWhenExtremeDisabled) {
                    if (mExtremeCheckBox != null) {
                        mSevereCheckBox.setEnabled(mExtremeCheckBox.isChecked());
                    }
                }
            }
            if (mAmberCheckBox != null) {
                mAmberCheckBox.setOnPreferenceChangeListener(startConfigServiceListener);
            }
            if (mTestCheckBox != null) {
                mTestCheckBox.setOnPreferenceChangeListener(startConfigServiceListener);
            }
            if (mStateLocalTestCheckBox != null) {
                mStateLocalTestCheckBox.setOnPreferenceChangeListener(
                        startConfigServiceListener);
            }

            if (mOverrideDndCheckBox != null) {
                if (!sp.getBoolean(KEY_OVERRIDE_DND_SETTINGS_CHANGED, false)) {
                    // If the user hasn't changed this settings yet, use the default settings
                    // from resource overlay.
                    mOverrideDndCheckBox.setChecked(res.getBoolean(R.bool.override_dnd_default));
                }
                mOverrideDndCheckBox.setOnPreferenceChangeListener(
                        (pref, newValue) -> {
                            sp.edit().putBoolean(KEY_OVERRIDE_DND_SETTINGS_CHANGED,
                                    true).apply();
                            updateVibrationPreference((boolean) newValue);
                            return true;
                        });
            }

            if (mAlertHistory != null) {
                mAlertHistory.setOnPreferenceClickListener(
                        preference -> {
                            final Intent intent = new Intent(getContext(),
                                    CellBroadcastListActivity.class);
                            startActivity(intent);
                            return true;
                        });
            }

            updateVibrationPreference(sp.getBoolean(CellBroadcastSettings.KEY_OVERRIDE_DND,
                    false));
            updatePreferenceVisibility();
        }

        /**
         * Update the vibration preference based on override DND. If DND is overridden, then do
         * not allow users to turn off vibration.
         *
         * @param overrideDnd {@code true} if the alert will be played at full volume, regardless
         * DND settings.
         */
        private void updateVibrationPreference(boolean overrideDnd) {
            if (mEnableVibrateCheckBox != null) {
                if (overrideDnd) {
                    // If DND is enabled, always enable vibration.
                    mEnableVibrateCheckBox.setChecked(true);
                }
                // Grey out the preference if DND is overridden.
                mEnableVibrateCheckBox.setEnabled(!overrideDnd);
            }
        }

        /**
         * Dynamically update each preference's visibility based on configuration.
         */
        private void updatePreferenceVisibility() {
            Resources res = CellBroadcastSettings.getResources(getContext(),
                    SubscriptionManager.DEFAULT_SUBSCRIPTION_ID);

            CellBroadcastChannelManager channelManager = new CellBroadcastChannelManager(
                    getContext(), SubscriptionManager.DEFAULT_SUBSCRIPTION_ID);

            if (mPresidentialCheckBox != null) {
                mPresidentialCheckBox.setVisible(
                        res.getBoolean(R.bool.show_presidential_alerts_settings));
            }

            if (mExtremeCheckBox != null) {
                mExtremeCheckBox.setVisible(res.getBoolean(R.bool.show_extreme_alert_settings)
                        && !channelManager.getCellBroadcastChannelRanges(
                                R.array.cmas_alert_extreme_channels_range_strings).isEmpty());
            }

            if (mSevereCheckBox != null) {
                mSevereCheckBox.setVisible(res.getBoolean(R.bool.show_severe_alert_settings)
                        && !channelManager.getCellBroadcastChannelRanges(
                                R.array.cmas_alerts_severe_range_strings).isEmpty());
            }

            if (mAmberCheckBox != null) {
                mAmberCheckBox.setVisible(res.getBoolean(R.bool.show_amber_alert_settings)
                        && !channelManager.getCellBroadcastChannelRanges(
                                R.array.cmas_amber_alerts_channels_range_strings).isEmpty());
            }

            if (mPublicSafetyMessagesChannelCheckBox != null) {
                mPublicSafetyMessagesChannelCheckBox.setVisible(
                        res.getBoolean(R.bool.show_public_safety_settings)
                                && !channelManager.getCellBroadcastChannelRanges(
                                        R.array.public_safety_messages_channels_range_strings)
                                .isEmpty());
            }

            if (mTestCheckBox != null) {
                mTestCheckBox.setVisible(isTestAlertsToggleVisible(getContext()));
            }

            if (mEmergencyAlertsCheckBox != null) {
                mEmergencyAlertsCheckBox.setVisible(!channelManager.getCellBroadcastChannelRanges(
                        R.array.emergency_alerts_channels_range_strings).isEmpty());
            }

            if (mStateLocalTestCheckBox != null) {
                mStateLocalTestCheckBox.setVisible(
                        res.getBoolean(R.bool.show_state_local_test_settings)
                                && !channelManager.getCellBroadcastChannelRanges(
                                        R.array.state_local_test_alert_range_strings).isEmpty());
            }

            if (mReceiveCmasInSecondLanguageCheckBox != null) {
                mReceiveCmasInSecondLanguageCheckBox.setVisible(!res.getString(
                        R.string.emergency_alert_second_language_code).isEmpty());
            }

            if (mAreaUpdateInfoCheckBox != null) {
                mAreaUpdateInfoCheckBox.setVisible(
                        res.getBoolean(R.bool.config_showAreaUpdateInfoSettings));
            }

            if (mOverrideDndCheckBox != null) {
                mOverrideDndCheckBox.setVisible(res.getBoolean(R.bool.show_override_dnd_settings));
            }

            if (mEnableVibrateCheckBox != null) {
                // Only show vibrate toggle when override DND toggle is available to users, or when
                // override DND default is turned off.
                // In some countries, override DND is always on, which means vibration is always on.
                // In that case, no need to show vibration toggle for users.
                mEnableVibrateCheckBox.setVisible(
                        res.getBoolean(R.bool.show_override_dnd_settings)
                                || !res.getBoolean(R.bool.override_dnd_default));
            }
            if (mAlertsHeader != null) {
                mAlertsHeader.setVisible(
                        !getContext().getString(R.string.alerts_header_summary).isEmpty());
            }
        }

        private void initReminderIntervalList() {
            Resources res = CellBroadcastSettings.getResources(
                    getContext(), SubscriptionManager.DEFAULT_SUBSCRIPTION_ID);

            String[] activeValues =
                    res.getStringArray(R.array.alert_reminder_interval_active_values);
            String[] allEntries = res.getStringArray(R.array.alert_reminder_interval_entries);
            String[] newEntries = new String[activeValues.length];

            // Only add active interval to the list
            for (int i = 0; i < activeValues.length; i++) {
                int index = mReminderInterval.findIndexOfValue(activeValues[i]);
                if (index != -1) {
                    newEntries[i] = allEntries[index];
                    if (DBG) Log.d(TAG, "Added " + allEntries[index]);
                } else {
                    Log.e(TAG, "Can't find " + activeValues[i]);
                }
            }

            mReminderInterval.setEntries(newEntries);
            mReminderInterval.setEntryValues(activeValues);
            mReminderInterval.setSummary(mReminderInterval.getEntry());
            mReminderInterval.setOnPreferenceChangeListener(
                    new Preference.OnPreferenceChangeListener() {
                        @Override
                        public boolean onPreferenceChange(Preference pref, Object newValue) {
                            final ListPreference listPref = (ListPreference) pref;
                            final int idx = listPref.findIndexOfValue((String) newValue);
                            listPref.setSummary(listPref.getEntries()[idx]);
                            return true;
                        }
                    });
        }


        private void setAlertsEnabled(boolean alertsEnabled) {
            if (mSevereCheckBox != null) {
                mSevereCheckBox.setEnabled(alertsEnabled);
                mSevereCheckBox.setChecked(alertsEnabled);
            }
            if (mExtremeCheckBox != null) {
                mExtremeCheckBox.setEnabled(alertsEnabled);
                mExtremeCheckBox.setChecked(alertsEnabled);
            }
            if (mAmberCheckBox != null) {
                mAmberCheckBox.setEnabled(alertsEnabled);
                mAmberCheckBox.setChecked(alertsEnabled);
            }
            if (mAreaUpdateInfoCheckBox != null) {
                mAreaUpdateInfoCheckBox.setEnabled(alertsEnabled);
                mAreaUpdateInfoCheckBox.setChecked(alertsEnabled);
            }
            if (mAlertPreferencesCategory != null) {
                mAlertPreferencesCategory.setEnabled(alertsEnabled);
            }
            if (mEmergencyAlertsCheckBox != null) {
                mEmergencyAlertsCheckBox.setEnabled(alertsEnabled);
                mEmergencyAlertsCheckBox.setChecked(alertsEnabled);
            }
            if (mPublicSafetyMessagesChannelCheckBox != null) {
                mPublicSafetyMessagesChannelCheckBox.setEnabled(alertsEnabled);
                mPublicSafetyMessagesChannelCheckBox.setChecked(alertsEnabled);
            }
            if (mStateLocalTestCheckBox != null) {
                mStateLocalTestCheckBox.setEnabled(alertsEnabled);
                mStateLocalTestCheckBox.setChecked(alertsEnabled);
            }
            if (mTestCheckBox != null) {
                mTestCheckBox.setEnabled(alertsEnabled);
                mTestCheckBox.setChecked(alertsEnabled);
            }
        }

        @Override
        public void onResume() {
            super.onResume();
            updatePreferenceVisibility();
        }

        @Override
        public void onDestroy() {
            super.onDestroy();
            LocalBroadcastManager.getInstance(getContext())
                    .unregisterReceiver(mTestingModeChangedReeiver);
        }
    }

    public static boolean isTestAlertsToggleVisible(Context context) {
        CellBroadcastChannelManager channelManager = new CellBroadcastChannelManager(context,
                SubscriptionManager.DEFAULT_SUBSCRIPTION_ID);
        Resources res = CellBroadcastSettings.getResources(context,
                SubscriptionManager.DEFAULT_SUBSCRIPTION_ID);
        boolean isTestAlertsAvailable = !channelManager.getCellBroadcastChannelRanges(
                R.array.required_monthly_test_range_strings).isEmpty()
                || !channelManager.getCellBroadcastChannelRanges(
                R.array.exercise_alert_range_strings).isEmpty()
                || !channelManager.getCellBroadcastChannelRanges(
                R.array.operator_defined_alert_range_strings).isEmpty()
                || !channelManager.getCellBroadcastChannelRanges(
                R.array.etws_test_alerts_range_strings).isEmpty();

        return (res.getBoolean(R.bool.show_test_settings)
                || CellBroadcastReceiver.isTestingMode(context))
                && isTestAlertsAvailable;
    }

    public static boolean isFeatureEnabled(Context context, String feature, boolean defaultValue) {
        CarrierConfigManager configManager =
                (CarrierConfigManager) context.getSystemService(Context.CARRIER_CONFIG_SERVICE);

        if (configManager != null) {
            PersistableBundle carrierConfig = configManager.getConfig();
            if (carrierConfig != null) {
                return carrierConfig.getBoolean(feature, defaultValue);
            }
        }

        return defaultValue;
    }

    /**
     * Override used by tests so that we don't call
     * SubscriptionManager.getResourcesForSubId, which is a static unmockable
     * method.
     */
    @VisibleForTesting
    public static void setUseResourcesForSubId(boolean useResourcesForSubId) {
        sUseResourcesForSubId = useResourcesForSubId;
    }

    /**
     * Get the device resource based on SIM
     *
     * @param context Context
     * @param subId Subscription index
     *
     * @return The resource
     */
    public static @NonNull Resources getResources(@NonNull Context context, int subId) {
        if (subId == SubscriptionManager.DEFAULT_SUBSCRIPTION_ID
                || !SubscriptionManager.isValidSubscriptionId(subId) || !sUseResourcesForSubId) {
            return context.getResources();
        }

        if (sResourcesCache.containsKey(subId)) {
            return sResourcesCache.get(subId);
        }

        Resources res = SubscriptionManager.getResourcesForSubId(context, subId);
        sResourcesCache.put(subId, res);

        return res;
    }
}
