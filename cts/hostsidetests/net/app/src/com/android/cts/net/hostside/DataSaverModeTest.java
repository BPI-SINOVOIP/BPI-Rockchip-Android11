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

import static android.net.ConnectivityManager.RESTRICT_BACKGROUND_STATUS_DISABLED;
import static android.net.ConnectivityManager.RESTRICT_BACKGROUND_STATUS_ENABLED;
import static android.net.ConnectivityManager.RESTRICT_BACKGROUND_STATUS_WHITELISTED;

import static com.android.cts.net.hostside.NetworkPolicyTestUtils.setRestrictBackground;
import static com.android.cts.net.hostside.Property.DATA_SAVER_MODE;
import static com.android.cts.net.hostside.Property.METERED_NETWORK;
import static com.android.cts.net.hostside.Property.NO_DATA_SAVER_MODE;

import static org.junit.Assert.fail;

import com.android.compatibility.common.util.CddTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import androidx.test.filters.LargeTest;

@RequiredProperties({DATA_SAVER_MODE, METERED_NETWORK})
@LargeTest
public class DataSaverModeTest extends AbstractRestrictBackgroundNetworkTestCase {

    private static final String[] REQUIRED_WHITELISTED_PACKAGES = {
        "com.android.providers.downloads"
    };

    @Before
    public void setUp() throws Exception {
        super.setUp();

        // Set initial state.
        setRestrictBackground(false);
        removeRestrictBackgroundWhitelist(mUid);
        removeRestrictBackgroundBlacklist(mUid);

        registerBroadcastReceiver();
        assertRestrictBackgroundChangedReceived(0);
   }

    @After
    public void tearDown() throws Exception {
        super.tearDown();

        setRestrictBackground(false);
    }

    @Test
    public void testGetRestrictBackgroundStatus_disabled() throws Exception {
        assertDataSaverStatusOnBackground(RESTRICT_BACKGROUND_STATUS_DISABLED);

        // Sanity check: make sure status is always disabled, never whitelisted
        addRestrictBackgroundWhitelist(mUid);
        assertRestrictBackgroundChangedReceived(0);
        assertDataSaverStatusOnBackground(RESTRICT_BACKGROUND_STATUS_DISABLED);

        assertsForegroundAlwaysHasNetworkAccess();
        assertDataSaverStatusOnBackground(RESTRICT_BACKGROUND_STATUS_DISABLED);
    }

    @Test
    public void testGetRestrictBackgroundStatus_whitelisted() throws Exception {
        setRestrictBackground(true);
        assertRestrictBackgroundChangedReceived(1);
        assertDataSaverStatusOnBackground(RESTRICT_BACKGROUND_STATUS_ENABLED);

        addRestrictBackgroundWhitelist(mUid);
        assertRestrictBackgroundChangedReceived(2);
        assertDataSaverStatusOnBackground(RESTRICT_BACKGROUND_STATUS_WHITELISTED);

        removeRestrictBackgroundWhitelist(mUid);
        assertRestrictBackgroundChangedReceived(3);
        assertDataSaverStatusOnBackground(RESTRICT_BACKGROUND_STATUS_ENABLED);

        assertsForegroundAlwaysHasNetworkAccess();
        assertDataSaverStatusOnBackground(RESTRICT_BACKGROUND_STATUS_ENABLED);
    }

    @Test
    public void testGetRestrictBackgroundStatus_enabled() throws Exception {
        setRestrictBackground(true);
        assertRestrictBackgroundChangedReceived(1);
        assertDataSaverStatusOnBackground(RESTRICT_BACKGROUND_STATUS_ENABLED);

        assertsForegroundAlwaysHasNetworkAccess();
        assertDataSaverStatusOnBackground(RESTRICT_BACKGROUND_STATUS_ENABLED);

        // Make sure foreground app doesn't lose access upon enabling Data Saver.
        setRestrictBackground(false);
        launchComponentAndAssertNetworkAccess(TYPE_COMPONENT_ACTIVTIY);
        setRestrictBackground(true);
        assertForegroundNetworkAccess();

        // Although it should not have access while the screen is off.
        turnScreenOff();
        assertBackgroundNetworkAccess(false);
        turnScreenOn();
        assertForegroundNetworkAccess();

        // Goes back to background state.
        finishActivity();
        assertBackgroundNetworkAccess(false);

        // Make sure foreground service doesn't lose access upon enabling Data Saver.
        setRestrictBackground(false);
        launchComponentAndAssertNetworkAccess(TYPE_COMPONENT_FOREGROUND_SERVICE);
        setRestrictBackground(true);
        assertForegroundNetworkAccess();
        stopForegroundService();
        assertBackgroundNetworkAccess(false);
    }

    @Test
    public void testGetRestrictBackgroundStatus_blacklisted() throws Exception {
        addRestrictBackgroundBlacklist(mUid);
        assertRestrictBackgroundChangedReceived(1);
        assertDataSaverStatusOnBackground(RESTRICT_BACKGROUND_STATUS_ENABLED);

        assertsForegroundAlwaysHasNetworkAccess();
        assertRestrictBackgroundChangedReceived(1);
        assertDataSaverStatusOnBackground(RESTRICT_BACKGROUND_STATUS_ENABLED);

        // UID policies live by the Highlander rule: "There can be only one".
        // Hence, if app is whitelisted, it should not be blacklisted anymore.
        setRestrictBackground(true);
        assertRestrictBackgroundChangedReceived(2);
        assertDataSaverStatusOnBackground(RESTRICT_BACKGROUND_STATUS_ENABLED);
        addRestrictBackgroundWhitelist(mUid);
        assertRestrictBackgroundChangedReceived(3);
        assertDataSaverStatusOnBackground(RESTRICT_BACKGROUND_STATUS_WHITELISTED);

        // Check status after removing blacklist.
        // ...re-enables first
        addRestrictBackgroundBlacklist(mUid);
        assertRestrictBackgroundChangedReceived(4);
        assertDataSaverStatusOnBackground(RESTRICT_BACKGROUND_STATUS_ENABLED);
        assertsForegroundAlwaysHasNetworkAccess();
        // ... remove blacklist - access's still rejected because Data Saver is on
        removeRestrictBackgroundBlacklist(mUid);
        assertRestrictBackgroundChangedReceived(4);
        assertDataSaverStatusOnBackground(RESTRICT_BACKGROUND_STATUS_ENABLED);
        assertsForegroundAlwaysHasNetworkAccess();
        // ... finally, disable Data Saver
        setRestrictBackground(false);
        assertRestrictBackgroundChangedReceived(5);
        assertDataSaverStatusOnBackground(RESTRICT_BACKGROUND_STATUS_DISABLED);
        assertsForegroundAlwaysHasNetworkAccess();
    }

    @Test
    public void testGetRestrictBackgroundStatus_requiredWhitelistedPackages() throws Exception {
        final StringBuilder error = new StringBuilder();
        for (String packageName : REQUIRED_WHITELISTED_PACKAGES) {
            int uid = -1;
            try {
                uid = getUid(packageName);
                assertRestrictBackgroundWhitelist(uid, true);
            } catch (Throwable t) {
                error.append("\nFailed for '").append(packageName).append("'");
                if (uid > 0) {
                    error.append(" (uid ").append(uid).append(")");
                }
                error.append(": ").append(t).append("\n");
            }
        }
        if (error.length() > 0) {
            fail(error.toString());
        }
    }

    @RequiredProperties({NO_DATA_SAVER_MODE})
    @CddTest(requirement="7.4.7/C-2-2")
    @Test
    public void testBroadcastNotSentOnUnsupportedDevices() throws Exception {
        setRestrictBackground(true);
        assertRestrictBackgroundChangedReceived(0);

        setRestrictBackground(false);
        assertRestrictBackgroundChangedReceived(0);

        setRestrictBackground(true);
        assertRestrictBackgroundChangedReceived(0);
    }

    private void assertDataSaverStatusOnBackground(int expectedStatus) throws Exception {
        assertRestrictBackgroundStatus(expectedStatus);
        assertBackgroundNetworkAccess(expectedStatus != RESTRICT_BACKGROUND_STATUS_ENABLED);
    }
}
