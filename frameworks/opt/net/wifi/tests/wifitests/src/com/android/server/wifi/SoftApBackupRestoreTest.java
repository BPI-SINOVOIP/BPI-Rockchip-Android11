/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.server.wifi;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.*;
import static org.mockito.Mockito.*;

import android.content.Context;
import android.net.MacAddress;
import android.net.wifi.SoftApConfiguration;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiMigration;
import android.util.BackupUtils;

import androidx.test.filters.SmallTest;

import com.android.server.wifi.util.ApConfigUtil;
import com.android.server.wifi.util.SettingsMigrationDataHolder;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * Unit tests for {@link com.android.server.wifi.SoftApBackupRestore}.
 */
@SmallTest
public class SoftApBackupRestoreTest extends WifiBaseTest {

    @Mock private Context mContext;
    @Mock private SettingsMigrationDataHolder mSettingsMigrationDataHolder;
    @Mock private WifiMigration.SettingsMigrationData mOemMigrationData;
    private SoftApBackupRestore mSoftApBackupRestore;
    private final ArrayList<MacAddress> mTestBlockedList = new ArrayList<>();
    private final ArrayList<MacAddress> mTestAllowedList = new ArrayList<>();
    private static final int LAST_WIFICOFIGURATION_BACKUP_VERSION = 3;
    private static final boolean TEST_CLIENTCONTROLENABLE = false;
    private static final int TEST_MAXNUMBEROFCLIENTS = 10;
    private static final long TEST_SHUTDOWNTIMEOUTMILLIS = 600_000;
    private static final String TEST_BLOCKED_CLIENT = "11:22:33:44:55:66";
    private static final String TEST_ALLOWED_CLIENT = "aa:bb:cc:dd:ee:ff";
    private static final String TEST_SSID = "TestAP";
    private static final String TEST_PASSPHRASE = "TestPskPassphrase";
    private static final int TEST_SECURITY = SoftApConfiguration.SECURITY_TYPE_WPA3_SAE_TRANSITION;
    private static final int TEST_BAND = SoftApConfiguration.BAND_5GHZ;
    private static final int TEST_CHANNEL = 40;
    private static final boolean TEST_HIDDEN = false;

    /**
     * Asserts that the WifiConfigurations equal to SoftApConfiguration.
     * This only compares the elements saved
     * for softAp used.
     */
    public static void assertWifiConfigurationEqualSoftApConfiguration(
            WifiConfiguration backup, SoftApConfiguration restore) {
        assertEquals(backup.SSID, restore.getSsid());
        assertEquals(backup.BSSID, restore.getBssid());
        assertEquals(ApConfigUtil.convertWifiConfigBandToSoftApConfigBand(backup.apBand),
                restore.getBand());
        assertEquals(backup.apChannel, restore.getChannel());
        assertEquals(backup.preSharedKey, restore.getPassphrase());
        int authType = backup.getAuthType();
        if (backup.getAuthType() == WifiConfiguration.KeyMgmt.WPA2_PSK) {
            assertEquals(SoftApConfiguration.SECURITY_TYPE_WPA2_PSK, restore.getSecurityType());
        } else {
            assertEquals(SoftApConfiguration.SECURITY_TYPE_OPEN, restore.getSecurityType());
        }
        assertEquals(backup.hiddenSSID, restore.isHiddenSsid());
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        when(mSettingsMigrationDataHolder.retrieveData()).thenReturn(mOemMigrationData);
        when(mOemMigrationData.isSoftApTimeoutEnabled()).thenReturn(true);

        mSoftApBackupRestore = new SoftApBackupRestore(mContext, mSettingsMigrationDataHolder);
    }

    @After
    public void tearDown() throws Exception {
    }

    /**
     * Copy from WifiConfiguration for test backup/restore is backward compatible.
     */
    private byte[] getBytesForBackup(WifiConfiguration wificonfig) throws IOException {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        DataOutputStream out = new DataOutputStream(baos);

        out.writeInt(LAST_WIFICOFIGURATION_BACKUP_VERSION);
        BackupUtils.writeString(out, wificonfig.SSID);
        out.writeInt(wificonfig.apBand);
        out.writeInt(wificonfig.apChannel);
        BackupUtils.writeString(out, wificonfig.preSharedKey);
        out.writeInt(wificonfig.getAuthType());
        out.writeBoolean(wificonfig.hiddenSSID);
        return baos.toByteArray();
    }

    /**
     * Verifies that the serialization/de-serialization for wpa2 softap config works.
     */
    @Test
    public void testSoftApConfigBackupAndRestoreWithWpa2Config() throws Exception {
        SoftApConfiguration.Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setSsid("TestAP");
        configBuilder.setChannel(40, SoftApConfiguration.BAND_5GHZ);
        configBuilder.setPassphrase("TestPskPassphrase",
                SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
        configBuilder.setHiddenSsid(true);

        SoftApConfiguration config = configBuilder.build();

        byte[] data = mSoftApBackupRestore.retrieveBackupDataFromSoftApConfiguration(config);
        SoftApConfiguration restoredConfig =
                mSoftApBackupRestore.retrieveSoftApConfigurationFromBackupData(data);

        assertThat(config).isEqualTo(restoredConfig);
    }

    /**
     * Verifies that the serialization/de-serialization for open security softap config works.
     */
    @Test
    public void testSoftApConfigBackupAndRestoreWithOpenSecurityConfig() throws Exception {
        SoftApConfiguration.Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setSsid("TestAP");
        configBuilder.setChannel(12, SoftApConfiguration.BAND_2GHZ);
        configBuilder.setHiddenSsid(false);
        SoftApConfiguration config = configBuilder.build();

        byte[] data = mSoftApBackupRestore.retrieveBackupDataFromSoftApConfiguration(config);
        SoftApConfiguration restoredConfig =
                mSoftApBackupRestore.retrieveSoftApConfigurationFromBackupData(data);

        assertThat(config).isEqualTo(restoredConfig);
    }

    /**
     * Verifies that the serialization/de-serialization for old softap config works.
     */
    @Test
    public void testSoftApConfigBackupAndRestoreWithOldConfig() throws Exception {
        WifiConfiguration wifiConfig = new WifiConfiguration();
        wifiConfig.SSID = "TestAP";
        wifiConfig.apBand = WifiConfiguration.AP_BAND_2GHZ;
        wifiConfig.apChannel = 12;
        wifiConfig.hiddenSSID = true;
        wifiConfig.preSharedKey = "test_pwd";
        wifiConfig.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA2_PSK);
        byte[] data = getBytesForBackup(wifiConfig);
        SoftApConfiguration restoredConfig =
                mSoftApBackupRestore.retrieveSoftApConfigurationFromBackupData(data);

        assertWifiConfigurationEqualSoftApConfiguration(wifiConfig, restoredConfig);
    }

    /**
     * Verifies that the serialization/de-serialization for wpa3-sae softap config works.
     */
    @Test
    public void testSoftApConfigBackupAndRestoreWithWpa3SaeConfig() throws Exception {
        SoftApConfiguration.Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setSsid("TestAP");
        configBuilder.setBand(SoftApConfiguration.BAND_5GHZ);
        configBuilder.setChannel(40, SoftApConfiguration.BAND_5GHZ);
        configBuilder.setPassphrase("TestPskPassphrase",
                SoftApConfiguration.SECURITY_TYPE_WPA3_SAE);
        configBuilder.setHiddenSsid(true);
        configBuilder.setAutoShutdownEnabled(true);
        SoftApConfiguration config = configBuilder.build();

        byte[] data = mSoftApBackupRestore.retrieveBackupDataFromSoftApConfiguration(config);
        SoftApConfiguration restoredConfig =
                mSoftApBackupRestore.retrieveSoftApConfigurationFromBackupData(data);

        assertThat(config).isEqualTo(restoredConfig);
    }

    /**
     * Verifies that the serialization/de-serialization for wpa3-sae-transition softap config.
     */
    @Test
    public void testSoftApConfigBackupAndRestoreWithWpa3SaeTransitionConfig() throws Exception {
        SoftApConfiguration.Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setSsid("TestAP");
        configBuilder.setBand(SoftApConfiguration.BAND_5GHZ);
        configBuilder.setChannel(40, SoftApConfiguration.BAND_5GHZ);
        configBuilder.setPassphrase("TestPskPassphrase",
                SoftApConfiguration.SECURITY_TYPE_WPA3_SAE_TRANSITION);
        configBuilder.setHiddenSsid(true);
        configBuilder.setAutoShutdownEnabled(false);
        SoftApConfiguration config = configBuilder.build();

        byte[] data = mSoftApBackupRestore.retrieveBackupDataFromSoftApConfiguration(config);
        SoftApConfiguration restoredConfig =
                mSoftApBackupRestore.retrieveSoftApConfigurationFromBackupData(data);

        assertThat(config).isEqualTo(restoredConfig);
    }

    /**
     * Verifies that the serialization/de-serialization for wpa3-sae-transition softap config.
     */
    @Test
    public void testSoftApConfigBackupAndRestoreWithMaxShutDownClientList() throws Exception {
        mTestBlockedList.add(MacAddress.fromString(TEST_BLOCKED_CLIENT));
        mTestAllowedList.add(MacAddress.fromString(TEST_ALLOWED_CLIENT));
        SoftApConfiguration.Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setSsid(TEST_SSID);
        configBuilder.setBand(SoftApConfiguration.BAND_5GHZ);
        configBuilder.setChannel(40, SoftApConfiguration.BAND_5GHZ);
        configBuilder.setPassphrase(TEST_PASSPHRASE, TEST_SECURITY);
        configBuilder.setHiddenSsid(TEST_HIDDEN);
        configBuilder.setMaxNumberOfClients(TEST_MAXNUMBEROFCLIENTS);
        configBuilder.setShutdownTimeoutMillis(TEST_SHUTDOWNTIMEOUTMILLIS);
        configBuilder.setClientControlByUserEnabled(TEST_CLIENTCONTROLENABLE);
        configBuilder.setBlockedClientList(mTestBlockedList);
        configBuilder.setAllowedClientList(mTestAllowedList);
        SoftApConfiguration config = configBuilder.build();

        byte[] data = mSoftApBackupRestore.retrieveBackupDataFromSoftApConfiguration(config);
        SoftApConfiguration restoredConfig =
                mSoftApBackupRestore.retrieveSoftApConfigurationFromBackupData(data);

        assertThat(config).isEqualTo(restoredConfig);
    }

    /**
     * Verifies that the restore of version 5 backup data will read the auto shutdown enable/disable
     * tag from {@link WifiMigration#loadFromSettings(Context)}
     */
    @Test
    public void testSoftApConfigRestoreFromVersion5() throws Exception {
        mTestBlockedList.add(MacAddress.fromString(TEST_BLOCKED_CLIENT));
        mTestAllowedList.add(MacAddress.fromString(TEST_ALLOWED_CLIENT));
        SoftApConfiguration.Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setSsid(TEST_SSID);
        configBuilder.setBand(TEST_BAND);
        configBuilder.setChannel(TEST_CHANNEL, TEST_BAND);
        configBuilder.setPassphrase(TEST_PASSPHRASE, TEST_SECURITY);
        configBuilder.setHiddenSsid(TEST_HIDDEN);
        configBuilder.setMaxNumberOfClients(TEST_MAXNUMBEROFCLIENTS);
        configBuilder.setShutdownTimeoutMillis(TEST_SHUTDOWNTIMEOUTMILLIS);
        configBuilder.setClientControlByUserEnabled(TEST_CLIENTCONTROLENABLE);
        configBuilder.setBlockedClientList(mTestBlockedList);
        configBuilder.setAllowedClientList(mTestAllowedList);

        // Toggle on when migrating.
        when(mOemMigrationData.isSoftApTimeoutEnabled()).thenReturn(true);
        SoftApConfiguration expectedConfig = configBuilder.setAutoShutdownEnabled(true).build();
        SoftApConfiguration restoredConfig =
                mSoftApBackupRestore.retrieveSoftApConfigurationFromBackupData(
                        retrieveVersion5BackupDataFromSoftApConfiguration(expectedConfig));
        assertEquals(expectedConfig, restoredConfig);

        // Toggle off when migrating.
        when(mOemMigrationData.isSoftApTimeoutEnabled()).thenReturn(false);
        expectedConfig = configBuilder.setAutoShutdownEnabled(false).build();
        restoredConfig = mSoftApBackupRestore.retrieveSoftApConfigurationFromBackupData(
                retrieveVersion5BackupDataFromSoftApConfiguration(expectedConfig));
        assertEquals(expectedConfig, restoredConfig);
    }

    /**
     * This is a copy of the old serialization code (version 5)
     *
     * Changes: Version = 5, AutoShutdown tag is missing.
     */
    private byte[] retrieveVersion5BackupDataFromSoftApConfiguration(SoftApConfiguration config)
            throws Exception {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        DataOutputStream out = new DataOutputStream(baos);

        out.writeInt(5);
        BackupUtils.writeString(out, config.getSsid());
        out.writeInt(config.getBand());
        out.writeInt(config.getChannel());
        BackupUtils.writeString(out, config.getPassphrase());
        out.writeInt(config.getSecurityType());
        out.writeBoolean(config.isHiddenSsid());
        out.writeInt(config.getMaxNumberOfClients());
        out.writeInt((int) config.getShutdownTimeoutMillis());
        out.writeBoolean(config.isClientControlByUserEnabled());
        writeMacAddressList(out, config.getBlockedClientList());
        writeMacAddressList(out, config.getAllowedClientList());
        return baos.toByteArray();
    }


    /**
     * Verifies that the restore of version 6 backup data will read the auto shutdown with int.
     */
    @Test
    public void testSoftApConfigRestoreFromVersion6() throws Exception {
        mTestBlockedList.add(MacAddress.fromString(TEST_BLOCKED_CLIENT));
        mTestAllowedList.add(MacAddress.fromString(TEST_ALLOWED_CLIENT));
        SoftApConfiguration.Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setSsid(TEST_SSID);
        configBuilder.setBand(TEST_BAND);
        configBuilder.setChannel(TEST_CHANNEL, TEST_BAND);
        configBuilder.setPassphrase(TEST_PASSPHRASE, TEST_SECURITY);
        configBuilder.setHiddenSsid(TEST_HIDDEN);
        configBuilder.setMaxNumberOfClients(TEST_MAXNUMBEROFCLIENTS);
        configBuilder.setShutdownTimeoutMillis(TEST_SHUTDOWNTIMEOUTMILLIS);
        configBuilder.setClientControlByUserEnabled(TEST_CLIENTCONTROLENABLE);
        configBuilder.setBlockedClientList(mTestBlockedList);
        configBuilder.setAllowedClientList(mTestAllowedList);

        SoftApConfiguration expectedConfig = configBuilder.build();
        SoftApConfiguration restoredConfig = mSoftApBackupRestore
                .retrieveSoftApConfigurationFromBackupData(
                retrieveVersion6BackupDataFromSoftApConfiguration(expectedConfig));
        assertEquals(expectedConfig, restoredConfig);
    }

    /**
     * This is a copy of the old serialization code (version 6)
     *
     * Changes: Version = 6, AutoShutdown use int type
     */
    private byte[] retrieveVersion6BackupDataFromSoftApConfiguration(SoftApConfiguration config)
            throws Exception {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        DataOutputStream out = new DataOutputStream(baos);

        out.writeInt(6);
        BackupUtils.writeString(out, config.getSsid());
        out.writeInt(config.getBand());
        out.writeInt(config.getChannel());
        BackupUtils.writeString(out, config.getPassphrase());
        out.writeInt(config.getSecurityType());
        out.writeBoolean(config.isHiddenSsid());
        out.writeInt(config.getMaxNumberOfClients());
        out.writeInt((int) config.getShutdownTimeoutMillis());
        out.writeBoolean(config.isClientControlByUserEnabled());
        writeMacAddressList(out, config.getBlockedClientList());
        writeMacAddressList(out, config.getAllowedClientList());
        out.writeBoolean(config.isAutoShutdownEnabled());
        return baos.toByteArray();
    }


    private void writeMacAddressList(DataOutputStream out, List<MacAddress> macList)
            throws IOException {
        out.writeInt(macList.size());
        Iterator<MacAddress> iterator = macList.iterator();
        while (iterator.hasNext()) {
            byte[] mac = iterator.next().toByteArray();
            out.write(mac, 0,  6);
        }
    }
}
