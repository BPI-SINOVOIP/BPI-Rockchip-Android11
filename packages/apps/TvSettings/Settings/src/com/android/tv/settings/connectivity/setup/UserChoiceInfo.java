/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.tv.settings.connectivity.setup;

import android.net.wifi.ScanResult;
import android.net.wifi.WifiConfiguration;
import android.text.TextUtils;
import android.util.ArrayMap;

import androidx.annotation.IntDef;
import androidx.lifecycle.ViewModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;

/**
 * Class that stores the user choice information for basic Wi-Fi flow.
 */
public class UserChoiceInfo extends ViewModel {
    public static final int SELECT_WIFI = 1;
    public static final int PASSWORD = 2;
    public static final int SECURITY = 3;
    public static final int SSID = 4;
    public static final int EAP_METHOD = 5;
    public static final int PHASE_2_AUTHENTICATION = 6;
    public static final int CA_CERTIFICATE = 7;
    public static final int USER_CERT = 8;
    public static final int DOMAIN = 9;
    public static final int IDENTITY = 10;
    public static final int ANONYMOUS_IDENTITY = 11;
    Map<Integer, Boolean> mIsPageVisible = new ArrayMap<>();
    private HashMap<Integer, String> mDataSummary = new HashMap<>();
    private HashMap<Integer, Integer> mChoiceSummary = new HashMap<>();
    private WifiConfiguration mWifiConfiguration = new WifiConfiguration();
    private int mWifiSecurity;
    private ScanResult mChosenNetwork;
    private String mConnectedNetwork;
    private boolean mIsPasswordHidden = false;
    private ConnectionFailedStatus mConnectionFailedStatus;

    /**
     * Store the page summary into a HashMap.
     *
     * @param page The page as the key.
     * @param info The info as the value.
     */
    public void put(@PAGE int page, String info) {
        mDataSummary.put(page, info);
    }

    public void put(@PAGE int page, int choice) {
        mChoiceSummary.put(page, choice);
    }

    /**
     * Check if the summary of the queried page matches with expected string.
     *
     * @param choice The expected string.
     * @param page   The page queried.
     * @return true if matched.
     */
    public boolean choiceChosen(CharSequence choice, @PAGE int page) {
        if (!mDataSummary.containsKey(page)) {
            return false;
        }
        return TextUtils.equals(choice, mDataSummary.get(page));
    }

    public Integer getChoice(@PAGE int page) {
        if (!mChoiceSummary.containsKey(page)) {
            return null;
        }
        return mChoiceSummary.get(page);
    }

    /**
     * Get summary of a page.
     *
     * @param page The queried page.
     * @return The summary of the page.
     */
    public String getPageSummary(@PAGE int page) {
        if (!mDataSummary.containsKey(page)) {
            return null;
        }
        return mDataSummary.get(page);
    }

    /**
     * Remove the summary of a page.
     *
     * @param page The page.
     */
    public void removePageSummary(@PAGE int page) {
        mDataSummary.remove(page);
    }

    /**
     * Get {@link ScanResult} of the chosen network.
     */
    public ScanResult getChosenNetwork() {
        return mChosenNetwork;
    }

    /**
     * Set {@link ScanResult} of the chosen network.
     */
    public void setChosenNetwork(ScanResult result) {
        mChosenNetwork = result;
    }

    /**
     * Get {@link WifiConfiguration}
     */
    public WifiConfiguration getWifiConfiguration() {
        return mWifiConfiguration;
    }

    /**
     * Set {@link WifiConfiguration}
     */
    public void setWifiConfiguration(WifiConfiguration wifiConfiguration) {
        this.mWifiConfiguration = wifiConfiguration;
    }

    /**
     * Get WifiSecurity category. The category value is defined in
     * {@link com.android.settingslib.wifi.AccessPoint}
     */
    public int getWifiSecurity() {
        return mWifiSecurity;
    }

    /**
     * Set WifiSecurity
     *
     * @param wifiSecurity WifiSecurity category defined in
     *                     {@link com.android.settingslib.wifi.AccessPoint}.
     */
    public void setWifiSecurity(int wifiSecurity) {
        this.mWifiSecurity = wifiSecurity;
    }

    /**
     * Get the SSID of the connected network.
     *
     * @return the SSID.
     */
    public String getConnectedNetwork() {
        return mConnectedNetwork;
    }

    /**
     * Set the SSID of the connected network.
     *
     * @param connectedNetwork SSID of the network.
     */
    public void setConnectedNetwork(String connectedNetwork) {
        mConnectedNetwork = connectedNetwork;
    }

    /**
     * Determine whether the password is hidden.
     *
     * @return True if hidden.
     */
    public boolean isPasswordHidden() {
        return this.mIsPasswordHidden;
    }

    /**
     * Set whether the password is hidden.
     *
     * @param hidden true if hidden.
     */
    public void setPasswordHidden(boolean hidden) {
        this.mIsPasswordHidden = hidden;
    }

    public ConnectionFailedStatus getConnectionFailedStatus() {
        return mConnectionFailedStatus;
    }

    public void setConnectionFailedStatus(ConnectionFailedStatus status) {
        mConnectionFailedStatus = status;
    }

    /**
     * Initialize all the information.
     */
    public void init() {
        mDataSummary = new HashMap<>();
        mWifiConfiguration = new WifiConfiguration();
        mWifiSecurity = 0;
        mChosenNetwork = null;
        mChosenNetwork = null;
        mIsPasswordHidden = false;
    }

    public void setVisible(@PAGE int page, boolean visible) {
        mIsPageVisible.put(page, visible);
    }

    public boolean isVisible(@PAGE int page) {
        if (!mIsPageVisible.containsKey(page)) {
            return true;
        }
        return mIsPageVisible.get(page);
    }


    public enum ConnectionFailedStatus {
        AUTHENTICATION,
        REJECTED,
        TIMEOUT,
        UNKNOWN
    }

    @IntDef({
            SELECT_WIFI,
            PASSWORD,
            SECURITY,
            SSID,
            EAP_METHOD,
            PHASE_2_AUTHENTICATION,
            CA_CERTIFICATE,
            USER_CERT,
            DOMAIN,
            IDENTITY,
            ANONYMOUS_IDENTITY,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PAGE {
    }
}
