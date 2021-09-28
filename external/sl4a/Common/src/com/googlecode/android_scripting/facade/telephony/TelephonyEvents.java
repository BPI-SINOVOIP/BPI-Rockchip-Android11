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

package com.googlecode.android_scripting.facade.telephony;

import android.telephony.DataConnectionRealTimeInfo;
import android.telephony.PhysicalChannelConfig;
import android.telephony.PreciseCallState;
import android.telephony.ServiceState;
import android.telephony.TelephonyDisplayInfo;

import com.googlecode.android_scripting.jsonrpc.JsonSerializable;

import java.util.List;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

public class TelephonyEvents {

    public static class CallStateEvent implements JsonSerializable {
        private String mCallState;
        private String mIncomingNumber;
        private int mSubscriptionId;

        CallStateEvent(int state, String incomingNumber, int subscriptionId) {
            mCallState = null;
            mIncomingNumber = TelephonyUtils.formatIncomingNumber(
                    incomingNumber);
            mCallState = TelephonyUtils.getTelephonyCallStateString(
                    state);
            mSubscriptionId = subscriptionId;
        }

        public String getIncomingNumber() {
            return mIncomingNumber;
        }

        public int getSubscriptionId() {
            return mSubscriptionId;
        }

        public JSONObject toJSON() throws JSONException {
            JSONObject callState = new JSONObject();

            callState.put(
                    TelephonyConstants.CallStateContainer.SUBSCRIPTION_ID,
                    mSubscriptionId);
            callState.put(
                    TelephonyConstants.CallStateContainer.INCOMING_NUMBER,
                    mIncomingNumber);
            callState.put(TelephonyConstants.CallStateContainer.CALL_STATE,
                    mCallState);

            return callState;
        }
    }

    public static class PreciseCallStateEvent implements JsonSerializable {
        private PreciseCallState mPreciseCallState;
        private String mPreciseCallStateString;
        private String mType;
        private int mCause;
        private int mSubscriptionId;

        PreciseCallStateEvent(int newState, String type,
                PreciseCallState preciseCallState, int subscriptionId) {
            mPreciseCallStateString = TelephonyUtils.getPreciseCallStateString(
                    newState);
            mPreciseCallState = preciseCallState;
            mType = type;
            mSubscriptionId = subscriptionId;
            mCause = preciseCallState.getPreciseDisconnectCause();
        }

        public String getType() {
            return mType;
        }

        public int getSubscriptionId() {
            return mSubscriptionId;
        }

        public PreciseCallState getPreciseCallState() {
            return mPreciseCallState;
        }

        public int getCause() {
            return mCause;
        }

        public JSONObject toJSON() throws JSONException {
            JSONObject preciseCallState = new JSONObject();

            preciseCallState.put(
                    TelephonyConstants.PreciseCallStateContainer.SUBSCRIPTION_ID,
                    mSubscriptionId);
            preciseCallState.put(
                    TelephonyConstants.PreciseCallStateContainer.TYPE, mType);
            preciseCallState.put(
                    TelephonyConstants.PreciseCallStateContainer.PRECISE_CALL_STATE,
                    mPreciseCallStateString);
            preciseCallState.put(
                    TelephonyConstants.PreciseCallStateContainer.CAUSE, mCause);

            return preciseCallState;
        }
    }

    public static class DataConnectionRealTimeInfoEvent implements JsonSerializable {
        private DataConnectionRealTimeInfo mDataConnectionRealTimeInfo;
        private String mDataConnectionPowerState;
        private int mSubscriptionId;
        private long mTime;

        DataConnectionRealTimeInfoEvent(
                DataConnectionRealTimeInfo dataConnectionRealTimeInfo,
                int subscriptionId) {
            mTime = dataConnectionRealTimeInfo.getTime();
            mSubscriptionId = subscriptionId;
            mDataConnectionPowerState = TelephonyUtils.getDcPowerStateString(
                    dataConnectionRealTimeInfo.getDcPowerState());
            mDataConnectionRealTimeInfo = dataConnectionRealTimeInfo;
        }

        public int getSubscriptionId() {
            return mSubscriptionId;
        }

        public long getTime() {
            return mTime;
        }

        public JSONObject toJSON() throws JSONException {
            JSONObject dataConnectionRealTimeInfo = new JSONObject();

            dataConnectionRealTimeInfo.put(
                    TelephonyConstants.DataConnectionRealTimeInfoContainer.SUBSCRIPTION_ID,
                    mSubscriptionId);
            dataConnectionRealTimeInfo.put(
                    TelephonyConstants.DataConnectionRealTimeInfoContainer.TIME,
                    mTime);
            dataConnectionRealTimeInfo.put(
                    TelephonyConstants.DataConnectionRealTimeInfoContainer.DATA_CONNECTION_POWER_STATE,
                    mDataConnectionPowerState);

            return dataConnectionRealTimeInfo;
        }
    }

    public static class DataConnectionStateEvent implements JsonSerializable {
        private String mDataConnectionState;
        private int mSubscriptionId;
        private int mState;
        private String mDataNetworkType;

        DataConnectionStateEvent(int state, String dataNetworkType,
                int subscriptionId) {
            mSubscriptionId = subscriptionId;
            mDataConnectionState = TelephonyUtils.getDataConnectionStateString(
                    state);
            mDataNetworkType = dataNetworkType;
            mState = state;
        }

        public int getSubscriptionId() {
            return mSubscriptionId;
        }

        public int getState() {
            return mState;
        }

        public String getDataNetworkType() {
            return mDataNetworkType;
        }

        public JSONObject toJSON() throws JSONException {
            JSONObject dataConnectionState = new JSONObject();

            dataConnectionState.put(
                    TelephonyConstants.DataConnectionStateContainer.SUBSCRIPTION_ID,
                    mSubscriptionId);
            dataConnectionState.put(
                    TelephonyConstants.DataConnectionStateContainer.DATA_CONNECTION_STATE,
                    mDataConnectionState);
            dataConnectionState.put(
                    TelephonyConstants.DataConnectionStateContainer.DATA_NETWORK_TYPE,
                    mDataNetworkType);
            dataConnectionState.put(
                    TelephonyConstants.DataConnectionStateContainer.STATE_CODE,
                    mState);

            return dataConnectionState;
        }
    }

    public static class ServiceStateEvent implements JsonSerializable {
        private String mServiceStateString;
        private int mSubscriptionId;
        private ServiceState mServiceState;

        ServiceStateEvent(ServiceState serviceState, int subscriptionId) {
            mServiceState = serviceState;
            mSubscriptionId = subscriptionId;
            mServiceStateString = TelephonyUtils.getNetworkStateString(
                    serviceState.getState());
            if (mServiceStateString.equals(
                    TelephonyConstants.SERVICE_STATE_OUT_OF_SERVICE) &&
                    serviceState.isEmergencyOnly()) {
                mServiceStateString = TelephonyConstants.SERVICE_STATE_EMERGENCY_ONLY;
            }
        }

        public int getSubscriptionId() {
            return mSubscriptionId;
        }

        public ServiceState getServiceState() {
            return mServiceState;
        }

        public JSONObject toJSON() throws JSONException {
            JSONObject serviceState =
                    com.googlecode.android_scripting.jsonrpc.JsonBuilder.buildServiceState(
                            mServiceState);
            serviceState.put(
                    TelephonyConstants.ServiceStateContainer.SUBSCRIPTION_ID,
                    mSubscriptionId);

            return serviceState;
        }
    }

    public static class MessageWaitingIndicatorEvent implements JsonSerializable {
        private boolean mMessageWaitingIndicator;

        MessageWaitingIndicatorEvent(boolean messageWaitingIndicator) {
            mMessageWaitingIndicator = messageWaitingIndicator;
        }

        public boolean getMessageWaitingIndicator() {
            return mMessageWaitingIndicator;
        }

        public JSONObject toJSON() throws JSONException {
            JSONObject messageWaitingIndicator = new JSONObject();

            messageWaitingIndicator.put(
                    TelephonyConstants.MessageWaitingIndicatorContainer.IS_MESSAGE_WAITING,
                    mMessageWaitingIndicator);

            return messageWaitingIndicator;
        }
    }

    public static class DisplayInfoChangedEvent implements JsonSerializable {
        private TelephonyDisplayInfo mDisplayInfoString;
        private int mSubscriptionId;
        private String mOverrideDataNetworkType;
        private String mDataNetworkType;

        DisplayInfoChangedEvent(TelephonyDisplayInfo DisplayInfoString, int subscriptionId) {
            mDisplayInfoString = DisplayInfoString;
            mSubscriptionId = subscriptionId;
            mOverrideDataNetworkType = TelephonyUtils.getDisplayInfoString(
                    DisplayInfoString.getOverrideNetworkType());
            mDataNetworkType = TelephonyUtils.getNetworkTypeString(
                    DisplayInfoString.getNetworkType());
        }

        public String getOverrideDataNetworkType() {
            return mOverrideDataNetworkType;
        }

        public int getSubscriptionId() {
            return mSubscriptionId;
        }

        public String getDataNetworkType() {
            return mDataNetworkType;
        }

        public JSONObject toJSON() throws JSONException {
            JSONObject displayInfoState = new JSONObject();

            displayInfoState.put(
                    TelephonyConstants.DisplayInfoContainer.OVERRIDE,
                    mOverrideDataNetworkType);

            displayInfoState.put(
                    TelephonyConstants.DisplayInfoContainer.NETWORK,
                    mDataNetworkType);

            displayInfoState.put(
                    TelephonyConstants.DisplayInfoContainer.SUBSCRIPTION_ID,
                    mSubscriptionId);

            return displayInfoState;
        }
    }

    public static class PhysicalChannelConfigChangedEvent implements JsonSerializable {
        private final List<PhysicalChannelConfig> mConfigs;

        PhysicalChannelConfigChangedEvent(List<PhysicalChannelConfig> configs) {
            mConfigs = configs;
        }

        List<PhysicalChannelConfig> getConfigs() {
            return mConfigs;
        }

        public JSONObject toJSON() throws JSONException {
            JSONArray jsonConfigs = new JSONArray();
            for(PhysicalChannelConfig c : mConfigs) {
                JSONObject cfg  = new JSONObject();
                cfg.put(
                        TelephonyConstants.PhysicalChannelConfigContainer.CELL_BANDWIDTH_DOWNLINK,
                        c.getCellBandwidthDownlink());
                cfg.put(
                        TelephonyConstants.PhysicalChannelConfigContainer.CONNECTION_STATUS,
                        c.getConnectionStatus());
               jsonConfigs.put(cfg);
            }
            return new JSONObject().put(
                    TelephonyConstants.PhysicalChannelConfigContainer.CONFIGS, jsonConfigs);
        }
    }
}
