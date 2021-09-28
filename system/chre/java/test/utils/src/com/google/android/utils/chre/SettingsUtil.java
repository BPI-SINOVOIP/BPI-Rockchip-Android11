/*
 * Copyright (C) 2020 The Android Open Source Project
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
package com.google.android.utils.chre;

import android.app.Instrumentation;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.location.LocationManager;
import android.os.UserHandle;

import androidx.test.InstrumentationRegistry;

import org.junit.Assert;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * A class to get or set settings parameters.
 */
public class SettingsUtil {
    private final Context mContext;

    private final Instrumentation mInstrumentation = InstrumentationRegistry.getInstrumentation();

    private final LocationManager mLocationManager;

    public class LocationUpdateListener {
        public CountDownLatch mLocationLatch = new CountDownLatch(1);

        public BroadcastReceiver mLocationSettingReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (LocationManager.MODE_CHANGED_ACTION.equals(intent.getAction())) {
                    mLocationLatch.countDown();
                }
            }
        };
    }

    public SettingsUtil(Context context) {
        mContext = context;
        mLocationManager = (LocationManager) context.getSystemService(Context.LOCATION_SERVICE);
        Assert.assertTrue(mLocationManager != null);
    }

    /**
     * @param enable true to enable always WiFi scanning.
     */
    public void setWifiScanningSettings(boolean enable) {
        String value = enable ? "1" : "0";
        ChreTestUtil.executeShellCommand(
                mInstrumentation, "settings put global wifi_scan_always_enabled " + value);

        Assert.assertTrue(isWifiScanningAlwaysEnabled() == enable);
    }

    /**
     * @param enable true to enable WiFi service.
     */
    public void setWifi(boolean enable) {
        String value = enable ? "enable" : "disable";
        ChreTestUtil.executeShellCommand(mInstrumentation, "svc wifi " + value);

        Assert.assertTrue(isWifiEnabled() == enable);
    }

    /**
     * @return true if the WiFi scanning is always enabled.
     */
    public boolean isWifiScanningAlwaysEnabled() {
        String out = ChreTestUtil.executeShellCommand(
                mInstrumentation, "settings get global wifi_scan_always_enabled");
        return ChreTestUtil.convertToIntegerOrFail(out) > 0;
    }

    /**
     * @return true if the WiFi service is enabled.
     */
    public boolean isWifiEnabled() {
        String out = ChreTestUtil.executeShellCommand(
                mInstrumentation, "settings get global wifi_on");
        return ChreTestUtil.convertToIntegerOrFail(out) > 0;
    }

    /**
     * Sets the location mode on the device.
     * @param enable True to enable location, false to disable it.
     * @param timeoutSeconds The maximum amount of time in seconds to wait.
     */
    public void setLocationMode(boolean enable, long timeoutSeconds) {
        if (isLocationEnabled() != enable) {
            LocationUpdateListener listener = new LocationUpdateListener();

            mContext.registerReceiver(
                    listener.mLocationSettingReceiver,
                    new IntentFilter(LocationManager.MODE_CHANGED_ACTION));
            mLocationManager.setLocationEnabledForUser(enable, UserHandle.CURRENT);

            try {
                listener.mLocationLatch.await(timeoutSeconds, TimeUnit.SECONDS);

                // Wait 1 additional second to make sure setting gets propagated to CHRE
                Thread.sleep(1000);
            } catch (InterruptedException e) {
                Assert.fail("InterruptedException while waiting for location update");
            }

            Assert.assertTrue(isLocationEnabled() == enable);

            mContext.unregisterReceiver(listener.mLocationSettingReceiver);
        }
    }

    /**
     * @return True if location is enabled.
     */
    public boolean isLocationEnabled() {
        return mLocationManager.isLocationEnabledForUser(UserHandle.CURRENT);
    }
}
