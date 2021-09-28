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

import static com.android.cts.net.hostside.Property.DOZE_MODE;
import static com.android.cts.net.hostside.Property.NOT_LOW_RAM_DEVICE;

import android.os.SystemClock;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

/**
 * Base class for metered and non-metered Doze Mode tests.
 */
@RequiredProperties({DOZE_MODE})
abstract class AbstractDozeModeTestCase extends AbstractRestrictBackgroundNetworkTestCase {

    @Before
    public final void setUp() throws Exception {
        super.setUp();

        // Set initial state.
        removePowerSaveModeWhitelist(TEST_APP2_PKG);
        removePowerSaveModeExceptIdleWhitelist(TEST_APP2_PKG);
        setDozeMode(false);

        registerBroadcastReceiver();
    }

    @After
    public final void tearDown() throws Exception {
        super.tearDown();

        setDozeMode(false);
    }

    @Test
    public void testBackgroundNetworkAccess_enabled() throws Exception {
        setDozeMode(true);
        assertBackgroundNetworkAccess(false);

        assertsForegroundAlwaysHasNetworkAccess();
        assertBackgroundNetworkAccess(false);

        // Make sure foreground service doesn't lose network access upon enabling doze.
        setDozeMode(false);
        launchComponentAndAssertNetworkAccess(TYPE_COMPONENT_FOREGROUND_SERVICE);
        setDozeMode(true);
        assertForegroundNetworkAccess();
        stopForegroundService();
        assertBackgroundState();
        assertBackgroundNetworkAccess(false);
    }

    @Test
    public void testBackgroundNetworkAccess_whitelisted() throws Exception {
        setDozeMode(true);
        assertBackgroundNetworkAccess(false);

        addPowerSaveModeWhitelist(TEST_APP2_PKG);
        assertBackgroundNetworkAccess(true);

        removePowerSaveModeWhitelist(TEST_APP2_PKG);
        assertBackgroundNetworkAccess(false);

        addPowerSaveModeExceptIdleWhitelist(TEST_APP2_PKG);
        assertBackgroundNetworkAccess(false);

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

    @RequiredProperties({NOT_LOW_RAM_DEVICE})
    @Test
    public void testBackgroundNetworkAccess_enabledButWhitelistedOnNotificationAction()
            throws Exception {
        setPendingIntentWhitelistDuration(NETWORK_TIMEOUT_MS);
        try {
            registerNotificationListenerService();
            setDozeMode(true);
            assertBackgroundNetworkAccess(false);

            testNotification(4, NOTIFICATION_TYPE_CONTENT);
            testNotification(8, NOTIFICATION_TYPE_DELETE);
            testNotification(15, NOTIFICATION_TYPE_FULL_SCREEN);
            testNotification(16, NOTIFICATION_TYPE_BUNDLE);
            testNotification(23, NOTIFICATION_TYPE_ACTION);
            testNotification(42, NOTIFICATION_TYPE_ACTION_BUNDLE);
            testNotification(108, NOTIFICATION_TYPE_ACTION_REMOTE_INPUT);
        } finally {
            resetDeviceIdleSettings();
        }
    }

    private void testNotification(int id, String type) throws Exception {
        sendNotification(id, type);
        assertBackgroundNetworkAccess(true);
        if (type.equals(NOTIFICATION_TYPE_ACTION)) {
            // Make sure access is disabled after it expires. Since this check considerably slows
            // downs the CTS tests, do it just once.
            SystemClock.sleep(NETWORK_TIMEOUT_MS);
            assertBackgroundNetworkAccess(false);
        }
    }

    // Must override so it only tests foreground service - once an app goes to foreground, device
    // leaves Doze Mode.
    @Override
    protected void assertsForegroundAlwaysHasNetworkAccess() throws Exception {
        launchComponentAndAssertNetworkAccess(TYPE_COMPONENT_FOREGROUND_SERVICE);
        stopForegroundService();
        assertBackgroundState();
    }
}
