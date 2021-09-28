/*
 * Copyright (C) 2017 The Android Open Source Project
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

import static android.os.Process.SYSTEM_UID;

import static org.junit.Assert.*;
import static org.mockito.Mockito.*;

import android.content.Context;
import android.content.pm.PackageManager;
import android.net.MacAddress;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiEnterpriseConfig;
import android.util.Xml;

import androidx.test.filters.SmallTest;

import com.android.internal.util.FastXmlSerializer;
import com.android.server.wifi.util.ScanResultUtil;
import com.android.server.wifi.util.WifiConfigStoreEncryptionUtil;
import com.android.server.wifi.util.XmlUtilTest;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlSerializer;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * Unit tests for {@link com.android.server.wifi.NetworkListStoreData}.
 */
@SmallTest
public class NetworkListStoreDataTest extends WifiBaseTest {

    private static final String TEST_SSID = "WifiConfigStoreDataSSID_";
    private static final String TEST_CONNECT_CHOICE = "XmlUtilConnectChoice";
    private static final long TEST_CONNECT_CHOICE_TIMESTAMP = 0x4566;
    private static final String TEST_CREATOR_NAME = "CreatorName";
    private static final MacAddress TEST_RANDOMIZED_MAC =
            MacAddress.fromString("da:a1:19:c4:26:fa");

    private static final String SINGLE_OPEN_NETWORK_DATA_XML_STRING_FORMAT =
            "<Network>\n"
                    + "<WifiConfiguration>\n"
                    + "<string name=\"ConfigKey\">%s</string>\n"
                    + "<string name=\"SSID\">%s</string>\n"
                    + "<null name=\"PreSharedKey\" />\n"
                    + "<null name=\"WEPKeys\" />\n"
                    + "<int name=\"WEPTxKeyIndex\" value=\"0\" />\n"
                    + "<boolean name=\"HiddenSSID\" value=\"false\" />\n"
                    + "<boolean name=\"RequirePMF\" value=\"false\" />\n"
                    + "<byte-array name=\"AllowedKeyMgmt\" num=\"1\">01</byte-array>\n"
                    + "<byte-array name=\"AllowedProtocols\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedAuthAlgos\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedGroupCiphers\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedPairwiseCiphers\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedGroupMgmtCiphers\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedSuiteBCiphers\" num=\"0\"></byte-array>\n"
                    + "<boolean name=\"Shared\" value=\"%s\" />\n"
                    + "<boolean name=\"AutoJoinEnabled\" value=\"true\" />\n"
                    + "<boolean name=\"Trusted\" value=\"true\" />\n"
                    + "<null name=\"BSSID\" />\n"
                    + "<int name=\"Status\" value=\"2\" />\n"
                    + "<null name=\"FQDN\" />\n"
                    + "<null name=\"ProviderFriendlyName\" />\n"
                    + "<null name=\"LinkedNetworksList\" />\n"
                    + "<null name=\"DefaultGwMacAddress\" />\n"
                    + "<boolean name=\"ValidatedInternetAccess\" value=\"false\" />\n"
                    + "<boolean name=\"NoInternetAccessExpected\" value=\"false\" />\n"
                    + "<boolean name=\"MeteredHint\" value=\"false\" />\n"
                    + "<int name=\"MeteredOverride\" value=\"2\" />\n"
                    + "<boolean name=\"UseExternalScores\" value=\"false\" />\n"
                    + "<int name=\"CreatorUid\" value=\"%d\" />\n"
                    + "<string name=\"CreatorName\">%s</string>\n"
                    + "<int name=\"LastUpdateUid\" value=\"-1\" />\n"
                    + "<null name=\"LastUpdateName\" />\n"
                    + "<int name=\"LastConnectUid\" value=\"0\" />\n"
                    + "<boolean name=\"IsLegacyPasspointConfig\" value=\"false\" />\n"
                    + "<long-array name=\"RoamingConsortiumOIs\" num=\"0\" />\n"
                    + "<string name=\"RandomizedMacAddress\">%s</string>\n"
                    + "<int name=\"MacRandomizationSetting\" value=\"1\" />\n"
                    + "<int name=\"CarrierId\" value=\"-1\" />\n"
                    + "<boolean name=\"IsMostRecentlyConnected\" value=\"false\" />\n"
                    + "</WifiConfiguration>\n"
                    + "<NetworkStatus>\n"
                    + "<string name=\"SelectionStatus\">NETWORK_SELECTION_ENABLED</string>\n"
                    + "<string name=\"DisableReason\">NETWORK_SELECTION_ENABLE</string>\n"
                    + "<null name=\"ConnectChoice\" />\n"
                    + "<boolean name=\"HasEverConnected\" value=\"false\" />\n"
                    + "</NetworkStatus>\n"
                    + "<IpConfiguration>\n"
                    + "<string name=\"IpAssignment\">DHCP</string>\n"
                    + "<string name=\"ProxySettings\">NONE</string>\n"
                    + "</IpConfiguration>\n"
                    + "</Network>\n";

    private static final String SINGLE_EAP_NETWORK_DATA_XML_STRING_FORMAT =
            "<Network>\n"
                    + "<WifiConfiguration>\n"
                    + "<string name=\"ConfigKey\">%s</string>\n"
                    + "<string name=\"SSID\">%s</string>\n"
                    + "<null name=\"PreSharedKey\" />\n"
                    + "<null name=\"WEPKeys\" />\n"
                    + "<int name=\"WEPTxKeyIndex\" value=\"0\" />\n"
                    + "<boolean name=\"HiddenSSID\" value=\"false\" />\n"
                    + "<boolean name=\"RequirePMF\" value=\"false\" />\n"
                    + "<byte-array name=\"AllowedKeyMgmt\" num=\"1\">0c</byte-array>\n"
                    + "<byte-array name=\"AllowedProtocols\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedAuthAlgos\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedGroupCiphers\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedPairwiseCiphers\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedGroupMgmtCiphers\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedSuiteBCiphers\" num=\"0\"></byte-array>\n"
                    + "<boolean name=\"Shared\" value=\"%s\" />\n"
                    + "<boolean name=\"AutoJoinEnabled\" value=\"true\" />\n"
                    + "<boolean name=\"Trusted\" value=\"true\" />\n"
                    + "<null name=\"BSSID\" />\n"
                    + "<int name=\"Status\" value=\"2\" />\n"
                    + "<null name=\"FQDN\" />\n"
                    + "<null name=\"ProviderFriendlyName\" />\n"
                    + "<null name=\"LinkedNetworksList\" />\n"
                    + "<null name=\"DefaultGwMacAddress\" />\n"
                    + "<boolean name=\"ValidatedInternetAccess\" value=\"false\" />\n"
                    + "<boolean name=\"NoInternetAccessExpected\" value=\"false\" />\n"
                    + "<boolean name=\"MeteredHint\" value=\"false\" />\n"
                    + "<int name=\"MeteredOverride\" value=\"0\" />\n"
                    + "<boolean name=\"UseExternalScores\" value=\"false\" />\n"
                    + "<int name=\"CreatorUid\" value=\"%d\" />\n"
                    + "<string name=\"CreatorName\">%s</string>\n"
                    + "<int name=\"LastUpdateUid\" value=\"-1\" />\n"
                    + "<null name=\"LastUpdateName\" />\n"
                    + "<int name=\"LastConnectUid\" value=\"0\" />\n"
                    + "<boolean name=\"IsLegacyPasspointConfig\" value=\"false\" />\n"
                    + "<long-array name=\"RoamingConsortiumOIs\" num=\"0\" />\n"
                    + "<string name=\"RandomizedMacAddress\">%s</string>\n"
                    + "<int name=\"MacRandomizationSetting\" value=\"1\" />\n"
                    + "<int name=\"CarrierId\" value=\"-1\" />\n"
                    + "<boolean name=\"IsMostRecentlyConnected\" value=\"false\" />\n"
                    + "</WifiConfiguration>\n"
                    + "<NetworkStatus>\n"
                    + "<string name=\"SelectionStatus\">NETWORK_SELECTION_ENABLED</string>\n"
                    + "<string name=\"DisableReason\">NETWORK_SELECTION_ENABLE</string>\n"
                    + "<null name=\"ConnectChoice\" />\n"
                    + "<boolean name=\"HasEverConnected\" value=\"false\" />\n"
                    + "</NetworkStatus>\n"
                    + "<IpConfiguration>\n"
                    + "<string name=\"IpAssignment\">DHCP</string>\n"
                    + "<string name=\"ProxySettings\">NONE</string>\n"
                    + "</IpConfiguration>\n"
                    + "<WifiEnterpriseConfiguration>\n"
                    + "<string name=\"Identity\"></string>\n"
                    + "<string name=\"AnonIdentity\"></string>\n"
                    + "<string name=\"Password\"></string>\n"
                    + "<string name=\"ClientCert\"></string>\n"
                    + "<string name=\"CaCert\"></string>\n"
                    + "<string name=\"SubjectMatch\"></string>\n"
                    + "<string name=\"Engine\"></string>\n"
                    + "<string name=\"EngineId\"></string>\n"
                    + "<string name=\"PrivateKeyId\"></string>\n"
                    + "<string name=\"AltSubjectMatch\"></string>\n"
                    + "<string name=\"DomSuffixMatch\">%s</string>\n"
                    + "<string name=\"CaPath\">%s</string>\n"
                    + "<int name=\"EapMethod\" value=\"2\" />\n"
                    + "<int name=\"Phase2Method\" value=\"0\" />\n"
                    + "<string name=\"PLMN\"></string>\n"
                    + "<string name=\"Realm\"></string>\n"
                    + "<int name=\"Ocsp\" value=\"0\" />\n"
                    + "<string name=\"WapiCertSuite\"></string>\n"
                    + "</WifiEnterpriseConfiguration>\n"
                    + "</Network>\n";

    private static final String SINGLE_SAE_NETWORK_DATA_XML_STRING_FORMAT =
            "<Network>\n"
                    + "<WifiConfiguration>\n"
                    + "<string name=\"ConfigKey\">%s</string>\n"
                    + "<string name=\"SSID\">%s</string>\n"
                    + "<string name=\"PreSharedKey\">&quot;WifiConfigurationTestUtilPsk&quot;"
                    + "</string>\n"
                    + "<null name=\"WEPKeys\" />\n"
                    + "<int name=\"WEPTxKeyIndex\" value=\"0\" />\n"
                    + "<boolean name=\"HiddenSSID\" value=\"false\" />\n"
                    + "<boolean name=\"RequirePMF\" value=\"true\" />\n"
                    + "<byte-array name=\"AllowedKeyMgmt\" num=\"2\">0001</byte-array>\n"
                    + "<byte-array name=\"AllowedProtocols\" num=\"1\">02</byte-array>\n"
                    + "<byte-array name=\"AllowedAuthAlgos\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedGroupCiphers\" num=\"1\">28</byte-array>\n"
                    + "<byte-array name=\"AllowedPairwiseCiphers\" num=\"1\">0c</byte-array>\n"
                    + "<byte-array name=\"AllowedGroupMgmtCiphers\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedSuiteBCiphers\" num=\"0\"></byte-array>\n"
                    + "<boolean name=\"Shared\" value=\"%s\" />\n"
                    + "<boolean name=\"AutoJoinEnabled\" value=\"true\" />\n"
                    + "<boolean name=\"Trusted\" value=\"true\" />\n"
                    + "<null name=\"BSSID\" />\n"
                    + "<int name=\"Status\" value=\"2\" />\n"
                    + "<null name=\"FQDN\" />\n"
                    + "<null name=\"ProviderFriendlyName\" />\n"
                    + "<null name=\"LinkedNetworksList\" />\n"
                    + "<null name=\"DefaultGwMacAddress\" />\n"
                    + "<boolean name=\"ValidatedInternetAccess\" value=\"false\" />\n"
                    + "<boolean name=\"NoInternetAccessExpected\" value=\"false\" />\n"
                    + "<boolean name=\"MeteredHint\" value=\"false\" />\n"
                    + "<int name=\"MeteredOverride\" value=\"0\" />\n"
                    + "<boolean name=\"UseExternalScores\" value=\"false\" />\n"
                    + "<int name=\"CreatorUid\" value=\"%d\" />\n"
                    + "<string name=\"CreatorName\">%s</string>\n"
                    + "<int name=\"LastUpdateUid\" value=\"-1\" />\n"
                    + "<null name=\"LastUpdateName\" />\n"
                    + "<int name=\"LastConnectUid\" value=\"0\" />\n"
                    + "<boolean name=\"IsLegacyPasspointConfig\" value=\"false\" />\n"
                    + "<long-array name=\"RoamingConsortiumOIs\" num=\"0\" />\n"
                    + "<string name=\"RandomizedMacAddress\">%s</string>\n"
                    + "<int name=\"MacRandomizationSetting\" value=\"1\" />\n"
                    + "<int name=\"CarrierId\" value=\"-1\" />\n"
                    + "<boolean name=\"IsMostRecentlyConnected\" value=\"false\" />\n"
                    + "</WifiConfiguration>\n"
                    + "<NetworkStatus>\n"
                    + "<string name=\"SelectionStatus\">NETWORK_SELECTION_ENABLED</string>\n"
                    + "<string name=\"DisableReason\">NETWORK_SELECTION_ENABLE</string>\n"
                    + "<null name=\"ConnectChoice\" />\n"
                    + "<boolean name=\"HasEverConnected\" value=\"false\" />\n"
                    + "</NetworkStatus>\n"
                    + "<IpConfiguration>\n"
                    + "<string name=\"IpAssignment\">DHCP</string>\n"
                    + "<string name=\"ProxySettings\">NONE</string>\n"
                    + "</IpConfiguration>\n"
                    + "</Network>\n";

    /**
     * Repro'es the scenario in b/153435438.
     * Network has
     *  - Valid preSharedKey
     *  - KeyMgmt set to KeyMgmt.OSEN
     *  - ConfigKey set to "SSID"NONE
     */
    private static final String SINGLE_INVALID_NETWORK_DATA_XML_STRING_FORMAT =
            "<Network>\n"
                    + "<WifiConfiguration>\n"
                    + "<string name=\"ConfigKey\">%s</string>\n"
                    + "<string name=\"SSID\">%s</string>\n"
                    + "<string name=\"PreSharedKey\">%s</string>\n"
                    + "<null name=\"WEPKeys\" />\n"
                    + "<int name=\"WEPTxKeyIndex\" value=\"0\" />\n"
                    + "<boolean name=\"HiddenSSID\" value=\"false\" />\n"
                    + "<boolean name=\"RequirePMF\" value=\"false\" />\n"
                    + "<byte-array name=\"AllowedKeyMgmt\" num=\"1\">20</byte-array>\n"
                    + "<byte-array name=\"AllowedProtocols\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedAuthAlgos\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedGroupCiphers\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedPairwiseCiphers\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedGroupMgmtCiphers\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedSuiteBCiphers\" num=\"0\"></byte-array>\n"
                    + "<boolean name=\"Shared\" value=\"%s\" />\n"
                    + "<boolean name=\"AutoJoinEnabled\" value=\"true\" />\n"
                    + "<boolean name=\"Trusted\" value=\"true\" />\n"
                    + "<null name=\"BSSID\" />\n"
                    + "<int name=\"Status\" value=\"2\" />\n"
                    + "<null name=\"FQDN\" />\n"
                    + "<null name=\"ProviderFriendlyName\" />\n"
                    + "<null name=\"LinkedNetworksList\" />\n"
                    + "<null name=\"DefaultGwMacAddress\" />\n"
                    + "<boolean name=\"ValidatedInternetAccess\" value=\"false\" />\n"
                    + "<boolean name=\"NoInternetAccessExpected\" value=\"false\" />\n"
                    + "<boolean name=\"MeteredHint\" value=\"false\" />\n"
                    + "<int name=\"MeteredOverride\" value=\"0\" />\n"
                    + "<boolean name=\"UseExternalScores\" value=\"false\" />\n"
                    + "<int name=\"CreatorUid\" value=\"%d\" />\n"
                    + "<string name=\"CreatorName\">%s</string>\n"
                    + "<int name=\"LastUpdateUid\" value=\"-1\" />\n"
                    + "<null name=\"LastUpdateName\" />\n"
                    + "<int name=\"LastConnectUid\" value=\"0\" />\n"
                    + "<boolean name=\"IsLegacyPasspointConfig\" value=\"false\" />\n"
                    + "<long-array name=\"RoamingConsortiumOIs\" num=\"0\" />\n"
                    + "<string name=\"RandomizedMacAddress\">%s</string>\n"
                    + "<int name=\"MacRandomizationSetting\" value=\"1\" />\n"
                    + "<int name=\"CarrierId\" value=\"-1\" />\n"
                    + "<boolean name=\"IsMostRecentlyConnected\" value=\"false\" />\n"
                    + "</WifiConfiguration>\n"
                    + "<NetworkStatus>\n"
                    + "<string name=\"SelectionStatus\">NETWORK_SELECTION_ENABLED</string>\n"
                    + "<string name=\"DisableReason\">NETWORK_SELECTION_ENABLE</string>\n"
                    + "<null name=\"ConnectChoice\" />\n"
                    + "<boolean name=\"HasEverConnected\" value=\"false\" />\n"
                    + "</NetworkStatus>\n"
                    + "<IpConfiguration>\n"
                    + "<string name=\"IpAssignment\">UNASSIGNED</string>\n"
                    + "<string name=\"ProxySettings\">UNASSIGNED</string>\n"
                    + "</IpConfiguration>\n"
                    + "</Network>\n";

    // We use {@link NetworkListSharedStoreData} instance because {@link NetworkListStoreData} is
    // abstract.
    private NetworkListSharedStoreData mNetworkListSharedStoreData;
    @Mock private Context mContext;
    @Mock private PackageManager mPackageManager;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        when(mContext.getPackageManager()).thenReturn(mPackageManager);
        when(mPackageManager.getNameForUid(anyInt())).thenReturn(TEST_CREATOR_NAME);
        mNetworkListSharedStoreData = new NetworkListSharedStoreData(mContext);
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
        mNetworkListSharedStoreData.serializeData(out, mock(WifiConfigStoreEncryptionUtil.class));
        out.flush();
        return outputStream.toByteArray();
    }

    /**
     * Helper function for parsing configuration data from a XML block.
     *
     * @param data XML data to parse from
     * @return List of WifiConfiguration parsed
     * @throws Exception
     */
    private List<WifiConfiguration> deserializeData(byte[] data) throws Exception {
        final XmlPullParser in = Xml.newPullParser();
        final ByteArrayInputStream inputStream = new ByteArrayInputStream(data);
        in.setInput(inputStream, StandardCharsets.UTF_8.name());
        mNetworkListSharedStoreData.deserializeData(in, in.getDepth(),
                WifiConfigStore.ENCRYPT_CREDENTIALS_CONFIG_STORE_DATA_VERSION,
                mock(WifiConfigStoreEncryptionUtil.class));
        return mNetworkListSharedStoreData.getConfigurations();
    }

    /**
     * Helper function for generating a network list for testing purpose.  The network list
     * will contained an open, an EAP and an SAE networks.
     *
     * @param shared Flag indicating shared network
     * @return List of WifiConfiguration
     */
    private List<WifiConfiguration> getTestNetworksConfig(boolean shared) {
        WifiConfiguration openNetwork = WifiConfigurationTestUtil.createOpenNetwork();
        openNetwork.creatorName = TEST_CREATOR_NAME;
        openNetwork.shared = shared;
        openNetwork.setIpConfiguration(
                WifiConfigurationTestUtil.createDHCPIpConfigurationWithNoProxy());
        openNetwork.setRandomizedMacAddress(TEST_RANDOMIZED_MAC);
        openNetwork.meteredOverride = WifiConfiguration.METERED_OVERRIDE_NOT_METERED;
        WifiConfiguration eapNetwork = WifiConfigurationTestUtil.createEapNetwork();
        eapNetwork.shared = shared;
        eapNetwork.creatorName = TEST_CREATOR_NAME;
        eapNetwork.setIpConfiguration(
                WifiConfigurationTestUtil.createDHCPIpConfigurationWithNoProxy());
        eapNetwork.setRandomizedMacAddress(TEST_RANDOMIZED_MAC);
        WifiConfiguration saeNetwork = WifiConfigurationTestUtil.createSaeNetwork();
        saeNetwork.shared = shared;
        saeNetwork.creatorName = TEST_CREATOR_NAME;
        saeNetwork.setIpConfiguration(
                WifiConfigurationTestUtil.createDHCPIpConfigurationWithNoProxy());
        saeNetwork.setRandomizedMacAddress(TEST_RANDOMIZED_MAC);
        saeNetwork.setSecurityParams(WifiConfiguration.SECURITY_TYPE_SAE);
        List<WifiConfiguration> networkList = new ArrayList<>();
        networkList.add(openNetwork);
        networkList.add(eapNetwork);
        networkList.add(saeNetwork);
        return networkList;
    }

    /**
     * Helper function for generating XML block containing two networks, an open and an EAP
     * network.
     *
     * @param openNetwork The WifiConfiguration for an open network
     * @param eapNetwork The WifiConfiguration for an EAP network
     * @param saeNetwork The WifiConfiguration for an SAE network
     * @return byte[] of the XML data
     */
    private byte[] getTestNetworksXmlBytes(WifiConfiguration openNetwork,
            WifiConfiguration eapNetwork, WifiConfiguration saeNetwork) {
        String openNetworkXml = String.format(SINGLE_OPEN_NETWORK_DATA_XML_STRING_FORMAT,
                openNetwork.getKey().replaceAll("\"", "&quot;"),
                openNetwork.SSID.replaceAll("\"", "&quot;"),
                openNetwork.shared, openNetwork.creatorUid,
                openNetwork.creatorName, openNetwork.getRandomizedMacAddress());
        String eapNetworkXml = String.format(SINGLE_EAP_NETWORK_DATA_XML_STRING_FORMAT,
                eapNetwork.getKey().replaceAll("\"", "&quot;"),
                eapNetwork.SSID.replaceAll("\"", "&quot;"),
                eapNetwork.shared, eapNetwork.creatorUid,
                eapNetwork.creatorName, eapNetwork.getRandomizedMacAddress(),
                eapNetwork.enterpriseConfig.getDomainSuffixMatch(),
                eapNetwork.enterpriseConfig.getCaPath());
        String saeNetworkXml = String.format(SINGLE_SAE_NETWORK_DATA_XML_STRING_FORMAT,
                saeNetwork.getKey().replaceAll("\"", "&quot;"),
                saeNetwork.SSID.replaceAll("\"", "&quot;"),
                saeNetwork.shared, saeNetwork.creatorUid,
                saeNetwork.creatorName, saeNetwork.getRandomizedMacAddress());
        return (openNetworkXml + eapNetworkXml + saeNetworkXml).getBytes(StandardCharsets.UTF_8);
    }

    /**
     * Verify that serializing the store data without any configuration doesn't cause any crash
     * and no data should be serialized.
     *
     * @throws Exception
     */
    @Test
    public void serializeEmptyConfigs() throws Exception {
        assertEquals(0, serializeData().length);
    }

    /**
     * Verify that parsing an empty data doesn't cause any crash and no configuration should
     * be parsed.
     *
     * @throws Exception
     */
    @Test
    public void deserializeEmptyData() throws Exception {
        assertTrue(deserializeData(new byte[0]).isEmpty());
    }

    /**
     * Verify that {@link NetworkListSharedStoreData} is written to
     * {@link WifiConfigStore#STORE_FILE_NAME_SHARED_GENERAL}.
     * Verify that {@link NetworkListUserStoreData} is written to
     * {@link WifiConfigStore#STORE_FILE_NAME_USER_GENERAL}.
     *
     * @throws Exception
     */
    @Test
    public void getUserStoreFileId() throws Exception {
        assertEquals(WifiConfigStore.STORE_FILE_SHARED_GENERAL,
                mNetworkListSharedStoreData.getStoreFileId());
        assertEquals(WifiConfigStore.STORE_FILE_USER_GENERAL,
                new NetworkListUserStoreData(mContext).getStoreFileId());
    }

    /**
     * Verify that the shared configurations (containing an open, an EAP and an SAE networks) are
     * serialized correctly, matching the expected XML string.
     *
     * @throws Exception
     */
    @Test
    public void serializeSharedConfigurations() throws Exception {
        List<WifiConfiguration> networkList = getTestNetworksConfig(true /* shared */);
        mNetworkListSharedStoreData.setConfigurations(networkList);
        byte[] expectedData = getTestNetworksXmlBytes(networkList.get(0), networkList.get(1),
                networkList.get(2));
        byte[] serializedData = serializeData();
        assertArrayEquals(expectedData, serializeData());
    }

    /**
     * Verify that the shared configurations are parsed correctly from a XML string containing
     * test networks (an open and an EAP network).
     * @throws Exception
     */
    @Test
    public void deserializeSharedConfigurations() throws Exception {
        List<WifiConfiguration> networkList = getTestNetworksConfig(true /* shared */);
        byte[] xmlData = getTestNetworksXmlBytes(networkList.get(0), networkList.get(1),
                networkList.get(2));
        WifiConfigurationTestUtil.assertConfigurationsEqualForConfigStore(
                networkList, deserializeData(xmlData));
    }

    /**
     * Verify that we ignore any unknown tags when parsing a <Network> block.
     */
    @Test
    public void parseNetworkWithUnknownTag() throws Exception {
        String configFormat =
                "<Network>\n"
                        + "<WifiConfiguration>\n"
                        + "<string name=\"ConfigKey\">%s</string>\n"
                        + "<string name=\"SSID\">%s</string>\n"
                        + "<null name=\"PreSharedKey\" />\n"
                        + "<null name=\"WEPKeys\" />\n"
                        + "<int name=\"WEPTxKeyIndex\" value=\"0\" />\n"
                        + "<boolean name=\"HiddenSSID\" value=\"false\" />\n"
                        + "<boolean name=\"RequirePMF\" value=\"false\" />\n"
                        + "<byte-array name=\"AllowedKeyMgmt\" num=\"1\">01</byte-array>\n"
                        + "<byte-array name=\"AllowedProtocols\" num=\"0\"></byte-array>\n"
                        + "<byte-array name=\"AllowedAuthAlgos\" num=\"0\"></byte-array>\n"
                        + "<byte-array name=\"AllowedGroupCiphers\" num=\"0\"></byte-array>\n"
                        + "<byte-array name=\"AllowedPairwiseCiphers\" num=\"0\"></byte-array>\n"
                        + "<byte-array name=\"AllowedGroupMgmtCiphers\" num=\"0\"></byte-array>\n"
                        + "<byte-array name=\"AllowedSuiteBCiphers\" num=\"0\"></byte-array>\n"
                        + "<boolean name=\"Shared\" value=\"%s\" />\n"
                        + "<boolean name=\"AutoJoinEnabled\" value=\"true\" />\n"
                        + "<boolean name=\"Trusted\" value=\"true\" />\n"
                        + "<null name=\"BSSID\" />\n"
                        + "<null name=\"FQDN\" />\n"
                        + "<null name=\"ProviderFriendlyName\" />\n"
                        + "<null name=\"LinkedNetworksList\" />\n"
                        + "<null name=\"DefaultGwMacAddress\" />\n"
                        + "<boolean name=\"ValidatedInternetAccess\" value=\"false\" />\n"
                        + "<boolean name=\"NoInternetAccessExpected\" value=\"false\" />\n"
                        + "<boolean name=\"MeteredHint\" value=\"false\" />\n"
                        + "<boolean name=\"UseExternalScores\" value=\"false\" />\n"
                        + "<int name=\"CreatorUid\" value=\"%d\" />\n"
                        + "<null name=\"CreatorName\" />\n"
                        + "<int name=\"LastUpdateUid\" value=\"-1\" />\n"
                        + "<null name=\"LastUpdateName\" />\n"
                        + "<int name=\"LastConnectUid\" value=\"0\" />\n"
                        + "<string name=\"RandomizedMacAddress\">%s</string>\n"
                        + "<int name=\"MacRandomizationSetting\" value=\"1\" />\n"
                        + "</WifiConfiguration>\n"
                        + "<NetworkStatus>\n"
                        + "<string name=\"SelectionStatus\">NETWORK_SELECTION_ENABLED</string>\n"
                        + "<string name=\"DisableReason\">NETWORK_SELECTION_ENABLE</string>\n"
                        + "<null name=\"ConnectChoice\" />\n"
                        + "<boolean name=\"HasEverConnected\" value=\"false\" />\n"
                        + "</NetworkStatus>\n"
                        + "<IpConfiguration>\n"
                        + "<string name=\"IpAssignment\">DHCP</string>\n"
                        + "<string name=\"ProxySettings\">NONE</string>\n"
                        + "</IpConfiguration>\n"
                        + "<Unknown>"       // Unknown tag.
                        + "<int name=\"test\" value=\"0\" />\n"
                        + "</Unknown>"
                        + "</Network>\n";
        WifiConfiguration openNetwork = WifiConfigurationTestUtil.createOpenNetwork();
        byte[] xmlData = String.format(configFormat,
                openNetwork.getKey().replaceAll("\"", "&quot;"),
                openNetwork.SSID.replaceAll("\"", "&quot;"),
                openNetwork.shared, openNetwork.creatorUid, openNetwork.getRandomizedMacAddress())
            .getBytes(StandardCharsets.UTF_8);
        List<WifiConfiguration> deserializedConfigs = deserializeData(xmlData);
        assertEquals(1, deserializedConfigs.size());
        WifiConfiguration deserializedConfig  = deserializedConfigs.get(0);

        assertEquals(openNetwork.SSID, deserializedConfig.SSID);
        assertEquals(openNetwork.getKey(), deserializedConfig.getKey());
    }

    /**
     * Verify that a XmlPullParseException will be thrown when parsing a network configuration
     * containing a mismatched config key.
     *
     * @throws Exception
     */
    @Test(expected = XmlPullParserException.class)
    public void parseNetworkWithMismatchConfigKey() throws Exception {
        WifiConfiguration openNetwork = WifiConfigurationTestUtil.createOpenNetwork();
        byte[] xmlData = String.format(SINGLE_OPEN_NETWORK_DATA_XML_STRING_FORMAT,
                "InvalidConfigKey",
                openNetwork.SSID.replaceAll("\"", "&quot;"),
                openNetwork.shared, openNetwork.creatorUid,
                openNetwork.creatorName, openNetwork.getRandomizedMacAddress())
            .getBytes(StandardCharsets.UTF_8);
        deserializeData(xmlData);
    }

    /**
     * Tests that an invalid data in one of the WifiConfiguration object parsing would be skipped
     * gracefully. The other networks in the XML should still be parsed out correctly.
     */
    @Test
    public void parseNetworkListWithOneNetworkIllegalArgException() throws Exception {
        WifiConfiguration openNetwork = WifiConfigurationTestUtil.createOpenNetwork();
        WifiConfiguration eapNetwork = WifiConfigurationTestUtil.createEapNetwork();
        WifiConfiguration saeNetwork = WifiConfigurationTestUtil.createSaeNetwork();
        String xmlString = new String(getTestNetworksXmlBytes(openNetwork, eapNetwork, saeNetwork));
        // Manipulate the XML data to set the EAP method to None, this should raise an Illegal
        // argument exception in WifiEnterpriseConfig.setEapMethod().
        xmlString = xmlString.replaceAll(
                String.format(XmlUtilTest.XML_STRING_EAP_METHOD_REPLACE_FORMAT,
                        eapNetwork.enterpriseConfig.getEapMethod()),
                String.format(XmlUtilTest.XML_STRING_EAP_METHOD_REPLACE_FORMAT,
                        WifiEnterpriseConfig.Eap.NONE));
        List<WifiConfiguration> retrievedNetworkList =
                deserializeData(xmlString.getBytes(StandardCharsets.UTF_8));
        // Retrieved network should not contain the eap network.
        assertEquals(2, retrievedNetworkList.size());
        for (WifiConfiguration network : retrievedNetworkList) {
            assertNotEquals(eapNetwork.SSID, network.SSID);
        }
    }

    /**
     * Verify that a saved network config with invalid creatorUid resets it to
     * {@link android.os.Process#SYSTEM_UID}.
     */
    @Test
    public void parseNetworkWithInvalidCreatorUidResetsToSystem() throws Exception {
        WifiConfiguration openNetwork = WifiConfigurationTestUtil.createOpenNetwork();
        openNetwork.creatorUid = -1;
        // Return null for invalid uid.
        when(mPackageManager.getNameForUid(eq(openNetwork.creatorUid))).thenReturn(null);

        byte[] xmlData = String.format(SINGLE_OPEN_NETWORK_DATA_XML_STRING_FORMAT,
                openNetwork.getKey().replaceAll("\"", "&quot;"),
                openNetwork.SSID.replaceAll("\"", "&quot;"),
                openNetwork.shared, openNetwork.creatorUid,
                openNetwork.creatorName, openNetwork.getRandomizedMacAddress())
            .getBytes(StandardCharsets.UTF_8);
        List<WifiConfiguration> deserializedNetworks = deserializeData(xmlData);
        assertEquals(1, deserializedNetworks.size());
        assertEquals(openNetwork.getKey(), deserializedNetworks.get(0).getKey());
        assertEquals(SYSTEM_UID, deserializedNetworks.get(0).creatorUid);
        assertEquals(TEST_CREATOR_NAME, deserializedNetworks.get(0).creatorName);
    }

    /**
     * Verify that a saved network config with invalid creatorName resets it to the package name
     * provided {@link PackageManager} for the creatorUid.
     */
    @Test
    public void parseNetworkWithInvalidCreatorNameResetsToPackageNameForCreatorUid()
            throws Exception {
        String badCreatorName = "bad";
        String correctCreatorName = "correct";
        WifiConfiguration openNetwork = WifiConfigurationTestUtil.createOpenNetwork();
        openNetwork.creatorUid = 1324422;
        openNetwork.creatorName = badCreatorName;
        when(mPackageManager.getNameForUid(eq(openNetwork.creatorUid)))
            .thenReturn(correctCreatorName);

        byte[] xmlData = String.format(SINGLE_OPEN_NETWORK_DATA_XML_STRING_FORMAT,
                openNetwork.getKey().replaceAll("\"", "&quot;"),
                openNetwork.SSID.replaceAll("\"", "&quot;"),
                openNetwork.shared, openNetwork.creatorUid,
                openNetwork.creatorName, openNetwork.getRandomizedMacAddress())
            .getBytes(StandardCharsets.UTF_8);
        List<WifiConfiguration> deserializedNetworks = deserializeData(xmlData);
        assertEquals(1, deserializedNetworks.size());
        assertEquals(openNetwork.getKey(), deserializedNetworks.get(0).getKey());
        assertEquals(openNetwork.creatorUid, deserializedNetworks.get(0).creatorUid);
        assertEquals(correctCreatorName, deserializedNetworks.get(0).creatorName);
    }

    /**
     * Verify that a saved network config with invalid creatorName resets it to the package name
     * provided {@link PackageManager} for the creatorUid.
     */
    @Test
    public void parseNetworkWithNullCreatorNameResetsToPackageNameForCreatorUid()
            throws Exception {
        String correctCreatorName = "correct";
        WifiConfiguration openNetwork = WifiConfigurationTestUtil.createOpenNetwork();
        openNetwork.creatorUid = 1324422;
        openNetwork.creatorName = null;
        when(mPackageManager.getNameForUid(eq(openNetwork.creatorUid)))
            .thenReturn(correctCreatorName);

        byte[] xmlData = String.format(SINGLE_OPEN_NETWORK_DATA_XML_STRING_FORMAT,
                openNetwork.getKey().replaceAll("\"", "&quot;"),
                openNetwork.SSID.replaceAll("\"", "&quot;"),
                openNetwork.shared, openNetwork.creatorUid,
                openNetwork.creatorName, openNetwork.getRandomizedMacAddress())
            .getBytes(StandardCharsets.UTF_8);
        List<WifiConfiguration> deserializedNetworks = deserializeData(xmlData);
        assertEquals(1, deserializedNetworks.size());
        assertEquals(openNetwork.getKey(), deserializedNetworks.get(0).getKey());
        assertEquals(openNetwork.creatorUid, deserializedNetworks.get(0).creatorUid);
        assertEquals(correctCreatorName, deserializedNetworks.get(0).creatorName);
    }

    /**
     * Verify that a saved network config with valid creatorUid is preserved.
     */
    @Test
    public void parseNetworkWithValidCreatorUid() throws Exception {
        WifiConfiguration openNetwork = WifiConfigurationTestUtil.createOpenNetwork();
        openNetwork.creatorUid = 1324422;

        byte[] xmlData = String.format(SINGLE_OPEN_NETWORK_DATA_XML_STRING_FORMAT,
                openNetwork.getKey().replaceAll("\"", "&quot;"),
                openNetwork.SSID.replaceAll("\"", "&quot;"),
                openNetwork.shared, openNetwork.creatorUid,
                openNetwork.creatorName, openNetwork.getRandomizedMacAddress())
            .getBytes(StandardCharsets.UTF_8);
        List<WifiConfiguration> deserializedNetworks = deserializeData(xmlData);
        assertEquals(1, deserializedNetworks.size());
        assertEquals(openNetwork.getKey(), deserializedNetworks.get(0).getKey());
        assertEquals(openNetwork.creatorUid, deserializedNetworks.get(0).creatorUid);
        assertEquals(TEST_CREATOR_NAME, deserializedNetworks.get(0).creatorName);
    }

    /**
     * Verify that an SAE saved network config with legacy security settings is cleared from them
     * when deserializing it.
     */
    @Test
    public void fixSaeNetworkWithLegacySecurity() throws Exception {
        WifiConfiguration saeNetwork = WifiConfigurationTestUtil.createSaeNetwork();
        saeNetwork.shared = false;
        saeNetwork.creatorName = TEST_CREATOR_NAME;
        saeNetwork.setIpConfiguration(
                WifiConfigurationTestUtil.createDHCPIpConfigurationWithNoProxy());
        saeNetwork.setRandomizedMacAddress(TEST_RANDOMIZED_MAC);

        String saeNetworkWithOpenAuthXml = String.format(SINGLE_SAE_NETWORK_DATA_XML_STRING_FORMAT,
                saeNetwork.getKey().replaceAll("\"", "&quot;"),
                saeNetwork.SSID.replaceAll("\"", "&quot;"),
                saeNetwork.shared, saeNetwork.creatorUid,
                saeNetwork.creatorName, saeNetwork.getRandomizedMacAddress());

        saeNetworkWithOpenAuthXml.replaceAll("name=\"AllowedAuthAlgos\" num=\"0\"></byte-array>",
                "<name=\"AllowedAuthAlgos\" num=\"1\">01</byte-array>");

        saeNetworkWithOpenAuthXml.replaceAll("name=\"AllowedProtocols\" num=\"1\">02</byte-array>",
                "name=\"AllowedProtocols\" num=\"1\">03</byte-array>");

        saeNetworkWithOpenAuthXml.replaceAll(
                "name=\"AllowedGroupCiphers\" num=\"1\">28</byte-array>",
                "name=\"AllowedGroupCiphers\" num=\"1\">0f</byte-array>");

        saeNetworkWithOpenAuthXml.replaceAll(
                "name=\"AllowedPairwiseCiphers\" num=\"1\">0c</byte-array>",
                "name=\"AllowedPairwiseCiphers\" num=\"1\">06</byte-array>");

        List<WifiConfiguration> retrievedNetworkList =
                deserializeData(saeNetworkWithOpenAuthXml.getBytes(StandardCharsets.UTF_8));

        assertEquals(1, retrievedNetworkList.size());

        assertFalse(retrievedNetworkList.get(0).allowedAuthAlgorithms
                .get(WifiConfiguration.AuthAlgorithm.OPEN));

        assertTrue(retrievedNetworkList.get(0).allowedPairwiseCiphers
                .get(WifiConfiguration.PairwiseCipher.GCMP_256));
        assertTrue(retrievedNetworkList.get(0).allowedGroupCiphers
                .get(WifiConfiguration.GroupCipher.GCMP_256));
    }

    /**
     * The WifiConfiguration store should follow the sort of the SSIDs.
     */
    @Test
    public void testWifiConfigSaveToStoreOrder() throws Exception {
        String testSSID = "TEST_SSID";
        List<WifiConfiguration> storedWIfiConfig = new ArrayList<>();
        for (int i = 1; i <= 1; i++) {
            WifiConfiguration network = WifiConfigurationTestUtil.createOpenNetwork(
                    ScanResultUtil.createQuotedSSID(testSSID + (1 - i)));
            network.creatorName = TEST_CREATOR_NAME;
        }
        // Add to store data based on added order.
        mNetworkListSharedStoreData.setConfigurations(storedWIfiConfig);
        byte[] output1 = serializeData();
        // Add to store data based on SSID sort.
        Collections.sort(storedWIfiConfig, Comparator.comparing(a -> a.SSID));
        mNetworkListSharedStoreData.setConfigurations(storedWIfiConfig);
        byte[] output2 = serializeData();
        assertArrayEquals(output2, output1);
    }

    /**
     * Verify that we parse out a badly formed WifiConfiguration saved on the device because
     * our previous validation logic did not catch it.
     *
     * See b/153435438#comment26 for the exact problem.
     */
    @Test
    public void parseNetworkWithInvalidConfigKeyAndKeyMmt() throws Exception {
        // Valid psk network (that we should recreate after deserialization)
        WifiConfiguration pskNetwork = WifiConfigurationTestUtil.createPskNetwork();
        pskNetwork.setRandomizedMacAddress(TEST_RANDOMIZED_MAC);
        pskNetwork.creatorName = TEST_CREATOR_NAME;
        String invalidConfigKey = pskNetwork.getKey();
        invalidConfigKey.replace("WPA_PSK", "NONE");
        // XML data has 2 things that needs to be corrected:
        // - ConfigKey is set to "SSID"NONE instead of "SSID"WPA_PSK
        // - KeyMgmt has KeyMgmt.OSEN bit set instead of KeyMgmt.WPA_PSK
        byte[] xmlData = String.format(SINGLE_INVALID_NETWORK_DATA_XML_STRING_FORMAT,
                invalidConfigKey,
                pskNetwork.SSID.replaceAll("\"", "&quot;"),
                pskNetwork.preSharedKey.replaceAll("\"", "&quot;"),
                pskNetwork.shared, pskNetwork.creatorUid,
                pskNetwork.creatorName, pskNetwork.getRandomizedMacAddress())
                .getBytes(StandardCharsets.UTF_8);
        List<WifiConfiguration> deserializedNetworks = deserializeData(xmlData);
        assertEquals(1, deserializedNetworks.size());

        WifiConfiguration deserializedPskNetwork = deserializedNetworks.get(0);
        WifiConfigurationTestUtil.assertConfigurationEqualForConfigStore(
                pskNetwork, deserializedPskNetwork);
    }
}
