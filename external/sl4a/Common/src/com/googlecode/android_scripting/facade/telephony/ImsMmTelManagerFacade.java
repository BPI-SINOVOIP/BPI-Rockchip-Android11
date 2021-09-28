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

package com.googlecode.android_scripting.facade.telephony;

import android.telephony.AccessNetworkConstants;
import android.telephony.ims.ImsException;
import android.telephony.ims.ImsMmTelManager;
import android.telephony.ims.feature.MmTelFeature;

import com.googlecode.android_scripting.Log;
import com.googlecode.android_scripting.facade.FacadeManager;
import com.googlecode.android_scripting.jsonrpc.RpcReceiver;
import com.googlecode.android_scripting.rpc.Rpc;
import com.googlecode.android_scripting.rpc.RpcParameter;

import java.util.concurrent.Executor;
import java.util.concurrent.LinkedBlockingQueue;

/**
 * Exposes ImsMmManager functionality
 */
public class ImsMmTelManagerFacade extends RpcReceiver {

    /**
     * Exposes ImsMmTelManager functionality
     */
    public ImsMmTelManagerFacade(FacadeManager manager) {
        super(manager);
    }

    /**
     * Get whether Advanced Calling is enabled for a subId
     *
     * @param subId The subscription ID of the sim you want to check
     */
    @Rpc(description = "Return True if Enhanced 4g Lte mode is enabled.")
    public boolean imsMmTelIsAdvancedCallingEnabled(
                        @RpcParameter(name = "subId") Integer subId) {
        return ImsMmTelManager.createForSubscriptionId(subId).isAdvancedCallingSettingEnabled();
    }

    /**
     * Set Advanced Calling for a subId
     *
     * @param subId The subscription ID of the sim you want to check
     * @param isEnabled Whether the sim should have Enhanced 4g Lte on or off
     */
    @Rpc(description = "Set Enhanced 4g Lte mode")
    public void imsMmTelSetAdvancedCallingEnabled(
                        @RpcParameter(name = "subId") Integer subId,
                        @RpcParameter(name = "isEnabled") Boolean isEnabled) {
        ImsMmTelManager.createForSubscriptionId(subId).setAdvancedCallingSettingEnabled(isEnabled);
    }

    /**
     * Get whether VoWiFi Roaming setting is enabled for a subId
     *
     * @param subId The subscription ID of the sim you want to check
     */
    @Rpc(description = "Return True if VoWiFi Roaming is enabled.")
    public boolean imsMmTelIsVoWiFiRoamingSettingEnabled(
                        @RpcParameter(name = "subId") Integer subId) {
        return ImsMmTelManager.createForSubscriptionId(subId).isVoWiFiRoamingSettingEnabled();
    }

    /**
     * Set VoWiFi Roaming setting for a subId
     *
     * @param subId The subscription ID of the sim you want to check
     * @param isEnabled Whether the sim should have VoWiFi Roaming on or off
     */
    @Rpc(description = "Set VoWiFi Roaming setting")
    public void imsMmTelSetVoWiFiRoamingSettingEnabled(
                        @RpcParameter(name = "subId") Integer subId,
                        @RpcParameter(name = "isEnabled") Boolean isEnabled) {
        ImsMmTelManager.createForSubscriptionId(subId).setVoWiFiRoamingSettingEnabled(isEnabled);
    }

    /**
     * Get whether VoWiFi setting is enabled for a subId
     *
     * @param subId The subscription ID of the sim you want to check
     */
    @Rpc(description = "Return True if VoWiFi is enabled.")
    public boolean imsMmTelIsVoWiFiSettingEnabled(@RpcParameter(name = "subId") Integer subId) {
        return ImsMmTelManager.createForSubscriptionId(subId).isVoWiFiSettingEnabled();
    }

    /**
     * Set VoWiFi setting for a subId
     *
     * @param subId The subscription ID of the sim you want to check
     * @param isEnabled Whether the sim should have VoWiFi on or off
     */
    @Rpc(description = "Set VoWiFi setting")
    public void imsMmTelSetVoWiFiSettingEnabled(
                        @RpcParameter(name = "subId") Integer subId,
                        @RpcParameter(name = "isEnabled") Boolean isEnabled) {
        ImsMmTelManager.createForSubscriptionId(subId).setVoWiFiSettingEnabled(isEnabled);
    }

    /**
     * Get whether Video Telephony setting is enabled for a subId
     *
     * @param subId The subscription ID of the sim you want to check
     */
    @Rpc(description = "Return True if VT is enabled.")
    public boolean imsMmTelIsVtSettingEnabled(
                        @RpcParameter(name = "subId") Integer subId) {
        return ImsMmTelManager.createForSubscriptionId(subId).isVtSettingEnabled();
    }

    /**
     * Set Video Telephony setting for a subId
     *
     * @param subId The subscription ID of the sim you want to check
     * @param isEnabled Whether the sim should have VT on or off
     */
    @Rpc(description = "Set VT setting")
    public void imsMmTelSetVtSettingEnabled(
                        @RpcParameter(name = "subId") Integer subId,
                        @RpcParameter(name = "isEnabled") Boolean isEnabled) {
        ImsMmTelManager.createForSubscriptionId(subId).setVtSettingEnabled(isEnabled);
    }

    /**
     * Get current VoWiFi Mode Pref for a subId
     *
     * @param subId The subscription ID of the sim you want to check
     */
    @Rpc(description = "Return Preferred WFC Mode if Enabled.")
    public String imsMmTelGetVoWiFiModeSetting(
                        @RpcParameter(name = "subId") Integer subId) {
        return TelephonyUtils.getWfcModeString(
            ImsMmTelManager.createForSubscriptionId(subId).getVoWiFiModeSetting());
    }

    /**
     * Set VoWiFi Mode Pref for a subId
     *
     * @param subId The subscription ID of the sim you want to check
     * @mode mode pref can be one of the following
     * - WIFI_ONLY
     * - WIFI_PREFERRED
     * - CELLULAR_PREFERRED
     */
    @Rpc(description = "Set the Preferred WFC Mode")
    public void imsMmTelSetVoWiFiModeSetting(
                        @RpcParameter(name = "subId") Integer subId,
                        @RpcParameter(name = "mode") String mode)
            throws IllegalArgumentException {

        int mode_val;

        switch (mode.toUpperCase()) {
            case TelephonyConstants.WFC_MODE_WIFI_ONLY:
                mode_val = ImsMmTelManager.WIFI_MODE_WIFI_ONLY;
                break;
            case TelephonyConstants.WFC_MODE_CELLULAR_PREFERRED:
                mode_val = ImsMmTelManager.WIFI_MODE_CELLULAR_PREFERRED;
                break;
            case TelephonyConstants.WFC_MODE_WIFI_PREFERRED:
                mode_val = ImsMmTelManager.WIFI_MODE_WIFI_PREFERRED;
                break;
            default:
                throw new IllegalArgumentException("Invalid WfcMode");
        }

        ImsMmTelManager.createForSubscriptionId(subId).setVoWiFiModeSetting(mode_val);
        return;
    }

    /**
     * Check MmTel capability is supported by the carrier
     *
     * @param subId The subscription ID of the sim you want to check
     * @param capability includes voice, video, sms
     * @param transportType includes wlan, wwan
     */
    @Rpc(description = "Return Preferred WFC Mode if Enabled.")
    public Boolean imsMmTelIsSupported(
                        @RpcParameter(name = "subId") Integer subId,
                        @RpcParameter(name = "capability") String capability,
                        @RpcParameter(name = "transportType") String transportType)
            throws IllegalArgumentException {

        int capability_val;
        int transport_val;

        LinkedBlockingQueue<Boolean> resultQueue = new LinkedBlockingQueue<>(1);

        Executor executor = new Executor() {
            public void execute(Runnable r) {
                Log.d("Running MmTel Executor");
                r.run();
                }
            };

        switch (capability.toUpperCase()) {
            case TelephonyConstants.CAPABILITY_TYPE_VOICE:
                capability_val = MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VOICE;
                break;
            case TelephonyConstants.CAPABILITY_TYPE_VIDEO:
                capability_val = MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VIDEO;
                break;
            case TelephonyConstants.CAPABILITY_TYPE_UT:
                capability_val = MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_UT;
                break;
            case TelephonyConstants.CAPABILITY_TYPE_SMS:
                capability_val = MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_SMS;
                break;
            default:
                throw new IllegalArgumentException("Invalid Capability");
        }

        switch (transportType.toUpperCase()) {
            case TelephonyConstants.TRANSPORT_TYPE_INVALID:
                transport_val = AccessNetworkConstants.TRANSPORT_TYPE_INVALID;
                break;
            case TelephonyConstants.TRANSPORT_TYPE_WWAN:
                transport_val = AccessNetworkConstants.TRANSPORT_TYPE_WWAN;
                break;
            case TelephonyConstants.TRANSPORT_TYPE_WLAN:
                transport_val = AccessNetworkConstants.TRANSPORT_TYPE_WLAN;
                break;
            default:
                throw new IllegalArgumentException("Invalid transportType");
        }

        try {
            ImsMmTelManager.createForSubscriptionId(subId)
                .isSupported(capability_val, transport_val, executor, resultQueue::offer);
        } catch (ImsException e) {
            Log.d("ImsException " + e);
            return false;
        }
        return true;
    }

    @Override
    public void shutdown() {

    }
}
