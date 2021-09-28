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

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Looper;
import android.os.PersistableBundle;
import android.provider.Settings;
import android.telecom.PhoneAccountHandle;
import android.telecom.TelecomManager;
import android.telephony.CarrierConfigManager;
import android.telephony.PhoneStateListener;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import com.android.ims.ImsConfig;
import com.android.ims.ImsManager;
import com.android.car.developeroptions.R;
import com.android.settingslib.core.lifecycle.LifecycleObserver;
import com.android.settingslib.core.lifecycle.events.OnStart;
import com.android.settingslib.core.lifecycle.events.OnStop;

import java.util.List;

/**
 * Preference controller for "Wifi Calling"
 */
public class WifiCallingPreferenceController extends TelephonyBasePreferenceController implements
        LifecycleObserver, OnStart, OnStop {

    @VisibleForTesting
    static final String KEY_PREFERENCE_CATEGORY = "calling_category";

    private TelephonyManager mTelephonyManager;
    @VisibleForTesting
    CarrierConfigManager mCarrierConfigManager;
    @VisibleForTesting
    ImsManager mImsManager;
    @VisibleForTesting
    PhoneAccountHandle mSimCallManager;
    private PhoneCallStateListener mPhoneStateListener;
    private Preference mPreference;
    private boolean mEditableWfcRoamingMode;
    private boolean mUseWfcHomeModeForRoaming;

    public WifiCallingPreferenceController(Context context, String key) {
        super(context, key);
        mCarrierConfigManager = context.getSystemService(CarrierConfigManager.class);
        mTelephonyManager = context.getSystemService(TelephonyManager.class);
        mSimCallManager = context.getSystemService(TelecomManager.class).getSimCallManager();
        mPhoneStateListener = new PhoneCallStateListener(Looper.getMainLooper());
        mEditableWfcRoamingMode = true;
        mUseWfcHomeModeForRoaming = false;
    }

    @Override
    public int getAvailabilityStatus(int subId) {
        return subId != SubscriptionManager.INVALID_SUBSCRIPTION_ID
                && MobileNetworkUtils.isWifiCallingEnabled(mContext,
                SubscriptionManager.getPhoneId(subId))
                ? AVAILABLE
                : UNSUPPORTED_ON_DEVICE;
    }

    @Override
    public void onStart() {
        mPhoneStateListener.register(mSubId);
    }

    @Override
    public void onStop() {
        mPhoneStateListener.unregister();
    }

    @Override
    public void displayPreference(PreferenceScreen screen) {
        super.displayPreference(screen);
        mPreference = screen.findPreference(getPreferenceKey());
        Intent intent = mPreference.getIntent();
        if (intent != null) {
            intent.putExtra(Settings.EXTRA_SUB_ID, mSubId);
        }
        if (!isAvailable()) {
            // Set category as invisible
            final Preference preferenceCateogry = screen.findPreference(KEY_PREFERENCE_CATEGORY);
            if (preferenceCateogry != null) {
                preferenceCateogry.setVisible(false);
            }
        }
    }

    @Override
    public void updateState(Preference preference) {
        super.updateState(preference);
        if (mSimCallManager != null) {
            Intent intent = MobileNetworkUtils.buildPhoneAccountConfigureIntent(mContext,
                    mSimCallManager);
            final PackageManager pm = mContext.getPackageManager();
            List<ResolveInfo> resolutions = pm.queryIntentActivities(intent, 0);
            preference.setTitle(resolutions.get(0).loadLabel(pm));
            preference.setSummary(null);
            preference.setIntent(intent);
        } else {
            final String title = SubscriptionManager.getResourcesForSubId(mContext, mSubId)
                    .getString(R.string.wifi_calling_settings_title);
            preference.setTitle(title);
            int resId = com.android.internal.R.string.wifi_calling_off_summary;
            if (mImsManager.isWfcEnabledByUser()) {
                boolean wfcRoamingEnabled = mEditableWfcRoamingMode && !mUseWfcHomeModeForRoaming;
                final boolean isRoaming = mTelephonyManager.isNetworkRoaming();
                int wfcMode = mImsManager.getWfcMode(isRoaming && wfcRoamingEnabled);
                switch (wfcMode) {
                    case ImsConfig.WfcModeFeatureValueConstants.WIFI_ONLY:
                        resId = com.android.internal.R.string.wfc_mode_wifi_only_summary;
                        break;
                    case ImsConfig.WfcModeFeatureValueConstants.CELLULAR_PREFERRED:
                        resId = com.android.internal.R.string
                                .wfc_mode_cellular_preferred_summary;
                        break;
                    case ImsConfig.WfcModeFeatureValueConstants.WIFI_PREFERRED:
                        resId = com.android.internal.R.string.wfc_mode_wifi_preferred_summary;
                        break;
                    default:
                        break;
                }
            }
            preference.setSummary(resId);
        }
        preference.setEnabled(
                mTelephonyManager.getCallState(mSubId) == TelephonyManager.CALL_STATE_IDLE);
    }

    public void init(int subId) {
        mSubId = subId;
        mTelephonyManager = TelephonyManager.from(mContext).createForSubscriptionId(mSubId);
        mImsManager = ImsManager.getInstance(mContext, SubscriptionManager.getPhoneId(mSubId));
        if (mCarrierConfigManager != null) {
            final PersistableBundle carrierConfig = mCarrierConfigManager.getConfigForSubId(mSubId);
            if (carrierConfig != null) {
                mEditableWfcRoamingMode = carrierConfig.getBoolean(
                        CarrierConfigManager.KEY_EDITABLE_WFC_ROAMING_MODE_BOOL);
                mUseWfcHomeModeForRoaming = carrierConfig.getBoolean(
                        CarrierConfigManager
                                .KEY_USE_WFC_HOME_NETWORK_MODE_IN_ROAMING_NETWORK_BOOL);
            }
        }
    }

    private class PhoneCallStateListener extends PhoneStateListener {

        public PhoneCallStateListener(Looper looper) {
            super(looper);
        }

        @Override
        public void onCallStateChanged(int state, String incomingNumber) {
            updateState(mPreference);
        }

        public void register(int subId) {
            mSubId = subId;
            mTelephonyManager.listen(this, PhoneStateListener.LISTEN_CALL_STATE);
        }

        public void unregister() {
            mTelephonyManager.listen(this, PhoneStateListener.LISTEN_NONE);
        }
    }
}
