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
import android.net.wifi.SoftApConfiguration;
import android.net.wifi.WifiManager;

import com.android.car.settings.wifi.CarWifiManager;
import com.android.settingslib.wifi.AccessPoint;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import java.util.List;

/** TODO(b/148971715): Refactor all methods to run without relying on sInstance. */
@Implements(CarWifiManager.class)
public class ShadowCarWifiManager {

    public static final int STATE_UNKNOWN = -1;
    public static final int STATE_STARTED = 0;
    public static final int STATE_STOPPED = 1;
    public static final int STATE_DESTROYED = 2;

    private static CarWifiManager sInstance;
    private static int sCurrentState = STATE_UNKNOWN;
    private static SoftApConfiguration sSoftApConfiguration =
            new SoftApConfiguration.Builder().build();
    private static boolean sIs5GhzBandSupported = true;
    private static int sWifiState = WifiManager.WIFI_STATE_UNKNOWN;

    public static void setInstance(CarWifiManager wifiManager) {
        sInstance = wifiManager;
    }

    @Resetter
    public static void reset() {
        sInstance = null;
        sSoftApConfiguration = new SoftApConfiguration.Builder().build();
        sCurrentState = STATE_UNKNOWN;
        sIs5GhzBandSupported = true;
        sWifiState = WifiManager.WIFI_STATE_UNKNOWN;
    }

    @Implementation
    public void __constructor__(Context context) {
    }

    @Implementation
    public void start() {
        sCurrentState = STATE_STARTED;
    }

    @Implementation
    public void stop() {
        sCurrentState = STATE_STOPPED;
    }

    @Implementation
    public void destroy() {
        sCurrentState = STATE_DESTROYED;
    }

    @Implementation
    public void setSoftApConfig(SoftApConfiguration config) {
        sSoftApConfiguration = config;
    }

    @Implementation
    public SoftApConfiguration getSoftApConfig() {
        return sSoftApConfiguration;
    }

    @Implementation
    public boolean isWifiEnabled() {
        return sInstance.isWifiEnabled();
    }

    @Implementation
    public int getWifiState() {
        return sWifiState;
    }

    public static void setWifiState(int wifiState) {
        sWifiState = wifiState;
    }

    @Implementation
    public boolean isWifiApEnabled() {
        return sInstance.isWifiApEnabled();
    }

    @Implementation
    public List<AccessPoint> getAllAccessPoints() {
        return sInstance.getAllAccessPoints();
    }

    @Implementation
    public List<AccessPoint> getSavedAccessPoints() {
        return sInstance.getSavedAccessPoints();
    }

    @Implementation
    public void connectToPublicWifi(AccessPoint accessPoint, WifiManager.ActionListener listener) {
        sInstance.connectToPublicWifi(accessPoint, listener);
    }

    @Implementation
    protected void connectToSavedWifi(AccessPoint accessPoint,
            WifiManager.ActionListener listener) {
        sInstance.connectToSavedWifi(accessPoint, listener);
    }

    @Implementation
    protected String getCountryCode() {
        return "1";
    }

    @Implementation
    protected boolean is5GhzBandSupported() {
        return sIs5GhzBandSupported;
    }

    @Implementation
    protected boolean addListener(CarWifiManager.Listener listener) {
        return sInstance.addListener(listener);
    }

    public static void setIs5GhzBandSupported(boolean supported) {
        sIs5GhzBandSupported = supported;
    }

    public static int getCurrentState() {
        return sCurrentState;
    }
}
