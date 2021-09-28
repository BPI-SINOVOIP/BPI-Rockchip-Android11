/*
 * Copyright (c) 2015, Motorola Mobility LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     - Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     - Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     - Neither the name of Motorola Mobility nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MOTOROLA MOBILITY LLC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

package com.android.service.ims.presence;

import android.content.Context;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.ims.ProvisioningManager;

import com.android.ims.internal.Logger;

import java.util.List;

public class PresenceSetting {
    private static Logger logger = Logger.getLogger("PresenceSetting");
    private static Context sContext = null;

    // Default result for getCapabilityPollInterval in seconds, equates to 7 days.
    private static final long DEFAULT_CAPABILITY_POLL_INTERVAL_SEC = 604800L;
    // Default result for getCapabilityCacheExpiration in seconds, equates to 90 days.
    private static final long DEFAULT_CAPABILITY_CACHE_EXPIRATION_SEC = 604800L;
    // Default result for getPublishTimer in seconds, equates to 20 minutes.
    private static final int DEFAULT_PUBLISH_TIMER_SEC = 1200;
    // Default result for getPublishTimerExtended in seconds, equates to 1 day.
    private static final int DEFAULT_PUBLISH_TIMER_EXTENDED_SEC = 86400;
    // Default result for getMaxNumberOfEntriesInRequestContainedList, 100 entries.
    private static final int DEFAULT_NUM_ENTRIES_IN_RCL = 100;
    // Default result for getCapabilityPollListSubscriptionExpiration in seconds, equates to 30
    // seconds.
    private static final int DEFAULT_CAPABILITY_POLL_LIST_SUB_EXPIRATION_SEC = 30;


    public static void init(Context context) {
        sContext = context;
    }

    public static long getCapabilityPollInterval(int subId) {
        long value = -1;
        if (sContext != null) {
            if (!SubscriptionManager.isValidSubscriptionId(subId)) {
                return DEFAULT_CAPABILITY_POLL_INTERVAL_SEC;
            }
            try {
            ProvisioningManager pm = ProvisioningManager.createForSubscriptionId(subId);
            value = pm.getProvisioningIntValue(
                    ProvisioningManager.KEY_RCS_CAPABILITIES_POLL_INTERVAL_SEC);
            } catch (Exception ex) {
                logger.warn("getCapabilityPollInterval, exception = " + ex.getMessage());
            }
        }
        if (value <= 0) {
            value = DEFAULT_CAPABILITY_POLL_INTERVAL_SEC;
            logger.error("Failed to get CapabilityPollInterval, the default: " + value);
        }
        return value;
    }

    public static long getCapabilityCacheExpiration(int subId) {
        long value = -1;
        if (sContext != null) {
            if (!SubscriptionManager.isValidSubscriptionId(subId)) {
                return DEFAULT_CAPABILITY_CACHE_EXPIRATION_SEC;
            }
            try {
                ProvisioningManager pm = ProvisioningManager.createForSubscriptionId(subId);
                value = pm.getProvisioningIntValue(
                        ProvisioningManager.KEY_RCS_CAPABILITIES_CACHE_EXPIRATION_SEC);
            } catch (Exception ex) {
                logger.warn("getCapabilityCacheExpiration, exception = " + ex.getMessage());
            }
        }
        if (value <= 0) {
            value = DEFAULT_CAPABILITY_CACHE_EXPIRATION_SEC;
            logger.error("Failed to get CapabilityCacheExpiration, the default: " + value);
        }
        return value;
    }

    public static int getPublishTimer(int subId) {
        int value = -1;
        if (sContext != null) {
            if (!SubscriptionManager.isValidSubscriptionId(subId)) {
                return DEFAULT_PUBLISH_TIMER_SEC;
            }
            try {
                ProvisioningManager pm = ProvisioningManager.createForSubscriptionId(subId);
                value = pm.getProvisioningIntValue(ProvisioningManager.KEY_RCS_PUBLISH_TIMER_SEC);
            } catch (Exception ex) {
                logger.warn("getPublishTimer, exception = " + ex.getMessage());
            }
        }
        if (value <= 0) {
            value = DEFAULT_PUBLISH_TIMER_SEC;
            logger.error("Failed to get PublishTimer, the default: " + value);
        }
        return value;
    }

    public static int getPublishTimerExtended(int subId) {
        int value = -1;
        if (sContext != null) {
            if (!SubscriptionManager.isValidSubscriptionId(subId)) {
                return DEFAULT_PUBLISH_TIMER_EXTENDED_SEC;
            }
            try {
                ProvisioningManager pm = ProvisioningManager.createForSubscriptionId(subId);
                value = pm.getProvisioningIntValue(
                        ProvisioningManager.
                                KEY_RCS_PUBLISH_OFFLINE_AVAILABILITY_TIMER_SEC);
            } catch (Exception ex) {
                logger.warn("getPublishTimerExtended, exception = " + ex.getMessage());
            }
        }
        if (value <= 0) {
            value = DEFAULT_PUBLISH_TIMER_EXTENDED_SEC;
            logger.error("Failed to get PublishTimerExtended, the default: " + value);
        }
        return value;
    }

    public static int getMaxNumberOfEntriesInRequestContainedList(int subId) {
        int value = -1;
        if (sContext != null) {
            if (!SubscriptionManager.isValidSubscriptionId(subId)) {
                return DEFAULT_NUM_ENTRIES_IN_RCL;
            }
            try {
                ProvisioningManager pm = ProvisioningManager.createForSubscriptionId(subId);
                value = pm.getProvisioningIntValue(
                        ProvisioningManager.KEY_RCS_MAX_NUM_ENTRIES_IN_RCL);
            } catch (Exception ex) {
                logger.warn("getMaxNumberOfEntriesInRequestContainedList, exception = "
                        + ex.getMessage());
            }
        }
        if (value <= 0) {
            value = DEFAULT_NUM_ENTRIES_IN_RCL;
            logger.error("Failed to get MaxNumEntriesInRCL, the default: " + value);
        }
        return value;
    }

    public static int getCapabilityPollListSubscriptionExpiration(int subId) {
        int value = -1;
        if (sContext != null) {
            if (!SubscriptionManager.isValidSubscriptionId(subId)) {
                return DEFAULT_CAPABILITY_POLL_LIST_SUB_EXPIRATION_SEC;
            }
            try {
                ProvisioningManager pm = ProvisioningManager.createForSubscriptionId(subId);
                value = pm.getProvisioningIntValue(
                        ProvisioningManager.KEY_RCS_CAPABILITY_POLL_LIST_SUB_EXP_SEC);
            } catch (Exception ex) {
                logger.warn("getMaxNumberOfEntriesInRequestContainedList, exception = "
                        + ex.getMessage());
            }
        }
        if (value <= 0) {
            value = DEFAULT_CAPABILITY_POLL_LIST_SUB_EXPIRATION_SEC;
            logger.error("Failed to get CapabPollListSubExp, the default: " + value);
        }
        return value;
    }

    /**
     * Get VoLte provisioning configuration for the default voice subscription.
     * @return {@link ProvisioningManager#PROVISIONING_VALUE_ENABLED} if VoLTE is provisioned,
     * {@link ProvisioningManager#PROVISIONING_VALUE_DISABLED} if VoLTE is not provisioned, and
     * {@link ProvisioningManager#PROVISIONING_RESULT_UNKNOWN} if there is no value set.
     */
    public static int getVoLteProvisioningConfig(int subId) {
        try {
            ProvisioningManager pm = ProvisioningManager.createForSubscriptionId(subId);
            return pm.getProvisioningIntValue(
                    ProvisioningManager.KEY_VOLTE_PROVISIONING_STATUS);
        } catch (Exception e) {
            logger.warn("getVoLteProvisioningConfig, exception accessing ProvisioningManager, "
                    + "exception=" + e.getMessage());
            return ProvisioningManager.PROVISIONING_RESULT_UNKNOWN;
        }
    }

    /**
     * Get VT provisioning configuration for the default voice subscription.
     * @return {@link ProvisioningManager#PROVISIONING_VALUE_ENABLED} if VT is provisioned,
     * {@link ProvisioningManager#PROVISIONING_VALUE_DISABLED} if VT is not provisioned, nd
     *      * {@link ProvisioningManager#PROVISIONING_RESULT_UNKNOWN} if there is no value set.
     */
    public static int getVtProvisioningConfig(int subId) {
        try {
            ProvisioningManager pm = ProvisioningManager.createForSubscriptionId(subId);
            return pm.getProvisioningIntValue(
                    ProvisioningManager.KEY_VT_PROVISIONING_STATUS);
        } catch (Exception e) {
            logger.warn("getVtProvisioningConfig, exception accessing ProvisioningManager, "
                    + "exception=" + e.getMessage());
            return ProvisioningManager.PROVISIONING_RESULT_UNKNOWN;
        }
    }

    /**
     * Get EAB provisioning configuration for the default voice subscription.
     * @return {@link ProvisioningManager#PROVISIONING_VALUE_ENABLED} if EAB is provisioned,
     * {@link ProvisioningManager#PROVISIONING_VALUE_DISABLED} if EAB is not provisioned, nd
     *      * {@link ProvisioningManager#PROVISIONING_RESULT_UNKNOWN} if there is no value set.
     */
    public static int getEabProvisioningConfig(int subId) {
        try {
            ProvisioningManager pm = ProvisioningManager.createForSubscriptionId(subId);
            return pm.getProvisioningIntValue(
                    ProvisioningManager.KEY_EAB_PROVISIONING_STATUS);
        } catch (Exception e) {
            logger.warn("getEabProvisioningConfig, exception accessing ProvisioningManager, "
                    + "exception=" + e.getMessage());
            return ProvisioningManager.PROVISIONING_RESULT_UNKNOWN;
        }
    }

    public static int getDefaultSubscriptionId() {
        SubscriptionManager sm = sContext.getSystemService(SubscriptionManager.class);
        if (sm == null) return SubscriptionManager.INVALID_SUBSCRIPTION_ID;
        List<SubscriptionInfo> infos = sm.getActiveSubscriptionInfoList();
        if (infos == null || infos.isEmpty()) {
            // There are no active subscriptions right now.
            return SubscriptionManager.INVALID_SUBSCRIPTION_ID;
        }
        // This code does not support MSIM unfortunately, so only provide presence on the default
        // voice subscription that the user chose.
        int defaultSub = SubscriptionManager.getDefaultVoiceSubscriptionId();
        if (!SubscriptionManager.isValidSubscriptionId(defaultSub)) {
            // The voice sub may not have been specified, in this case, use the default data.
            defaultSub = SubscriptionManager.getDefaultDataSubscriptionId();
        }
        // If the user has no default set, just pick the first as backup.
        if (!SubscriptionManager.isValidSubscriptionId(defaultSub)) {
            for (SubscriptionInfo info : infos) {
                if (!info.isOpportunistic()) {
                    defaultSub = info.getSubscriptionId();
                    break;
                }
            }
        }
        return defaultSub;
    }
}

