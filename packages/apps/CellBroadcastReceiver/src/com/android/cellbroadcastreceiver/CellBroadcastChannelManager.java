/*
 * Copyright (C) 2016 The Android Open Source Project
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

import static android.telephony.ServiceState.ROAMING_TYPE_NOT_ROAMING;

import android.annotation.NonNull;
import android.content.Context;
import android.telephony.AccessNetworkConstants;
import android.telephony.NetworkRegistrationInfo;
import android.telephony.ServiceState;
import android.telephony.SmsCbMessage;
import android.telephony.TelephonyManager;
import android.util.Log;

import com.android.cellbroadcastreceiver.CellBroadcastAlertService.AlertType;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * CellBroadcastChannelManager handles the additional cell broadcast channels that
 * carriers might enable through resources.
 * Syntax: "<channel id range>:[type=<alert type>], [emergency=true/false]"
 * For example,
 * <string-array name="additional_cbs_channels_strings" translatable="false">
 *     <item>"43008:type=earthquake, emergency=true"</item>
 *     <item>"0xAFEE:type=tsunami, emergency=true"</item>
 *     <item>"0xAC00-0xAFED:type=other"</item>
 *     <item>"1234-5678"</item>
 * </string-array>
 * If no tones are specified, the alert type will be set to DEFAULT. If emergency is not set,
 * by default it's not emergency.
 */
public class CellBroadcastChannelManager {

    private static final String TAG = "CBChannelManager";

    private static List<Integer> sCellBroadcastRangeResourceKeys = new ArrayList<>(
            Arrays.asList(R.array.additional_cbs_channels_strings,
                    R.array.emergency_alerts_channels_range_strings,
                    R.array.cmas_presidential_alerts_channels_range_strings,
                    R.array.cmas_alert_extreme_channels_range_strings,
                    R.array.cmas_alerts_severe_range_strings,
                    R.array.cmas_amber_alerts_channels_range_strings,
                    R.array.required_monthly_test_range_strings,
                    R.array.exercise_alert_range_strings,
                    R.array.operator_defined_alert_range_strings,
                    R.array.etws_alerts_range_strings,
                    R.array.etws_test_alerts_range_strings,
                    R.array.public_safety_messages_channels_range_strings,
                    R.array.state_local_test_alert_range_strings
            ));

    private static ArrayList<CellBroadcastChannelRange> sAllCellBroadcastChannelRanges = null;

    private final Context mContext;

    private final int mSubId;

    /**
     * Cell broadcast channel range
     * A range is consisted by starting channel id, ending channel id, and the alert type
     */
    public static class CellBroadcastChannelRange {
        /** Defines the type of the alert. */
        private static final String KEY_TYPE = "type";
        /** Defines if the alert is emergency. */
        private static final String KEY_EMERGENCY = "emergency";
        /** Defines the network RAT for the alert. */
        private static final String KEY_RAT = "rat";
        /** Defines the scope of the alert. */
        private static final String KEY_SCOPE = "scope";
        /** Defines the vibration pattern of the alert. */
        private static final String KEY_VIBRATION = "vibration";
        /** Defines the duration of the alert. */
        private static final String KEY_ALERT_DURATION = "alert_duration";
        /** Defines if Do Not Disturb should be overridden for this alert */
        private static final String KEY_OVERRIDE_DND = "override_dnd";
        /** Defines whether writing alert message should exclude from SMS inbox. */
        private static final String KEY_EXCLUDE_FROM_SMS_INBOX = "exclude_from_sms_inbox";

        /**
         * Defines whether the channel needs language filter or not. True indicates that the alert
         * will only pop-up when the alert's language matches the device's language.
         */
        private static final String KEY_FILTER_LANGUAGE = "filter_language";


        public static final int SCOPE_UNKNOWN       = 0;
        public static final int SCOPE_CARRIER       = 1;
        public static final int SCOPE_DOMESTIC      = 2;
        public static final int SCOPE_INTERNATIONAL = 3;

        public static final int LEVEL_UNKNOWN          = 0;
        public static final int LEVEL_NOT_EMERGENCY    = 1;
        public static final int LEVEL_EMERGENCY        = 2;

        public int mStartId;
        public int mEndId;
        public AlertType mAlertType;
        public int mEmergencyLevel;
        public int mRanType;
        public int mScope;
        public int[] mVibrationPattern;
        public boolean mFilterLanguage;
        // by default no custom alert duration. play the alert tone with the tone's duration.
        public int mAlertDuration = -1;
        public boolean mOverrideDnd = false;
        // If enable_write_alerts_to_sms_inbox is true, write to sms inbox is enabled by default
        // for all channels except for channels which explicitly set to exclude from sms inbox.
        public boolean mWriteToSmsInbox = true;

        public CellBroadcastChannelRange(Context context, int subId, String channelRange) {

            mAlertType = AlertType.DEFAULT;
            mEmergencyLevel = LEVEL_UNKNOWN;
            mRanType = SmsCbMessage.MESSAGE_FORMAT_3GPP;
            mScope = SCOPE_UNKNOWN;
            mVibrationPattern =
                    CellBroadcastSettings.getResources(context, subId)
                            .getIntArray(R.array.default_vibration_pattern);
            mFilterLanguage = false;

            int colonIndex = channelRange.indexOf(':');
            if (colonIndex != -1) {
                // Parse the alert type and emergency flag
                String[] pairs = channelRange.substring(colonIndex + 1).trim().split(",");
                for (String pair : pairs) {
                    pair = pair.trim();
                    String[] tokens = pair.split("=");
                    if (tokens.length == 2) {
                        String key = tokens[0].trim();
                        String value = tokens[1].trim();
                        switch (key) {
                            case KEY_TYPE:
                                mAlertType = AlertType.valueOf(value.toUpperCase());
                                break;
                            case KEY_EMERGENCY:
                                if (value.equalsIgnoreCase("true")) {
                                    mEmergencyLevel = LEVEL_EMERGENCY;
                                } else if (value.equalsIgnoreCase("false")) {
                                    mEmergencyLevel = LEVEL_NOT_EMERGENCY;
                                }
                                break;
                            case KEY_RAT:
                                mRanType = value.equalsIgnoreCase("cdma")
                                        ? SmsCbMessage.MESSAGE_FORMAT_3GPP2 :
                                        SmsCbMessage.MESSAGE_FORMAT_3GPP;
                                break;
                            case KEY_SCOPE:
                                if (value.equalsIgnoreCase("carrier")) {
                                    mScope = SCOPE_CARRIER;
                                } else if (value.equalsIgnoreCase("domestic")) {
                                    mScope = SCOPE_DOMESTIC;
                                } else if (value.equalsIgnoreCase("international")) {
                                    mScope = SCOPE_INTERNATIONAL;
                                }
                                break;
                            case KEY_VIBRATION:
                                String[] vibration = value.split("\\|");
                                if (vibration.length > 0) {
                                    mVibrationPattern = new int[vibration.length];
                                    for (int i = 0; i < vibration.length; i++) {
                                        mVibrationPattern[i] = Integer.parseInt(vibration[i]);
                                    }
                                }
                                break;
                            case KEY_FILTER_LANGUAGE:
                                if (value.equalsIgnoreCase("true")) {
                                    mFilterLanguage = true;
                                }
                                break;
                            case KEY_ALERT_DURATION:
                                mAlertDuration = Integer.parseInt(value);
                                break;
                            case KEY_OVERRIDE_DND:
                                if (value.equalsIgnoreCase("true")) {
                                    mOverrideDnd = true;
                                }
                                break;
                            case KEY_EXCLUDE_FROM_SMS_INBOX:
                                if (value.equalsIgnoreCase("true")) {
                                    mWriteToSmsInbox = false;
                                }
                                break;
                        }
                    }
                }
                channelRange = channelRange.substring(0, colonIndex).trim();
            }

            // Parse the channel range
            int dashIndex = channelRange.indexOf('-');
            if (dashIndex != -1) {
                // range that has start id and end id
                mStartId = Integer.decode(channelRange.substring(0, dashIndex).trim());
                mEndId = Integer.decode(channelRange.substring(dashIndex + 1).trim());
            } else {
                // Not a range, only a single id
                mStartId = mEndId = Integer.decode(channelRange);
            }
        }

        @Override
        public String toString() {
            return "Range:[channels=" + mStartId + "-" + mEndId + ",emergency level="
                    + mEmergencyLevel + ",type=" + mAlertType + ",scope=" + mScope + ",vibration="
                    + Arrays.toString(mVibrationPattern) + ",alertDuration=" + mAlertDuration
                    + ",filter_language=" + mFilterLanguage + ",override_dnd=" + mOverrideDnd + "]";
        }
    }

    /**
     * Constructor
     *
     * @param context Context
     * @param subId Subscription index
     */
    public CellBroadcastChannelManager(Context context, int subId) {
        mContext = context;
        mSubId = subId;
    }

    /**
     * Get cell broadcast channels enabled by the carriers from resource key
     *
     * @param key Resource key
     *
     * @return The list of channel ranges enabled by the carriers.
     */
    public @NonNull ArrayList<CellBroadcastChannelRange> getCellBroadcastChannelRanges(int key) {
        ArrayList<CellBroadcastChannelRange> result = new ArrayList<>();
        String[] ranges =
                CellBroadcastSettings.getResources(mContext, mSubId).getStringArray(key);

        for (String range : ranges) {
            try {
                result.add(new CellBroadcastChannelRange(mContext, mSubId, range));
            } catch (Exception e) {
                loge("Failed to parse \"" + range + "\". e=" + e);
            }
        }

        return result;
    }

    /**
     * Get all cell broadcast channels
     *
     * @return all cell broadcast channels
     */
    public @NonNull ArrayList<CellBroadcastChannelRange> getAllCellBroadcastChannelRanges() {
        if (sAllCellBroadcastChannelRanges != null) return sAllCellBroadcastChannelRanges;

        ArrayList<CellBroadcastChannelRange> result = new ArrayList<>();

        for (int key : sCellBroadcastRangeResourceKeys) {
            result.addAll(getCellBroadcastChannelRanges(key));
        }

        sAllCellBroadcastChannelRanges = result;
        return result;
    }

    /**
     * @param channel Cell broadcast message channel
     * @param key Resource key
     *
     * @return {@code TRUE} if the input channel is within the channel range defined from resource.
     * return {@code FALSE} otherwise
     */
    public boolean checkCellBroadcastChannelRange(int channel, int key) {
        ArrayList<CellBroadcastChannelRange> ranges = getCellBroadcastChannelRanges(key);

        for (CellBroadcastChannelRange range : ranges) {
            if (channel >= range.mStartId && channel <= range.mEndId) {
                return checkScope(range.mScope);
            }
        }

        return false;
    }

    /**
     * Check if the channel scope matches the current network condition.
     *
     * @param rangeScope Range scope. Must be SCOPE_CARRIER, SCOPE_DOMESTIC, or SCOPE_INTERNATIONAL.
     * @return True if the scope matches the current network roaming condition.
     */
    public boolean checkScope(int rangeScope) {
        if (rangeScope == CellBroadcastChannelRange.SCOPE_UNKNOWN) return true;
        TelephonyManager tm =
                (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
        tm = tm.createForSubscriptionId(mSubId);
        ServiceState ss = tm.getServiceState();
        if (ss != null) {
            NetworkRegistrationInfo regInfo = ss.getNetworkRegistrationInfo(
                    NetworkRegistrationInfo.DOMAIN_CS,
                    AccessNetworkConstants.TRANSPORT_TYPE_WWAN);
            if (regInfo != null) {
                if (regInfo.getRegistrationState()
                        == NetworkRegistrationInfo.REGISTRATION_STATE_HOME
                        || regInfo.getRegistrationState()
                        == NetworkRegistrationInfo.REGISTRATION_STATE_ROAMING
                        || regInfo.isEmergencyEnabled()) {
                    int voiceRoamingType = regInfo.getRoamingType();
                    if (voiceRoamingType == ROAMING_TYPE_NOT_ROAMING) {
                        return true;
                    } else if (voiceRoamingType == ServiceState.ROAMING_TYPE_DOMESTIC
                            && rangeScope == CellBroadcastChannelRange.SCOPE_DOMESTIC) {
                        return true;
                    } else if (voiceRoamingType == ServiceState.ROAMING_TYPE_INTERNATIONAL
                            && rangeScope == CellBroadcastChannelRange.SCOPE_INTERNATIONAL) {
                        return true;
                    }
                    return false;
                }
            }
        }
        // If we can't determine the scope, for safe we should assume it's in.
        return true;
    }

    /**
     * Return corresponding cellbroadcast range where message belong to
     *
     * @param message Cell broadcast message
     */
    public CellBroadcastChannelRange getCellBroadcastChannelRangeFromMessage(SmsCbMessage message) {
        if (mSubId != message.getSubscriptionId()) {
            Log.e(TAG, "getCellBroadcastChannelRangeFromMessage: This manager is created for "
                    + "sub " + mSubId + ", should not be used for message from sub "
                    + message.getSubscriptionId());
        }

        int channel = message.getServiceCategory();
        ArrayList<CellBroadcastChannelRange> ranges = null;

        for (int key : sCellBroadcastRangeResourceKeys) {
            if (checkCellBroadcastChannelRange(channel, key)) {
                ranges = getCellBroadcastChannelRanges(key);
                break;
            }
        }
        if (ranges != null) {
            for (CellBroadcastChannelRange range : ranges) {
                if (range.mStartId <= message.getServiceCategory()
                        && range.mEndId >= message.getServiceCategory()) {
                    return range;
                }
            }
        }
        return null;
    }

    /**
     * Check if the cell broadcast message is an emergency message or not
     *
     * @param message Cell broadcast message
     * @return True if the message is an emergency message, otherwise false.
     */
    public boolean isEmergencyMessage(SmsCbMessage message) {
        if (message == null) {
            return false;
        }

        if (mSubId != message.getSubscriptionId()) {
            Log.e(TAG, "This manager is created for sub " + mSubId
                    + ", should not be used for message from sub " + message.getSubscriptionId());
        }

        int id = message.getServiceCategory();

        for (int key : sCellBroadcastRangeResourceKeys) {
            ArrayList<CellBroadcastChannelRange> ranges =
                    getCellBroadcastChannelRanges(key);
            for (CellBroadcastChannelRange range : ranges) {
                if (range.mStartId <= id && range.mEndId >= id) {
                    switch (range.mEmergencyLevel) {
                        case CellBroadcastChannelRange.LEVEL_EMERGENCY:
                            Log.d(TAG, "isEmergencyMessage: true, message id = " + id);
                            return true;
                        case CellBroadcastChannelRange.LEVEL_NOT_EMERGENCY:
                            Log.d(TAG, "isEmergencyMessage: false, message id = " + id);
                            return false;
                        case CellBroadcastChannelRange.LEVEL_UNKNOWN:
                        default:
                            break;
                    }
                    break;
                }
            }
        }

        Log.d(TAG, "isEmergencyMessage: " + message.isEmergencyMessage()
                + ", message id = " + id);
        // If the configuration does not specify whether the alert is emergency or not, use the
        // emergency property from the message itself, which is checking if the channel is between
        // MESSAGE_ID_PWS_FIRST_IDENTIFIER (4352) and MESSAGE_ID_PWS_LAST_IDENTIFIER (6399).
        return message.isEmergencyMessage();
    }

    private static void log(String msg) {
        Log.d(TAG, msg);
    }

    private static void loge(String msg) {
        Log.e(TAG, msg);
    }
}
