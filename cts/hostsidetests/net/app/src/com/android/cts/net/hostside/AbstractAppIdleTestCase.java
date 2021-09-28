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

import static com.android.cts.net.hostside.Property.APP_STANDBY_MODE;
import static com.android.cts.net.hostside.Property.BATTERY_SAVER_MODE;

import static org.junit.Assert.assertEquals;

import android.os.SystemClock;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

/**
 * Base class for metered and non-metered tests on idle apps.
 */
@RequiredProperties({APP_STANDBY_MODE})
abstract class AbstractAppIdleTestCase extends AbstractRestrictBackgroundNetworkTestCase {

    @Before
    public final void setUp() throws Exception {
        super.setUp();

        // Set initial state.
        removePowerSaveModeWhitelist(TEST_APP2_PKG);
        removePowerSaveModeExceptIdleWhitelist(TEST_APP2_PKG);
        setAppIdle(false);
        turnBatteryOn();

        registerBroadcastReceiver();
    }

    @After
    public final void tearDown() throws Exception {
        super.tearDown();

        executeSilentShellCommand("cmd battery reset");
        setAppIdle(false);
    }

    @Test
    public void testBackgroundNetworkAccess_enabled() throws Exception {
        setAppIdle(true);
        assertBackgroundNetworkAccess(false);

        assertsForegroundAlwaysHasNetworkAccess();
        setAppIdle(true);
        assertBackgroundNetworkAccess(false);

        // Make sure foreground app doesn't lose access upon enabling it.
        setAppIdle(true);
        launchComponentAndAssertNetworkAccess(TYPE_COMPONENT_ACTIVTIY);
        finishActivity();
        assertAppIdle(false); // Sanity check - not idle anymore, since activity was launched...
        assertBackgroundNetworkAccess(true);
        setAppIdle(true);
        assertBackgroundNetworkAccess(false);

        // Same for foreground service.
        setAppIdle(true);
        launchComponentAndAssertNetworkAccess(TYPE_COMPONENT_FOREGROUND_SERVICE);
        stopForegroundService();
        assertAppIdle(true);
        assertBackgroundNetworkAccess(false);
    }

    @Test
    public void testBackgroundNetworkAccess_whitelisted() throws Exception {
        setAppIdle(true);
        assertBackgroundNetworkAccess(false);

        addPowerSaveModeWhitelist(TEST_APP2_PKG);
        assertAppIdle(false); // Sanity check - not idle anymore, since whitelisted
        assertBackgroundNetworkAccess(true);

        setAppIdleNoAssert(true);
        assertAppIdle(false); // app is still whitelisted
        removePowerSaveModeWhitelist(TEST_APP2_PKG);
        assertAppIdle(true); // Sanity check - idle again, once whitelisted was removed
        assertBackgroundNetworkAccess(false);

        setAppIdle(true);
        addPowerSaveModeExceptIdleWhitelist(TEST_APP2_PKG);
        assertAppIdle(false); // Sanity check - not idle anymore, since whitelisted
        assertBackgroundNetworkAccess(true);

        setAppIdleNoAssert(true);
        assertAppIdle(false); // app is still whitelisted
        removePowerSaveModeExceptIdleWhitelist(TEST_APP2_PKG);
        assertAppIdle(true); // Sanity check - idle again, once whitelisted was removed
        assertBackgroundNetworkAccess(false);

        assertsForegroundAlwaysHasNetworkAccess();

        // Sanity check - no whitelist, no access!
        setAppIdle(true);
        assertBackgroundNetworkAccess(false);
    }

    @Test
    public void testBackgroundNetworkAccess_tempWhitelisted() throws Exception {
        setAppIdle(true);
        assertBackgroundNetworkAccess(false);

        addTempPowerSaveModeWhitelist(TEST_APP2_PKG, TEMP_POWERSAVE_WHITELIST_DURATION_MS);
        assertBackgroundNetworkAccess(true);
        // Wait until the whitelist duration is expired.
        SystemClock.sleep(TEMP_POWERSAVE_WHITELIST_DURATION_MS);
        assertBackgroundNetworkAccess(false);
    }

    @Test
    public void testBackgroundNetworkAccess_disabled() throws Exception {
        assertBackgroundNetworkAccess(true);

        assertsForegroundAlwaysHasNetworkAccess();
        assertBackgroundNetworkAccess(true);
    }

    @RequiredProperties({BATTERY_SAVER_MODE})
    @Test
    public void testAppIdleNetworkAccess_whenCharging() throws Exception {
        // Check that app is paroled when charging
        setAppIdle(true);
        assertBackgroundNetworkAccess(false);
        turnBatteryOff();
        assertBackgroundNetworkAccess(true);
        turnBatteryOn();
        assertBackgroundNetworkAccess(false);

        // Check that app is restricted when not idle but power-save is on
        setAppIdle(false);
        assertBackgroundNetworkAccess(true);
        setBatterySaverMode(true);
        assertBackgroundNetworkAccess(false);
        // Use setBatterySaverMode API to leave power-save mode instead of plugging in charger
        setBatterySaverMode(false);
        turnBatteryOff();
        assertBackgroundNetworkAccess(true);

        // And when no longer charging, it still has network access, since it's not idle
        turnBatteryOn();
        assertBackgroundNetworkAccess(true);
    }

    @Test
    public void testAppIdleNetworkAccess_idleWhitelisted() throws Exception {
        setAppIdle(true);
        assertAppIdle(true);
        assertBackgroundNetworkAccess(false);

        addAppIdleWhitelist(mUid);
        assertBackgroundNetworkAccess(true);

        removeAppIdleWhitelist(mUid);
        assertBackgroundNetworkAccess(false);

        // Make sure whitelisting a random app doesn't affect the tested app.
        addAppIdleWhitelist(mUid + 1);
        assertBackgroundNetworkAccess(false);
        removeAppIdleWhitelist(mUid + 1);
    }

    @Test
    public void testAppIdle_toast() throws Exception {
        setAppIdle(true);
        assertAppIdle(true);
        assertEquals("Shown", showToast());
        assertAppIdle(true);
        // Wait for a couple of seconds for the toast to actually be shown
        SystemClock.sleep(2000);
        assertAppIdle(true);
    }
}
