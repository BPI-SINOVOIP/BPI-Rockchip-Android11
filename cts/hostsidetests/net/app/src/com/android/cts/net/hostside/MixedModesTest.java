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
 * limitations under the License.
 */
package com.android.cts.net.hostside;

import static com.android.cts.net.hostside.NetworkPolicyTestUtils.setRestrictBackground;
import static com.android.cts.net.hostside.Property.APP_STANDBY_MODE;
import static com.android.cts.net.hostside.Property.BATTERY_SAVER_MODE;
import static com.android.cts.net.hostside.Property.DATA_SAVER_MODE;
import static com.android.cts.net.hostside.Property.DOZE_MODE;
import static com.android.cts.net.hostside.Property.METERED_NETWORK;
import static com.android.cts.net.hostside.Property.NON_METERED_NETWORK;

import android.os.SystemClock;
import android.util.Log;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

/**
 * Test cases for the more complex scenarios where multiple restrictions (like Battery Saver Mode
 * and Data Saver Mode) are applied simultaneously.
 * <p>
 * <strong>NOTE: </strong>it might sound like the test methods on this class are testing too much,
 * which would make it harder to diagnose individual failures, but the assumption is that such
 * failure most likely will happen when the restriction is tested individually as well.
 */
public class MixedModesTest extends AbstractRestrictBackgroundNetworkTestCase {
    private static final String TAG = "MixedModesTest";

    @Before
    public void setUp() throws Exception {
        super.setUp();

        // Set initial state.
        removeRestrictBackgroundWhitelist(mUid);
        removeRestrictBackgroundBlacklist(mUid);
        removePowerSaveModeWhitelist(TEST_APP2_PKG);
        removePowerSaveModeExceptIdleWhitelist(TEST_APP2_PKG);

        registerBroadcastReceiver();
    }

    @After
    public void tearDown() throws Exception {
        super.tearDown();

        try {
            setRestrictBackground(false);
        } finally {
            setBatterySaverMode(false);
        }
    }

    /**
     * Tests all DS ON and BS ON scenarios from network-policy-restrictions.md on metered networks.
     */
    @RequiredProperties({DATA_SAVER_MODE, BATTERY_SAVER_MODE, METERED_NETWORK})
    @Test
    public void testDataAndBatterySaverModes_meteredNetwork() throws Exception {
        final MeterednessConfigurationRule meterednessConfiguration
                = new MeterednessConfigurationRule();
        meterednessConfiguration.configureNetworkMeteredness(true);
        try {
            setRestrictBackground(true);
            setBatterySaverMode(true);

            Log.v(TAG, "Not whitelisted for any.");
            assertBackgroundNetworkAccess(false);
            assertsForegroundAlwaysHasNetworkAccess();
            assertBackgroundNetworkAccess(false);

            Log.v(TAG, "Whitelisted for Data Saver but not for Battery Saver.");
            addRestrictBackgroundWhitelist(mUid);
            removePowerSaveModeWhitelist(TEST_APP2_PKG);
            assertBackgroundNetworkAccess(false);
            assertsForegroundAlwaysHasNetworkAccess();
            assertBackgroundNetworkAccess(false);
            removeRestrictBackgroundWhitelist(mUid);

            Log.v(TAG, "Whitelisted for Battery Saver but not for Data Saver.");
            addPowerSaveModeWhitelist(TEST_APP2_PKG);
            removeRestrictBackgroundWhitelist(mUid);
            assertBackgroundNetworkAccess(false);
            assertsForegroundAlwaysHasNetworkAccess();
            assertBackgroundNetworkAccess(false);
            removePowerSaveModeWhitelist(TEST_APP2_PKG);

            Log.v(TAG, "Whitelisted for both.");
            addRestrictBackgroundWhitelist(mUid);
            addPowerSaveModeWhitelist(TEST_APP2_PKG);
            assertBackgroundNetworkAccess(true);
            assertsForegroundAlwaysHasNetworkAccess();
            assertBackgroundNetworkAccess(true);
            removePowerSaveModeWhitelist(TEST_APP2_PKG);
            assertBackgroundNetworkAccess(false);
            removeRestrictBackgroundWhitelist(mUid);

            Log.v(TAG, "Blacklisted for Data Saver, not whitelisted for Battery Saver.");
            addRestrictBackgroundBlacklist(mUid);
            removePowerSaveModeWhitelist(TEST_APP2_PKG);
            assertBackgroundNetworkAccess(false);
            assertsForegroundAlwaysHasNetworkAccess();
            assertBackgroundNetworkAccess(false);
            removeRestrictBackgroundBlacklist(mUid);

            Log.v(TAG, "Blacklisted for Data Saver, whitelisted for Battery Saver.");
            addRestrictBackgroundBlacklist(mUid);
            addPowerSaveModeWhitelist(TEST_APP2_PKG);
            assertBackgroundNetworkAccess(false);
            assertsForegroundAlwaysHasNetworkAccess();
            assertBackgroundNetworkAccess(false);
            removeRestrictBackgroundBlacklist(mUid);
            removePowerSaveModeWhitelist(TEST_APP2_PKG);
        } finally {
            meterednessConfiguration.resetNetworkMeteredness();
        }
    }

    /**
     * Tests all DS ON and BS ON scenarios from network-policy-restrictions.md on non-metered
     * networks.
     */
    @RequiredProperties({DATA_SAVER_MODE, BATTERY_SAVER_MODE, NON_METERED_NETWORK})
    @Test
    public void testDataAndBatterySaverModes_nonMeteredNetwork() throws Exception {
        final MeterednessConfigurationRule meterednessConfiguration
                = new MeterednessConfigurationRule();
        meterednessConfiguration.configureNetworkMeteredness(false);
        try {
            setRestrictBackground(true);
            setBatterySaverMode(true);

            Log.v(TAG, "Not whitelisted for any.");
            assertBackgroundNetworkAccess(false);
            assertsForegroundAlwaysHasNetworkAccess();
            assertBackgroundNetworkAccess(false);

            Log.v(TAG, "Whitelisted for Data Saver but not for Battery Saver.");
            addRestrictBackgroundWhitelist(mUid);
            removePowerSaveModeWhitelist(TEST_APP2_PKG);
            assertBackgroundNetworkAccess(false);
            assertsForegroundAlwaysHasNetworkAccess();
            assertBackgroundNetworkAccess(false);
            removeRestrictBackgroundWhitelist(mUid);

            Log.v(TAG, "Whitelisted for Battery Saver but not for Data Saver.");
            addPowerSaveModeWhitelist(TEST_APP2_PKG);
            removeRestrictBackgroundWhitelist(mUid);
            assertBackgroundNetworkAccess(true);
            assertsForegroundAlwaysHasNetworkAccess();
            assertBackgroundNetworkAccess(true);
            removePowerSaveModeWhitelist(TEST_APP2_PKG);

            Log.v(TAG, "Whitelisted for both.");
            addRestrictBackgroundWhitelist(mUid);
            addPowerSaveModeWhitelist(TEST_APP2_PKG);
            assertBackgroundNetworkAccess(true);
            assertsForegroundAlwaysHasNetworkAccess();
            assertBackgroundNetworkAccess(true);
            removePowerSaveModeWhitelist(TEST_APP2_PKG);
            assertBackgroundNetworkAccess(false);
            removeRestrictBackgroundWhitelist(mUid);

            Log.v(TAG, "Blacklisted for Data Saver, not whitelisted for Battery Saver.");
            addRestrictBackgroundBlacklist(mUid);
            removePowerSaveModeWhitelist(TEST_APP2_PKG);
            assertBackgroundNetworkAccess(false);
            assertsForegroundAlwaysHasNetworkAccess();
            assertBackgroundNetworkAccess(false);
            removeRestrictBackgroundBlacklist(mUid);

            Log.v(TAG, "Blacklisted for Data Saver, whitelisted for Battery Saver.");
            addRestrictBackgroundBlacklist(mUid);
            addPowerSaveModeWhitelist(TEST_APP2_PKG);
            assertBackgroundNetworkAccess(true);
            assertsForegroundAlwaysHasNetworkAccess();
            assertBackgroundNetworkAccess(true);
            removeRestrictBackgroundBlacklist(mUid);
            removePowerSaveModeWhitelist(TEST_APP2_PKG);
        } finally {
            meterednessConfiguration.resetNetworkMeteredness();
        }
    }

    /**
     * Tests that powersave whitelists works as expected when doze and battery saver modes
     * are enabled.
     */
    @RequiredProperties({DOZE_MODE, BATTERY_SAVER_MODE})
    @Test
    public void testDozeAndBatterySaverMode_powerSaveWhitelists() throws Exception {
        setBatterySaverMode(true);
        setDozeMode(true);

        try {
            addPowerSaveModeWhitelist(TEST_APP2_PKG);
            assertBackgroundNetworkAccess(true);

            removePowerSaveModeWhitelist(TEST_APP2_PKG);
            assertBackgroundNetworkAccess(false);

            addPowerSaveModeExceptIdleWhitelist(TEST_APP2_PKG);
            assertBackgroundNetworkAccess(false);

            removePowerSaveModeExceptIdleWhitelist(TEST_APP2_PKG);
            assertBackgroundNetworkAccess(false);
        } finally {
            setBatterySaverMode(false);
            setDozeMode(false);
        }
    }

    /**
     * Tests that powersave whitelists works as expected when doze and appIdle modes
     * are enabled.
     */
    @RequiredProperties({DOZE_MODE, APP_STANDBY_MODE})
    @Test
    public void testDozeAndAppIdle_powerSaveWhitelists() throws Exception {
        setDozeMode(true);
        setAppIdle(true);

        try {
            addPowerSaveModeWhitelist(TEST_APP2_PKG);
            assertBackgroundNetworkAccess(true);

            removePowerSaveModeWhitelist(TEST_APP2_PKG);
            assertBackgroundNetworkAccess(false);

            addPowerSaveModeExceptIdleWhitelist(TEST_APP2_PKG);
            assertBackgroundNetworkAccess(false);

            removePowerSaveModeExceptIdleWhitelist(TEST_APP2_PKG);
            assertBackgroundNetworkAccess(false);
        } finally {
            setAppIdle(false);
            setDozeMode(false);
        }
    }

    @RequiredProperties({APP_STANDBY_MODE, DOZE_MODE})
    @Test
    public void testAppIdleAndDoze_tempPowerSaveWhitelists() throws Exception {
        setDozeMode(true);
        setAppIdle(true);

        try {
            assertBackgroundNetworkAccess(false);

            addTempPowerSaveModeWhitelist(TEST_APP2_PKG, TEMP_POWERSAVE_WHITELIST_DURATION_MS);
            assertBackgroundNetworkAccess(true);

            // Wait until the whitelist duration is expired.
            SystemClock.sleep(TEMP_POWERSAVE_WHITELIST_DURATION_MS);
            assertBackgroundNetworkAccess(false);
        } finally {
            setAppIdle(false);
            setDozeMode(false);
        }
    }

    @RequiredProperties({APP_STANDBY_MODE, BATTERY_SAVER_MODE})
    @Test
    public void testAppIdleAndBatterySaver_tempPowerSaveWhitelists() throws Exception {
        setBatterySaverMode(true);
        setAppIdle(true);

        try {
            assertBackgroundNetworkAccess(false);

            addTempPowerSaveModeWhitelist(TEST_APP2_PKG, TEMP_POWERSAVE_WHITELIST_DURATION_MS);
            assertBackgroundNetworkAccess(true);

            // Wait until the whitelist duration is expired.
            SystemClock.sleep(TEMP_POWERSAVE_WHITELIST_DURATION_MS);
            assertBackgroundNetworkAccess(false);
        } finally {
            setAppIdle(false);
            setBatterySaverMode(false);
        }
    }

    /**
     * Tests that the app idle whitelist works as expected when doze and appIdle mode are enabled.
     */
    @RequiredProperties({DOZE_MODE, APP_STANDBY_MODE})
    @Test
    public void testDozeAndAppIdle_appIdleWhitelist() throws Exception {
        setDozeMode(true);
        setAppIdle(true);

        try {
            assertBackgroundNetworkAccess(false);

            // UID still shouldn't have access because of Doze.
            addAppIdleWhitelist(mUid);
            assertBackgroundNetworkAccess(false);

            removeAppIdleWhitelist(mUid);
            assertBackgroundNetworkAccess(false);
        } finally {
            setAppIdle(false);
            setDozeMode(false);
        }
    }

    @RequiredProperties({APP_STANDBY_MODE, DOZE_MODE})
    @Test
    public void testAppIdleAndDoze_tempPowerSaveAndAppIdleWhitelists() throws Exception {
        setDozeMode(true);
        setAppIdle(true);

        try {
            assertBackgroundNetworkAccess(false);

            addAppIdleWhitelist(mUid);
            assertBackgroundNetworkAccess(false);

            addTempPowerSaveModeWhitelist(TEST_APP2_PKG, TEMP_POWERSAVE_WHITELIST_DURATION_MS);
            assertBackgroundNetworkAccess(true);

            // Wait until the whitelist duration is expired.
            SystemClock.sleep(TEMP_POWERSAVE_WHITELIST_DURATION_MS);
            assertBackgroundNetworkAccess(false);
        } finally {
            setAppIdle(false);
            setDozeMode(false);
            removeAppIdleWhitelist(mUid);
        }
    }

    @RequiredProperties({APP_STANDBY_MODE, BATTERY_SAVER_MODE})
    @Test
    public void testAppIdleAndBatterySaver_tempPowerSaveAndAppIdleWhitelists() throws Exception {
        setBatterySaverMode(true);
        setAppIdle(true);

        try {
            assertBackgroundNetworkAccess(false);

            addAppIdleWhitelist(mUid);
            assertBackgroundNetworkAccess(false);

            addTempPowerSaveModeWhitelist(TEST_APP2_PKG, TEMP_POWERSAVE_WHITELIST_DURATION_MS);
            assertBackgroundNetworkAccess(true);

            // Wait until the whitelist duration is expired.
            SystemClock.sleep(TEMP_POWERSAVE_WHITELIST_DURATION_MS);
            assertBackgroundNetworkAccess(false);
        } finally {
            setAppIdle(false);
            setBatterySaverMode(false);
            removeAppIdleWhitelist(mUid);
        }
    }
}
