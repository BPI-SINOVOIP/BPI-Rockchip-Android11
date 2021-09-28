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
import static com.android.tv.settings.util.InstrumentationUtils.logToggleInteracted;

import android.app.tvsettings.TvSettingsEnums;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.SystemClock;
import android.provider.Settings;

import androidx.annotation.Keep;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceManager;
import androidx.preference.TwoStatePreference;

import android.annotation.UiThread;
import android.annotation.WorkerThread;
import android.util.ArrayMap;
import android.util.ArraySet;
import com.android.internal.logging.nano.MetricsProto;
import com.android.settingslib.wifi.AccessPoint;
import com.android.settingslib.wifi.AccessPointPreference;
import com.android.tv.settings.R;
import com.android.tv.settings.data.ConstData;
import com.android.tv.settings.vpn.*;
import com.android.tv.settings.SettingsPreferenceFragment;
import com.android.tv.settings.util.SliceUtils;
import com.android.tv.twopanelsettings.slices.SlicePreference;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.ConnectivityManager;
import android.net.IConnectivityManager;
import android.os.Message;
import android.os.UserHandle;
import android.os.UserManager;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.SystemProperties;
import com.android.internal.net.LegacyVpnInfo;
import com.android.internal.net.VpnConfig;
import com.android.internal.net.VpnProfile;
import com.android.internal.util.ArrayUtils;
import com.android.settingslib.RestrictedLockUtils;
import com.google.android.collect.Lists;
import android.security.Credentials;
import android.security.KeyStore;
import android.util.Log;

import java.util.Collection;
import java.util.HashSet;
import java.util.Map;
import java.util.List;
import java.util.Set;
import java.util.ArrayList;
import java.util.Collections;

import static android.app.AppOpsManager.OP_ACTIVATE_VPN;

/**
 * Fragment for controlling network connectivity
 */
@Keep
public class NetworkFragment extends SettingsPreferenceFragment implements
        ConnectivityListener.Listener, ConnectivityListener.WifiNetworkListener,
        AccessPoint.AccessPointListener, Preference.OnPreferenceClickListener, Handler.Callback {
    private static final String TAG = "NetworkFragment";

    private static final String KEY_WIFI_ENABLE = "wifi_enable";
    private static final String KEY_WIFI_LIST = "wifi_list";
    private static final String KEY_WIFI_COLLAPSE = "wifi_collapse";
    private static final String KEY_WIFI_OTHER = "wifi_other";
    private static final String KEY_WIFI_ADD = "wifi_add";
    private static final String KEY_WIFI_ALWAYS_SCAN = "wifi_always_scan";
    private static final String KEY_ETHERNET = "ethernet";
    private static final String KEY_ETHERNET_STATUS = "ethernet_status";
    private static final String KEY_ETHERNET_PROXY = "ethernet_proxy";
    private static final String KEY_ETHERNET_DHCP = "ethernet_dhcp";
    private static final String KEY_DATA_SAVER_SLICE = "data_saver_slice";
    private static final String KEY_DATA_ALERT_SLICE = "data_alert_slice";
    private static final String ACTION_DATA_ALERT_SETTINGS = "android.settings.DATA_ALERT_SETTINGS";
    private static final String KEY_HOTPOT = "hotpot";
    private static final String KEY_VPN = "avaliable_vpns";
    private static final String KEY_EDIT_VPN = "edit_vpn";

    private static final int INITIAL_UPDATE_DELAY = 500;

    private final KeyStore mKeyStore = KeyStore.getInstance();
    private final IConnectivityManager mConnectivityService = IConnectivityManager.Stub
            .asInterface(ServiceManager.getService(Context.CONNECTIVITY_SERVICE));
    private ConnectivityListener mConnectivityListener;
    private WifiManager mWifiManager;
    private ConnectivityManager mConnectivityManager;
    private AccessPointPreference.UserBadgeCache mUserBadgeCache;

    private TwoStatePreference mEnableWifiPref;
    private CollapsibleCategory mWifiNetworksCategory;
    private Preference mCollapsePref;
    private Preference mAddPref;
    private TwoStatePreference mAlwaysScan;
    private PreferenceCategory mEthernetCategory;
    private PreferenceCategory mVpnCategory;
    private Preference mEthernetStatusPref;
    private Preference mEthernetProxyPref;
    private Preference mEthernetDhcpPref;
    private PreferenceCategory mWifiOther;
    private Preference mHotsPot;

    private final Handler mHandler = new Handler();
    private long mNoWifiUpdateBeforeMillis;
    private Preference mVpnCreatePref;
    private LegacyVpnInfo mConnectedLegacyVpn;
    private Handler mUpdater;
    private static final int RESCAN_MESSAGE = 0;
    private static final int RESCAN_INTERVAL_MS = 1000;
    private Map<String, LegacyVpnPreference> mLegacyVpnPreferences = new ArrayMap<>();
    private Runnable mInitialUpdateWifiListRunnable = new Runnable() {
        @Override
        public void run() {
            mNoWifiUpdateBeforeMillis = 0;
            updateWifiList();
        }
    };
    private boolean mIsWifiHardwarePresent;

    public static NetworkFragment newInstance() {
        return new NetworkFragment();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        mIsWifiHardwarePresent = getContext().getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_WIFI);
        mConnectivityListener = new ConnectivityListener(getContext(), this, getLifecycle());
        mWifiManager = getContext().getSystemService(WifiManager.class);
        mConnectivityManager = getContext().getSystemService(ConnectivityManager.class);
        mUserBadgeCache =
                new AccessPointPreference.UserBadgeCache(getContext().getPackageManager());
        super.onCreate(savedInstanceState);
    }

    @Override
    public void onStart() {
        super.onStart();
        mConnectivityListener.setWifiListener(this);
        mNoWifiUpdateBeforeMillis = SystemClock.elapsedRealtime() + INITIAL_UPDATE_DELAY;
        updateWifiList();
    }

    @Override
    public void onResume() {
        super.onResume();
        // There doesn't seem to be an API to listen to everything this could cover, so
        // tickle it here and hope for the best.
        updateConnectivity();
        if (mUpdater == null) {
            mUpdater = new Handler(this);
        }
        mUpdater.sendEmptyMessage(RESCAN_MESSAGE);
    }

    @Override
    public void onPause() {
        super.onPause();
        if (mUpdater != null) {
            mUpdater.removeCallbacksAndMessages(null);
        }
    }

    @Override
    public void onStop() {
        super.onStop();
        mConnectivityListener.stop();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mConnectivityListener != null) {
            mConnectivityListener.destroy();
        }
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getPreferenceManager().setPreferenceComparisonCallback(
                new PreferenceManager.SimplePreferenceComparisonCallback());
        setPreferencesFromResource(R.xml.network, null);

        mEnableWifiPref = (TwoStatePreference) findPreference(KEY_WIFI_ENABLE);
        mWifiNetworksCategory = (CollapsibleCategory) findPreference(KEY_WIFI_LIST);
        mCollapsePref = findPreference(KEY_WIFI_COLLAPSE);
        mAddPref = findPreference(KEY_WIFI_ADD);
        mAlwaysScan = (TwoStatePreference) findPreference(KEY_WIFI_ALWAYS_SCAN);
        mWifiOther = (PreferenceCategory) findPreference(KEY_WIFI_OTHER);
        mHotsPot = findPreference(KEY_HOTPOT);

        mEthernetCategory = (PreferenceCategory) findPreference(KEY_ETHERNET);
        mEthernetStatusPref = findPreference(KEY_ETHERNET_STATUS);
        mEthernetProxyPref = findPreference(KEY_ETHERNET_PROXY);
        mEthernetProxyPref.setIntent(EditProxySettingsActivity.createIntent(getContext(),
                WifiConfiguration.INVALID_NETWORK_ID));
        mEthernetDhcpPref = findPreference(KEY_ETHERNET_DHCP);
        mEthernetDhcpPref.setIntent(EditIpSettingsActivity.createIntent(getContext(),
                WifiConfiguration.INVALID_NETWORK_ID));

        if (!mIsWifiHardwarePresent) {
            mEnableWifiPref.setVisible(false);
            mWifiOther.setVisible(false);
        }
        Preference dataSaverSlicePref = findPreference(KEY_DATA_SAVER_SLICE);
        Preference dataAlertSlicePref = findPreference(KEY_DATA_ALERT_SLICE);
        boolean isDataSaverVisible = isConnected() && SliceUtils.isSliceProviderValid(
                getContext(), ((SlicePreference) dataSaverSlicePref).getUri());
        boolean isDataAlertVisible = isConnected() && SliceUtils.isSliceProviderValid(
                getContext(), ((SlicePreference) dataAlertSlicePref).getUri());

        dataSaverSlicePref.setVisible(isDataSaverVisible);
        dataAlertSlicePref.setVisible(isDataAlertVisible);
        Intent i = getActivity().getIntent();
        if (i != null && i.getAction() != null) {
            if (i.getAction().equals(Settings.ACTION_DATA_SAVER_SETTINGS)
                    && dataSaverSlicePref.isVisible()) {
                mHandler.post(() -> scrollToPreference(dataSaverSlicePref));
            } else if (i.getAction().equals(ACTION_DATA_ALERT_SETTINGS)
                    && dataAlertSlicePref.isVisible()) {
                mHandler.post(() -> scrollToPreference(dataAlertSlicePref));
            }
        }
        mVpnCategory = (PreferenceCategory) findPreference(KEY_VPN);
        getPreferenceScreen().removePreference(mVpnCategory);
        getPreferenceScreen().removePreference((PreferenceCategory) findPreference("vpn"));
        if (!SystemProperties.get("ro.target.product","box").equals("box")) {
            getPreferenceScreen().removePreference(mHotsPot);
        }
    }

    @Override
    public boolean onPreferenceTreeClick(Preference preference) {
        if (preference.getKey() == null) {
            return super.onPreferenceTreeClick(preference);
        }
        switch (preference.getKey()) {
            case KEY_WIFI_ENABLE:
                mConnectivityListener.setWifiEnabled(mEnableWifiPref.isChecked());
                logToggleInteracted(
                        TvSettingsEnums.NETWORK_WIFI_ON_OFF, mEnableWifiPref.isChecked());
                if (mMetricsFeatureProvider != null) {
                    if (mEnableWifiPref.isChecked()) {
                        mMetricsFeatureProvider.action(getContext(),
                                MetricsProto.MetricsEvent.ACTION_WIFI_ON);
                    } else {
                        // Log if user was connected at the time of switching off.
                        mMetricsFeatureProvider.action(getContext(),
                                MetricsProto.MetricsEvent.ACTION_WIFI_OFF,
                                mConnectivityListener.isWifiConnected());
                    }
                }
                return true;
            case KEY_WIFI_COLLAPSE:
                final boolean collapse = !mWifiNetworksCategory.isCollapsed();
                mCollapsePref.setTitle(collapse
                        ? R.string.wifi_setting_see_all : R.string.wifi_setting_see_fewer);
                mWifiNetworksCategory.setCollapsed(collapse);
                logEntrySelected(
                        collapse
                                ? TvSettingsEnums.NETWORK_SEE_FEWER
                                : TvSettingsEnums.NETWORK_SEE_ALL);
                return true;
            case KEY_WIFI_ALWAYS_SCAN:
                Settings.Global.putInt(getActivity().getContentResolver(),
                        Settings.Global.WIFI_SCAN_ALWAYS_AVAILABLE,
                        mAlwaysScan.isChecked() ? 1 : 0);
                logToggleInteracted(
                        TvSettingsEnums.NETWORK_ALWAYS_SCANNING_NETWORKS, mAlwaysScan.isChecked());
                return true;
            case KEY_ETHERNET_STATUS:
                return true;
            case KEY_WIFI_ADD:
                logEntrySelected(TvSettingsEnums.NETWORK_ADD_NEW_NETWORK);
                mMetricsFeatureProvider.action(getActivity(),
                        MetricsProto.MetricsEvent.ACTION_WIFI_ADD_NETWORK);
                break;
        }
        return super.onPreferenceTreeClick(preference);
    }

    private boolean isConnected() {
        NetworkInfo activeNetworkInfo = mConnectivityManager.getActiveNetworkInfo();
        return activeNetworkInfo != null && activeNetworkInfo.isConnected();
    }

    private void updateConnectivity() {
        if (!isAdded()) {
            return;
        }

        final boolean wifiEnabled = mIsWifiHardwarePresent
                && mConnectivityListener.isWifiEnabledOrEnabling();
        mEnableWifiPref.setChecked(wifiEnabled);

        mWifiNetworksCategory.setVisible(wifiEnabled);
        mCollapsePref.setVisible(wifiEnabled && mWifiNetworksCategory.shouldShowCollapsePref());
        mAddPref.setVisible(wifiEnabled);

        if (!wifiEnabled) {
            updateWifiList();
        }

        int scanAlwaysAvailable = 0;
        try {
            scanAlwaysAvailable = Settings.Global.getInt(getContext().getContentResolver(),
                    Settings.Global.WIFI_SCAN_ALWAYS_AVAILABLE);
        } catch (Settings.SettingNotFoundException e) {
            // Ignore
        }
        mAlwaysScan.setChecked(scanAlwaysAvailable == 1);

        final boolean ethernetAvailable = mConnectivityListener.isEthernetAvailable();
        mEthernetCategory.setVisible(ethernetAvailable);
        mEthernetStatusPref.setVisible(ethernetAvailable);
        mEthernetProxyPref.setVisible(ethernetAvailable);
        mEthernetProxyPref.setOnPreferenceClickListener(
                preference -> {
                    logEntrySelected(TvSettingsEnums.NETWORK_ETHERNET_PROXY_SETTINGS);
                    return false;
                });
        mEthernetDhcpPref.setVisible(ethernetAvailable);
        mEthernetDhcpPref.setOnPreferenceClickListener(
                preference -> {
                    logEntrySelected(TvSettingsEnums.NETWORK_ETHERNET_IP_SETTINGS);
                    return false;
                });

        if (ethernetAvailable) {
            final boolean ethernetConnected =
                    mConnectivityListener.isEthernetConnected();
            mEthernetStatusPref.setTitle(ethernetConnected
                    ? R.string.connected : R.string.not_connected);
            mEthernetStatusPref.setSummary(mConnectivityListener.getEthernetIpAddress());
        }
    }

    private void updateVPNList() {
        getActivity().runOnUiThread(new Runnable() {
            public void run() {

                List<VpnProfile> vpnProfiles = loadVpnProfiles(mKeyStore);
                String lockdownVpnKey = VpnUtils.getLockdownVpn();
                final Set<Preference> updates = new ArraySet<>();
                Map<String, LegacyVpnInfo> connectedLegacyVpns = getConnectedLegacyVpns();
                for (VpnProfile profile : vpnProfiles) {
                    LegacyVpnPreference p = findOrCreatePreference(profile);
                    if (connectedLegacyVpns.containsKey(profile.key)) {
                        p.setState(connectedLegacyVpns.get(profile.key).state);
                    } else {
                        p.setState(LegacyVpnPreference.STATE_NONE);
                    }
                    p.setAlwaysOn(lockdownVpnKey != null && lockdownVpnKey.equals(profile.key));
                    updates.add(p);
                }
                mLegacyVpnPreferences.values().retainAll(updates);
                for (int i = mVpnCategory.getPreferenceCount() - 1; i >= 0; i--) {
                    Preference p = mVpnCategory.getPreference(i);
                    if (updates.contains(p)) {
                        updates.remove(p);
                    } /*else if(!"vpn_create".equals(p.getKey())){
                         mVpnCategory.removePreference(p);
                     }*/
                }
                // Show any new preferences on the screen
                for (Preference pref : updates) {
                    mVpnCategory.addPreference(pref);
                }
 /*             Preference vpnCreatePref = new Preference(getPreferenceManager().getContext());
                vpnCreatePref.setTitle(R.string.create_vpn);
                Intent createVpnIntent = new Intent();
                createVpnIntent.setClassName(getActivity().getPackageName(), VpnCreateActivity.class.getName());
                VpnProfile createProfile = new VpnProfile(Long.toHexString(System.currentTimeMillis()));
                createVpnIntent.putExtra(ConstData.IntentKey.VPN_PROFILE, createProfile);
                createVpnIntent.putExtra(ConstData.IntentKey.VPN_EXIST, false);
                createVpnIntent.putExtra(ConstData.IntentKey.VPN_EDITING, true);
                vpnCreatePref.setIntent(createVpnIntent);
                mVpnCategory.addPreference(vpnCreatePref);*/
            }

            ;
        });

    }

    private void updateWifiList() {
        if (!isAdded()) {
            return;
        }

        if (!mIsWifiHardwarePresent || !mConnectivityListener.isWifiEnabledOrEnabling()) {
            mWifiNetworksCategory.removeAll();
            mNoWifiUpdateBeforeMillis = 0;
            return;
        }

        final long now = SystemClock.elapsedRealtime();
        if (mNoWifiUpdateBeforeMillis > now) {
            mHandler.removeCallbacks(mInitialUpdateWifiListRunnable);
            mHandler.postDelayed(mInitialUpdateWifiListRunnable,
                    mNoWifiUpdateBeforeMillis - now);
            return;
        }

        final int existingCount = mWifiNetworksCategory.getRealPreferenceCount();
        final Set<Preference> toRemove = new HashSet<>(existingCount);
        for (int i = 0; i < existingCount; i++) {
            toRemove.add(mWifiNetworksCategory.getPreference(i));
        }

        final Context themedContext = getPreferenceManager().getContext();
        final Collection<AccessPoint> accessPoints = mConnectivityListener.getAvailableNetworks();
        int index = 0;

        for (final AccessPoint accessPoint : accessPoints) {
            accessPoint.setListener(this);
            AccessPointPreference pref = (AccessPointPreference) accessPoint.getTag();
            if (pref == null) {
                pref = new AccessPointPreference(accessPoint, themedContext, mUserBadgeCache,
                        false);
                accessPoint.setTag(pref);
            } else {
                toRemove.remove(pref);
            }
            if (accessPoint.isActive() && !isCaptivePortal(accessPoint)) {
                pref.setFragment(WifiDetailsFragment.class.getName());
                // No need to track entry selection as new page will be focused
                pref.setOnPreferenceClickListener(preference -> false);
                WifiDetailsFragment.prepareArgs(pref.getExtras(), accessPoint);
                pref.setIntent(null);
            } else {
                pref.setFragment(null);
                pref.setIntent(WifiConnectionActivity.createIntent(getContext(), accessPoint));
                pref.setOnPreferenceClickListener(
                        preference -> {
                            logEntrySelected(TvSettingsEnums.NETWORK_NOT_CONNECTED_AP);
                            return false;
                        });
            }
            pref.setOrder(index++);
            // Double-adding is harmless
            mWifiNetworksCategory.addPreference(pref);
        }

        for (final Preference preference : toRemove) {
            mWifiNetworksCategory.removePreference(preference);
        }

        mCollapsePref.setVisible(mWifiNetworksCategory.shouldShowCollapsePref());
    }

    private boolean isCaptivePortal(AccessPoint accessPoint) {
        if (accessPoint.getDetailedState() != NetworkInfo.DetailedState.CONNECTED) {
            return false;
        }
        NetworkCapabilities nc = mConnectivityManager.getNetworkCapabilities(
                mWifiManager.getCurrentNetwork());
        return nc != null && nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_CAPTIVE_PORTAL);
    }

    @Override
    public void onConnectivityChange() {
        updateConnectivity();
    }

    @Override
    public void onWifiListChanged() {
        updateWifiList();
    }

    @Override
    public void onAccessPointChanged(AccessPoint accessPoint) {
        ((AccessPointPreference) accessPoint.getTag()).refresh();
    }

    @Override
    public void onLevelChanged(AccessPoint accessPoint) {
        ((AccessPointPreference) accessPoint.getTag()).onLevelChanged();
    }

    @Override
    public int getMetricsCategory() {
        return MetricsProto.MetricsEvent.SETTINGS_NETWORK_CATEGORY;
    }

    @Override
    protected int getPageId() {
        return TvSettingsEnums.NETWORK;
    }

    static List<VpnProfile> loadVpnProfiles(KeyStore keyStore, int... excludeTypes) {
        final ArrayList<VpnProfile> result = Lists.newArrayList();

        for (String key : keyStore.list(Credentials.VPN)) {
            final VpnProfile profile = VpnProfile.decode(key, keyStore.get(Credentials.VPN + key));
            if (profile != null && !ArrayUtils.contains(excludeTypes, profile.type)) {
                result.add(profile);
            }
        }
        return result;
    }

    @WorkerThread
    private Map<String, LegacyVpnInfo> getConnectedLegacyVpns() {
        try {
            mConnectedLegacyVpn = mConnectivityService.getLegacyVpnInfo(UserHandle.myUserId());
            if (mConnectedLegacyVpn != null) {
                return Collections.singletonMap(mConnectedLegacyVpn.key, mConnectedLegacyVpn);
            }
        } catch (RemoteException e) {
            Log.e(TAG, "Failure updating VPN list with connected legacy VPNs", e);
        }
        return Collections.emptyMap();
    }

    @UiThread
    private LegacyVpnPreference findOrCreatePreference(VpnProfile profile) {
        LegacyVpnPreference pref = mLegacyVpnPreferences.get(profile.key);
        if (pref == null) {
            pref = new LegacyVpnPreference(getPreferenceManager().getContext());
            //pref.setOnGearClickListener(mGearListener);
            pref.setOnPreferenceClickListener(this);
            mLegacyVpnPreferences.put(profile.key, pref);
        }
        // This may change as the profile can update and keep the same key.
        pref.setProfile(profile);
        return pref;
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (preference instanceof LegacyVpnPreference) {
            LegacyVpnPreference pref = (LegacyVpnPreference) preference;
            VpnProfile profile = pref.getProfile();
            if (mConnectedLegacyVpn != null && profile.key.equals(mConnectedLegacyVpn.key) &&
                    mConnectedLegacyVpn.state == LegacyVpnInfo.STATE_CONNECTED) {
                try {
                    mConnectedLegacyVpn.intent.send();
                    return true;
                } catch (Exception e) {
                    Log.w(TAG, "Starting config intent failed", e);
                }
            }
            Intent prefIntent = new Intent();
            prefIntent.setClass(getContext(), VpnCreateActivity.class);
            prefIntent.putExtra(ConstData.IntentKey.VPN_PROFILE, profile);
            prefIntent.putExtra(ConstData.IntentKey.VPN_EDITING, false);
            prefIntent.putExtra(ConstData.IntentKey.VPN_EXIST, true);
            startActivity(prefIntent);
            //ConfigDialogFragment.show(this, profile, false /* editing */, true /* exists */);
            return true;
        } /*else if (preference instanceof AppPreference) {
            AppPreference pref = (AppPreference) preference;
            boolean connected = (pref.getState() == AppPreference.STATE_CONNECTED);

            if (!connected) {
                try {
                    UserHandle user = UserHandle.of(pref.getUserId());
                    Context userContext = getActivity().createPackageContextAsUser(
                            getActivity().getPackageName(), 0  flags , user);
                    PackageManager pm = userContext.getPackageManager();
                    Intent appIntent = pm.getLaunchIntentForPackage(pref.getPackageName());
                    if (appIntent != null) {
                        userContext.startActivityAsUser(appIntent, user);
                        return true;
                    }
                } catch (PackageManager.NameNotFoundException nnfe) {
                    Log.w(LOG_TAG, "VPN provider does not exist: " + pref.getPackageName(), nnfe);
                }
            }

            // Already connected or no launch intent available - show an info dialog
            PackageInfo pkgInfo = pref.getPackageInfo();
            AppDialogFragment.show(this, pkgInfo, pref.getLabel(), false  editing , connected);
            return true;
        }else{
            createVPN();
        }*/
        return false;
    }

    @Override
    public boolean handleMessage(Message message) {
        mUpdater.removeMessages(RESCAN_MESSAGE);
        updateVPNList();
        mUpdater.sendEmptyMessageDelayed(RESCAN_MESSAGE, RESCAN_INTERVAL_MS);
        return true;
    }
}
