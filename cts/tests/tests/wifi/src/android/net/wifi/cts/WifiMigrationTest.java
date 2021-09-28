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

package android.net.wifi.cts;

import android.app.ActivityManager;
import android.net.wifi.WifiMigration;
import android.os.UserHandle;
import android.os.UserManager;
import android.test.AndroidTestCase;

public class WifiMigrationTest extends WifiJUnit3TestBase {
    private static final String TEST_SSID_UNQUOTED = "testSsid1";

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
    }

    @Override
    protected void tearDown() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            super.tearDown();
            return;
        }
        super.tearDown();
    }

    /**
     * Tests {@link android.net.wifi.WifiMigration.SettingsMigrationData.Builder} class.
     */
    public void testWifiMigrationSettingsDataBuilder() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        WifiMigration.SettingsMigrationData migrationData =
                new WifiMigration.SettingsMigrationData.Builder()
                        .setScanAlwaysAvailable(true)
                        .setP2pFactoryResetPending(true)
                        .setScanThrottleEnabled(true)
                        .setSoftApTimeoutEnabled(true)
                        .setWakeUpEnabled(true)
                        .setVerboseLoggingEnabled(true)
                        .setP2pDeviceName(TEST_SSID_UNQUOTED)
                        .build();

        assertNotNull(migrationData);
        assertTrue(migrationData.isScanAlwaysAvailable());
        assertTrue(migrationData.isP2pFactoryResetPending());
        assertTrue(migrationData.isScanThrottleEnabled());
        assertTrue(migrationData.isSoftApTimeoutEnabled());
        assertTrue(migrationData.isWakeUpEnabled());
        assertTrue(migrationData.isVerboseLoggingEnabled());
        assertEquals(TEST_SSID_UNQUOTED, migrationData.getP2pDeviceName());
    }

    /**
     * Tests {@link android.net.wifi.WifiMigration.SettingsMigrationData} class.
     */
    public void testWifiMigrationSettings() throws Exception {
        try {
            WifiMigration.loadFromSettings(getContext());
        } catch (Exception ignore) {
        }
    }

    /**
     * Tests {@link WifiMigration#convertAndRetrieveSharedConfigStoreFile(int)},
     * {@link WifiMigration#convertAndRetrieveUserConfigStoreFile(int, UserHandle)},
     * {@link WifiMigration#removeSharedConfigStoreFile(int)} and
     * {@link WifiMigration#removeUserConfigStoreFile(int, UserHandle)}.
     */
    public void testWifiMigrationConfigStore() throws Exception {
        try {
            WifiMigration.convertAndRetrieveSharedConfigStoreFile(
                    WifiMigration.STORE_FILE_SHARED_GENERAL);
        } catch (Exception ignore) {
        }
        try {
            WifiMigration.convertAndRetrieveSharedConfigStoreFile(
                    WifiMigration.STORE_FILE_SHARED_SOFTAP);
        } catch (Exception ignore) {
        }
        try {
            WifiMigration.convertAndRetrieveUserConfigStoreFile(
                    WifiMigration.STORE_FILE_USER_GENERAL,
                    UserHandle.of(ActivityManager.getCurrentUser()));
        } catch (Exception ignore) {
        }
        try {
            WifiMigration.convertAndRetrieveUserConfigStoreFile(
                    WifiMigration.STORE_FILE_USER_NETWORK_SUGGESTIONS,
                    UserHandle.of(ActivityManager.getCurrentUser()));
        } catch (Exception ignore) {
        }
        try {
            WifiMigration.removeSharedConfigStoreFile(
                    WifiMigration.STORE_FILE_SHARED_GENERAL);
        } catch (Exception ignore) {
        }
        try {
            WifiMigration.removeSharedConfigStoreFile(
                    WifiMigration.STORE_FILE_SHARED_SOFTAP);
        } catch (Exception ignore) {
        }
        try {
            WifiMigration.removeUserConfigStoreFile(
                    WifiMigration.STORE_FILE_USER_GENERAL,
                    UserHandle.of(ActivityManager.getCurrentUser()));
        } catch (Exception ignore) {
        }
        try {
            WifiMigration.removeUserConfigStoreFile(
                    WifiMigration.STORE_FILE_USER_NETWORK_SUGGESTIONS,
                    UserHandle.of(ActivityManager.getCurrentUser()));
        } catch (Exception ignore) {
        }
    }
}
