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

import static com.android.cellbroadcastreceiver.CellBroadcastReceiver.VDBG;

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.preference.PreferenceManager;
import android.telephony.SmsManager;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.util.Log;

import androidx.annotation.NonNull;

import com.android.cellbroadcastreceiver.CellBroadcastChannelManager.CellBroadcastChannelRange;
import com.android.internal.annotations.VisibleForTesting;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * This service manages enabling and disabling ranges of message identifiers
 * that the radio should listen for. It operates independently of the other
 * services and runs at boot time and after exiting airplane mode.
 *
 * Note that the entire range of emergency channels is enabled. Test messages
 * and lower priority broadcasts are filtered out in CellBroadcastAlertService
 * if the user has not enabled them in settings.
 *
 * TODO: add notification to re-enable channels after a radio reset.
 */
public class CellBroadcastConfigService extends IntentService {
    private static final String TAG = "CellBroadcastConfigService";

    @VisibleForTesting
    public static final String ACTION_ENABLE_CHANNELS = "ACTION_ENABLE_CHANNELS";

    public CellBroadcastConfigService() {
        super(TAG);          // use class name for worker thread name
    }

    @Override
    protected void onHandleIntent(Intent intent) {
        if (ACTION_ENABLE_CHANNELS.equals(intent.getAction())) {
            try {
                SubscriptionManager subManager = (SubscriptionManager) getApplicationContext()
                        .getSystemService(Context.TELEPHONY_SUBSCRIPTION_SERVICE);

                if (subManager != null) {
                    // Retrieve all the active subscription indice and enable cell broadcast
                    // messages on all subs. The duplication detection will be done at the
                    // frameworks.
                    int[] subIds = getActiveSubIdList(subManager);
                    if (subIds.length != 0) {
                        for (int subId : subIds) {
                            log("Enable CellBroadcast on sub " + subId);
                            enableCellBroadcastChannels(subId);
                        }
                    } else {
                        // For no sim scenario.
                        enableCellBroadcastChannels(SubscriptionManager.DEFAULT_SUBSCRIPTION_ID);
                    }
                }
            } catch (Exception ex) {
                Log.e(TAG, "exception enabling cell broadcast channels", ex);
            }
        }
    }

    @NonNull
    private int[] getActiveSubIdList(SubscriptionManager subMgr) {
        List<SubscriptionInfo> subInfos = subMgr.getActiveSubscriptionInfoList();
        int size = subInfos != null ? subInfos.size() : 0;
        int[] subIds = new int[size];
        for (int i = 0; i < size; i++) {
            subIds[i] = subInfos.get(i).getSubscriptionId();
        }
        return subIds;
    }

    private void resetCellBroadcastChannels(int subId) {
        SmsManager manager;
        if (subId != SubscriptionManager.DEFAULT_SUBSCRIPTION_ID) {
            manager = SmsManager.getSmsManagerForSubscriptionId(subId);
        } else {
            manager = SmsManager.getDefault();
        }

        // TODO: Call manager.resetAllCellBroadcastRanges() in Android S.
        try {
            Method method = SmsManager.class.getDeclaredMethod("resetAllCellBroadcastRanges");
            method.invoke(manager);
        } catch (Exception e) {
            log("Can't reset cell broadcast ranges. e=" + e);
        }
    }

    /**
     * Enable cell broadcast messages channels. Messages can be only received on the
     * enabled channels.
     *
     * @param subId Subscription index
     */
    @VisibleForTesting
    public void enableCellBroadcastChannels(int subId) {
        resetCellBroadcastChannels(subId);

        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
        Resources res = CellBroadcastSettings.getResources(this, subId);

        // boolean for each user preference checkbox, true for checked, false for unchecked
        // Note: If enableAlertsMasterToggle is false, it disables ALL emergency broadcasts
        // except for CMAS presidential. i.e. to receive CMAS severe alerts, both
        // enableAlertsMasterToggle AND enableCmasSevereAlerts must be true.
        boolean enableAlertsMasterToggle = prefs.getBoolean(
                CellBroadcastSettings.KEY_ENABLE_ALERTS_MASTER_TOGGLE, true);

        boolean enableEtwsAlerts = enableAlertsMasterToggle;

        // CMAS Presidential must be always on (See 3GPP TS 22.268 Section 6.2) regardless
        // user's preference
        boolean enablePresidential = true;

        boolean enableCmasExtremeAlerts = enableAlertsMasterToggle && prefs.getBoolean(
                CellBroadcastSettings.KEY_ENABLE_CMAS_EXTREME_THREAT_ALERTS, true);

        boolean enableCmasSevereAlerts = enableAlertsMasterToggle && prefs.getBoolean(
                CellBroadcastSettings.KEY_ENABLE_CMAS_SEVERE_THREAT_ALERTS, true);

        boolean enableCmasAmberAlerts = enableAlertsMasterToggle && prefs.getBoolean(
                CellBroadcastSettings.KEY_ENABLE_CMAS_AMBER_ALERTS, true);

        boolean enableTestAlerts = enableAlertsMasterToggle
                && CellBroadcastSettings.isTestAlertsToggleVisible(getApplicationContext())
                && prefs.getBoolean(CellBroadcastSettings.KEY_ENABLE_TEST_ALERTS, false);

        boolean enableAreaUpdateInfoAlerts = res.getBoolean(
                R.bool.config_showAreaUpdateInfoSettings)
                && prefs.getBoolean(CellBroadcastSettings.KEY_ENABLE_AREA_UPDATE_INFO_ALERTS,
                false);

        boolean enablePublicSafetyMessagesChannelAlerts = enableAlertsMasterToggle
                && prefs.getBoolean(CellBroadcastSettings.KEY_ENABLE_PUBLIC_SAFETY_MESSAGES,
                true);
        boolean enableStateLocalTestAlerts = enableAlertsMasterToggle
                && prefs.getBoolean(CellBroadcastSettings.KEY_ENABLE_STATE_LOCAL_TEST_ALERTS,
                false);

        boolean enableEmergencyAlerts = enableAlertsMasterToggle && prefs.getBoolean(
                CellBroadcastSettings.KEY_ENABLE_EMERGENCY_ALERTS, true);

        boolean enableGeoFencingTriggerMessage = true;

        if (VDBG) {
            log("enableAlertsMasterToggle = " + enableAlertsMasterToggle);
            log("enableEtwsAlerts = " + enableEtwsAlerts);
            log("enablePresidential = " + enablePresidential);
            log("enableCmasExtremeAlerts = " + enableCmasExtremeAlerts);
            log("enableCmasSevereAlerts = " + enableCmasExtremeAlerts);
            log("enableCmasAmberAlerts = " + enableCmasAmberAlerts);
            log("enableTestAlerts = " + enableTestAlerts);
            log("enableAreaUpdateInfoAlerts = " + enableAreaUpdateInfoAlerts);
            log("enablePublicSafetyMessagesChannelAlerts = "
                    + enablePublicSafetyMessagesChannelAlerts);
            log("enableStateLocalTestAlerts = " + enableStateLocalTestAlerts);
            log("enableEmergencyAlerts = " + enableEmergencyAlerts);
            log("enableGeoFencingTriggerMessage = " + enableGeoFencingTriggerMessage);
        }

        CellBroadcastChannelManager channelManager = new CellBroadcastChannelManager(
                getApplicationContext(), subId);

        /** Enable CMAS series messages. */

        // Enable/Disable Presidential messages.
        setCellBroadcastRange(subId, enablePresidential,
                channelManager.getCellBroadcastChannelRanges(
                        R.array.cmas_presidential_alerts_channels_range_strings));

        // Enable/Disable CMAS extreme messages.
        setCellBroadcastRange(subId, enableCmasExtremeAlerts,
                channelManager.getCellBroadcastChannelRanges(
                        R.array.cmas_alert_extreme_channels_range_strings));

        // Enable/Disable CMAS severe messages.
        setCellBroadcastRange(subId, enableCmasSevereAlerts,
                channelManager.getCellBroadcastChannelRanges(
                        R.array.cmas_alerts_severe_range_strings));

        // Enable/Disable CMAS amber alert messages.
        setCellBroadcastRange(subId, enableCmasAmberAlerts,
                channelManager.getCellBroadcastChannelRanges(
                        R.array.cmas_amber_alerts_channels_range_strings));

        // Enable/Disable test messages.
        setCellBroadcastRange(subId, enableTestAlerts,
                channelManager.getCellBroadcastChannelRanges(
                        R.array.required_monthly_test_range_strings));

        // Exercise is part of test toggle with monthly test and operator defined. some carriers
        // mandate to show test settings in UI but always enable exercise alert.
        setCellBroadcastRange(subId, enableTestAlerts ||
                        res.getBoolean(R.bool.always_enable_exercise_alert),
                channelManager.getCellBroadcastChannelRanges(
                        R.array.exercise_alert_range_strings));

        setCellBroadcastRange(subId, enableTestAlerts,
                channelManager.getCellBroadcastChannelRanges(
                        R.array.operator_defined_alert_range_strings));

        // Enable/Disable GSM ETWS messages.
        setCellBroadcastRange(subId, enableEtwsAlerts,
                channelManager.getCellBroadcastChannelRanges(
                        R.array.etws_alerts_range_strings));

        // Enable/Disable GSM ETWS test messages.
        setCellBroadcastRange(subId, enableTestAlerts,
                channelManager.getCellBroadcastChannelRanges(
                        R.array.etws_test_alerts_range_strings));

        // Enable/Disable GSM public safety messages.
        setCellBroadcastRange(subId, enablePublicSafetyMessagesChannelAlerts,
                channelManager.getCellBroadcastChannelRanges(
                        R.array.public_safety_messages_channels_range_strings));

        // Enable/Disable GSM state/local test alerts.
        setCellBroadcastRange(subId, enableStateLocalTestAlerts,
                channelManager.getCellBroadcastChannelRanges(
                        R.array.state_local_test_alert_range_strings));

        // Enable/Disable GSM geo-fencing trigger messages.
        setCellBroadcastRange(subId, enableGeoFencingTriggerMessage,
                channelManager.getCellBroadcastChannelRanges(
                        R.array.geo_fencing_trigger_messages_range_strings));

        // Enable non-CMAS series messages.
        setCellBroadcastRange(subId, enableEmergencyAlerts,
                channelManager.getCellBroadcastChannelRanges(
                        R.array.emergency_alerts_channels_range_strings));

        // Enable/Disable additional channels based on carrier specific requirement.
        ArrayList<CellBroadcastChannelRange> ranges =
                channelManager.getCellBroadcastChannelRanges(
                        R.array.additional_cbs_channels_strings);

        for (CellBroadcastChannelRange range: ranges) {
            boolean enableAlerts;
            switch (range.mAlertType) {
                case AREA:
                    enableAlerts = enableAreaUpdateInfoAlerts;
                    break;
                case TEST:
                    enableAlerts = enableTestAlerts;
                    break;
                default:
                    enableAlerts = enableAlertsMasterToggle;
            }
            setCellBroadcastRange(subId, enableAlerts, new ArrayList<>(Arrays.asList(range)));
        }
    }
    /**
     * Enable/disable cell broadcast with messages id range
     * @param subId Subscription index
     * @param enable True for enabling cell broadcast with id range, otherwise for disabling.
     * @param ranges Cell broadcast id ranges
     */
    private void setCellBroadcastRange(int subId, boolean enable,
                                       List<CellBroadcastChannelRange> ranges) {
        SmsManager manager;
        if (subId != SubscriptionManager.DEFAULT_SUBSCRIPTION_ID) {
            manager = SmsManager.getSmsManagerForSubscriptionId(subId);
        } else {
            manager = SmsManager.getDefault();
        }

        if (ranges != null) {
            for (CellBroadcastChannelRange range: ranges) {
                if (enable) {
                    manager.enableCellBroadcastRange(range.mStartId, range.mEndId, range.mRanType);
                } else {
                    manager.disableCellBroadcastRange(range.mStartId, range.mEndId, range.mRanType);
                }
            }
        }
    }

    private static void log(String msg) {
        Log.d(TAG, msg);
    }
}
