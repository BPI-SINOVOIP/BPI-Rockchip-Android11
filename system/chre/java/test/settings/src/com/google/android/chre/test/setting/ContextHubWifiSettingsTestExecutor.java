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
package com.google.android.chre.test.setting;

import android.app.Instrumentation;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.location.NanoAppBinary;
import android.net.wifi.WifiManager;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import com.google.android.chre.nanoapp.proto.ChreSettingsTest;
import com.google.android.utils.chre.SettingsUtil;

import org.junit.Assert;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * A test to check for behavior when WiFi settings are changed.
 */
public class ContextHubWifiSettingsTestExecutor {
    private static final String TAG = "ContextHubWifiSettingsTestExecutor";
    private final ContextHubSettingsTestExecutor mExecutor;

    private final Instrumentation mInstrumentation = InstrumentationRegistry.getInstrumentation();

    private final Context mContext = mInstrumentation.getTargetContext();

    private final SettingsUtil mSettingsUtil =
            new SettingsUtil(InstrumentationRegistry.getTargetContext());

    private boolean mInitialWifiEnabled;

    private boolean mInitialWifiScanningAlwaysEnabled;

    private boolean mInitialLocationEnabled;

    public class WifiUpdateListener {
        public WifiUpdateListener(boolean enable) {
            mEnable = enable;
        }

        // True if WiFi is expected to become available
        private final boolean mEnable;

        public CountDownLatch mWifiLatch = new CountDownLatch(1);

        public BroadcastReceiver mWifiUpdateReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (WifiManager.WIFI_STATE_CHANGED_ACTION.equals(intent.getAction())) {
                    int state = intent.getIntExtra(WifiManager.EXTRA_WIFI_STATE, -1);
                    if ((mEnable && state == WifiManager.WIFI_STATE_ENABLED)
                            || (!mEnable && state == WifiManager.WIFI_STATE_DISABLED)) {
                        mWifiLatch.countDown();
                    }
                }
            }
        };
    }

    public ContextHubWifiSettingsTestExecutor(NanoAppBinary binary) {
        mExecutor = new ContextHubSettingsTestExecutor(binary);
    }

    /**
     * Should be called in a @Before method.
     */
    public void setUp() {
        mInitialLocationEnabled = mSettingsUtil.isLocationEnabled();
        mInitialWifiEnabled = mSettingsUtil.isWifiEnabled();
        mInitialWifiScanningAlwaysEnabled = mSettingsUtil.isWifiScanningAlwaysEnabled();
        Log.d(TAG, "isLocationEnalbed=" + mInitialLocationEnabled
                + ", isWifiEnabled=" + mInitialWifiEnabled
                + ", isWifiScanningAlwaysEnabled=" + mInitialWifiScanningAlwaysEnabled);
        mExecutor.init();
    }

    public void runWifiScanningTest() {
        runWifiScanningTest(false /* enableFeature */);
        runWifiScanningTest(true /* enableFeature */);
    }

    public void runWifiRangingTest() {
        runWifiRangingTest(false /* enableFeature */);
        runWifiRangingTest(true /* enableFeature */);
    }

    /**
     * Should be called in an @After method.
     */
    public void tearDown() {
        mExecutor.deinit();
        mSettingsUtil.setWifi(mInitialWifiEnabled);
        mSettingsUtil.setWifiScanningSettings(mInitialWifiScanningAlwaysEnabled);
        mSettingsUtil.setLocationMode(mInitialLocationEnabled, 30 /* timeoutSeconds */);
    }

    /**
     * Sets the WiFi scanning settings on the device.
     * @param enable true to enable WiFi settings, false to disable it.
     */
    private void setWifiSettings(boolean enable) {
        WifiUpdateListener wifiUpdateListener = new WifiUpdateListener(enable);
        mContext.registerReceiver(
                wifiUpdateListener.mWifiUpdateReceiver,
                new IntentFilter(WifiManager.WIFI_STATE_CHANGED_ACTION));

        mSettingsUtil.setWifiScanningSettings(enable);
        mSettingsUtil.setWifi(enable);

        try {
            wifiUpdateListener.mWifiLatch.await(30, TimeUnit.SECONDS);

            // Wait a few seconds to ensure setting is propagated to CHRE path
            Thread.sleep(10000);
        } catch (InterruptedException e) {
            Assert.fail(e.getMessage());
        }

        mContext.unregisterReceiver(wifiUpdateListener.mWifiUpdateReceiver);
    }

    /**
     * Helper function to run the test.
     * @param enableFeature True for enable.
     */
    private void runWifiScanningTest(boolean enableFeature) {
        setWifiSettings(enableFeature);

        ChreSettingsTest.TestCommand.State state = enableFeature
                ? ChreSettingsTest.TestCommand.State.ENABLED
                : ChreSettingsTest.TestCommand.State.DISABLED;
        mExecutor.startTestAssertSuccess(
                ChreSettingsTest.TestCommand.Feature.WIFI_SCANNING, state);
    }

    /**
     * Helper function to run the test.
     * @param enableFeature True for enable.
     */
    private void runWifiRangingTest(boolean enableFeature) {
        if (!mSettingsUtil.isWifiEnabled() || !mSettingsUtil.isWifiScanningAlwaysEnabled()) {
            setWifiSettings(true /* enable */);
        }

        // Set up is needed for the nanoapp to issue an initial WiFi scan.
        mExecutor.setupTestAssertSuccess(ChreSettingsTest.TestCommand.Feature.WIFI_RTT);

        mSettingsUtil.setLocationMode(enableFeature, 30 /* timeoutSeconds */);

        ChreSettingsTest.TestCommand.State state = enableFeature
                ? ChreSettingsTest.TestCommand.State.ENABLED
                : ChreSettingsTest.TestCommand.State.DISABLED;
        mExecutor.startTestAssertSuccess(ChreSettingsTest.TestCommand.Feature.WIFI_RTT, state);
    }
}
