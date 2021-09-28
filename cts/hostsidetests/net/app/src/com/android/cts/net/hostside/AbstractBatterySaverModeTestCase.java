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

import static com.android.cts.net.hostside.Property.BATTERY_SAVER_MODE;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

/**
 * Base class for metered and non-metered Battery Saver Mode tests.
 */
@RequiredProperties({BATTERY_SAVER_MODE})
abstract class AbstractBatterySaverModeTestCase extends AbstractRestrictBackgroundNetworkTestCase {

    @Before
    public final void setUp() throws Exception {
        super.setUp();

        // Set initial state.
        removePowerSaveModeWhitelist(TEST_APP2_PKG);
        removePowerSaveModeExceptIdleWhitelist(TEST_APP2_PKG);
        setBatterySaverMode(false);

        registerBroadcastReceiver();
    }

    @After
    public final void tearDown() throws Exception {
        super.tearDown();

        setBatterySaverMode(false);
    }

    @Test
    public void testBackgroundNetworkAccess_enabled() throws Exception {
        setBatterySaverMode(true);
        assertBackgroundNetworkAccess(false);

        assertsForegroundAlwaysHasNetworkAccess();
        assertBackgroundNetworkAccess(false);

        // Make sure foreground app doesn't lose access upon Battery Saver.
        setBatterySaverMode(false);
        launchComponentAndAssertNetworkAccess(TYPE_COMPONENT_ACTIVTIY);
        setBatterySaverMode(true);
        assertForegroundNetworkAccess();

        // Although it should not have access while the screen is off.
        turnScreenOff();
        assertBackgroundNetworkAccess(false);
        turnScreenOn();
        assertForegroundNetworkAccess();

        // Goes back to background state.
        finishActivity();
        assertBackgroundNetworkAccess(false);

        // Make sure foreground service doesn't lose access upon enabling Battery Saver.
        setBatterySaverMode(false);
        launchComponentAndAssertNetworkAccess(TYPE_COMPONENT_FOREGROUND_SERVICE);
        setBatterySaverMode(true);
        assertForegroundNetworkAccess();
        stopForegroundService();
        assertBackgroundNetworkAccess(false);
    }

    @Test
    public void testBackgroundNetworkAccess_whitelisted() throws Exception {
        setBatterySaverMode(true);
        assertBackgroundNetworkAccess(false);

        addPowerSaveModeWhitelist(TEST_APP2_PKG);
        assertBackgroundNetworkAccess(true);

        removePowerSaveModeWhitelist(TEST_APP2_PKG);
        assertBackgroundNetworkAccess(false);

        addPowerSaveModeExceptIdleWhitelist(TEST_APP2_PKG);
        assertBackgroundNetworkAccess(true);

        removePowerSaveModeExceptIdleWhitelist(TEST_APP2_PKG);
        assertBackgroundNetworkAccess(false);

        assertsForegroundAlwaysHasNetworkAccess();
        assertBackgroundNetworkAccess(false);
    }

    @Test
    public void testBackgroundNetworkAccess_disabled() throws Exception {
        assertBackgroundNetworkAccess(true);

        assertsForegroundAlwaysHasNetworkAccess();
        assertBackgroundNetworkAccess(true);
    }
}
