/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.car.settings.testutils;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.DhcpInfo;
import android.net.NetworkInfo;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.util.Pair;

import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowConnectivityManager;
import org.robolectric.shadows.ShadowNetworkInfo;
import org.robolectric.shadows.ShadowWifiInfo;

import java.util.LinkedHashMap;
import java.util.Map;

@Implements(WifiManager.class)
public class ShadowWifiManager extends org.robolectric.shadows.ShadowWifiManager {
    private static final int LOCAL_HOST = 2130706433;

    private static int sResetCalledCount = 0;

    private final Map<Integer, WifiConfiguration> mNetworkIdToConfiguredNetworks =
            new LinkedHashMap<>();
    private Pair<Integer, Boolean> mLastEnabledNetwork;
    private int mLastForgottenNetwork = Integer.MIN_VALUE;
    private WifiInfo mActiveWifiInfo;

    @Implementation
    @Override
    protected int addNetwork(WifiConfiguration config) {
        int networkId = mNetworkIdToConfiguredNetworks.size();
        config.networkId = -1;
        mNetworkIdToConfiguredNetworks.put(networkId, makeCopy(config, networkId));
        return networkId;
    }

    @Implementation
    @Override
    protected void connect(WifiConfiguration wifiConfiguration,
            WifiManager.ActionListener listener) {
        int networkId = mNetworkIdToConfiguredNetworks.size();
        mNetworkIdToConfiguredNetworks.put(networkId,
                makeCopy(wifiConfiguration, wifiConfiguration.networkId));
        mLastEnabledNetwork = new Pair<>(wifiConfiguration.networkId, true);

        WifiInfo wifiInfo = getConnectionInfo();

        String ssid = isQuoted(wifiConfiguration.SSID)
                ? stripQuotes(wifiConfiguration.SSID)
                : wifiConfiguration.SSID;

        ShadowWifiInfo shadowWifiInfo = Shadow.extract(wifiInfo);
        shadowWifiInfo.setSSID(ssid);
        shadowWifiInfo.setBSSID(wifiConfiguration.BSSID);
        shadowWifiInfo.setNetworkId(wifiConfiguration.networkId);
        setConnectionInfo(wifiInfo);

        // Now that we're "connected" to wifi, update Dhcp and point it to localhost.
        DhcpInfo dhcpInfo = new DhcpInfo();
        dhcpInfo.gateway = LOCAL_HOST;
        dhcpInfo.ipAddress = LOCAL_HOST;
        setDhcpInfo(dhcpInfo);

        // Now add the network to ConnectivityManager.
        NetworkInfo networkInfo =
                ShadowNetworkInfo.newInstance(
                        NetworkInfo.DetailedState.CONNECTED,
                        ConnectivityManager.TYPE_WIFI,
                        0 /* subType */,
                        true /* isAvailable */,
                        true /* isConnected */);
        ShadowConnectivityManager connectivityManager =
                Shadow.extract(RuntimeEnvironment.application
                        .getSystemService(Context.CONNECTIVITY_SERVICE));
        connectivityManager.setActiveNetworkInfo(networkInfo);

        mActiveWifiInfo = wifiInfo;

        if (listener != null) {
            listener.onSuccess();
        }
    }

    private static boolean isQuoted(String str) {
        if (str == null || str.length() < 2) {
            return false;
        }

        return str.charAt(0) == '"' && str.charAt(str.length() - 1) == '"';
    }

    private static String stripQuotes(String str) {
        return str.substring(1, str.length() - 1);
    }

    @Implementation
    @Override
    protected boolean reconnect() {
        WifiConfiguration wifiConfiguration = getMostRecentNetwork();
        if (wifiConfiguration == null) {
            return false;
        }

        connect(wifiConfiguration, null);
        return true;
    }

    private WifiConfiguration getMostRecentNetwork() {
        if (getLastEnabledNetwork() == null) {
            return null;
        }

        return getWifiConfiguration(getLastEnabledNetwork().first);
    }

    public WifiConfiguration getLastAddedNetworkConfiguration() {
        return mNetworkIdToConfiguredNetworks.get(getLastAddedNetworkId());
    }

    public int getLastAddedNetworkId() {
        return mNetworkIdToConfiguredNetworks.size() - 1;
    }

    @Override
    public Pair<Integer, Boolean> getLastEnabledNetwork() {
        return mLastEnabledNetwork;
    }

    public WifiInfo getActiveWifiInfo() {
        return mActiveWifiInfo;
    }

    public static boolean verifyFactoryResetCalled(int numTimes) {
        return sResetCalledCount == numTimes;
    }

    @Implementation
    protected void forget(int netId, WifiManager.ActionListener listener) {
        mLastForgottenNetwork = netId;
    }

    public int getLastForgottenNetwork() {
        return mLastForgottenNetwork;
    }

    @Implementation
    protected void factoryReset() {
        sResetCalledCount++;
    }

    @Resetter
    public static void reset() {
        sResetCalledCount = 0;
    }

    private WifiConfiguration makeCopy(WifiConfiguration config, int networkId) {
        WifiConfiguration copy = Shadows.shadowOf(config).copy();
        copy.networkId = networkId;
        return copy;
    }
}
