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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.net.MacAddress;
import android.net.wifi.SoftApConfiguration;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiMigration;
import android.util.Xml;

import androidx.test.filters.SmallTest;

import com.android.internal.util.FastXmlSerializer;
import com.android.server.wifi.util.SettingsMigrationDataHolder;
import com.android.server.wifi.util.WifiConfigStoreEncryptionUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlSerializer;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;

/**
 * Unit tests for {@link com.android.server.wifi.SoftApStoreData}.
 */
@SmallTest
public class SoftApStoreDataTest extends WifiBaseTest {
    private static final String TEST_SSID = "SSID";
    private static final String TEST_BSSID = "11:22:33:aa:bb:cc";
    private static final String TEST_PASSPHRASE = "TestPassphrase";
    private static final String TEST_WPA2_PASSPHRASE = "Wpa2Test";
    private static final int TEST_CHANNEL = 0;
    private static final boolean TEST_HIDDEN = false;
    private static final int TEST_BAND = SoftApConfiguration.BAND_2GHZ
            | SoftApConfiguration.BAND_5GHZ;
    private static final int TEST_OLD_BAND = WifiConfiguration.AP_BAND_ANY;
    private static final int TEST_SECURITY = SoftApConfiguration.SECURITY_TYPE_WPA2_PSK;
    private static final boolean TEST_CLIENT_CONTROL_BY_USER = false;
    private static final boolean TEST_AUTO_SHUTDOWN_ENABLED = true;
    private static final int TEST_MAX_NUMBER_OF_CLIENTS = 10;
    private static final long TEST_SHUTDOWN_TIMEOUT_MILLIS = 600_000;
    private static final ArrayList<MacAddress> TEST_BLOCKEDLIST = new ArrayList<>();
    private static final String TEST_BLOCKED_CLIENT = "11:22:33:44:55:66";
    private static final ArrayList<MacAddress> TEST_ALLOWEDLIST = new ArrayList<>();
    private static final String TEST_ALLOWED_CLIENT = "aa:bb:cc:dd:ee:ff";

    private static final String TEST_SOFTAP_CONFIG_XML_STRING =
            "<string name=\"SSID\">" + TEST_SSID + "</string>\n"
                    + "<int name=\"Band\" value=\"" + TEST_OLD_BAND + "\" />\n"
                    + "<int name=\"Channel\" value=\"" + TEST_CHANNEL + "\" />\n"
                    + "<boolean name=\"HiddenSSID\" value=\"" + TEST_HIDDEN + "\" />\n"
                    + "<int name=\"SecurityType\" value=\"" + TEST_SECURITY + "\" />\n"
                    + "<string name=\"Wpa2Passphrase\">" + TEST_WPA2_PASSPHRASE + "</string>\n";

    private static final String TEST_SOFTAP_CONFIG_XML_STRING_WITH_NEW_BAND_DESIGN =
            "<string name=\"SSID\">" + TEST_SSID + "</string>\n"
                    + "<int name=\"ApBand\" value=\"" + TEST_BAND + "\" />\n"
                    + "<int name=\"Channel\" value=\"" + TEST_CHANNEL + "\" />\n"
                    + "<boolean name=\"HiddenSSID\" value=\"" + TEST_HIDDEN + "\" />\n"
                    + "<int name=\"SecurityType\" value=\"" + TEST_SECURITY + "\" />\n"
                    + "<string name=\"Passphrase\">" + TEST_PASSPHRASE + "</string>\n";

    private static final String TEST_SOFTAP_CONFIG_XML_STRING_WITH_ALL_CONFIG =
            "<string name=\"SSID\">" + TEST_SSID + "</string>\n"
                    + "<string name=\"Bssid\">" + TEST_BSSID + "</string>\n"
                    + "<int name=\"ApBand\" value=\"" + TEST_BAND + "\" />\n"
                    + "<int name=\"Channel\" value=\"" + TEST_CHANNEL + "\" />\n"
                    + "<boolean name=\"HiddenSSID\" value=\"" + TEST_HIDDEN + "\" />\n"
                    + "<int name=\"SecurityType\" value=\"" + TEST_SECURITY + "\" />\n"
                    + "<string name=\"Passphrase\">" + TEST_PASSPHRASE + "</string>\n"
                    + "<int name=\"MaxNumberOfClients\" value=\""
                    + TEST_MAX_NUMBER_OF_CLIENTS + "\" />\n"
                    + "<boolean name=\"ClientControlByUser\" value=\""
                    + TEST_CLIENT_CONTROL_BY_USER + "\" />\n"
                    + "<boolean name=\"AutoShutdownEnabled\" value=\""
                    + TEST_AUTO_SHUTDOWN_ENABLED + "\" />\n"
                    + "<long name=\"ShutdownTimeoutMillis\" value=\""
                    + TEST_SHUTDOWN_TIMEOUT_MILLIS + "\" />\n"
                    + "<BlockedClientList>\n"
                    + "<string name=\"ClientMacAddress\">" + TEST_BLOCKED_CLIENT + "</string>\n"
                    + "</BlockedClientList>\n"
                    + "<AllowedClientList>\n"
                    + "<string name=\"ClientMacAddress\">" + TEST_ALLOWED_CLIENT + "</string>\n"
                    + "</AllowedClientList>\n";

    private static final String TEST_SOFTAP_CONFIG_XML_STRING_WITH_ALL_CONFIG_EXCEPT_AUTO_SHUTDOWN =
            "<string name=\"SSID\">" + TEST_SSID + "</string>\n"
                    + "<int name=\"ApBand\" value=\"" + TEST_BAND + "\" />\n"
                    + "<int name=\"Channel\" value=\"" + TEST_CHANNEL + "\" />\n"
                    + "<boolean name=\"HiddenSSID\" value=\"" + TEST_HIDDEN + "\" />\n"
                    + "<int name=\"SecurityType\" value=\"" + TEST_SECURITY + "\" />\n"
                    + "<string name=\"Passphrase\">" + TEST_PASSPHRASE + "</string>\n"
                    + "<int name=\"MaxNumberOfClients\" value=\""
                    + TEST_MAX_NUMBER_OF_CLIENTS + "\" />\n"
                    + "<boolean name=\"ClientControlByUser\" value=\""
                    + TEST_CLIENT_CONTROL_BY_USER + "\" />\n"
                    + "<long name=\"ShutdownTimeoutMillis\" value=\""
                    + TEST_SHUTDOWN_TIMEOUT_MILLIS + "\" />\n"
                    + "<BlockedClientList>\n"
                    + "<string name=\"ClientMacAddress\">" + TEST_BLOCKED_CLIENT + "</string>\n"
                    + "</BlockedClientList>\n"
                    + "<AllowedClientList>\n"
                    + "<string name=\"ClientMacAddress\">" + TEST_ALLOWED_CLIENT + "</string>\n"
                    + "</AllowedClientList>\n";

    private static final String TEST_SOFTAP_CONFIG_XML_STRING_WITH_INT_TYPE_SHUTDOWNTIMOUTMILLIS =
            "<string name=\"SSID\">" + TEST_SSID + "</string>\n"
                    + "<int name=\"ApBand\" value=\"" + TEST_BAND + "\" />\n"
                    + "<int name=\"Channel\" value=\"" + TEST_CHANNEL + "\" />\n"
                    + "<boolean name=\"HiddenSSID\" value=\"" + TEST_HIDDEN + "\" />\n"
                    + "<int name=\"SecurityType\" value=\"" + TEST_SECURITY + "\" />\n"
                    + "<string name=\"Passphrase\">" + TEST_PASSPHRASE + "</string>\n"
                    + "<int name=\"MaxNumberOfClients\" value=\""
                    + TEST_MAX_NUMBER_OF_CLIENTS + "\" />\n"
                    + "<boolean name=\"ClientControlByUser\" value=\""
                    + TEST_CLIENT_CONTROL_BY_USER + "\" />\n"
                    + "<boolean name=\"AutoShutdownEnabled\" value=\""
                    + TEST_AUTO_SHUTDOWN_ENABLED + "\" />\n"
                    + "<int name=\"ShutdownTimeoutMillis\" value=\""
                    + TEST_SHUTDOWN_TIMEOUT_MILLIS + "\" />\n"
                    + "<BlockedClientList>\n"
                    + "<string name=\"ClientMacAddress\">" + TEST_BLOCKED_CLIENT + "</string>\n"
                    + "</BlockedClientList>\n"
                    + "<AllowedClientList>\n"
                    + "<string name=\"ClientMacAddress\">" + TEST_ALLOWED_CLIENT + "</string>\n"
                    + "</AllowedClientList>\n";

    private static final String TEST_SOFTAP_CONFIG_XML_STRING_WITH_ALL_CONFIG_EXCEPT_BSSID =
            "<string name=\"SSID\">" + TEST_SSID + "</string>\n"
                    + "<int name=\"ApBand\" value=\"" + TEST_BAND + "\" />\n"
                    + "<int name=\"Channel\" value=\"" + TEST_CHANNEL + "\" />\n"
                    + "<boolean name=\"HiddenSSID\" value=\"" + TEST_HIDDEN + "\" />\n"
                    + "<int name=\"SecurityType\" value=\"" + TEST_SECURITY + "\" />\n"
                    + "<string name=\"Passphrase\">" + TEST_PASSPHRASE + "</string>\n"
                    + "<int name=\"MaxNumberOfClients\" value=\""
                    + TEST_MAX_NUMBER_OF_CLIENTS + "\" />\n"
                    + "<boolean name=\"ClientControlByUser\" value=\""
                    + TEST_CLIENT_CONTROL_BY_USER + "\" />\n"
                    + "<boolean name=\"AutoShutdownEnabled\" value=\""
                    + TEST_AUTO_SHUTDOWN_ENABLED + "\" />\n"
                    + "<int name=\"ShutdownTimeoutMillis\" value=\""
                    + TEST_SHUTDOWN_TIMEOUT_MILLIS + "\" />\n"
                    + "<BlockedClientList>\n"
                    + "<string name=\"ClientMacAddress\">" + TEST_BLOCKED_CLIENT + "</string>\n"
                    + "</BlockedClientList>\n"
                    + "<AllowedClientList>\n"
                    + "<string name=\"ClientMacAddress\">" + TEST_ALLOWED_CLIENT + "</string>\n"
                    + "</AllowedClientList>\n";

    @Mock private Context mContext;
    @Mock SoftApStoreData.DataSource mDataSource;
    @Mock private WifiMigration.SettingsMigrationData mOemMigrationData;
    @Mock private SettingsMigrationDataHolder mSettingsMigrationDataHolder;
    SoftApStoreData mSoftApStoreData;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        when(mSettingsMigrationDataHolder.retrieveData())
                .thenReturn(mOemMigrationData);
        when(mOemMigrationData.isSoftApTimeoutEnabled()).thenReturn(true);

        mSoftApStoreData = new SoftApStoreData(mContext, mSettingsMigrationDataHolder, mDataSource);
        TEST_BLOCKEDLIST.add(MacAddress.fromString(TEST_BLOCKED_CLIENT));
        TEST_ALLOWEDLIST.add(MacAddress.fromString(TEST_ALLOWED_CLIENT));
    }

    /**
     * Called after each test
     */
    @After
    public void cleanup() {
        TEST_BLOCKEDLIST.clear();
        TEST_ALLOWEDLIST.clear();
    }

    /**
     * Helper function for serializing configuration data to a XML block.
     *
     * @return byte[] of the XML data
     * @throws Exception
     */
    private byte[] serializeData() throws Exception {
        final XmlSerializer out = new FastXmlSerializer();
        final ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
        out.setOutput(outputStream, StandardCharsets.UTF_8.name());
        mSoftApStoreData.serializeData(out, mock(WifiConfigStoreEncryptionUtil.class));
        out.flush();
        return outputStream.toByteArray();
    }

    /**
     * Helper function for parsing configuration data from a XML block.
     *
     * @param data XML data to parse from
     * @throws Exception
     */
    private void deserializeData(byte[] data) throws Exception {
        final XmlPullParser in = Xml.newPullParser();
        final ByteArrayInputStream inputStream = new ByteArrayInputStream(data);
        in.setInput(inputStream, StandardCharsets.UTF_8.name());
        mSoftApStoreData.deserializeData(in, in.getDepth(),
                WifiConfigStore.ENCRYPT_CREDENTIALS_CONFIG_STORE_DATA_VERSION,
                mock(WifiConfigStoreEncryptionUtil.class));
    }

    /**
     * Verify that parsing an empty data doesn't cause any crash and no configuration should
     * be deserialized.
     *
     * @throws Exception
     */
    @Test
    public void deserializeEmptyStoreData() throws Exception {
        deserializeData(new byte[0]);
        verify(mDataSource, never()).fromDeserialized(any());
    }

    /**
     * Verify that SoftApStoreData is written to
     * {@link WifiConfigStore#STORE_FILE_SHARED_SOFTAP}.
     *
     * @throws Exception
     */
    @Test
    public void getUserStoreFileId() throws Exception {
        assertEquals(WifiConfigStore.STORE_FILE_SHARED_SOFTAP,
                mSoftApStoreData.getStoreFileId());
    }

    /**
     * Verify that the store data is serialized correctly, matches the predefined test XML data.
     *
     * @throws Exception
     */
    @Test
    public void serializeSoftAp() throws Exception {
        SoftApConfiguration.Builder softApConfigBuilder = new SoftApConfiguration.Builder();
        softApConfigBuilder.setSsid(TEST_SSID);
        softApConfigBuilder.setBssid(MacAddress.fromString(TEST_BSSID));
        softApConfigBuilder.setPassphrase(TEST_PASSPHRASE,
                SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
        softApConfigBuilder.setBand(TEST_BAND);
        softApConfigBuilder.setClientControlByUserEnabled(TEST_CLIENT_CONTROL_BY_USER);
        softApConfigBuilder.setMaxNumberOfClients(TEST_MAX_NUMBER_OF_CLIENTS);
        softApConfigBuilder.setAutoShutdownEnabled(true);
        softApConfigBuilder.setShutdownTimeoutMillis(TEST_SHUTDOWN_TIMEOUT_MILLIS);
        softApConfigBuilder.setAllowedClientList(TEST_ALLOWEDLIST);
        softApConfigBuilder.setBlockedClientList(TEST_BLOCKEDLIST);

        when(mDataSource.toSerialize()).thenReturn(softApConfigBuilder.build());
        byte[] actualData = serializeData();
        assertEquals(TEST_SOFTAP_CONFIG_XML_STRING_WITH_ALL_CONFIG, new String(actualData));
    }

    /**
     * Verify that the store data is deserialized correctly using the predefined test XML data.
     *
     * @throws Exception
     */
    @Test
    public void deserializeSoftAp() throws Exception {
        deserializeData(TEST_SOFTAP_CONFIG_XML_STRING_WITH_ALL_CONFIG.getBytes());

        ArgumentCaptor<SoftApConfiguration> softapConfigCaptor =
                ArgumentCaptor.forClass(SoftApConfiguration.class);
        verify(mDataSource).fromDeserialized(softapConfigCaptor.capture());
        SoftApConfiguration softApConfig = softapConfigCaptor.getValue();
        assertNotNull(softApConfig);
        assertEquals(softApConfig.getSsid(), TEST_SSID);
        assertEquals(softApConfig.getBssid().toString(), TEST_BSSID);
        assertEquals(softApConfig.getPassphrase(), TEST_PASSPHRASE);
        assertEquals(softApConfig.getSecurityType(), SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
        assertEquals(softApConfig.isHiddenSsid(), TEST_HIDDEN);
        assertEquals(softApConfig.getBand(), TEST_BAND);
        assertEquals(softApConfig.isClientControlByUserEnabled(), TEST_CLIENT_CONTROL_BY_USER);
        assertEquals(softApConfig.getMaxNumberOfClients(), TEST_MAX_NUMBER_OF_CLIENTS);
        assertTrue(softApConfig.isAutoShutdownEnabled());
        assertEquals(softApConfig.getShutdownTimeoutMillis(), TEST_SHUTDOWN_TIMEOUT_MILLIS);
        assertEquals(softApConfig.getBlockedClientList(), TEST_BLOCKEDLIST);
        assertEquals(softApConfig.getAllowedClientList(), TEST_ALLOWEDLIST);
    }

    /**
     * Verify that the store data is deserialized correctly using the predefined test XML data.
     *
     * @throws Exception
     */
    @Test
    public void deserializeOldSoftApXMLWhichShutdownTimeoutIsInt() throws Exception {
        deserializeData(TEST_SOFTAP_CONFIG_XML_STRING_WITH_INT_TYPE_SHUTDOWNTIMOUTMILLIS
                .getBytes());

        ArgumentCaptor<SoftApConfiguration> softapConfigCaptor =
                ArgumentCaptor.forClass(SoftApConfiguration.class);
        verify(mDataSource).fromDeserialized(softapConfigCaptor.capture());
        SoftApConfiguration softApConfig = softapConfigCaptor.getValue();
        assertNotNull(softApConfig);
        assertEquals(softApConfig.getSsid(), TEST_SSID);
        assertEquals(softApConfig.getPassphrase(), TEST_PASSPHRASE);
        assertEquals(softApConfig.getSecurityType(), SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
        assertEquals(softApConfig.isHiddenSsid(), TEST_HIDDEN);
        assertEquals(softApConfig.getBand(), TEST_BAND);
        assertEquals(softApConfig.isClientControlByUserEnabled(), TEST_CLIENT_CONTROL_BY_USER);
        assertEquals(softApConfig.getMaxNumberOfClients(), TEST_MAX_NUMBER_OF_CLIENTS);
        assertTrue(softApConfig.isAutoShutdownEnabled());
        assertEquals(softApConfig.getShutdownTimeoutMillis(), TEST_SHUTDOWN_TIMEOUT_MILLIS);
        assertEquals(softApConfig.getBlockedClientList(), TEST_BLOCKEDLIST);
        assertEquals(softApConfig.getAllowedClientList(), TEST_ALLOWEDLIST);
    }

    /**
     * Verify that the old format is deserialized correctly.
     *
     * @throws Exception
     */
    @Test
    public void deserializeOldBandSoftAp() throws Exception {
        // Start with the old serialized data
        deserializeData(TEST_SOFTAP_CONFIG_XML_STRING.getBytes());

        ArgumentCaptor<SoftApConfiguration> softapConfigCaptor =
                ArgumentCaptor.forClass(SoftApConfiguration.class);
        verify(mDataSource).fromDeserialized(softapConfigCaptor.capture());
        SoftApConfiguration softApConfig = softapConfigCaptor.getValue();
        assertNotNull(softApConfig);
        assertEquals(softApConfig.getSsid(), TEST_SSID);
        assertEquals(softApConfig.getPassphrase(), TEST_WPA2_PASSPHRASE);
        assertEquals(softApConfig.getSecurityType(), SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
        assertEquals(softApConfig.isHiddenSsid(), TEST_HIDDEN);
        assertEquals(softApConfig.getBand(), TEST_BAND);
    }

    /**
     * Verify that the old format is deserialized correctly.
     *
     * @throws Exception
     */
    @Test
    public void deserializeNewBandSoftApButNoNewConfig() throws Exception {
        // Start with the old serialized data
        deserializeData(TEST_SOFTAP_CONFIG_XML_STRING_WITH_NEW_BAND_DESIGN.getBytes());

        ArgumentCaptor<SoftApConfiguration> softapConfigCaptor =
                ArgumentCaptor.forClass(SoftApConfiguration.class);
        verify(mDataSource).fromDeserialized(softapConfigCaptor.capture());
        SoftApConfiguration softApConfig = softapConfigCaptor.getValue();
        assertNotNull(softApConfig);
        assertEquals(softApConfig.getSsid(), TEST_SSID);
        assertEquals(softApConfig.getPassphrase(), TEST_PASSPHRASE);
        assertEquals(softApConfig.getSecurityType(), SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
        assertEquals(softApConfig.isHiddenSsid(), TEST_HIDDEN);
        assertEquals(softApConfig.getBand(), TEST_BAND);
    }

    /**
     * Verify that the store data is serialized/deserialized correctly.
     *
     * @throws Exception
     */
    @Test
    public void serializeDeserializeSoftAp() throws Exception {
        SoftApConfiguration.Builder softApConfigBuilder = new SoftApConfiguration.Builder();
        softApConfigBuilder.setSsid(TEST_SSID);
        softApConfigBuilder.setPassphrase(TEST_PASSPHRASE,
                SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
        softApConfigBuilder.setBand(TEST_BAND);
        SoftApConfiguration softApConfig = softApConfigBuilder.build();

        // Serialize first.
        when(mDataSource.toSerialize()).thenReturn(softApConfigBuilder.build());
        byte[] serializedData = serializeData();

        // Now deserialize first.
        deserializeData(serializedData);
        ArgumentCaptor<SoftApConfiguration> softapConfigCaptor =
                ArgumentCaptor.forClass(SoftApConfiguration.class);
        verify(mDataSource).fromDeserialized(softapConfigCaptor.capture());
        SoftApConfiguration softApConfigDeserialized = softapConfigCaptor.getValue();
        assertNotNull(softApConfigDeserialized);

        assertEquals(softApConfig.getSsid(), softApConfigDeserialized.getSsid());
        assertEquals(softApConfig.getPassphrase(),
                softApConfigDeserialized.getPassphrase());
        assertEquals(softApConfig.getSecurityType(),
                softApConfigDeserialized.getSecurityType());
        assertEquals(softApConfig.isHiddenSsid(), softApConfigDeserialized.isHiddenSsid());
        assertEquals(softApConfig.getBand(), softApConfigDeserialized.getBand());
        assertEquals(softApConfig.getChannel(), softApConfigDeserialized.getChannel());
    }

    /**
     * Verify that the stored wpa3-sae data is serialized/deserialized correctly.
     *
     * @throws Exception
     */
    @Test
    public void serializeDeserializeSoftApWpa3Sae() throws Exception {
        SoftApConfiguration.Builder softApConfigBuilder = new SoftApConfiguration.Builder();
        softApConfigBuilder.setSsid(TEST_SSID);
        softApConfigBuilder.setPassphrase(TEST_PASSPHRASE,
                SoftApConfiguration.SECURITY_TYPE_WPA3_SAE);
        softApConfigBuilder.setBand(TEST_BAND);

        SoftApConfiguration softApConfig = softApConfigBuilder.build();

        // Serialize first.
        when(mDataSource.toSerialize()).thenReturn(softApConfigBuilder.build());
        byte[] serializedData = serializeData();

        // Now deserialize first.
        deserializeData(serializedData);
        ArgumentCaptor<SoftApConfiguration> softapConfigCaptor =
                ArgumentCaptor.forClass(SoftApConfiguration.class);
        verify(mDataSource).fromDeserialized(softapConfigCaptor.capture());
        SoftApConfiguration softApConfigDeserialized = softapConfigCaptor.getValue();
        assertNotNull(softApConfigDeserialized);

        assertEquals(softApConfig.getSsid(), softApConfigDeserialized.getSsid());
        assertEquals(softApConfig.getPassphrase(),
                softApConfigDeserialized.getPassphrase());
        assertEquals(softApConfig.getSecurityType(),
                softApConfigDeserialized.getSecurityType());
        assertEquals(softApConfig.isHiddenSsid(), softApConfigDeserialized.isHiddenSsid());
        assertEquals(softApConfig.getBand(), softApConfigDeserialized.getBand());
        assertEquals(softApConfig.getChannel(), softApConfigDeserialized.getChannel());
    }

    /**
     * Verify that the stored wpa3-sae-transition data is serialized/deserialized correctly.
     *
     * @throws Exception
     */
    @Test
    public void serializeDeserializeSoftApWpa3SaeTransition() throws Exception {
        SoftApConfiguration.Builder softApConfigBuilder = new SoftApConfiguration.Builder();
        softApConfigBuilder.setSsid(TEST_SSID);
        softApConfigBuilder.setPassphrase(TEST_PASSPHRASE,
                SoftApConfiguration.SECURITY_TYPE_WPA3_SAE_TRANSITION);
        softApConfigBuilder.setBand(TEST_BAND);

        SoftApConfiguration softApConfig = softApConfigBuilder.build();

        // Serialize first.
        when(mDataSource.toSerialize()).thenReturn(softApConfigBuilder.build());
        byte[] serializedData = serializeData();

        // Now deserialize first.
        deserializeData(serializedData);
        ArgumentCaptor<SoftApConfiguration> softapConfigCaptor =
                ArgumentCaptor.forClass(SoftApConfiguration.class);
        verify(mDataSource).fromDeserialized(softapConfigCaptor.capture());
        SoftApConfiguration softApConfigDeserialized = softapConfigCaptor.getValue();
        assertNotNull(softApConfigDeserialized);

        assertEquals(softApConfig.getSsid(), softApConfigDeserialized.getSsid());
        assertEquals(softApConfig.getPassphrase(),
                softApConfigDeserialized.getPassphrase());
        assertEquals(softApConfig.getSecurityType(),
                softApConfigDeserialized.getSecurityType());
        assertEquals(softApConfig.isHiddenSsid(), softApConfigDeserialized.isHiddenSsid());
        assertEquals(softApConfig.getBand(), softApConfigDeserialized.getBand());
        assertEquals(softApConfig.getChannel(), softApConfigDeserialized.getChannel());
    }

    /**
     * Verify that the store data is deserialized correctly using the predefined test XML data
     * when the auto shutdown tag is retrieved from
     * {@link WifiMigration.loadFromSettings(Context)}.
     *
     * @throws Exception
     */
    @Test
    public void deserializeSoftApWithNoAutoShutdownTag() throws Exception {

        // Toggle on when migrating.
        when(mOemMigrationData.isSoftApTimeoutEnabled()).thenReturn(true);
        deserializeData(
                TEST_SOFTAP_CONFIG_XML_STRING_WITH_ALL_CONFIG_EXCEPT_AUTO_SHUTDOWN.getBytes());
        ArgumentCaptor<SoftApConfiguration> softapConfigCaptor =
                ArgumentCaptor.forClass(SoftApConfiguration.class);
        verify(mDataSource).fromDeserialized(softapConfigCaptor.capture());
        SoftApConfiguration softApConfig = softapConfigCaptor.getValue();
        assertNotNull(softApConfig);
        assertEquals(softApConfig.getSsid(), TEST_SSID);
        assertTrue(softApConfig.isAutoShutdownEnabled());

        // Toggle off when migrating.
        when(mOemMigrationData.isSoftApTimeoutEnabled()).thenReturn(false);
        deserializeData(
                TEST_SOFTAP_CONFIG_XML_STRING_WITH_ALL_CONFIG_EXCEPT_AUTO_SHUTDOWN.getBytes());
        verify(mDataSource, times(2)).fromDeserialized(softapConfigCaptor.capture());
        softApConfig = softapConfigCaptor.getValue();
        assertNotNull(softApConfig);
        assertEquals(softApConfig.getSsid(), TEST_SSID);
        assertFalse(softApConfig.isAutoShutdownEnabled());
    }

    /**
     * Verify that the old format is deserialized correctly.
     *
     * @throws Exception
     */
    @Test
    public void deserializeSoftApWithNoBssidTag() throws Exception {
        // Start with the old serialized data
        deserializeData(TEST_SOFTAP_CONFIG_XML_STRING_WITH_ALL_CONFIG_EXCEPT_BSSID.getBytes());
        ArgumentCaptor<SoftApConfiguration> softapConfigCaptor =
                ArgumentCaptor.forClass(SoftApConfiguration.class);
        verify(mDataSource).fromDeserialized(softapConfigCaptor.capture());
        SoftApConfiguration softApConfig = softapConfigCaptor.getValue();
        assertNotNull(softApConfig);
        assertEquals(softApConfig.getSsid(), TEST_SSID);
        assertEquals(softApConfig.getPassphrase(), TEST_PASSPHRASE);
        assertEquals(softApConfig.getSecurityType(), SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
        assertEquals(softApConfig.isHiddenSsid(), TEST_HIDDEN);
        assertEquals(softApConfig.getBand(), TEST_BAND);
        assertEquals(softApConfig.isClientControlByUserEnabled(), TEST_CLIENT_CONTROL_BY_USER);
        assertEquals(softApConfig.getMaxNumberOfClients(), TEST_MAX_NUMBER_OF_CLIENTS);
        assertTrue(softApConfig.isAutoShutdownEnabled());
        assertEquals(softApConfig.getShutdownTimeoutMillis(), TEST_SHUTDOWN_TIMEOUT_MILLIS);
        assertEquals(softApConfig.getBlockedClientList(), TEST_BLOCKEDLIST);
        assertEquals(softApConfig.getAllowedClientList(), TEST_ALLOWEDLIST);
    }
}
