/*
 * Copyright (C) 2015 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.phone.settings;

import android.content.Context;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.PersistableBundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.preference.SwitchPreference;
import android.provider.Settings;
import android.telephony.AccessNetworkConstants;
import android.telephony.CarrierConfigManager;
import android.telephony.PhoneStateListener;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.util.Log;

import com.android.ims.ImsManager;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneFactory;
import com.android.internal.telephony.SubscriptionController;
import com.android.phone.PhoneGlobals;
import com.android.phone.R;

import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

public class AccessibilitySettingsFragment extends PreferenceFragment {
    private static final String LOG_TAG = AccessibilitySettingsFragment.class.getSimpleName();
    private static final boolean DBG = (PhoneGlobals.DBG_LEVEL >= 2);

    private static final String BUTTON_TTY_KEY = "button_tty_mode_key";
    private static final String BUTTON_HAC_KEY = "button_hac_key";
    private static final String BUTTON_RTT_KEY = "button_rtt_key";
    private static final String RTT_INFO_PREF = "button_rtt_more_information_key";

    private static final int WFC_QUERY_TIMEOUT_MILLIS = 20;

    private final PhoneStateListener mPhoneStateListener = new PhoneStateListener() {
        /**
         * Disable the TTY setting when in/out of a call (and if carrier doesn't
         * support VoLTE with TTY).
         * @see android.telephony.PhoneStateListener#onCallStateChanged(int,
         * java.lang.String)
         */
        @Override
        public void onCallStateChanged(int state, String incomingNumber) {
            if (DBG) Log.d(LOG_TAG, "PhoneStateListener.onCallStateChanged: state=" + state);
            Preference pref = getPreferenceScreen().findPreference(BUTTON_TTY_KEY);
            if (pref != null) {
                // Use TelephonyManager#getCallState instead of 'state' parameter because
                // needs to check the current state of all phone calls to
                // support multi sim configuration.
                TelephonyManager telephonyManager =
                        (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
                final boolean isVolteTtySupported = isVolteTtySupportedInAnySlot();
                pref.setEnabled((isVolteTtySupported && !isVideoCallOrConferenceInProgress())
                        || (telephonyManager.getCallState() == TelephonyManager.CALL_STATE_IDLE));
            }
        }
    };

    private Context mContext;
    private AudioManager mAudioManager;

    private TtyModeListPreference mButtonTty;
    private SwitchPreference mButtonHac;
    private SwitchPreference mButtonRtt;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mContext = getActivity().getApplicationContext();
        mAudioManager = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);

        addPreferencesFromResource(R.xml.accessibility_settings);

        mButtonTty = (TtyModeListPreference) findPreference(
                getResources().getString(R.string.tty_mode_key));
        mButtonHac = (SwitchPreference) findPreference(BUTTON_HAC_KEY);
        mButtonRtt = (SwitchPreference) findPreference(BUTTON_RTT_KEY);

        if (PhoneGlobals.getInstance().phoneMgr.isTtyModeSupported() && isTtySupportedByCarrier()) {
            mButtonTty.init();
        } else {
            getPreferenceScreen().removePreference(mButtonTty);
            mButtonTty = null;
        }

        if (PhoneGlobals.getInstance().phoneMgr.isHearingAidCompatibilitySupported()) {
            int hac = Settings.System.getInt(mContext.getContentResolver(),
                    Settings.System.HEARING_AID, SettingsConstants.HAC_DISABLED);
            mButtonHac.setChecked(hac == SettingsConstants.HAC_ENABLED);
        } else {
            getPreferenceScreen().removePreference(mButtonHac);
            mButtonHac = null;
        }

        if (shouldShowRttSetting()) {
            TelephonyManager tm =
                    (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
            boolean isRoaming = tm.isNetworkRoaming(
                    SubscriptionManager.getDefaultVoiceSubscriptionId());
            boolean alwaysAllowWhileRoaming = isCarrierAllowRttWhenRoaming(
                    SubscriptionManager.getDefaultVoiceSubscriptionId());

            boolean shouldDisableBecauseRoamingOffWfc =
                    (isRoaming && !isOnWfc()) && !alwaysAllowWhileRoaming;

            if (shouldDisableBecauseRoamingOffWfc) {
                mButtonRtt.setSummary(TextUtils.concat(getText(R.string.rtt_mode_summary), "\n",
                        getText(R.string.no_rtt_when_roaming)));
            }
            boolean rttOn = Settings.Secure.getInt(
                    mContext.getContentResolver(), Settings.Secure.RTT_CALLING_MODE, 0) != 0;
            mButtonRtt.setChecked(rttOn);
        } else {
            getPreferenceScreen().removePreference(mButtonRtt);
            Preference rttInfoPref = findPreference(RTT_INFO_PREF);
            getPreferenceScreen().removePreference(rttInfoPref);
            mButtonRtt = null;
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        TelephonyManager tm =
                (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
        tm.listen(mPhoneStateListener, PhoneStateListener.LISTEN_CALL_STATE);
    }

    @Override
    public void onPause() {
        super.onPause();
        TelephonyManager tm =
                (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
        tm.listen(mPhoneStateListener, PhoneStateListener.LISTEN_NONE);
    }

    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen, Preference preference) {
        if (preference == mButtonTty) {
            return true;
        } else if (preference == mButtonHac) {
            int hac = mButtonHac.isChecked()
                    ? SettingsConstants.HAC_ENABLED : SettingsConstants.HAC_DISABLED;
            // Update HAC value in Settings database.
            Settings.System.putInt(mContext.getContentResolver(), Settings.System.HEARING_AID, hac);

            // Update HAC Value in AudioManager.
            mAudioManager.setParameters(
                    SettingsConstants.HAC_KEY + "=" + (hac == SettingsConstants.HAC_ENABLED
                            ? SettingsConstants.HAC_VAL_ON : SettingsConstants.HAC_VAL_OFF));
            return true;
        } else if (preference == mButtonRtt) {
            Log.i(LOG_TAG, "RTT setting changed -- now " + mButtonRtt.isChecked());
            int rttMode = mButtonRtt.isChecked() ? 1 : 0;
            Settings.Secure.putInt(mContext.getContentResolver(), Settings.Secure.RTT_CALLING_MODE,
                    rttMode);
            // Update RTT config with IMS Manager if the always-on carrier config isn't set to true.
            CarrierConfigManager configManager = (CarrierConfigManager) mContext.getSystemService(
                            Context.CARRIER_CONFIG_SERVICE);
            for (int subId : SubscriptionController.getInstance().getActiveSubIdList(true)) {
                if (!configManager.getConfigForSubId(subId).getBoolean(
                        CarrierConfigManager.KEY_IGNORE_RTT_MODE_SETTING_BOOL, false)) {
                    int phoneId = SubscriptionController.getInstance().getPhoneId(subId);
                    ImsManager imsManager = ImsManager.getInstance(getContext(), phoneId);
                    imsManager.setRttEnabled(mButtonRtt.isChecked());
                }
            }
            return true;
        }

        return false;
    }

    private boolean isVolteTtySupportedInAnySlot() {
        final Phone[] phones = PhoneFactory.getPhones();
        if (phones == null) {
            if (DBG) Log.d(LOG_TAG, "isVolteTtySupportedInAnySlot: No phones found.");
            return false;
        }

        CarrierConfigManager configManager =
                (CarrierConfigManager) mContext.getSystemService(Context.CARRIER_CONFIG_SERVICE);
        for (Phone phone : phones) {
            // Check if this phone supports VoLTE.
            ImsManager imsManager = ImsManager.getInstance(mContext, phone.getPhoneId());
            boolean volteEnabled = false;
            if (imsManager != null) {
                volteEnabled = imsManager.isVolteEnabledByPlatform();
            }

            // Check if this phone suports VoLTE TTY.
            boolean volteTtySupported = false;
            PersistableBundle carrierConfig = configManager.getConfigForSubId(phone.getSubId());
            if (carrierConfig != null) {
                volteTtySupported = carrierConfig.getBoolean(
                        CarrierConfigManager.KEY_CARRIER_VOLTE_TTY_SUPPORTED_BOOL);
            }

            if (volteEnabled && volteTtySupported) {
                // VoLTE TTY is supported on this phone that also suports VoLTE.
                return true;
            }
        }
        // VoLTE TTY was not supported on any phone that also supports VoLTE.
        return false;
    }

    private boolean isVideoCallOrConferenceInProgress() {
        final Phone[] phones = PhoneFactory.getPhones();
        if (phones == null) {
            if (DBG) Log.d(LOG_TAG, "isVideoCallOrConferenceInProgress: No phones found.");
            return false;
        }

        for (Phone phone : phones) {
            if (phone.isImsVideoCallOrConferencePresent()) {
                return true;
            }
        }
        return false;
    }

    private boolean isOnWfc() {
        LinkedBlockingQueue<Integer> result = new LinkedBlockingQueue<>(1);
        Executor executor = Executors.newSingleThreadExecutor();
        mContext.getSystemService(android.telephony.ims.ImsManager.class)
                .getImsMmTelManager(SubscriptionManager.getDefaultSubscriptionId())
                .getRegistrationTransportType(executor, result::offer);
        try {
            Integer transportType = result.poll(WFC_QUERY_TIMEOUT_MILLIS, TimeUnit.MILLISECONDS);
            return transportType != null
                    && transportType == AccessNetworkConstants.TRANSPORT_TYPE_WLAN;
        } catch (InterruptedException e) {
            return false;
        }
    }

    private boolean shouldShowRttSetting() {
        // Go through all the subs -- if we want to display the RTT setting for any of them, do
        // display it.
        for (int subId : SubscriptionController.getInstance().getActiveSubIdList(true)) {
            if (PhoneGlobals.getInstance().phoneMgr.isRttSupported(subId)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Determines if the device supports TTY per carrier config.
     * @return {@code true} if the carrier supports TTY, {@code false} otherwise.
     */
    private boolean isTtySupportedByCarrier() {
        CarrierConfigManager configManager =
                (CarrierConfigManager) mContext.getSystemService(Context.CARRIER_CONFIG_SERVICE);
        return configManager.getConfig().getBoolean(
                CarrierConfigManager.KEY_TTY_SUPPORTED_BOOL);
    }

    /**
     * Determines from carrier config whether to always allow RTT while roaming.
     */
    private boolean isCarrierAllowRttWhenRoaming(int subId) {
        PersistableBundle b =
                PhoneGlobals.getInstance().getCarrierConfigForSubId(subId);
        return b.getBoolean(CarrierConfigManager.KEY_RTT_SUPPORTED_WHILE_ROAMING_BOOL);
    }
}
