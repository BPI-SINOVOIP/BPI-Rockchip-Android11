/*
 * Copyright (C) 2011 The Android Open Source Project
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

package com.android.cellbroadcastreceiver;

import android.app.ActivityManager;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.ContentProviderClient;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.os.Bundle;
import android.os.RemoteException;
import android.os.SystemProperties;
import android.os.UserManager;
import android.preference.PreferenceManager;
import android.provider.Telephony;
import android.provider.Telephony.CellBroadcasts;
import android.telephony.CarrierConfigManager;
import android.telephony.ServiceState;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.telephony.cdma.CdmaSmsCbProgramData;
import android.text.TextUtils;
import android.util.EventLog;
import android.util.Log;
import android.widget.Toast;

import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import com.android.cellbroadcastservice.CellBroadcastStatsLog;
import com.android.internal.annotations.VisibleForTesting;

import java.util.ArrayList;

public class CellBroadcastReceiver extends BroadcastReceiver {
    private static final String TAG = "CellBroadcastReceiver";
    static final boolean DBG = true;
    static final boolean VDBG = false;    // STOPSHIP: change to false before ship

    // Key to access the shared preference of reminder interval default value.
    @VisibleForTesting
    public static final String CURRENT_INTERVAL_DEFAULT = "current_interval_default";

    // Key to access the shared preference of cell broadcast testing mode.
    @VisibleForTesting
    public static final String TESTING_MODE = "testing_mode";

    // Key to access the shared preference of service state.
    private static final String SERVICE_STATE = "service_state";

    // shared preference under developer settings
    private static final String ENABLE_ALERT_MASTER_PREF = "enable_alerts_master_toggle";

    public static final String ACTION_SERVICE_STATE = "android.intent.action.SERVICE_STATE";
    public static final String EXTRA_VOICE_REG_STATE = "voiceRegState";

    // Intent actions and extras
    public static final String CELLBROADCAST_START_CONFIG_ACTION =
            "com.android.cellbroadcastreceiver.intent.START_CONFIG";
    public static final String ACTION_MARK_AS_READ =
            "com.android.cellbroadcastreceiver.intent.action.MARK_AS_READ";
    public static final String EXTRA_DELIVERY_TIME =
            "com.android.cellbroadcastreceiver.intent.extra.ID";

    public static final String ACTION_TESTING_MODE_CHANGED =
            "com.android.cellbroadcastreceiver.intent.ACTION_TESTING_MODE_CHANGED";

    private Context mContext;

    /**
     * helper method for easier testing. To generate a new CellBroadcastTask
     * @param deliveryTime message delivery time
     */
    @VisibleForTesting
    public void getCellBroadcastTask(final long deliveryTime) {
        new CellBroadcastContentProvider.AsyncCellBroadcastTask(mContext.getContentResolver())
                .execute(new CellBroadcastContentProvider.CellBroadcastOperation() {
                    @Override
                    public boolean execute(CellBroadcastContentProvider provider) {
                        return provider.markBroadcastRead(CellBroadcasts.DELIVERY_TIME,
                                deliveryTime);
                    }
                });
    }

    /**
     * this method is to make this class unit-testable, because CellBroadcastSettings.getResources()
     * is a static method and cannot be stubbed.
     * @return resources
     */
    @VisibleForTesting
    public Resources getResourcesMethod() {
        return CellBroadcastSettings.getResources(mContext,
                SubscriptionManager.DEFAULT_SUBSCRIPTION_ID);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (DBG) log("onReceive " + intent);

        mContext = context.getApplicationContext();
        String action = intent.getAction();
        Resources res = getResourcesMethod();

        if (ACTION_MARK_AS_READ.equals(action)) {
            // The only way this'll be called is if someone tries to maliciously set something as
            // read. Log an event.
            EventLog.writeEvent(0x534e4554, "162741784", -1, null);
        } else if (CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED.equals(action)) {
            initializeSharedPreference();
            enableLauncher();
            startConfigService();
        } else if (ACTION_SERVICE_STATE.equals(action)) {
            // lower layer clears channel configurations under APM, thus need to resend
            // configurations once moving back from APM. This should be fixed in lower layer
            // going forward.
            int ss = intent.getIntExtra(EXTRA_VOICE_REG_STATE, ServiceState.STATE_IN_SERVICE);
            if (ss != ServiceState.STATE_POWER_OFF
                    && getServiceState(context) == ServiceState.STATE_POWER_OFF) {
                startConfigService();
            }
            setServiceState(ss);
        } else if (CELLBROADCAST_START_CONFIG_ACTION.equals(action)
                || SubscriptionManager.ACTION_DEFAULT_SMS_SUBSCRIPTION_CHANGED.equals(action)) {
            startConfigService();
        } else if (Telephony.Sms.Intents.ACTION_SMS_EMERGENCY_CB_RECEIVED.equals(action) ||
                Telephony.Sms.Intents.SMS_CB_RECEIVED_ACTION.equals(action)) {
            intent.setClass(mContext, CellBroadcastAlertService.class);
            mContext.startService(intent);
        } else if (Telephony.Sms.Intents.SMS_SERVICE_CATEGORY_PROGRAM_DATA_RECEIVED_ACTION
                .equals(action)) {
            ArrayList<CdmaSmsCbProgramData> programDataList =
                    intent.getParcelableArrayListExtra("program_data");
            CellBroadcastStatsLog.write(CellBroadcastStatsLog.CB_MESSAGE_REPORTED,
                    CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_REPORTED__TYPE__CDMA_SPC,
                    CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_REPORTED__SOURCE__CB_RECEIVER_APP);
            if (programDataList != null) {
                handleCdmaSmsCbProgramData(programDataList);
            } else {
                loge("SCPD intent received with no program_data");
            }
        } else if (Intent.ACTION_LOCALE_CHANGED.equals(action)) {
            // rename registered notification channels on locale change
            CellBroadcastAlertService.createNotificationChannels(mContext);
        } else if (TelephonyManager.ACTION_SECRET_CODE.equals(action)) {
            if (SystemProperties.getInt("ro.debuggable", 0) == 1
                    || res.getBoolean(R.bool.allow_testing_mode_on_user_build)) {
                setTestingMode(!isTestingMode(mContext));
                int msgId = (isTestingMode(mContext)) ? R.string.testing_mode_enabled
                        : R.string.testing_mode_disabled;
                String msg =  res.getString(msgId);
                Toast.makeText(mContext, msg, Toast.LENGTH_SHORT).show();
                LocalBroadcastManager.getInstance(mContext)
                        .sendBroadcast(new Intent(ACTION_TESTING_MODE_CHANGED));
                log(msg);
            }
        } else {
            Log.w(TAG, "onReceive() unexpected action " + action);
        }
    }

    /**
     * Enable/disable cell broadcast receiver testing mode.
     *
     * @param on {@code true} if testing mode is on, otherwise off.
     */
    @VisibleForTesting
    public void setTestingMode(boolean on) {
        SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(mContext);
        sp.edit().putBoolean(TESTING_MODE, on).commit();
    }

    /**
     * @return {@code true} if operating in testing mode, which enables some features for testing
     * purposes.
     */
    public static boolean isTestingMode(Context context) {
        SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(context);
        return sp.getBoolean(TESTING_MODE, false);
    }

    /**
     * Store the current service state for voice registration.
     *
     * @param ss current voice registration service state.
     */
    private void setServiceState(int ss) {
        SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(mContext);
        sp.edit().putInt(SERVICE_STATE, ss).commit();
    }

    /**
     * @return the stored voice registration service state
     */
    private static int getServiceState(Context context) {
        SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(context);
        return sp.getInt(SERVICE_STATE, ServiceState.STATE_IN_SERVICE);
    }

    /**
     * update reminder interval
     */
    @VisibleForTesting
    public void adjustReminderInterval() {
        SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(mContext);
        String currentIntervalDefault = sp.getString(CURRENT_INTERVAL_DEFAULT, "0");

        // If interval default changes, reset the interval to the new default value.
        String newIntervalDefault = CellBroadcastSettings.getResources(mContext,
                SubscriptionManager.DEFAULT_SUBSCRIPTION_ID).getString(
                        R.string.alert_reminder_interval_in_min_default);
        if (!newIntervalDefault.equals(currentIntervalDefault)) {
            Log.d(TAG, "Default interval changed from " + currentIntervalDefault + " to " +
                    newIntervalDefault);

            Editor editor = sp.edit();
            // Reset the value to default.
            editor.putString(
                    CellBroadcastSettings.KEY_ALERT_REMINDER_INTERVAL, newIntervalDefault);
            // Save the new default value.
            editor.putString(CURRENT_INTERVAL_DEFAULT, newIntervalDefault);
            editor.commit();
        } else {
            if (DBG) Log.d(TAG, "Default interval " + currentIntervalDefault + " did not change.");
        }
    }
    /**
     * This method's purpose if to enable unit testing
     * @return sharedePreferences for mContext
     */
    @VisibleForTesting
    public SharedPreferences getDefaultSharedPreferences() {
        return PreferenceManager.getDefaultSharedPreferences(mContext);
    }

    /**
     * return if there are default values in shared preferences
     * @return boolean
     */
    @VisibleForTesting
    public Boolean sharedPrefsHaveDefaultValues() {
        return mContext.getSharedPreferences(PreferenceManager.KEY_HAS_SET_DEFAULT_VALUES,
                Context.MODE_PRIVATE).getBoolean(PreferenceManager.KEY_HAS_SET_DEFAULT_VALUES,
                false);
    }
    /**
     * initialize shared preferences before starting services
     */
    @VisibleForTesting
    public void initializeSharedPreference() {
        if (isSystemUser()) {
            Log.d(TAG, "initializeSharedPreference");
            SharedPreferences sp = getDefaultSharedPreferences();

            if (!sharedPrefsHaveDefaultValues()) {
                // Sets the default values of the shared preference if there isn't any.
                PreferenceManager.setDefaultValues(mContext, R.xml.preferences, false);

                sp.edit().putBoolean(CellBroadcastSettings.KEY_OVERRIDE_DND_SETTINGS_CHANGED,
                        false).apply();

                // migrate sharedpref from legacy app
                migrateSharedPreferenceFromLegacy();

                // If the device is in test harness mode, we need to disable emergency alert by
                // default.
                if (ActivityManager.isRunningInUserTestHarness()) {
                    Log.d(TAG, "In test harness mode. Turn off emergency alert by default.");
                    sp.edit().putBoolean(CellBroadcastSettings.KEY_ENABLE_ALERTS_MASTER_TOGGLE,
                            false).apply();
                }

            } else {
                Log.d(TAG, "Skip setting default values of shared preference.");
            }

            adjustReminderInterval();
        } else {
            Log.e(TAG, "initializeSharedPreference: Not system user.");
        }
    }

    /**
     * migrate shared preferences from legacy content provider client
     */
    @VisibleForTesting
    public void migrateSharedPreferenceFromLegacy() {
        String[] PREF_KEYS = {
                CellBroadcasts.Preference.ENABLE_CMAS_AMBER_PREF,
                CellBroadcasts.Preference.ENABLE_AREA_UPDATE_INFO_PREF,
                CellBroadcasts.Preference.ENABLE_TEST_ALERT_PREF,
                CellBroadcasts.Preference.ENABLE_STATE_LOCAL_TEST_PREF,
                CellBroadcasts.Preference.ENABLE_PUBLIC_SAFETY_PREF,
                CellBroadcasts.Preference.ENABLE_CMAS_SEVERE_THREAT_PREF,
                CellBroadcasts.Preference.ENABLE_CMAS_EXTREME_THREAT_PREF,
                CellBroadcasts.Preference.ENABLE_CMAS_PRESIDENTIAL_PREF,
                CellBroadcasts.Preference.ENABLE_EMERGENCY_PERF,
                CellBroadcasts.Preference.ENABLE_ALERT_VIBRATION_PREF,
                CellBroadcasts.Preference.ENABLE_CMAS_IN_SECOND_LANGUAGE_PREF,
                ENABLE_ALERT_MASTER_PREF,
        };
        try (ContentProviderClient client = mContext.getContentResolver()
                .acquireContentProviderClient(Telephony.CellBroadcasts.AUTHORITY_LEGACY)) {
            if (client == null) {
                Log.d(TAG, "No legacy provider available for sharedpreference migration");
                return;
            }
            SharedPreferences.Editor sp = PreferenceManager
                    .getDefaultSharedPreferences(mContext).edit();
            for (String key : PREF_KEYS) {
                try {
                    Bundle pref = client.call(
                            CellBroadcasts.AUTHORITY_LEGACY,
                            CellBroadcasts.CALL_METHOD_GET_PREFERENCE,
                            key, null);
                    if (pref != null && pref.containsKey(key)) {
                        Log.d(TAG, "migrateSharedPreferenceFromLegacy: " + key + "val: "
                                + pref.getBoolean(key));
                        sp.putBoolean(key, pref.getBoolean(key));
                    } else {
                        Log.d(TAG, "migrateSharedPreferenceFromLegacy: unsupported key: " + key);
                    }
                } catch (RemoteException e) {
                    Log.e(TAG, "fails to get shared preference " + e);
                }
            }
            sp.apply();
        } catch (Exception e) {
            // We have to guard ourselves against any weird behavior of the
            // legacy provider by trying to catch everything
            loge("Failed migration from legacy provider: " + e);
        }
    }

    /**
     * Handle Service Category Program Data message.
     * TODO: Send Service Category Program Results response message to sender
     *
     * @param programDataList
     */
    @VisibleForTesting
    public void handleCdmaSmsCbProgramData(ArrayList<CdmaSmsCbProgramData> programDataList) {
        for (CdmaSmsCbProgramData programData : programDataList) {
            switch (programData.getOperation()) {
                case CdmaSmsCbProgramData.OPERATION_ADD_CATEGORY:
                    tryCdmaSetCategory(mContext, programData.getCategory(), true);
                    break;

                case CdmaSmsCbProgramData.OPERATION_DELETE_CATEGORY:
                    tryCdmaSetCategory(mContext, programData.getCategory(), false);
                    break;

                case CdmaSmsCbProgramData.OPERATION_CLEAR_CATEGORIES:
                    tryCdmaSetCategory(mContext,
                            CdmaSmsCbProgramData.CATEGORY_CMAS_EXTREME_THREAT, false);
                    tryCdmaSetCategory(mContext,
                            CdmaSmsCbProgramData.CATEGORY_CMAS_SEVERE_THREAT, false);
                    tryCdmaSetCategory(mContext,
                            CdmaSmsCbProgramData.CATEGORY_CMAS_CHILD_ABDUCTION_EMERGENCY, false);
                    tryCdmaSetCategory(mContext,
                            CdmaSmsCbProgramData.CATEGORY_CMAS_TEST_MESSAGE, false);
                    break;

                default:
                    loge("Ignoring unknown SCPD operation " + programData.getOperation());
            }
        }
    }

    /**
     * set CDMA category in shared preferences
     * @param context
     * @param category CDMA category
     * @param enable true for add category, false otherwise
     */
    @VisibleForTesting
    public void tryCdmaSetCategory(Context context, int category, boolean enable) {
        SharedPreferences sharedPrefs = PreferenceManager.getDefaultSharedPreferences(context);

        switch (category) {
            case CdmaSmsCbProgramData.CATEGORY_CMAS_EXTREME_THREAT:
                sharedPrefs.edit().putBoolean(
                        CellBroadcastSettings.KEY_ENABLE_CMAS_EXTREME_THREAT_ALERTS, enable)
                        .apply();
                break;

            case CdmaSmsCbProgramData.CATEGORY_CMAS_SEVERE_THREAT:
                sharedPrefs.edit().putBoolean(
                        CellBroadcastSettings.KEY_ENABLE_CMAS_SEVERE_THREAT_ALERTS, enable)
                        .apply();
                break;

            case CdmaSmsCbProgramData.CATEGORY_CMAS_CHILD_ABDUCTION_EMERGENCY:
                sharedPrefs.edit().putBoolean(
                        CellBroadcastSettings.KEY_ENABLE_CMAS_AMBER_ALERTS, enable).apply();
                break;

            case CdmaSmsCbProgramData.CATEGORY_CMAS_TEST_MESSAGE:
                sharedPrefs.edit().putBoolean(
                        CellBroadcastSettings.KEY_ENABLE_TEST_ALERTS, enable).apply();
                break;

            default:
                Log.w(TAG, "Ignoring SCPD command to " + (enable ? "enable" : "disable")
                        + " alerts in category " + category);
        }
    }

    /**
     * This method's purpose if to enable unit testing
     * @return if the mContext user is a system user
     */
    @VisibleForTesting
    public boolean isSystemUser() {
        return isSystemUser(mContext);
    }

    /**
     * This method's purpose if to enable unit testing
     */
    @VisibleForTesting
    public void startConfigService() {
        startConfigService(mContext);
    }

    /**
     * Check if user from context is system user
     * @param context
     * @return whether the user is system user
     */
    private static boolean isSystemUser(Context context) {
        UserManager userManager = (UserManager) context.getSystemService(Context.USER_SERVICE);
        return userManager.isSystemUser();
    }

    /**
     * Tell {@link CellBroadcastConfigService} to enable the CB channels.
     * @param context the broadcast receiver context
     */
    static void startConfigService(Context context) {
        if (isSystemUser(context)) {
            Intent serviceIntent = new Intent(CellBroadcastConfigService.ACTION_ENABLE_CHANNELS,
                    null, context, CellBroadcastConfigService.class);
            Log.d(TAG, "Start Cell Broadcast configuration.");
            context.startService(serviceIntent);
        } else {
            Log.e(TAG, "startConfigService: Not system user.");
        }
    }

    /**
     * Enable Launcher.
     */
    @VisibleForTesting
    public void enableLauncher() {
        boolean enable = getResourcesMethod().getBoolean(R.bool.show_message_history_in_launcher);
        final PackageManager pm = mContext.getPackageManager();
        // This alias presents the target activity, CellBroadcastListActivity, as a independent
        // entity with its own intent filter for android.intent.category.LAUNCHER.
        // This alias will be enabled/disabled at run-time based on resource overlay. Once enabled,
        // it will appear in the Launcher as a top-level application
        String aliasLauncherActivity = null;
        try {
            PackageInfo p = pm.getPackageInfo(mContext.getPackageName(),
                PackageManager.GET_ACTIVITIES | PackageManager.MATCH_DISABLED_COMPONENTS);
            if (p != null) {
                for (ActivityInfo activityInfo : p.activities) {
                    String targetActivity = activityInfo.targetActivity;
                    if (CellBroadcastListActivity.class.getName().equals(targetActivity)) {
                        aliasLauncherActivity = activityInfo.name;
                        break;
                    }
                }
            }
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, e.toString());
        }
        if (TextUtils.isEmpty(aliasLauncherActivity)) {
            Log.e(TAG, "cannot find launcher activity");
            return;
        }

        if (enable) {
            Log.d(TAG, "enable launcher activity: " + aliasLauncherActivity);
            pm.setComponentEnabledSetting(
                new ComponentName(mContext, aliasLauncherActivity),
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED, PackageManager.DONT_KILL_APP);
        } else {
            Log.d(TAG, "disable launcher activity: " + aliasLauncherActivity);
            pm.setComponentEnabledSetting(
                new ComponentName(mContext, aliasLauncherActivity),
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED, PackageManager.DONT_KILL_APP);
        }
    }

    private static void log(String msg) {
        Log.d(TAG, msg);
    }

    private static void loge(String msg) {
        Log.e(TAG, msg);
    }
}
