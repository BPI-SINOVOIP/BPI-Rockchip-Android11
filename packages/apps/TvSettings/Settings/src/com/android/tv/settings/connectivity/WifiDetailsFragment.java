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
 * limitations under the License
 */

package com.android.tv.settings.connectivity;

import static com.android.tv.settings.util.InstrumentationUtils.logEntrySelected;

import android.app.tvsettings.TvSettingsEnums;
import android.content.Context;
import android.net.IpConfiguration.IpAssignment;
import android.net.IpConfiguration.ProxySettings;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.leanback.app.GuidedStepFragment;
import androidx.leanback.widget.GuidanceStylist;
import androidx.leanback.widget.GuidedAction;
import androidx.preference.ListPreference;
import androidx.preference.Preference;

import com.android.internal.logging.nano.MetricsProto;
import com.android.settingslib.core.instrumentation.MetricsFeatureProvider;
import com.android.settingslib.wifi.AccessPoint;
import com.android.tv.settings.R;
import com.android.tv.settings.SettingsPreferenceFragment;

import java.util.List;

/**
 * Fragment for displaying the details of a single wifi network
 */
public class WifiDetailsFragment extends SettingsPreferenceFragment
        implements ConnectivityListener.Listener, ConnectivityListener.WifiNetworkListener {

    private static final String ARG_ACCESS_POINT_STATE = "apBundle";

    private static final String KEY_CONNECTION_STATUS = "connection_status";
    private static final String KEY_IP_ADDRESS = "ip_address";
    private static final String KEY_MAC_ADDRESS = "mac_address";
    private static final String KEY_SIGNAL_STRENGTH = "signal_strength";
    private static final String KEY_RANDOM_MAC = "random_mac";
    private static final String KEY_PROXY_SETTINGS = "proxy_settings";
    private static final String KEY_IP_SETTINGS = "ip_settings";
    private static final String KEY_FORGET_NETWORK = "forget_network";

    private static final String VALUE_MAC_RANDOM = "random";
    private static final String VALUE_MAC_DEVICE = "device";

    private Preference mConnectionStatusPref;
    private Preference mIpAddressPref;
    private Preference mMacAddressPref;
    private Preference mSignalStrengthPref;
    private ListPreference mRandomMacPref;
    private Preference mProxySettingsPref;
    private Preference mIpSettingsPref;
    private Preference mForgetNetworkPref;

    private ConnectivityListener mConnectivityListener;
    private AccessPoint mAccessPoint;

    public static void prepareArgs(@NonNull Bundle args, AccessPoint accessPoint) {
        final Bundle apBundle = new Bundle();
        accessPoint.saveWifiState(apBundle);
        args.putParcelable(ARG_ACCESS_POINT_STATE, apBundle);
    }

    @Override
    public int getMetricsCategory() {
        return MetricsProto.MetricsEvent.WIFI_NETWORK_DETAILS;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        mConnectivityListener = new ConnectivityListener(getContext(), this, getLifecycle());
        mAccessPoint = new AccessPoint(getContext(),
                getArguments().getBundle(ARG_ACCESS_POINT_STATE));
        super.onCreate(savedInstanceState);
    }

    @Override
    public void onStart() {
        super.onStart();
        mConnectivityListener.setWifiListener(this);
    }

    @Override
    public void onResume() {
        super.onResume();
        update();
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.wifi_details, null);

        getPreferenceScreen().setTitle(mAccessPoint.getSsid());

        mConnectionStatusPref = findPreference(KEY_CONNECTION_STATUS);
        mIpAddressPref = findPreference(KEY_IP_ADDRESS);
        mMacAddressPref = findPreference(KEY_MAC_ADDRESS);
        mSignalStrengthPref = findPreference(KEY_SIGNAL_STRENGTH);
        mRandomMacPref = (ListPreference) findPreference(KEY_RANDOM_MAC);
        mProxySettingsPref = findPreference(KEY_PROXY_SETTINGS);
        mIpSettingsPref = findPreference(KEY_IP_SETTINGS);
        mForgetNetworkPref = findPreference(KEY_FORGET_NETWORK);
    }

    @Override
    public void onConnectivityChange() {
        update();
    }

    @Override
    public void onWifiListChanged() {
        final List<AccessPoint> accessPoints = mConnectivityListener.getAvailableNetworks();
        for (final AccessPoint accessPoint : accessPoints) {
            if (TextUtils.equals(mAccessPoint.getSsidStr(), accessPoint.getSsidStr())
                    && mAccessPoint.getSecurity() == accessPoint.getSecurity()) {
                // Make sure we're not holding on to the one we inflated from the bundle, because
                // it won't be updated
                mAccessPoint = accessPoint;
                break;
            }
        }
        update();
    }

    private void update() {
        if (!isAdded()) {
            return;
        }

        final boolean active = mAccessPoint.isActive();

        mConnectionStatusPref.setSummary(active ? R.string.connected : R.string.not_connected);
        mIpAddressPref.setVisible(active);
        mSignalStrengthPref.setVisible(active);

        if (active) {
            mIpAddressPref.setSummary(mConnectivityListener.getWifiIpAddress());
            mSignalStrengthPref.setSummary(getSignalStrength());
        }

        // Mac address related Preferences (info entry and random mac setting entry)
        String macAddress = mConnectivityListener.getWifiMacAddress(mAccessPoint);
        if (active && !TextUtils.isEmpty(macAddress)) {
            mMacAddressPref.setVisible(true);
            updateMacAddressPref(macAddress);
            updateRandomMacPref();
        } else {
            mMacAddressPref.setVisible(false);
            mRandomMacPref.setVisible(false);
        }

        WifiConfiguration wifiConfiguration = mAccessPoint.getConfig();
        if (wifiConfiguration != null) {
            final int networkId = wifiConfiguration.networkId;
            ProxySettings proxySettings = wifiConfiguration.getIpConfiguration().getProxySettings();
            mProxySettingsPref.setSummary(proxySettings == ProxySettings.NONE
                    ? R.string.wifi_action_proxy_none : R.string.wifi_action_proxy_manual);
            mProxySettingsPref.setIntent(EditProxySettingsActivity.createIntent(getContext(),
                    networkId));

            IpAssignment ipAssignment = wifiConfiguration.getIpConfiguration().getIpAssignment();
            mIpSettingsPref.setSummary(ipAssignment == IpAssignment.STATIC
                            ? R.string.wifi_action_static : R.string.wifi_action_dhcp);
            mIpSettingsPref.setIntent(EditIpSettingsActivity.createIntent(getContext(), networkId));

            mForgetNetworkPref.setFragment(ForgetNetworkConfirmFragment.class.getName());
            ForgetNetworkConfirmFragment.prepareArgs(mForgetNetworkPref.getExtras(), mAccessPoint);
        }

        mProxySettingsPref.setVisible(wifiConfiguration != null);
        mProxySettingsPref.setOnPreferenceClickListener(
                preference -> {
                    logEntrySelected(TvSettingsEnums.NETWORK_AP_INFO_PROXY_SETTINGS);
                    return false;
                });
        mIpSettingsPref.setVisible(wifiConfiguration != null);
        mIpSettingsPref.setOnPreferenceClickListener(
                preference -> {
                    logEntrySelected(TvSettingsEnums.NETWORK_AP_INFO_IP_SETTINGS);
                    return false;
                });
        mForgetNetworkPref.setVisible(wifiConfiguration != null);
        mForgetNetworkPref.setOnPreferenceClickListener(
                preference -> {
                    logEntrySelected(TvSettingsEnums.NETWORK_AP_INFO_FORGET_NETWORK);
                    return false;
                });
    }

    private String getSignalStrength() {
        String[] signalLevels = getResources().getStringArray(R.array.wifi_signal_strength);
        int strength = mConnectivityListener.getWifiSignalStrength(signalLevels.length);
        return signalLevels[strength];
    }

    private void updateMacAddressPref(String macAddress) {
        if (WifiInfo.DEFAULT_MAC_ADDRESS.equals(macAddress)) {
            mMacAddressPref.setSummary(R.string.mac_address_not_available);
        } else {
            mMacAddressPref.setSummary(macAddress);
        }
        if (mAccessPoint == null || mAccessPoint.getConfig() == null) {
            return;
        }
        // For saved Passpoint network, framework doesn't have the field to keep the MAC choice
        // persistently, so Passpoint network will always use the default value so far, which is
        // randomized MAC address, so don't need to modify title.
        if (mAccessPoint.isPasspoint() || mAccessPoint.isPasspointConfig()) {
            return;
        }
        mMacAddressPref.setTitle(
                (mAccessPoint.getConfig().macRandomizationSetting
                        == WifiConfiguration.RANDOMIZATION_PERSISTENT)
                        ? R.string.title_randomized_mac_address
                        : R.string.title_mac_address);
    }

    private void updateRandomMacPref() {
        mRandomMacPref.setVisible(mConnectivityListener.isMacAddressRandomizationSupported());
        boolean isMacRandomized =
                (mConnectivityListener.getWifiMacRandomizationSetting(mAccessPoint)
                        == WifiConfiguration.RANDOMIZATION_PERSISTENT);
        mRandomMacPref.setValue(isMacRandomized ? VALUE_MAC_RANDOM : VALUE_MAC_DEVICE);
        if (mAccessPoint.isEphemeral() || mAccessPoint.isPasspoint()
                || mAccessPoint.isPasspointConfig()) {
            mRandomMacPref.setSelectable(false);
            mRandomMacPref.setSummary(R.string.mac_address_ephemeral_summary);
        } else {
            mRandomMacPref.setSelectable(true);
            mRandomMacPref.setSummary(mRandomMacPref.getEntries()[isMacRandomized ? 0 : 1]);
        }
        mRandomMacPref.setOnPreferenceChangeListener(
                (pref, newValue) -> {
                    mConnectivityListener.applyMacRandomizationSetting(
                            mAccessPoint,
                            VALUE_MAC_RANDOM.equals(newValue));
                    // The above call should trigger a connectivity change which will refresh
                    // the UI.
                    return true;
                });
    }

    @Override
    protected int getPageId() {
        return TvSettingsEnums.NETWORK_AP_INFO;
    }

    public static class ForgetNetworkConfirmFragment extends GuidedStepFragment {

        private AccessPoint mAccessPoint;
        private final MetricsFeatureProvider mMetricsFeatureProvider = new MetricsFeatureProvider();

        public static void prepareArgs(@NonNull Bundle args, AccessPoint accessPoint) {
            final Bundle apBundle = new Bundle();
            accessPoint.saveWifiState(apBundle);
            args.putParcelable(ARG_ACCESS_POINT_STATE, apBundle);
        }

        @Override
        public void onCreate(Bundle savedInstanceState) {
            mAccessPoint = new AccessPoint(getContext(),
                    getArguments().getBundle(ARG_ACCESS_POINT_STATE));
            super.onCreate(savedInstanceState);
        }

        @NonNull
        @Override
        public GuidanceStylist.Guidance onCreateGuidance(Bundle savedInstanceState) {
            return new GuidanceStylist.Guidance(
                    getString(R.string.wifi_forget_network),
                    getString(R.string.wifi_forget_network_description),
                    mAccessPoint.getSsidStr(),
                    getContext().getDrawable(R.drawable.ic_wifi_signal_4_white_132dp));
        }

        @Override
        public void onCreateActions(@NonNull List<GuidedAction> actions,
                Bundle savedInstanceState) {
            final Context context = getContext();
            actions.add(new GuidedAction.Builder(context)
                    .clickAction(GuidedAction.ACTION_ID_OK)
                    .build());
            actions.add(new GuidedAction.Builder(context)
                    .clickAction(GuidedAction.ACTION_ID_CANCEL)
                    .build());
        }

        @Override
        public void onGuidedActionClicked(GuidedAction action) {
            if (action.getId() == GuidedAction.ACTION_ID_OK) {
                WifiManager wifiManager =
                        (WifiManager) getContext().getSystemService(Context.WIFI_SERVICE);
                wifiManager.forget(mAccessPoint.getConfig().networkId, null);
                mMetricsFeatureProvider.action(
                        getContext(), MetricsProto.MetricsEvent.ACTION_WIFI_FORGET);
            }
            getFragmentManager().popBackStack();
        }
    }
}
