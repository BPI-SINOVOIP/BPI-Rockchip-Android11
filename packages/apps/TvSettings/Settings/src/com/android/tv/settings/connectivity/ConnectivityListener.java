/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.tv.settings.connectivity;

import android.annotation.SuppressLint;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.EthernetManager;
import android.net.LinkAddress;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkInfo;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.telephony.PhoneStateListener;
import android.telephony.SignalStrength;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.UiThread;

import com.android.settingslib.core.lifecycle.Lifecycle;
import com.android.settingslib.core.lifecycle.LifecycleObserver;
import com.android.settingslib.core.lifecycle.events.OnStart;
import com.android.settingslib.core.lifecycle.events.OnStop;
import com.android.settingslib.wifi.AccessPoint;
import com.android.settingslib.wifi.WifiTracker;

import java.util.ArrayList;
import java.util.List;

/**
 * Listens for changes to the current connectivity status.
 */
public class ConnectivityListener implements WifiTracker.WifiListener, LifecycleObserver, OnStart,
        OnStop {

    private static final String TAG = "ConnectivityListener";

    private final Context mContext;
    private final Listener mListener;
    private boolean mStarted;

    private WifiTracker mWifiTracker;

    private final ConnectivityManager mConnectivityManager;
    private final WifiManager mWifiManager;
    private final EthernetManager mEthernetManager;
    private WifiNetworkListener mWifiListener;
    private final BroadcastReceiver mNetworkReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            updateConnectivityStatus();
            if (mListener != null) {
                mListener.onConnectivityChange();
            }
        }
    };
    private final PhoneStateListener mPhoneStateListener = new PhoneStateListener() {
        @Override
        public void onSignalStrengthsChanged(SignalStrength signalStrength) {
            mCellSignalStrength = signalStrength;
            mListener.onConnectivityChange();
        }
    };

    private SignalStrength mCellSignalStrength;
    private int mNetworkType;
    private String mWifiSsid;
    private int mWifiSignalStrength;

    /**
     * @deprecated use the constructor that provides a {@link Lifecycle} instead
     */
    @Deprecated
    public ConnectivityListener(Context context, Listener listener) {
        this(context, listener, null);
    }

    public ConnectivityListener(Context context, Listener listener, Lifecycle lifecycle) {
        mContext = context;
        mConnectivityManager = (ConnectivityManager) mContext.getSystemService(
                Context.CONNECTIVITY_SERVICE);
        mWifiManager = mContext.getSystemService(WifiManager.class);
        mEthernetManager = mContext.getSystemService(EthernetManager.class);
        mListener = listener;
        if (mWifiManager != null) {
            if (lifecycle != null) {
                lifecycle.addObserver(this);
                mWifiTracker = new WifiTracker(context, this, lifecycle, true, true);
            } else {
                mWifiTracker = new WifiTracker(context, this, true, true);
            }
        }
        updateConnectivityStatus();
    }

    /**
     * Starts {@link ConnectivityListener}.
     * This should be called only from main thread.
     * @deprecated not needed when a {@link Lifecycle} is provided
     */
    @UiThread
    @Deprecated
    public void start() {
        if (!mStarted && mWifiTracker != null) {
            mWifiTracker.onStart();
        }
        onStart();
    }

    @Override
    public void onStart() {
        if (!mStarted) {
            mStarted = true;
            updateConnectivityStatus();
            IntentFilter networkIntentFilter = new IntentFilter();
            networkIntentFilter.addAction(WifiManager.NETWORK_STATE_CHANGED_ACTION);
            networkIntentFilter.addAction(WifiManager.RSSI_CHANGED_ACTION);
            networkIntentFilter.addAction(WifiManager.WIFI_STATE_CHANGED_ACTION);
            networkIntentFilter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);

            mContext.registerReceiver(mNetworkReceiver, networkIntentFilter);
            final TelephonyManager telephonyManager = mContext
                    .getSystemService(TelephonyManager.class);
            if (telephonyManager != null) {
                telephonyManager.listen(mPhoneStateListener,
                        PhoneStateListener.LISTEN_SIGNAL_STRENGTHS);
            }
        }
    }

    /**
     * Stops {@link ConnectivityListener}.
     * This should be called only from main thread.
     * @deprecated not needed when a {@link Lifecycle} is provided
     */
    @UiThread
    @Deprecated
    public void stop() {
        if (mStarted && mWifiTracker != null) {
            mWifiTracker.onStop();
        }
        onStop();
    }

    @Override
    public void onStop() {
        if (mStarted) {
            mStarted = false;
            mContext.unregisterReceiver(mNetworkReceiver);
            mWifiListener = null;
            final TelephonyManager telephonyManager = mContext
                    .getSystemService(TelephonyManager.class);
            if (telephonyManager != null) {
                telephonyManager.listen(mPhoneStateListener, PhoneStateListener.LISTEN_NONE);
            }
        }
    }

    /**
     * Causes the background thread to quit.
     * @deprecated not needed when a {@link Lifecycle} is provided
     */
    @Deprecated
    public void destroy() {
        if (mWifiTracker != null) {
            mWifiTracker.onDestroy();
        }
    }

    public void setWifiListener(WifiNetworkListener wifiListener) {
        mWifiListener = wifiListener;
    }

    public String getWifiIpAddress() {
        if (isWifiConnected()) {
            Network network = mWifiManager.getCurrentNetwork();
            return formatIpAddresses(network);
        } else {
            return "";
        }
    }

    /**
     * Return the MAC address of the currently connected Wifi AP.
     */
    @SuppressLint("HardwareIds")
    public String getWifiMacAddress(AccessPoint ap) {
        if (isWifiConnected() && mWifiManager.getConnectionInfo() != null) {
            return mWifiManager.getConnectionInfo().getMacAddress();
        }
        if (ap != null) {
            WifiConfiguration wifiConfig = ap.getConfig();
            if (wifiConfig != null
                    && wifiConfig.macRandomizationSetting
                            == WifiConfiguration.RANDOMIZATION_PERSISTENT) {
                return wifiConfig.getRandomizedMacAddress().toString();
            }
        }

        // return device MAC address
        final String[] macAddresses = mWifiManager.getFactoryMacAddresses();
        if (macAddresses != null && macAddresses.length > 0) {
            return macAddresses[0];
        }

        Log.e(TAG, "Unable to get MAC address");
        return "";
    }

    /** Return whether the connected Wifi supports MAC address randomization. */
    public boolean isMacAddressRandomizationSupported() {
        return mWifiManager.isConnectedMacRandomizationSupported();
    }

    /** Return whether the MAC address of the currently connected Wifi AP is randomized. */
    public int getWifiMacRandomizationSetting(AccessPoint ap) {
        if (ap == null || ap.getConfig() == null) {
            return WifiConfiguration.RANDOMIZATION_NONE;
        }
        return ap.getConfig().macRandomizationSetting;
    }

    /** Apply the setting of whether to use MAC address randimization. */
    public void applyMacRandomizationSetting(AccessPoint ap, boolean enable) {
        if (ap != null && ap.getConfig() != null) {
            ap.getConfig().macRandomizationSetting = enable
                    ? WifiConfiguration.RANDOMIZATION_PERSISTENT
                    : WifiConfiguration.RANDOMIZATION_NONE;
            mWifiManager.updateNetwork(ap.getConfig());
            // To activate changing, we need to reconnect network. WiFi will auto connect to
            // current network after disconnect(). Only needed when this is connected network.
            final WifiInfo wifiInfo = mWifiManager.getConnectionInfo();
            if (wifiInfo != null && wifiInfo.getNetworkId() == ap.getConfig().networkId) {
                mWifiManager.disconnect();
            }
        }
    }

    public boolean isEthernetConnected() {
        return mNetworkType == ConnectivityManager.TYPE_ETHERNET;
    }

    public boolean isWifiConnected() {
        if (mNetworkType == ConnectivityManager.TYPE_WIFI) {
            return true;
        } else {
            if (mWifiManager != null) {
                WifiInfo connectionInfo = mWifiManager.getConnectionInfo();
                return connectionInfo.getNetworkId() != -1;
            }
        }
        return false;
    }

    public boolean isCellConnected() {
        return mNetworkType == ConnectivityManager.TYPE_MOBILE;
    }

    /**
     * Return whether Ethernet port is available.
     */
    public boolean isEthernetAvailable() {
        return mConnectivityManager.isNetworkSupported(ConnectivityManager.TYPE_ETHERNET)
                && mEthernetManager.getAvailableInterfaces().length > 0;
    }

    private Network getFirstEthernet() {
        final Network[] networks = mConnectivityManager.getAllNetworks();
        for (final Network network : networks) {
            NetworkInfo networkInfo = mConnectivityManager.getNetworkInfo(network);
            if (networkInfo != null && networkInfo.getType() == ConnectivityManager.TYPE_ETHERNET) {
                return network;
            }
        }
        return null;
    }

    private String formatIpAddresses(Network network) {
        final LinkProperties linkProperties = mConnectivityManager.getLinkProperties(network);
        if (linkProperties == null) {
            return null;
        }
        final StringBuilder sb = new StringBuilder();
        boolean gotAddress = false;
        for (LinkAddress linkAddress : linkProperties.getLinkAddresses()) {
            if (gotAddress) {
                sb.append("\n");
            }
            sb.append(linkAddress.getAddress().getHostAddress());
            gotAddress = true;
        }
        if (gotAddress) {
            return sb.toString();
        } else {
            return null;
        }
    }

    /**
     * Returns the formatted IP addresses of the Ethernet connection or null
     * if none available.
     */
    public String getEthernetIpAddress() {
        final Network network = getFirstEthernet();
        if (network == null) {
            return null;
        }
        return formatIpAddresses(network);
    }

    public int getWifiSignalStrength(int maxLevel) {
        if (mWifiManager != null) {
            WifiInfo wifiInfo = mWifiManager.getConnectionInfo();
            return WifiManager.calculateSignalLevel(wifiInfo.getRssi(), maxLevel);
        }
        return 0;
    }

    public int getCellSignalStrength() {
        if (isCellConnected() && mCellSignalStrength != null) {
            return mCellSignalStrength.getLevel();
        } else {
            return 0;
        }
    }

    /**
     * Return a list of wifi networks. Ensure that if a wifi network is connected that it appears
     * as the first item on the list.
     */
    public List<AccessPoint> getAvailableNetworks() {
        return mWifiTracker == null ? new ArrayList<>() : mWifiTracker.getAccessPoints();
    }

    public boolean isWifiEnabledOrEnabling() {
        return mWifiManager != null
                && (mWifiManager.getWifiState() == WifiManager.WIFI_STATE_ENABLED
                || mWifiManager.getWifiState() == WifiManager.WIFI_STATE_ENABLING);
    }

    public void setWifiEnabled(boolean enable) {
        if (mWifiManager != null) {
            mWifiManager.setWifiEnabled(enable);
        }
    }

    private void updateConnectivityStatus() {
        NetworkInfo networkInfo = mConnectivityManager.getActiveNetworkInfo();
        if (networkInfo == null) {
            mNetworkType = ConnectivityManager.TYPE_NONE;
        } else {
            switch (networkInfo.getType()) {
                case ConnectivityManager.TYPE_WIFI: {

                    if (mWifiManager == null) {
                        break;
                    }
                    // Determine if this is
                    // an open or secure wifi connection.
                    mNetworkType = ConnectivityManager.TYPE_WIFI;

                    String ssid = getSsid();
                    if (!TextUtils.equals(mWifiSsid, ssid)) {
                        mWifiSsid = ssid;
                    }

                    WifiInfo wifiInfo = mWifiManager.getConnectionInfo();
                    // Calculate the signal strength.
                    int signalStrength;
                    if (wifiInfo != null) {
                        // Calculate the signal strength between 0 and 3.
                        signalStrength = WifiManager.calculateSignalLevel(wifiInfo.getRssi(), 4);
                    } else {
                        signalStrength = 0;
                    }
                    if (mWifiSignalStrength != signalStrength) {
                        mWifiSignalStrength = signalStrength;
                    }
                    break;
                }

                case ConnectivityManager.TYPE_ETHERNET:
                    mNetworkType = ConnectivityManager.TYPE_ETHERNET;
                    break;

                case ConnectivityManager.TYPE_MOBILE:
                    mNetworkType = ConnectivityManager.TYPE_MOBILE;
                    break;

                default:
                    mNetworkType = ConnectivityManager.TYPE_NONE;
                    break;
            }
        }
    }

    @Override
    public void onWifiStateChanged(int state) {
        updateConnectivityStatus();
        if (mListener != null) {
            mListener.onConnectivityChange();
        }
    }

    @Override
    public void onConnectedChanged() {
        updateConnectivityStatus();
        if (mListener != null) {
            mListener.onConnectivityChange();
        }
    }

    @Override
    public void onAccessPointsChanged() {
        if (mWifiListener != null) {
            mWifiListener.onWifiListChanged();
        }
    }

    public interface Listener {
        void onConnectivityChange();
    }

    public interface WifiNetworkListener {
        void onWifiListChanged();
    }

    /**
     * Get the SSID of current connected network.
     * @return SSID
     */
    public String getSsid() {
        if (mWifiManager == null) {
            return null;
        }
        WifiInfo wifiInfo = mWifiManager.getConnectionInfo();
        // Find the SSID of network.
        String ssid = null;
        if (wifiInfo != null) {
            ssid = wifiInfo.getSSID();
            if (ssid != null) {
                ssid = WifiInfo.sanitizeSsid(ssid);
            }
        }
        return ssid;
    }
}
