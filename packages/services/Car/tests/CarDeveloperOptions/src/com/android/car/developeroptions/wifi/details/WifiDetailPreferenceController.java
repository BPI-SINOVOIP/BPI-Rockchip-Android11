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
package com.android.car.developeroptions.wifi.details;

import static android.net.NetworkCapabilities.NET_CAPABILITY_CAPTIVE_PORTAL;
import static android.net.NetworkCapabilities.NET_CAPABILITY_PARTIAL_CONNECTIVITY;
import static android.net.NetworkCapabilities.NET_CAPABILITY_VALIDATED;
import static android.net.NetworkCapabilities.TRANSPORT_WIFI;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.settings.SettingsEnums;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.VectorDrawable;
import android.net.ConnectivityManager;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.LinkAddress;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.NetworkRequest;
import android.net.NetworkUtils;
import android.net.RouteInfo;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.CountDownTimer;
import android.os.Handler;
import android.text.TextUtils;
import android.util.FeatureFlagUtils;
import android.util.Log;
import android.widget.ImageView;
import android.widget.Toast;

import androidx.annotation.VisibleForTesting;
import androidx.core.text.BidiFormatter;
import androidx.fragment.app.Fragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.Utils;
import com.android.car.developeroptions.core.FeatureFlags;
import com.android.car.developeroptions.core.PreferenceControllerMixin;
import com.android.car.developeroptions.datausage.WifiDataUsageSummaryPreferenceController;
import com.android.car.developeroptions.development.featureflags.FeatureFlagPersistent;
import com.android.car.developeroptions.widget.EntityHeaderController;
import com.android.car.developeroptions.wifi.WifiDialog;
import com.android.car.developeroptions.wifi.WifiDialog.WifiDialogListener;
import com.android.car.developeroptions.wifi.WifiUtils;
import com.android.car.developeroptions.wifi.dpp.WifiDppUtils;
import com.android.car.developeroptions.wifi.savedaccesspoints.SavedAccessPointsWifiSettings;
import com.android.settingslib.core.AbstractPreferenceController;
import com.android.settingslib.core.instrumentation.MetricsFeatureProvider;
import com.android.settingslib.core.lifecycle.Lifecycle;
import com.android.settingslib.core.lifecycle.LifecycleObserver;
import com.android.settingslib.core.lifecycle.events.OnPause;
import com.android.settingslib.core.lifecycle.events.OnResume;
import com.android.settingslib.widget.ActionButtonsPreference;
import com.android.settingslib.widget.LayoutPreference;
import com.android.settingslib.wifi.AccessPoint;
import com.android.settingslib.wifi.WifiTracker;
import com.android.settingslib.wifi.WifiTrackerFactory;

import java.net.Inet4Address;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.time.Duration;
import java.util.StringJoiner;
import java.util.stream.Collectors;

/**
 * Controller for logic pertaining to displaying Wifi information for the
 * {@link WifiNetworkDetailsFragment}.
 */
public class WifiDetailPreferenceController extends AbstractPreferenceController
        implements PreferenceControllerMixin, WifiDialogListener, LifecycleObserver, OnPause,
        OnResume {

    private static final String TAG = "WifiDetailsPrefCtrl";
    private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);

    @VisibleForTesting
    static final String KEY_HEADER = "connection_header";
    @VisibleForTesting
    static final String KEY_DATA_USAGE_HEADER = "status_header";
    @VisibleForTesting
    static final String KEY_BUTTONS_PREF = "buttons";
    @VisibleForTesting
    static final String KEY_SIGNAL_STRENGTH_PREF = "signal_strength";
    @VisibleForTesting
    static final String KEY_TX_LINK_SPEED = "tx_link_speed";
    @VisibleForTesting
    static final String KEY_RX_LINK_SPEED = "rx_link_speed";
    @VisibleForTesting
    static final String KEY_FREQUENCY_PREF = "frequency";
    @VisibleForTesting
    static final String KEY_SECURITY_PREF = "security";
    @VisibleForTesting
    static final String KEY_SSID_PREF = "ssid";
    @VisibleForTesting
    static final String KEY_MAC_ADDRESS_PREF = "mac_address";
    @VisibleForTesting
    static final String KEY_IP_ADDRESS_PREF = "ip_address";
    @VisibleForTesting
    static final String KEY_GATEWAY_PREF = "gateway";
    @VisibleForTesting
    static final String KEY_SUBNET_MASK_PREF = "subnet_mask";
    @VisibleForTesting
    static final String KEY_DNS_PREF = "dns";
    @VisibleForTesting
    static final String KEY_IPV6_CATEGORY = "ipv6_category";
    @VisibleForTesting
    static final String KEY_IPV6_ADDRESSES_PREF = "ipv6_addresses";

    private static final int STATE_NONE = 1;
    private static final int STATE_ENABLE_WIFI = 2;
    private static final int STATE_ENABLE_WIFI_FAILED = 3;
    private static final int STATE_CONNECTING = 4;
    private static final int STATE_CONNECTED = 5;
    private static final int STATE_FAILED = 6;
    private static final int STATE_NOT_IN_RANGE = 7;
    private static final int STATE_DISCONNECTED = 8;
    private static final long TIMEOUT = Duration.ofSeconds(10).toMillis();

    // Be static to avoid too much object not be reset.
    private static CountDownTimer mTimer;

    private AccessPoint mAccessPoint;
    private final ConnectivityManager mConnectivityManager;
    private final Fragment mFragment;
    private final Handler mHandler;
    private LinkProperties mLinkProperties;
    private Network mNetwork;
    private NetworkInfo mNetworkInfo;
    private NetworkCapabilities mNetworkCapabilities;
    private int mRssiSignalLevel = -1;
    private String[] mSignalStr;
    private WifiConfiguration mWifiConfig;
    private WifiInfo mWifiInfo;
    private final WifiManager mWifiManager;
    private final WifiTracker mWifiTracker;
    private final MetricsFeatureProvider mMetricsFeatureProvider;
    private boolean mIsOutOfRange;
    private boolean mIsEphemeral;
    private boolean mConnected;
    private int mConnectingState;
    private WifiManager.ActionListener mConnectListener;

    // UI elements - in order of appearance
    private ActionButtonsPreference mButtonsPref;
    private EntityHeaderController mEntityHeaderController;
    private Preference mSignalStrengthPref;
    private Preference mTxLinkSpeedPref;
    private Preference mRxLinkSpeedPref;
    private Preference mFrequencyPref;
    private Preference mSecurityPref;
    private Preference mSsidPref;
    private Preference mMacAddressPref;
    private Preference mIpAddressPref;
    private Preference mGatewayPref;
    private Preference mSubnetPref;
    private Preference mDnsPref;
    private PreferenceCategory mIpv6Category;
    private Preference mIpv6AddressPref;
    private Lifecycle mLifecycle;
    Preference mDataUsageSummaryPref;
    WifiDataUsageSummaryPreferenceController mSummaryHeaderController;

    private final IconInjector mIconInjector;
    private final IntentFilter mFilter;
    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            switch (intent.getAction()) {
                case WifiManager.CONFIGURED_NETWORKS_CHANGED_ACTION:
                    updateMatchingWifiConfig();
                    // fall through
                case WifiManager.NETWORK_STATE_CHANGED_ACTION:
                case WifiManager.RSSI_CHANGED_ACTION:
                    refreshPage();
                    break;
            }
        }

        private void updateMatchingWifiConfig() {
            // use getPrivilegedConfiguredNetworks() to get Passpoint & other ephemeral networks
            for (WifiConfiguration wifiConfiguration :
                    mWifiManager.getPrivilegedConfiguredNetworks()) {
                if (mAccessPoint.matches(wifiConfiguration)) {
                    mWifiConfig = wifiConfiguration;
                    break;
                }
            }
        }
    };

    private final NetworkRequest mNetworkRequest = new NetworkRequest.Builder()
            .clearCapabilities().addTransportType(TRANSPORT_WIFI).build();

    // Must be run on the UI thread since it directly manipulates UI state.
    private final NetworkCallback mNetworkCallback = new NetworkCallback() {
        @Override
        public void onLinkPropertiesChanged(Network network, LinkProperties lp) {
            if (network.equals(mNetwork) && !lp.equals(mLinkProperties)) {
                mLinkProperties = lp;
                refreshIpLayerInfo();
            }
        }

        private boolean hasCapabilityChanged(NetworkCapabilities nc, int cap) {
            // If this is the first time we get NetworkCapabilities, report that something changed.
            if (mNetworkCapabilities == null) return true;

            // nc can never be null, see ConnectivityService#callCallbackForRequest.
            return mNetworkCapabilities.hasCapability(cap) != nc.hasCapability(cap);
        }

        @Override
        public void onCapabilitiesChanged(Network network, NetworkCapabilities nc) {
            // If the network just validated or lost Internet access or detected partial internet
            // connectivity, refresh network state. Don't do this on every NetworkCapabilities
            // change because refreshNetworkState sends IPCs to the system server from the UI
            // thread, which can cause jank.
            if (network.equals(mNetwork) && !nc.equals(mNetworkCapabilities)) {
                if (hasCapabilityChanged(nc, NET_CAPABILITY_VALIDATED)
                        || hasCapabilityChanged(nc, NET_CAPABILITY_CAPTIVE_PORTAL)
                        || hasCapabilityChanged(nc, NET_CAPABILITY_PARTIAL_CONNECTIVITY)) {
                    mAccessPoint.update(mWifiConfig, mWifiInfo, mNetworkInfo);
                    refreshEntityHeader();
                }
                mNetworkCapabilities = nc;
                refreshButtons();
                refreshIpLayerInfo();
            }
        }

        @Override
        public void onLost(Network network) {
            final boolean lostCurrentNetwork = network.equals(mNetwork);
            if (lostCurrentNetwork) {
                // If support detail page for saved network, should update as disconnect but not
                // exit. Except for ephemeral network which should not show on saved network list.
                if (SavedAccessPointsWifiSettings.usingDetailsFragment(mContext) && !mIsEphemeral) {
                    return;
                }

                exitActivity();
            }
        }
    };

    private final WifiTracker.WifiListener mWifiListener = new WifiTracker.WifiListener() {
        /** Called when the state of Wifi has changed. */
        public void onWifiStateChanged(int state) {
            Log.d(TAG, "onWifiStateChanged(" + state + ")");
            if (mConnectingState == STATE_ENABLE_WIFI && state == WifiManager.WIFI_STATE_ENABLED) {
                updateConnectingState(STATE_CONNECTING);
            } else if (mConnectingState != STATE_NONE && state == WifiManager.WIFI_STATE_DISABLED) {
                // update as disconnected once Wi-Fi disabled since may not received
                // onConnectedChanged for this case.
                updateConnectingState(STATE_DISCONNECTED);
            }
        }

        /** Called when the connection state of wifi has changed. */
        public void onConnectedChanged() {
            updateAccessPointFromScannedList();
            if (mConnected != mAccessPoint.isActive()) {
                Log.d(TAG, "Connection state changed!");
                mConnected = mAccessPoint.isActive();
                if (mAccessPoint.isActive()) {
                    updateConnectingState(STATE_CONNECTED);
                } else {
                    updateConnectingState(STATE_DISCONNECTED);
                }
            }
        }

        /**
         * Called to indicate the list of AccessPoints has been updated and
         * {@link WifiTracker#getAccessPoints()} should be called to get the updated list.
         */
        public void onAccessPointsChanged() {
            refreshPage();
        }
    };

    public static WifiDetailPreferenceController newInstance(
            AccessPoint accessPoint,
            ConnectivityManager connectivityManager,
            Context context,
            Fragment fragment,
            Handler handler,
            Lifecycle lifecycle,
            WifiManager wifiManager,
            MetricsFeatureProvider metricsFeatureProvider) {
        return new WifiDetailPreferenceController(
                accessPoint, connectivityManager, context, fragment, handler, lifecycle,
                wifiManager, metricsFeatureProvider, new IconInjector(context));
    }

    @VisibleForTesting
        /* package */ WifiDetailPreferenceController(
            AccessPoint accessPoint,
            ConnectivityManager connectivityManager,
            Context context,
            Fragment fragment,
            Handler handler,
            Lifecycle lifecycle,
            WifiManager wifiManager,
            MetricsFeatureProvider metricsFeatureProvider,
            IconInjector injector) {
        super(context);

        mAccessPoint = accessPoint;
        mConnectivityManager = connectivityManager;
        mFragment = fragment;
        mHandler = handler;
        mSignalStr = context.getResources().getStringArray(R.array.wifi_signal);
        mWifiConfig = accessPoint.getConfig();
        mWifiManager = wifiManager;
        mMetricsFeatureProvider = metricsFeatureProvider;
        mIconInjector = injector;

        mFilter = new IntentFilter();
        mFilter.addAction(WifiManager.NETWORK_STATE_CHANGED_ACTION);
        mFilter.addAction(WifiManager.RSSI_CHANGED_ACTION);
        mFilter.addAction(WifiManager.CONFIGURED_NETWORKS_CHANGED_ACTION);

        mLifecycle = lifecycle;
        lifecycle.addObserver(this);

        if (SavedAccessPointsWifiSettings.usingDetailsFragment(mContext)) {
            mWifiTracker = WifiTrackerFactory.create(
                    mFragment.getActivity(),
                    mWifiListener,
                    mLifecycle,
                    true /*includeSaved*/,
                    true /*includeScans*/);
        } else {
            mWifiTracker = null;
        }
        mConnected = mAccessPoint.isActive();
        // When lost the network connection, WifiInfo/NetworkInfo will be clear. So causes we
        // could not check if the AccessPoint is ephemeral. Need to cache it in first.
        mIsEphemeral = mAccessPoint.isEphemeral();
        mConnectingState = STATE_NONE;
        mConnectListener = new WifiManager.ActionListener() {
            @Override
            public void onSuccess() {
                // Do nothing
            }

            @Override
            public void onFailure(int reason) {
                updateConnectingState(STATE_FAILED);
            }
        };
    }

    @Override
    public boolean isAvailable() {
        return true;
    }

    @Override
    public String getPreferenceKey() {
        // Returns null since this controller contains more than one Preference
        return null;
    }

    @Override
    public void displayPreference(PreferenceScreen screen) {
        super.displayPreference(screen);

        setupEntityHeader(screen);

        mButtonsPref = ((ActionButtonsPreference) screen.findPreference(KEY_BUTTONS_PREF))
                .setButton1Text(R.string.forget)
                .setButton1Icon(R.drawable.ic_settings_delete)
                .setButton1OnClickListener(view -> forgetNetwork())
                .setButton2Text(R.string.wifi_sign_in_button_text)
                .setButton2Icon(R.drawable.ic_settings_sign_in)
                .setButton2OnClickListener(view -> signIntoNetwork())
                .setButton3Text(R.string.wifi_connect)
                .setButton3Icon(R.drawable.ic_settings_wireless)
                .setButton3OnClickListener(view -> connectNetwork())
                .setButton3Enabled(true)
                .setButton4Text(R.string.share)
                .setButton4Icon(R.drawable.ic_qrcode_24dp)
                .setButton4OnClickListener(view -> shareNetwork())
                .setButton4Visible(
                        WifiDppUtils.isSupportConfiguratorQrCodeGenerator(mContext, mAccessPoint));

        mSignalStrengthPref = screen.findPreference(KEY_SIGNAL_STRENGTH_PREF);
        mTxLinkSpeedPref = screen.findPreference(KEY_TX_LINK_SPEED);
        mRxLinkSpeedPref = screen.findPreference(KEY_RX_LINK_SPEED);
        mFrequencyPref = screen.findPreference(KEY_FREQUENCY_PREF);
        mSecurityPref = screen.findPreference(KEY_SECURITY_PREF);

        mSsidPref = screen.findPreference(KEY_SSID_PREF);
        mMacAddressPref = screen.findPreference(KEY_MAC_ADDRESS_PREF);
        mIpAddressPref = screen.findPreference(KEY_IP_ADDRESS_PREF);
        mGatewayPref = screen.findPreference(KEY_GATEWAY_PREF);
        mSubnetPref = screen.findPreference(KEY_SUBNET_MASK_PREF);
        mDnsPref = screen.findPreference(KEY_DNS_PREF);

        mIpv6Category = screen.findPreference(KEY_IPV6_CATEGORY);
        mIpv6AddressPref = screen.findPreference(KEY_IPV6_ADDRESSES_PREF);

        mSecurityPref.setSummary(mAccessPoint.getSecurityString(/* concise */ false));
    }

    private void setupEntityHeader(PreferenceScreen screen) {
        LayoutPreference headerPref = screen.findPreference(KEY_HEADER);

        if (usingDataUsageHeader(mContext)) {
            headerPref.setVisible(false);
            mDataUsageSummaryPref = screen.findPreference(KEY_DATA_USAGE_HEADER);
            mDataUsageSummaryPref.setVisible(true);
            mSummaryHeaderController =
                new WifiDataUsageSummaryPreferenceController(mFragment.getActivity(),
                        mLifecycle, (PreferenceFragmentCompat) mFragment, mAccessPoint.getSsid());
            return;
        }

        mEntityHeaderController =
                EntityHeaderController.newInstance(
                        mFragment.getActivity(), mFragment,
                        headerPref.findViewById(R.id.entity_header));

        ImageView iconView = headerPref.findViewById(R.id.entity_header_icon);

        iconView.setScaleType(ImageView.ScaleType.CENTER_INSIDE);

        mEntityHeaderController.setLabel(mAccessPoint.getTitle());
    }

    private void refreshEntityHeader() {
        if (usingDataUsageHeader(mContext)) {
            mSummaryHeaderController.updateState(mDataUsageSummaryPref);
        } else {
            mEntityHeaderController.setSummary(mAccessPoint.getSettingsSummary())
                    .done(mFragment.getActivity(), true /* rebind */);
        }
    }

    private void updateNetworkInfo() {
        mNetwork = mWifiManager.getCurrentNetwork();
        mLinkProperties = mConnectivityManager.getLinkProperties(mNetwork);
        mNetworkCapabilities = mConnectivityManager.getNetworkCapabilities(mNetwork);
    }

    @Override
    public void onResume() {
        // Ensure mNetwork is set before any callbacks above are delivered, since our
        // NetworkCallback only looks at changes to mNetwork.
        updateNetworkInfo();
        refreshPage();
        mContext.registerReceiver(mReceiver, mFilter);
        mConnectivityManager.registerNetworkCallback(mNetworkRequest, mNetworkCallback,
                mHandler);
    }

    @Override
    public void onPause() {
        mNetwork = null;
        mLinkProperties = null;
        mNetworkCapabilities = null;
        mNetworkInfo = null;
        mWifiInfo = null;
        mContext.unregisterReceiver(mReceiver);
        mConnectivityManager.unregisterNetworkCallback(mNetworkCallback);
    }

    private void refreshPage() {
        if(!updateAccessPoint()) {
            return;
        }

        Log.d(TAG, "Update UI!");

        // refresh header
        refreshEntityHeader();

        // refresh Buttons
        refreshButtons();

        // Update Connection Header icon and Signal Strength Preference
        refreshRssiViews();
        // Frequency Pref
        refreshFrequency();
        // Transmit Link Speed Pref
        refreshTxSpeed();
        // Receive Link Speed Pref
        refreshRxSpeed();
        // IP related information
        refreshIpLayerInfo();
        // SSID Pref
        refreshSsid();
        // MAC Address Pref
        refreshMacAddress();
    }

    private boolean updateAccessPoint() {
        boolean changed = false;
        if (mWifiTracker != null) {
            // remember mIsOutOfRange as old before updated
            boolean oldState = mIsOutOfRange;
            updateAccessPointFromScannedList();
            // refresh UI if signal level changed for disconnect network.
            changed = mRssiSignalLevel != mAccessPoint.getLevel();
            changed |= oldState != mIsOutOfRange;
        }

        if (mAccessPoint.isActive()) {
            // Sometimes {@link WifiManager#getCurrentNetwork()} return null after connected,
            // refresh it if needed.
            if (mNetwork == null) {
                updateNetworkInfo();
            }
            mNetworkInfo = mConnectivityManager.getNetworkInfo(mNetwork);
            mWifiInfo = mWifiManager.getConnectionInfo();
            if (mNetwork == null || mNetworkInfo == null || mWifiInfo == null) {
                // Once connected, can't get mNetworkInfo immediately, return false and wait for
                // next time to update UI.
                if (SavedAccessPointsWifiSettings.usingDetailsFragment(mContext)) {
                    return false;
                }

                exitActivity();
                return false;
            }

            changed |= mAccessPoint.update(mWifiConfig, mWifiInfo, mNetworkInfo);
            // If feature for saved network not enabled, always return true.
            return mWifiTracker == null || changed;
        }

        return changed;
    }

    private void updateAccessPointFromScannedList() {
        if (mWifiTracker == null) return;

        mIsOutOfRange = true;

        if (mAccessPoint.getConfig() == null) {
            return;
        }

        for (AccessPoint ap : mWifiTracker.getAccessPoints()) {
            if (ap.getConfig() != null
                    && mAccessPoint.matches(ap.getConfig())) {
                mAccessPoint = ap;
                mIsOutOfRange = !mAccessPoint.isReachable();
                return;
            }
        }
    }

    private void exitActivity() {
        if (DEBUG) {
            Log.d(TAG, "Exiting the WifiNetworkDetailsPage");
        }
        mFragment.getActivity().finish();
    }

    private void refreshRssiViews() {
        int signalLevel = mAccessPoint.getLevel();

        // Disappears signal view if not in range. e.g. for saved networks.
        if (mIsOutOfRange) {
            mSignalStrengthPref.setVisible(false);
            mRssiSignalLevel = -1;
            return;
        }

        if (mRssiSignalLevel == signalLevel) {
            return;
        }
        mRssiSignalLevel = signalLevel;
        Drawable wifiIcon = mIconInjector.getIcon(mRssiSignalLevel);

        if (mEntityHeaderController != null) {
            mEntityHeaderController
                    .setIcon(redrawIconForHeader(wifiIcon)).done(mFragment.getActivity(),
                            true /* rebind */);
        }

        Drawable wifiIconDark = wifiIcon.getConstantState().newDrawable().mutate();
        wifiIconDark.setTintList(Utils.getColorAttr(mContext, android.R.attr.colorControlNormal));
        mSignalStrengthPref.setIcon(wifiIconDark);

        mSignalStrengthPref.setSummary(mSignalStr[mRssiSignalLevel]);
        mSignalStrengthPref.setVisible(true);
    }

    private Drawable redrawIconForHeader(Drawable original) {
        final int iconSize = mContext.getResources().getDimensionPixelSize(
                R.dimen.wifi_detail_page_header_image_size);
        final int actualWidth = original.getMinimumWidth();
        final int actualHeight = original.getMinimumHeight();

        if ((actualWidth == iconSize && actualHeight == iconSize)
                || !VectorDrawable.class.isInstance(original)) {
            return original;
        }

        // clear tint list to make sure can set 87% black after enlarge
        original.setTintList(null);

        // enlarge icon size
        final Bitmap bitmap = Utils.createBitmap(original,
                iconSize /*width*/,
                iconSize /*height*/);
        Drawable newIcon = new BitmapDrawable(null /*resource*/, bitmap);

        // config color for 87% black after enlarge
        newIcon.setTintList(Utils.getColorAttr(mContext, android.R.attr.textColorPrimary));

        return newIcon;
    }

    private void refreshFrequency() {
        if (mWifiInfo == null) {
            mFrequencyPref.setVisible(false);
            return;
        }

        final int frequency = mWifiInfo.getFrequency();
        String band = null;
        if (frequency >= AccessPoint.LOWER_FREQ_24GHZ
                && frequency < AccessPoint.HIGHER_FREQ_24GHZ) {
            band = mContext.getResources().getString(R.string.wifi_band_24ghz);
        } else if (frequency >= AccessPoint.LOWER_FREQ_5GHZ
                && frequency < AccessPoint.HIGHER_FREQ_5GHZ) {
            band = mContext.getResources().getString(R.string.wifi_band_5ghz);
        } else {
            Log.e(TAG, "Unexpected frequency " + frequency);
            // Connecting state is unstable, make it disappeared if unexpected
            if (mConnectingState == STATE_CONNECTING) {
                mFrequencyPref.setVisible(false);
            }
            return;
        }
        mFrequencyPref.setSummary(band);
        mFrequencyPref.setVisible(true);
    }

    private void refreshTxSpeed() {
        if (mWifiInfo == null) {
            mTxLinkSpeedPref.setVisible(false);
            return;
        }

        int txLinkSpeedMbps = mWifiInfo.getTxLinkSpeedMbps();
        mTxLinkSpeedPref.setVisible(txLinkSpeedMbps >= 0);
        mTxLinkSpeedPref.setSummary(mContext.getString(
                R.string.tx_link_speed, mWifiInfo.getTxLinkSpeedMbps()));
    }

    private void refreshRxSpeed() {
        if (mWifiInfo == null) {
            mRxLinkSpeedPref.setVisible(false);
            return;
        }

        int rxLinkSpeedMbps = mWifiInfo.getRxLinkSpeedMbps();
        mRxLinkSpeedPref.setVisible(rxLinkSpeedMbps >= 0);
        mRxLinkSpeedPref.setSummary(mContext.getString(
                R.string.rx_link_speed, mWifiInfo.getRxLinkSpeedMbps()));
    }

    private void refreshSsid() {
        if (mAccessPoint.isPasspoint() || mAccessPoint.isOsuProvider()) {
            mSsidPref.setVisible(true);
            mSsidPref.setSummary(mAccessPoint.getSsidStr());
        } else {
            mSsidPref.setVisible(false);
        }
    }

    private void refreshMacAddress() {
        String macAddress = getMacAddress();
        if (macAddress == null) {
            mMacAddressPref.setVisible(false);
            return;
        }

        mMacAddressPref.setVisible(true);
        mMacAddressPref.setSummary(macAddress);
    }

    private String getMacAddress() {
        if (mWifiInfo != null) {
            // get MAC address from connected network information
            return mWifiInfo.getMacAddress();
        }

        // return randomized MAC address
        if (mWifiConfig != null &&
                mWifiConfig.macRandomizationSetting == WifiConfiguration.RANDOMIZATION_PERSISTENT) {
            return mWifiConfig.getRandomizedMacAddress().toString();
        }

        // return device MAC address
        final String[] macAddresses = mWifiManager.getFactoryMacAddresses();
        if (macAddresses != null && macAddresses.length > 0) {
            return macAddresses[0];
        }

        Log.e(TAG, "Can't get device MAC address!");
        return null;
    }

    private void updatePreference(Preference pref, String detailText) {
        if (!TextUtils.isEmpty(detailText)) {
            pref.setSummary(detailText);
            pref.setVisible(true);
        } else {
            pref.setVisible(false);
        }
    }

    private void refreshButtons() {
        // Ephemeral network won't be removed permanently, but be putted in blacklist.
        mButtonsPref.setButton1Text(
                mIsEphemeral ? R.string.wifi_disconnect_button_text : R.string.forget);

        mButtonsPref.setButton1Visible(canForgetNetwork());
        mButtonsPref.setButton2Visible(canSignIntoNetwork());
        mButtonsPref.setButton3Visible(canConnectNetwork());
        mButtonsPref.setButton4Visible(canShareNetwork());
        mButtonsPref.setVisible(canSignIntoNetwork()
                || canForgetNetwork()
                || canShareNetwork()
                || canConnectNetwork());
    }

    private boolean canConnectNetwork() {
        // Display connect button for disconnected AP even not in the range.
        return SavedAccessPointsWifiSettings.usingDetailsFragment(mContext)
                && !mAccessPoint.isActive();
    }

    private void refreshIpLayerInfo() {
        // Hide IP layer info if not a connected network.
        if (!mAccessPoint.isActive() || mNetwork == null || mLinkProperties == null) {
            mIpAddressPref.setVisible(false);
            mSubnetPref.setVisible(false);
            mGatewayPref.setVisible(false);
            mDnsPref.setVisible(false);
            mIpv6Category.setVisible(false);
            return;
        }

        // Find IPv4 and IPv6 addresses.
        String ipv4Address = null;
        String subnet = null;
        StringJoiner ipv6Addresses = new StringJoiner("\n");

        for (LinkAddress addr : mLinkProperties.getLinkAddresses()) {
            if (addr.getAddress() instanceof Inet4Address) {
                ipv4Address = addr.getAddress().getHostAddress();
                subnet = ipv4PrefixLengthToSubnetMask(addr.getPrefixLength());
            } else if (addr.getAddress() instanceof Inet6Address) {
                ipv6Addresses.add(addr.getAddress().getHostAddress());
            }
        }

        // Find IPv4 default gateway.
        String gateway = null;
        for (RouteInfo routeInfo : mLinkProperties.getRoutes()) {
            if (routeInfo.isIPv4Default() && routeInfo.hasGateway()) {
                gateway = routeInfo.getGateway().getHostAddress();
                break;
            }
        }

        // Find all (IPv4 and IPv6) DNS addresses.
        String dnsServers = mLinkProperties.getDnsServers().stream()
                .map(InetAddress::getHostAddress)
                .collect(Collectors.joining("\n"));

        // Update UI.
        updatePreference(mIpAddressPref, ipv4Address);
        updatePreference(mSubnetPref, subnet);
        updatePreference(mGatewayPref, gateway);
        updatePreference(mDnsPref, dnsServers);

        if (ipv6Addresses.length() > 0) {
            mIpv6AddressPref.setSummary(
                    BidiFormatter.getInstance().unicodeWrap(ipv6Addresses.toString()));
            mIpv6Category.setVisible(true);
        } else {
            mIpv6Category.setVisible(false);
        }
    }

    private static String ipv4PrefixLengthToSubnetMask(int prefixLength) {
        try {
            InetAddress all = InetAddress.getByAddress(
                    new byte[]{(byte) 255, (byte) 255, (byte) 255, (byte) 255});
            return NetworkUtils.getNetworkPart(all, prefixLength).getHostAddress();
        } catch (UnknownHostException e) {
            return null;
        }
    }

    /**
     * Returns whether the network represented by this preference can be forgotten.
     */
    private boolean canForgetNetwork() {
        return (mWifiInfo != null && mWifiInfo.isEphemeral()) || canModifyNetwork();
    }

    /**
     * Returns whether the network represented by this preference can be modified.
     */
    public boolean canModifyNetwork() {
        return mWifiConfig != null && !WifiUtils.isNetworkLockedDown(mContext, mWifiConfig);
    }

    /**
     * Returns whether the user can sign into the network represented by this preference.
     */
    private boolean canSignIntoNetwork() {
        return mAccessPoint.isActive() && WifiUtils.canSignIntoNetwork(mNetworkCapabilities);
    }

    /**
     * Returns whether the user can share the network represented by this preference with QR code.
     */
    private boolean canShareNetwork() {
        return mAccessPoint.getConfig() != null;
    }

    /**
     * Forgets the wifi network associated with this preference.
     */
    private void forgetNetwork() {
        if (mWifiInfo != null && mWifiInfo.isEphemeral()) {
            mWifiManager.disableEphemeralNetwork(mWifiInfo.getSSID());
        } else if (mWifiConfig != null) {
            if (mWifiConfig.isPasspoint()) {
                // Post a dialog to confirm if user really want to forget the passpoint network.
                if (FeatureFlagPersistent.isEnabled(mContext, FeatureFlags.NETWORK_INTERNET_V2)) {
                    showConfirmForgetDialog();
                    return;
                }

                mWifiManager.removePasspointConfiguration(mWifiConfig.FQDN);
            } else {
                mWifiManager.forget(mWifiConfig.networkId, null /* action listener */);
            }
        }
        mMetricsFeatureProvider.action(
                mFragment.getActivity(), SettingsEnums.ACTION_WIFI_FORGET);
        mFragment.getActivity().finish();
    }

    @VisibleForTesting
    protected void showConfirmForgetDialog() {
        final AlertDialog dialog = new AlertDialog.Builder(mContext)
                .setPositiveButton(R.string.forget, ((dialog1, which) -> {
                    mWifiManager.removePasspointConfiguration(mWifiConfig.FQDN);
                    mMetricsFeatureProvider.action(
                            mFragment.getActivity(), SettingsEnums.ACTION_WIFI_FORGET);
                    mFragment.getActivity().finish();
                }))
                .setNegativeButton(R.string.cancel, null /* listener */)
                .setTitle(R.string.wifi_forget_dialog_title)
                .setMessage(R.string.forget_passpoint_dialog_message)
                .create();
        dialog.show();
    }

    /**
     * Show QR code to share the network represented by this preference.
     */
    public void launchWifiDppConfiguratorActivity() {
        final Intent intent = WifiDppUtils.getConfiguratorQrCodeGeneratorIntentOrNull(mContext,
                mWifiManager, mAccessPoint);

        if (intent == null) {
            Log.e(TAG, "Launch Wi-Fi DPP QR code generator with a wrong Wi-Fi network!");
        } else {
            mContext.startActivity(intent);
        }
    }

    /**
     * Share the wifi network with QR code.
     */
    private void shareNetwork() {
        WifiDppUtils.showLockScreen(mContext, () -> launchWifiDppConfiguratorActivity());
    }

    /**
     * Sign in to the captive portal found on this wifi network associated with this preference.
     */
    private void signIntoNetwork() {
        mMetricsFeatureProvider.action(
                mFragment.getActivity(), SettingsEnums.ACTION_WIFI_SIGNIN);
        mConnectivityManager.startCaptivePortalApp(mNetwork);
    }

    @Override
    public void onSubmit(WifiDialog dialog) {
        if (dialog.getController() != null) {
            mWifiManager.save(dialog.getController().getConfig(), new WifiManager.ActionListener() {
                @Override
                public void onSuccess() {
                }

                @Override
                public void onFailure(int reason) {
                    Activity activity = mFragment.getActivity();
                    if (activity != null) {
                        Toast.makeText(activity,
                                R.string.wifi_failed_save_message,
                                Toast.LENGTH_SHORT).show();
                    }
                }
            });
        }
    }

    /**
     * Wrapper for testing compatibility.
     */
    @VisibleForTesting
    static class IconInjector {
        private final Context mContext;

        public IconInjector(Context context) {
            mContext = context;
        }

        public Drawable getIcon(int level) {
            return mContext.getDrawable(Utils.getWifiIconResource(level)).mutate();
        }
    }

    private boolean usingDataUsageHeader(Context context) {
        return FeatureFlagUtils.isEnabled(context, FeatureFlags.WIFI_DETAILS_DATAUSAGE_HEADER);
    }

    private void connectNetwork() {
        final Activity activity = mFragment.getActivity();
        // error handling, connected/saved network should have mWifiConfig.
        if (mWifiConfig == null) {
            Toast.makeText(activity,
                    R.string.wifi_failed_connect_message,
                    Toast.LENGTH_SHORT).show();
            return;
        }

        // init state before connect
        mConnectingState = STATE_NONE;

        if (mWifiManager.isWifiEnabled()) {
            updateConnectingState(STATE_CONNECTING);
        } else {
            // Enable Wi-Fi automatically to connect AP
            updateConnectingState(STATE_ENABLE_WIFI);
        }
    }

    private void updateConnectingState(int state) {
        final Activity activity = mFragment.getActivity();
        Log.d(TAG, "updateConnectingState from " + mConnectingState + " to " + state);
        switch (mConnectingState) {
            case STATE_NONE:
            case STATE_ENABLE_WIFI:
                if (state == STATE_ENABLE_WIFI) {
                    Log.d(TAG, "Turn on Wi-Fi automatically!");
                    updateConnectedButton(STATE_ENABLE_WIFI);
                    Toast.makeText(activity,
                            R.string.wifi_turned_on_message,
                            Toast.LENGTH_SHORT).show();
                    mWifiManager.setWifiEnabled(true);
                    // start timer for error handling
                    startTimer();
                } else if (state == STATE_CONNECTING) {
                    Log.d(TAG, "connecting...");
                    updateConnectedButton(STATE_CONNECTING);
                    mWifiManager.connect(mWifiConfig.networkId, mConnectListener);
                    // start timer for error handling since framework didn't call back if failed
                    startTimer();
                } else if (state == STATE_ENABLE_WIFI_FAILED) {
                    Log.e(TAG, "Wi-Fi failed to enable network!");
                    stopTimer();
                    // reset state
                    state = STATE_NONE;
                    Toast.makeText(activity,
                            R.string.wifi_failed_connect_message,
                            Toast.LENGTH_SHORT).show();
                    updateConnectedButton(STATE_ENABLE_WIFI_FAILED);
                }
                // Do not break here for disconnected event.
            case STATE_CONNECTED:
                if (state == STATE_DISCONNECTED) {
                    Log.d(TAG, "disconnected");
                    // reset state
                    state = STATE_NONE;
                    updateConnectedButton(STATE_DISCONNECTED);
                    refreshPage();
                    // clear for getting MAC Address from saved configuration
                    mWifiInfo = null;
                }
                break;
            case STATE_CONNECTING:
                if (state == STATE_CONNECTED) {
                    Log.d(TAG, "connected");
                    stopTimer();
                    updateConnectedButton(STATE_CONNECTED);
                    Toast.makeText(activity,
                            mContext.getString(R.string.wifi_connected_to_message,
                                    mAccessPoint.getTitle()),
                            Toast.LENGTH_SHORT).show();

                    updateNetworkInfo();
                    refreshPage();
                } else if (state == STATE_NOT_IN_RANGE) {
                    Log.d(TAG, "AP not in range");
                    stopTimer();
                    // reset state
                    state = STATE_NONE;
                    Toast.makeText(activity,
                            R.string.wifi_not_in_range_message,
                            Toast.LENGTH_SHORT).show();
                    updateConnectedButton(STATE_NOT_IN_RANGE);
                } else if (state == STATE_FAILED) {
                    Log.d(TAG, "failed");
                    stopTimer();
                    // reset state
                    state = STATE_NONE;
                    Toast.makeText(activity,
                            R.string.wifi_failed_connect_message,
                            Toast.LENGTH_SHORT).show();
                    updateConnectedButton(STATE_FAILED);
                }
                break;
            default:
                Log.e(TAG, "Invalid state : " + mConnectingState);
                // don't update invalid state
                return;
        }

        mConnectingState = state;
    }

    private void updateConnectedButton(int state) {
        switch (state) {
            case STATE_ENABLE_WIFI:
            case STATE_CONNECTING:
                mButtonsPref.setButton3Text(R.string.wifi_connecting)
                        .setButton3Enabled(false);
                break;
            case STATE_CONNECTED:
                mButtonsPref.setButton3Visible(false);
                break;
            case STATE_DISCONNECTED:
            case STATE_NOT_IN_RANGE:
            case STATE_FAILED:
            case STATE_ENABLE_WIFI_FAILED:
                mButtonsPref.setButton3Text(R.string.wifi_connect)
                        .setButton3Icon(R.drawable.ic_settings_wireless)
                        .setButton3Enabled(true)
                        .setButton3Visible(true);
                break;
            default:
                Log.e(TAG, "Invalid connect button state : " + state);
                break;
        }
    }

    private void startTimer() {
        if (mTimer != null) {
            stopTimer();
        }

        mTimer = new CountDownTimer(TIMEOUT, TIMEOUT + 1) {
            @Override
            public void onTick(long millisUntilFinished) {
                // Do nothing
            }
            @Override
            public void onFinish() {
                Log.e(TAG, "Timeout for state:" + mConnectingState);
                if (mConnectingState == STATE_ENABLE_WIFI) {
                    updateConnectingState(STATE_ENABLE_WIFI_FAILED);
                } else if (mConnectingState == STATE_CONNECTING) {
                    updateAccessPointFromScannedList();
                    if (mIsOutOfRange) {
                        updateConnectingState(STATE_NOT_IN_RANGE);
                    } else {
                        updateConnectingState(STATE_FAILED);
                    }
                }
            }
        };
        mTimer.start();
    }

    private void stopTimer() {
        if (mTimer == null) return;

        mTimer.cancel();
        mTimer = null;
    }
}
