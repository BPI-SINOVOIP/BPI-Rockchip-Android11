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
import android.hardware.location.NanoAppBinary;

import androidx.test.InstrumentationRegistry;

import com.google.android.chre.nanoapp.proto.ChreSettingsTest;
import com.google.android.utils.chre.SettingsUtil;

/**
 * A test to check for behavior when GNSS settings are changed.
 */
public class ContextHubGnssSettingsTestExecutor {
    private final ContextHubSettingsTestExecutor mExecutor;

    private final Instrumentation mInstrumentation = InstrumentationRegistry.getInstrumentation();

    private final SettingsUtil mSettingsUtil =
            new SettingsUtil(InstrumentationRegistry.getTargetContext());

    private boolean mInitialLocationEnabled;

    public ContextHubGnssSettingsTestExecutor(NanoAppBinary binary) {
        mExecutor = new ContextHubSettingsTestExecutor(binary);
    }

    /**
     * Should be called in a @Before method.
     */
    public void setUp() {
        mInitialLocationEnabled = mSettingsUtil.isLocationEnabled();
        mExecutor.init();
    }

    public void runGnssLocationTest() {
        runTest(ChreSettingsTest.TestCommand.Feature.GNSS_LOCATION, false /* enableFeature */);
        runTest(ChreSettingsTest.TestCommand.Feature.GNSS_LOCATION, true /* enableFeature */);
    }

    public void runGnssMeasurementTest() {
        runTest(ChreSettingsTest.TestCommand.Feature.GNSS_MEASUREMENT, false /* enableFeature */);
        runTest(ChreSettingsTest.TestCommand.Feature.GNSS_MEASUREMENT, true /* enableFeature */);
    }

    /**
     * Should be called in an @After method.
     */
    public void tearDown() {
        mExecutor.deinit();
        mSettingsUtil.setLocationMode(mInitialLocationEnabled, 30 /* timeoutSeconds */);
    }

    /**
     * Helper function to run the test.
     * @param feature The GNSS feature to test.
     * @param enableFeature True for enable.
     */
    private void runTest(ChreSettingsTest.TestCommand.Feature feature, boolean enableFeature) {
        mSettingsUtil.setLocationMode(enableFeature, 30 /* timeoutSeconds */);

        ChreSettingsTest.TestCommand.State state = enableFeature
                ? ChreSettingsTest.TestCommand.State.ENABLED
                : ChreSettingsTest.TestCommand.State.DISABLED;
        mExecutor.startTestAssertSuccess(feature, state);
    }
}
