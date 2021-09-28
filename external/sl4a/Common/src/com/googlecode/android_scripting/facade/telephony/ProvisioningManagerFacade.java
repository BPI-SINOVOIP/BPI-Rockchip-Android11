/*
 * Copyright (C) 2020 The Android Open Source Project
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

import android.telephony.ims.ProvisioningManager;
import android.telephony.ims.feature.MmTelFeature;
import android.telephony.ims.stub.ImsRegistrationImplBase;

import com.googlecode.android_scripting.facade.FacadeManager;
import com.googlecode.android_scripting.jsonrpc.RpcReceiver;
import com.googlecode.android_scripting.rpc.Rpc;
import com.googlecode.android_scripting.rpc.RpcParameter;

/**
 * Exposes ProvisioningManager functionality
 */
public class ProvisioningManagerFacade extends RpcReceiver {

    public ProvisioningManagerFacade(FacadeManager manager) {
        super(manager);
    }

    /**
     * Get whether VoLTE, WFC, VT is provisioned on device
     *
     * @param subId The subscription ID of the sim you want to check
     * @param capability includes voice, video, sms
     * @param tech includes lte, iwlan
     */
    @Rpc(description = "Return True if Capability is provisioned on device.")
    public boolean provisioningGetProvisioningStatusForCapability(
                @RpcParameter(name = "subId") Integer subId,
                @RpcParameter(name = "capability") String capability,
                @RpcParameter(name = "tech") String tech)
            throws IllegalArgumentException {

        int capability_val;
        int tech_val;

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
                throw new IllegalArgumentException("Invalid CapabilityType");
        }

        switch (tech.toUpperCase()) {
            case TelephonyConstants.REGISTRATION_TECH_LTE:
                tech_val = ImsRegistrationImplBase.REGISTRATION_TECH_LTE;
                break;
            case TelephonyConstants.REGISTRATION_TECH_IWLAN:
                tech_val = ImsRegistrationImplBase.REGISTRATION_TECH_IWLAN;
                break;
            case TelephonyConstants.REGISTRATION_TECH_NONE:
                tech_val = ImsRegistrationImplBase.REGISTRATION_TECH_NONE;
                break;
            default:
                throw new IllegalArgumentException("Invalid TechType");
        }

        return ProvisioningManager.createForSubscriptionId(subId)
            .getProvisioningStatusForCapability(capability_val, tech_val);
    }

    /**
     * Sets provisioned value for VoLTE, WFC, VT on device
     *
     * @param subId The subscription ID of the sim you want to set
     * @param key includes volte, vt
     * @param value includes enable, disable
     */
    @Rpc(description = "Returns CapabilityValue after setting on device.")
    public int provisioningSetProvisioningIntValue(
                @RpcParameter(name = "subId") Integer subId,
                @RpcParameter(name = "key") String key,
                @RpcParameter(name = "value") String value)
            throws IllegalArgumentException {

        int capability_key;
        int capability_val;

        switch (key.toUpperCase()) {
            case TelephonyConstants.KEY_VOLTE_PROVISIONING_STATUS:
                capability_key = ProvisioningManager.KEY_VOLTE_PROVISIONING_STATUS;
                break;
            case TelephonyConstants.KEY_VT_PROVISIONING_STATUS:
                capability_key = ProvisioningManager.KEY_VT_PROVISIONING_STATUS;
                break;
            default:
                throw new IllegalArgumentException("Invalid Key");
        }

        switch (value.toUpperCase()) {
            case TelephonyConstants.PROVISIONING_VALUE_ENABLED:
                capability_val = ProvisioningManager.PROVISIONING_VALUE_ENABLED;
                break;
            case TelephonyConstants.PROVISIONING_VALUE_DISABLED:
                capability_val = ProvisioningManager.PROVISIONING_VALUE_DISABLED;
                break;
            default:
                throw new IllegalArgumentException("Invalid Value");
        }

        return ProvisioningManager.createForSubscriptionId(subId)
                    .setProvisioningIntValue(capability_key, capability_val);
    }

    @Override
    public void shutdown() {

    }
}
