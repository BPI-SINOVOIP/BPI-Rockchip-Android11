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

package com.android.car.developeroptions.network.telephony;

import static android.provider.Telephony.Carriers.ENFORCE_MANAGED_URI;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.database.Cursor;
import android.os.PersistableBundle;
import android.os.SystemProperties;
import android.provider.Settings;
import android.telecom.PhoneAccountHandle;
import android.telecom.TelecomManager;
import android.telephony.CarrierConfigManager;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.telephony.euicc.EuiccManager;
import android.telephony.ims.feature.ImsFeature;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.VisibleForTesting;

import com.android.ims.ImsException;
import com.android.ims.ImsManager;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneConstants;
import com.android.internal.util.ArrayUtils;
import com.android.car.developeroptions.core.BasePreferenceController;

import java.util.Arrays;
import java.util.List;

public class MobileNetworkUtils {

    private static final String TAG = "MobileNetworkUtils";

    // CID of the device.
    private static final String KEY_CID = "ro.boot.cid";
    // CIDs of devices which should not show anything related to eSIM.
    private static final String KEY_ESIM_CID_IGNORE = "ro.setupwizard.esim_cid_ignore";
    // System Property which is used to decide whether the default eSIM UI will be shown,
    // the default value is false.
    private static final String KEY_ENABLE_ESIM_UI_BY_DEFAULT =
            "esim.enable_esim_system_ui_by_default";
    private static final String LEGACY_ACTION_CONFIGURE_PHONE_ACCOUNT =
            "android.telecom.action.CONNECTION_SERVICE_CONFIGURE";

    /**
     * Returns if DPC APNs are enforced.
     */
    public static boolean isDpcApnEnforced(Context context) {
        try (Cursor enforceCursor = context.getContentResolver().query(ENFORCE_MANAGED_URI,
                null, null, null, null)) {
            if (enforceCursor == null || enforceCursor.getCount() != 1) {
                return false;
            }
            enforceCursor.moveToFirst();
            return enforceCursor.getInt(0) > 0;
        }
    }

    /**
     * Returns true if Wifi calling is enabled for at least one phone.
     */
    public static boolean isWifiCallingEnabled(Context context) {
        int phoneCount = context.getSystemService(TelephonyManager.class).getPhoneCount();
        for (int i = 0; i < phoneCount; i++) {
            if (isWifiCallingEnabled(context, i)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Returns true if Wifi calling is enabled for the specific phone with id {@code phoneId}.
     */
    public static boolean isWifiCallingEnabled(Context context, int phoneId) {
        final PhoneAccountHandle simCallManager =
                TelecomManager.from(context).getSimCallManager();

        boolean isWifiCallingEnabled;
        if (simCallManager != null) {
            Intent intent = buildPhoneAccountConfigureIntent(
                    context, simCallManager);

            isWifiCallingEnabled = intent != null;
        } else {
            ImsManager imsMgr = ImsManager.getInstance(context, phoneId);
            isWifiCallingEnabled = imsMgr != null
                    && imsMgr.isWfcEnabledByPlatform()
                    && imsMgr.isWfcProvisionedOnDevice()
                    && isImsServiceStateReady(imsMgr);
        }

        return isWifiCallingEnabled;
    }

    @VisibleForTesting
    static Intent buildPhoneAccountConfigureIntent(
            Context context, PhoneAccountHandle accountHandle) {
        Intent intent = buildConfigureIntent(
                context, accountHandle, TelecomManager.ACTION_CONFIGURE_PHONE_ACCOUNT);

        if (intent == null) {
            // If the new configuration didn't work, try the old configuration intent.
            intent = buildConfigureIntent(context, accountHandle,
                    LEGACY_ACTION_CONFIGURE_PHONE_ACCOUNT);
        }
        return intent;
    }

    private static Intent buildConfigureIntent(
            Context context, PhoneAccountHandle accountHandle, String actionStr) {
        if (accountHandle == null || accountHandle.getComponentName() == null
                || TextUtils.isEmpty(accountHandle.getComponentName().getPackageName())) {
            return null;
        }

        // Build the settings intent.
        Intent intent = new Intent(actionStr);
        intent.setPackage(accountHandle.getComponentName().getPackageName());
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        intent.putExtra(TelecomManager.EXTRA_PHONE_ACCOUNT_HANDLE, accountHandle);

        // Check to see that the phone account package can handle the setting intent.
        PackageManager pm = context.getPackageManager();
        List<ResolveInfo> resolutions = pm.queryIntentActivities(intent, 0);
        if (resolutions.size() == 0) {
            intent = null;  // set no intent if the package cannot handle it.
        }

        return intent;
    }

    public static boolean isImsServiceStateReady(ImsManager imsMgr) {
        boolean isImsServiceStateReady = false;

        try {
            if (imsMgr != null && imsMgr.getImsServiceState() == ImsFeature.STATE_READY) {
                isImsServiceStateReady = true;
            }
        } catch (ImsException ex) {
            Log.e(TAG, "Exception when trying to get ImsServiceStatus: " + ex);
        }

        Log.d(TAG, "isImsServiceStateReady=" + isImsServiceStateReady);
        return isImsServiceStateReady;
    }

    /**
     * Whether to show the entry point to eUICC settings.
     *
     * <p>We show the entry point on any device which supports eUICC as long as either the eUICC
     * was ever provisioned (that is, at least one profile was ever downloaded onto it), or if
     * the user has enabled development mode.
     */
    public static boolean showEuiccSettings(Context context) {
        EuiccManager euiccManager =
                (EuiccManager) context.getSystemService(EuiccManager.class);
        if (!euiccManager.isEnabled()) {
            return false;
        }

        final ContentResolver cr = context.getContentResolver();

        TelephonyManager tm =
                (TelephonyManager) context.getSystemService(TelephonyManager.class);
        String currentCountry = tm.getNetworkCountryIso().toLowerCase();
        String supportedCountries =
                Settings.Global.getString(cr, Settings.Global.EUICC_SUPPORTED_COUNTRIES);
        boolean inEsimSupportedCountries = false;
        if (TextUtils.isEmpty(currentCountry)) {
            inEsimSupportedCountries = true;
        } else if (!TextUtils.isEmpty(supportedCountries)) {
            List<String> supportedCountryList =
                    Arrays.asList(TextUtils.split(supportedCountries.toLowerCase(), ","));
            if (supportedCountryList.contains(currentCountry)) {
                inEsimSupportedCountries = true;
            }
        }
        final boolean esimIgnoredDevice =
                Arrays.asList(TextUtils.split(SystemProperties.get(KEY_ESIM_CID_IGNORE, ""), ","))
                        .contains(SystemProperties.get(KEY_CID, null));
        final boolean enabledEsimUiByDefault =
                SystemProperties.getBoolean(KEY_ENABLE_ESIM_UI_BY_DEFAULT, true);
        final boolean euiccProvisioned =
                Settings.Global.getInt(cr, Settings.Global.EUICC_PROVISIONED, 0) != 0;
        final boolean inDeveloperMode =
                Settings.Global.getInt(cr, Settings.Global.DEVELOPMENT_SETTINGS_ENABLED, 0) != 0;

        return (inDeveloperMode || euiccProvisioned
                || (!esimIgnoredDevice && enabledEsimUiByDefault && inEsimSupportedCountries));
    }

    /**
     * Set whether to enable data for {@code subId}, also whether to disable data for other
     * subscription
     */
    public static void setMobileDataEnabled(Context context, int subId, boolean enabled,
            boolean disableOtherSubscriptions) {
        final TelephonyManager telephonyManager = context.getSystemService(TelephonyManager.class)
                .createForSubscriptionId(subId);
        final SubscriptionManager subscriptionManager = context.getSystemService(
                SubscriptionManager.class);
        telephonyManager.setDataEnabled(enabled);

        if (disableOtherSubscriptions) {
            List<SubscriptionInfo> subInfoList =
                    subscriptionManager.getActiveSubscriptionInfoList(true);
            if (subInfoList != null) {
                for (SubscriptionInfo subInfo : subInfoList) {
                    // We never disable mobile data for opportunistic subscriptions.
                    if (subInfo.getSubscriptionId() != subId && !subInfo.isOpportunistic()) {
                        context.getSystemService(TelephonyManager.class).createForSubscriptionId(
                                subInfo.getSubscriptionId()).setDataEnabled(false);
                    }
                }
            }
        }
    }

    /**
     * Return {@code true} if show CDMA category
     */
    public static boolean isCdmaOptions(Context context, int subId) {
        if (subId == SubscriptionManager.INVALID_SUBSCRIPTION_ID) {
            return false;
        }
        final TelephonyManager telephonyManager = context.getSystemService(TelephonyManager.class)
                .createForSubscriptionId(subId);
        final PersistableBundle carrierConfig = context.getSystemService(
                CarrierConfigManager.class).getConfigForSubId(subId);


        if (telephonyManager.getPhoneType() == PhoneConstants.PHONE_TYPE_CDMA) {
            return true;
        } else if (carrierConfig != null
                && !carrierConfig.getBoolean(
                CarrierConfigManager.KEY_HIDE_CARRIER_NETWORK_SETTINGS_BOOL)
                && carrierConfig.getBoolean(CarrierConfigManager.KEY_WORLD_PHONE_BOOL)) {
            return true;
        }

        if (isWorldMode(context, subId)) {
            final int settingsNetworkMode = android.provider.Settings.Global.getInt(
                    context.getContentResolver(),
                    android.provider.Settings.Global.PREFERRED_NETWORK_MODE + subId,
                    Phone.PREFERRED_NT_MODE);
            if (settingsNetworkMode == TelephonyManager.NETWORK_MODE_LTE_GSM_WCDMA
                    || settingsNetworkMode == TelephonyManager.NETWORK_MODE_LTE_CDMA_EVDO) {
                return true;
            }

            if (settingsNetworkMode == TelephonyManager.NETWORK_MODE_LTE_CDMA_EVDO_GSM_WCDMA
                    && !isTdscdmaSupported(context, telephonyManager)) {
                return true;
            }
        }

        return false;
    }

    /**
     * return {@code true} if we need show Gsm related settings
     */
    public static boolean isGsmOptions(Context context, int subId) {
        if (subId == SubscriptionManager.INVALID_SUBSCRIPTION_ID) {
            return false;
        }
        if (isGsmBasicOptions(context, subId)) {
            return true;
        }
        final int settingsNetworkMode = android.provider.Settings.Global.getInt(
                context.getContentResolver(),
                android.provider.Settings.Global.PREFERRED_NETWORK_MODE + subId,
                Phone.PREFERRED_NT_MODE);
        if (isWorldMode(context, subId)) {
            if (settingsNetworkMode == TelephonyManager.NETWORK_MODE_LTE_CDMA_EVDO
                    || settingsNetworkMode == TelephonyManager.NETWORK_MODE_LTE_GSM_WCDMA) {
                return true;
            } else if (settingsNetworkMode == TelephonyManager.NETWORK_MODE_LTE_CDMA_EVDO_GSM_WCDMA
                    && !MobileNetworkUtils.isTdscdmaSupported(context, subId)) {
                return true;
            }
        }

        return false;
    }

    private static boolean isGsmBasicOptions(Context context, int subId) {
        final TelephonyManager telephonyManager = context.getSystemService(TelephonyManager.class)
                .createForSubscriptionId(subId);
        final PersistableBundle carrierConfig = context.getSystemService(
                CarrierConfigManager.class).getConfigForSubId(subId);

        if (telephonyManager.getPhoneType() == PhoneConstants.PHONE_TYPE_GSM) {
            return true;
        } else if (carrierConfig != null
                && !carrierConfig.getBoolean(
                CarrierConfigManager.KEY_HIDE_CARRIER_NETWORK_SETTINGS_BOOL)
                && carrierConfig.getBoolean(CarrierConfigManager.KEY_WORLD_PHONE_BOOL)) {
            return true;
        }

        return false;
    }

    /**
     * Return {@code true} if it is world mode, and we may show advanced options in telephony
     * settings
     */
    public static boolean isWorldMode(Context context, int subId) {
        final PersistableBundle carrierConfig = context.getSystemService(
                CarrierConfigManager.class).getConfigForSubId(subId);
        return carrierConfig == null
                ? false
                : carrierConfig.getBoolean(CarrierConfigManager.KEY_WORLD_MODE_ENABLED_BOOL);
    }

    /**
     * Return {@code true} if we need show settings for network selection(i.e. Verizon)
     */
    public static boolean shouldDisplayNetworkSelectOptions(Context context, int subId) {
        final TelephonyManager telephonyManager = TelephonyManager.from(context)
                .createForSubscriptionId(subId);
        final PersistableBundle carrierConfig = context.getSystemService(
                CarrierConfigManager.class).getConfigForSubId(subId);
        if (subId == SubscriptionManager.INVALID_SUBSCRIPTION_ID
                || carrierConfig == null
                || !carrierConfig.getBoolean(
                CarrierConfigManager.KEY_OPERATOR_SELECTION_EXPAND_BOOL)
                || (carrierConfig.getBoolean(CarrierConfigManager.KEY_CSP_ENABLED_BOOL)
                && !telephonyManager.isManualNetworkSelectionAllowed())) {
            return false;
        }

        if (isGsmBasicOptions(context, subId)) {
            return true;
        }

        final int settingsNetworkMode = android.provider.Settings.Global.getInt(
                context.getContentResolver(),
                android.provider.Settings.Global.PREFERRED_NETWORK_MODE + subId,
                Phone.PREFERRED_NT_MODE);
        if (isWorldMode(context, subId)) {
            if (settingsNetworkMode == TelephonyManager.NETWORK_MODE_LTE_GSM_WCDMA) {
                return true;
            }
        }

        return false;
    }

    /**
     * Return {@code true} if Tdscdma is supported in current subscription
     */
    public static boolean isTdscdmaSupported(Context context, int subId) {
        return isTdscdmaSupported(context,
                context.getSystemService(TelephonyManager.class).createForSubscriptionId(subId));
    }

    //TODO(b/117651939): move it to telephony
    private static boolean isTdscdmaSupported(Context context, TelephonyManager telephonyManager) {
        final PersistableBundle carrierConfig = context.getSystemService(
                CarrierConfigManager.class).getConfig();

        if (carrierConfig == null) {
            return false;
        }

        if (carrierConfig.getBoolean(CarrierConfigManager.KEY_SUPPORT_TDSCDMA_BOOL)) {
            return true;
        }

        String operatorNumeric = telephonyManager.getServiceState().getOperatorNumeric();
        String[] numericArray = carrierConfig.getStringArray(
                CarrierConfigManager.KEY_SUPPORT_TDSCDMA_ROAMING_NETWORKS_STRING_ARRAY);
        if (numericArray == null || operatorNumeric == null) {
            return false;
        }
        for (String numeric : numericArray) {
            if (operatorNumeric.equals(numeric)) {
                return true;
            }
        }
        return false;
    }


    /**
     * Return subId that supported by search. If there are more than one, return first one,
     * otherwise return {@link SubscriptionManager#INVALID_SUBSCRIPTION_ID}
     */
    public static int getSearchableSubscriptionId(Context context) {
        final SubscriptionManager subscriptionManager = context.getSystemService(
                SubscriptionManager.class);
        final int subIds[] = subscriptionManager.getActiveSubscriptionIdList();

        return subIds.length >= 1 ? subIds[0] : SubscriptionManager.INVALID_SUBSCRIPTION_ID;
    }

    /**
     * Return availability for a default subscription id. If subId already been set, use it to
     * check, otherwise traverse all active subIds on device to check.
     * @param context context
     * @param defSubId Default subId get from telephony preference controller
     * @param callback Callback to check availability for a specific subId
     * @return Availability
     *
     * @see BasePreferenceController#getAvailabilityStatus()
     */
    public static int getAvailability(Context context, int defSubId,
            TelephonyAvailabilityCallback callback) {
        final SubscriptionManager subscriptionManager = context.getSystemService(
                SubscriptionManager.class);
        if (defSubId != SubscriptionManager.INVALID_SUBSCRIPTION_ID) {
            // If subId has been set, return the corresponding status
            return callback.getAvailabilityStatus(defSubId);
        } else {
            // Otherwise, search whether there is one subId in device that support this preference
            final int[] subIds = subscriptionManager.getActiveSubscriptionIdList();
            if (ArrayUtils.isEmpty(subIds)) {
                return callback.getAvailabilityStatus(
                        SubscriptionManager.INVALID_SUBSCRIPTION_ID);
            } else {
                for (final int subId : subIds) {
                    final int status = callback.getAvailabilityStatus(subId);
                    if (status == BasePreferenceController.AVAILABLE) {
                        return status;
                    }
                }
                return callback.getAvailabilityStatus(subIds[0]);
            }
        }
    }
}
