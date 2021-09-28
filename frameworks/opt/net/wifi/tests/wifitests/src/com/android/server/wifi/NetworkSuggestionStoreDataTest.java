/*
 * Copyright (C) 2018 The Android Open Source Project
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

import static org.junit.Assert.*;
import static org.mockito.Mockito.*;

import android.net.wifi.EAPConstants;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiNetworkSuggestion;
import android.net.wifi.hotspot2.PasspointConfiguration;
import android.net.wifi.hotspot2.pps.Credential;
import android.net.wifi.hotspot2.pps.HomeSp;
import android.util.Xml;

import androidx.test.filters.SmallTest;

import com.android.internal.util.FastXmlSerializer;
import com.android.server.wifi.WifiNetworkSuggestionsManager.ExtendedWifiNetworkSuggestion;
import com.android.server.wifi.WifiNetworkSuggestionsManager.PerAppInfo;

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
import java.util.HashMap;
import java.util.Map;

/**
 * Unit tests for {@link com.android.server.wifi.NetworkSuggestionStoreData}.
 */
@SmallTest
public class NetworkSuggestionStoreDataTest extends WifiBaseTest {
    private static final int TEST_UID_1 = 14556;
    private static final int TEST_UID_2 = 14536;
    private static final String TEST_PACKAGE_NAME_1 = "com.android.test.1";
    private static final String TEST_PACKAGE_NAME_2 = "com.android.test.2";
    private static final String  TEST_FEATURE_ID = "com.android.feature.1";
    private static final String TEST_FQDN = "FQDN";
    private static final String TEST_FRIENDLY_NAME = "test_friendly_name";
    private static final String TEST_REALM = "realm.test.com";
    private static final String TEST_PRE_R_STORE_FORMAT_XML_STRING =
            "<NetworkSuggestionPerApp>\n"
                    + "<string name=\"SuggestorPackageName\">%1$s</string>\n"
                    + "<string name=\"SuggestorFeatureId\">com.android.feature.1</string>\n"
                    + "<boolean name=\"SuggestorHasUserApproved\" value=\"false\" />\n"
                    + "<int name=\"SuggestorMaxSize\" value=\"100\" />\n"
                    + "<NetworkSuggestion>\n"
                    + "<WifiConfiguration>\n"
                    + "<string name=\"ConfigKey\">&quot;WifiConfigurationTestSSID0&quot;"
                    + "WPA_PSK</string>\n"
                    + "<string name=\"SSID\">&quot;WifiConfigurationTestSSID0&quot;</string>\n"
                    + "<null name=\"BSSID\" />\n"
                    + "<string name=\"PreSharedKey\">&quot;WifiConfigurationTestUtilPsk&quot;"
                    + "</string>\n"
                    + "<null name=\"SaePasswordId\" />\n"
                    + "<null name=\"WEPKeys\" />\n"
                    + "<int name=\"WEPTxKeyIndex\" value=\"0\" />\n"
                    + "<boolean name=\"HiddenSSID\" value=\"false\" />\n"
                    + "<boolean name=\"RequirePMF\" value=\"false\" />\n"
                    + "<byte-array name=\"AllowedKeyMgmt\" num=\"1\">02</byte-array>\n"
                    + "<byte-array name=\"AllowedProtocols\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedAuthAlgos\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedGroupCiphers\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedPairwiseCiphers\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedGroupMgmtCiphers\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedSuiteBCiphers\" num=\"0\"></byte-array>\n"
                    + "<boolean name=\"Shared\" value=\"true\" />\n"
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
                    + "<int name=\"NumAssociation\" value=\"0\" />\n"
                    + "<int name=\"CreatorUid\" value=\"5\" />\n"
                    + "<null name=\"CreatorName\" />\n"
                    + "<null name=\"CreationTime\" />\n"
                    + "<int name=\"LastUpdateUid\" value=\"-1\" />\n"
                    + "<null name=\"LastUpdateName\" />\n"
                    + "<int name=\"LastConnectUid\" value=\"0\" />\n"
                    + "<boolean name=\"IsLegacyPasspointConfig\" value=\"false\" />\n"
                    + "<long-array name=\"RoamingConsortiumOIs\" num=\"0\" />\n"
                    + "<string name=\"RandomizedMacAddress\">02:00:00:00:00:00</string>\n"
                    + "<int name=\"MacRandomizationSetting\" value=\"1\" />\n"
                    + "<int name=\"CarrierId\" value=\"-1\" />\n"
                    + "</WifiConfiguration>\n"
                    + "<boolean name=\"IsAppInteractionRequired\" value=\"true\" />\n"
                    + "<boolean name=\"IsUserInteractionRequired\" value=\"false\" />\n"
                    + "<boolean name=\"IsUserAllowedToManuallyConnect\" value=\"true\" />\n"
                    + "<int name=\"SuggestorUid\" value=\"%2$d\" />\n"
                    + "<string name=\"SuggestorPackageName\">%1$s</string>\n"
                    + "</NetworkSuggestion>\n"
                    + "</NetworkSuggestionPerApp>";
    private static final String TEST_POST_R_STORE_FORMAT_XML_STRING =
            "<NetworkSuggestionPerApp>\n"
                    + "<string name=\"SuggestorPackageName\">%1$s</string>\n"
                    + "<string name=\"SuggestorFeatureId\">com.android.feature.1</string>\n"
                    + "<boolean name=\"SuggestorHasUserApproved\" value=\"false\" />\n"
                    + "<int name=\"SuggestorMaxSize\" value=\"100\" />\n"
                    + "<int name=\"SuggestorUid\" value=\"%2$d\" />\n"
                    + "<NetworkSuggestion>\n"
                    + "<WifiConfiguration>\n"
                    + "<string name=\"ConfigKey\">&quot;WifiConfigurationTestSSID0&quot;"
                    + "WPA_PSK</string>\n"
                    + "<string name=\"SSID\">&quot;WifiConfigurationTestSSID0&quot;</string>\n"
                    + "<null name=\"BSSID\" />\n"
                    + "<string name=\"PreSharedKey\">&quot;WifiConfigurationTestUtilPsk&quot;"
                    + "</string>\n"
                    + "<null name=\"SaePasswordId\" />\n"
                    + "<null name=\"WEPKeys\" />\n"
                    + "<int name=\"WEPTxKeyIndex\" value=\"0\" />\n"
                    + "<boolean name=\"HiddenSSID\" value=\"false\" />\n"
                    + "<boolean name=\"RequirePMF\" value=\"false\" />\n"
                    + "<byte-array name=\"AllowedKeyMgmt\" num=\"1\">02</byte-array>\n"
                    + "<byte-array name=\"AllowedProtocols\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedAuthAlgos\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedGroupCiphers\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedPairwiseCiphers\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedGroupMgmtCiphers\" num=\"0\"></byte-array>\n"
                    + "<byte-array name=\"AllowedSuiteBCiphers\" num=\"0\"></byte-array>\n"
                    + "<boolean name=\"Shared\" value=\"true\" />\n"
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
                    + "<int name=\"NumAssociation\" value=\"0\" />\n"
                    + "<int name=\"CreatorUid\" value=\"5\" />\n"
                    + "<null name=\"CreatorName\" />\n"
                    + "<null name=\"CreationTime\" />\n"
                    + "<int name=\"LastUpdateUid\" value=\"-1\" />\n"
                    + "<null name=\"LastUpdateName\" />\n"
                    + "<int name=\"LastConnectUid\" value=\"0\" />\n"
                    + "<boolean name=\"IsLegacyPasspointConfig\" value=\"false\" />\n"
                    + "<long-array name=\"RoamingConsortiumOIs\" num=\"0\" />\n"
                    + "<string name=\"RandomizedMacAddress\">02:00:00:00:00:00</string>\n"
                    + "<int name=\"MacRandomizationSetting\" value=\"1\" />\n"
                    + "<int name=\"CarrierId\" value=\"-1\" />\n"
                    + "</WifiConfiguration>\n"
                    + "<boolean name=\"IsAppInteractionRequired\" value=\"true\" />\n"
                    + "<boolean name=\"IsUserInteractionRequired\" value=\"false\" />\n"
                    + "<boolean name=\"IsUserAllowedToManuallyConnect\" value=\"true\" />\n"
                    + "</NetworkSuggestion>\n"
                    + "</NetworkSuggestionPerApp>";
    private static final String TEST_CORRUPT_DATA_INVALID_SSID =
            "<NetworkSuggestionPerApp>\n"
            + "<string name=\"SuggestorPackageName\">com.android.test.1</string>\n"
            + "<boolean name=\"SuggestorHasUserApproved\" value=\"false\" />\n"
            + "<NetworkSuggestion>\n"
            + "<WifiConfiguration>\n"
            + "<string name=\"ConfigKey\">&quot;WifiConfigurationTestUtilSSID7&quot;NONE</string>\n"
            + "<blah blah=\"SSID\">&quot;WifiConfigurationTestUtilSSID7&quot;</blah>\n"
            + "<null name=\"BSSID\" />\n"
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
            + "<boolean name=\"Shared\" value=\"true\" />\n"
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
            + "<int name=\"NumAssociation\" value=\"0\" />\n"
            + "<int name=\"CreatorUid\" value=\"5\" />\n"
            + "<null name=\"CreatorName\" />\n"
            + "<null name=\"CreationTime\" />\n"
            + "<int name=\"LastUpdateUid\" value=\"-1\" />\n"
            + "<null name=\"LastUpdateName\" />\n"
            + "<int name=\"LastConnectUid\" value=\"0\" />\n"
            + "<boolean name=\"IsLegacyPasspointConfig\" value=\"false\" />\n"
            + "<long-array name=\"RoamingConsortiumOIs\" num=\"0\" />\n"
            + "<string name=\"RandomizedMacAddress\">02:00:00:00:00:00</string>\n"
            + "</WifiConfiguration>\n"
            + "<boolean name=\"IsAppInteractionRequired\" value=\"false\" />\n"
            + "<boolean name=\"IsUserInteractionRequired\" value=\"false\" />\n"
            + "<int name=\"SuggestorUid\" value=\"14556\" />\n"
            + "</NetworkSuggestion>\n"
            + "</NetworkSuggestionPerApp>";
    private static final String TEST_POST_R_APP_WITH_EMPTY_SUGGESTION =
            "<NetworkSuggestionPerApp>\n"
            + "<string name=\"SuggestorPackageName\">%1$s</string>\n"
            + "<string name=\"SuggestorFeatureId\">com.android.feature.1</string>\n"
            + "<boolean name=\"SuggestorHasUserApproved\" value=\"false\" />\n"
            + "<int name=\"SuggestorMaxSize\" value=\"100\" />\n"
            + "<int name=\"SuggestorUid\" value=\"%2$d\" />\n"
            + "</NetworkSuggestionPerApp>";
    private static final String TEST_PRE_R_APP_WITH_EMPTY_SUGGESTION =
            "<NetworkSuggestionPerApp>\n"
                    + "<string name=\"SuggestorPackageName\">%1$s</string>\n"
                    + "<string name=\"SuggestorFeatureId\">com.android.feature.1</string>\n"
                    + "<boolean name=\"SuggestorHasUserApproved\" value=\"false\" />\n"
                    + "<int name=\"SuggestorMaxSize\" value=\"100\" />\n"
                    + "</NetworkSuggestionPerApp>";

    private @Mock NetworkSuggestionStoreData.DataSource mDataSource;
    private NetworkSuggestionStoreData mNetworkSuggestionStoreData;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mNetworkSuggestionStoreData = new NetworkSuggestionStoreData(mDataSource);
    }

    /**
     * Helper function for serializing configuration data to a XML block.
     */
    private byte[] serializeData() throws Exception {
        final XmlSerializer out = new FastXmlSerializer();
        final ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
        out.setOutput(outputStream, StandardCharsets.UTF_8.name());
        mNetworkSuggestionStoreData.serializeData(out, null);
        out.flush();
        return outputStream.toByteArray();
    }

    /**
     * Helper function for parsing configuration data from a XML block.
     */
    private void deserializeData(byte[] data) throws Exception {
        final XmlPullParser in = Xml.newPullParser();
        final ByteArrayInputStream inputStream = new ByteArrayInputStream(data);
        in.setInput(inputStream, StandardCharsets.UTF_8.name());
        mNetworkSuggestionStoreData.deserializeData(in, in.getDepth(),
                WifiConfigStore.ENCRYPT_CREDENTIALS_CONFIG_STORE_DATA_VERSION, null);
    }

    /**
     * Verify store file Id.
     */
    @Test
    public void verifyStoreFileId() throws Exception {
        assertEquals(WifiConfigStore.STORE_FILE_USER_NETWORK_SUGGESTIONS,
                mNetworkSuggestionStoreData.getStoreFileId());
    }

    /**
     * Serialize/Deserialize a single network suggestion from a single app.
     */
    @Test
    public void serializeDeserializeSingleNetworkSuggestionFromSingleApp() throws Exception {
        Map<String, PerAppInfo> networkSuggestionsMap = new HashMap<>();

        PerAppInfo appInfo = new PerAppInfo(TEST_UID_1, TEST_PACKAGE_NAME_1, TEST_FEATURE_ID);

        WifiConfiguration configuration = WifiConfigurationTestUtil.createEapNetwork();
        configuration.enterpriseConfig =
                WifiConfigurationTestUtil.createPEAPWifiEnterpriseConfigWithGTCPhase2();
        WifiNetworkSuggestion networkSuggestion =
                new WifiNetworkSuggestion(configuration, null, false, false, true, true);
        appInfo.hasUserApproved = false;
        appInfo.extNetworkSuggestions.add(
                ExtendedWifiNetworkSuggestion.fromWns(networkSuggestion, appInfo, true));
        networkSuggestionsMap.put(TEST_PACKAGE_NAME_1, appInfo);

        Map<String, PerAppInfo> deserializedPerAppInfoMap =
                assertSerializeDeserialize(networkSuggestionsMap);
        ExtendedWifiNetworkSuggestion deserializedSuggestion =
                deserializedPerAppInfoMap.get(TEST_PACKAGE_NAME_1).extNetworkSuggestions.stream()
                        .findAny()
                        .orElse(null);

        WifiConfigurationTestUtil.assertConfigurationEqual(
                configuration, deserializedSuggestion.wns.wifiConfiguration);
        WifiConfigurationTestUtil.assertWifiEnterpriseConfigEqualForConfigStore(
                configuration.enterpriseConfig,
                deserializedSuggestion.wns.wifiConfiguration.enterpriseConfig);
    }

    /**
     * Serialize/Deserialize a single network suggestion from multiple apps.
     */
    @Test
    public void serializeDeserializeSingleNetworkSuggestionFromMultipleApps() throws Exception {
        Map<String, PerAppInfo> networkSuggestionsMap = new HashMap<>();

        PerAppInfo appInfo1 = new PerAppInfo(TEST_UID_1, TEST_PACKAGE_NAME_1, TEST_FEATURE_ID);
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        appInfo1.hasUserApproved = false;
        appInfo1.extNetworkSuggestions.add(
                ExtendedWifiNetworkSuggestion.fromWns(networkSuggestion1, appInfo1, true));
        networkSuggestionsMap.put(TEST_PACKAGE_NAME_1, appInfo1);

        PerAppInfo appInfo2 = new PerAppInfo(TEST_UID_2, TEST_PACKAGE_NAME_2, TEST_FEATURE_ID);
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true);
        appInfo2.hasUserApproved = true;
        appInfo2.extNetworkSuggestions.add(
                ExtendedWifiNetworkSuggestion.fromWns(networkSuggestion2, appInfo2, true));
        networkSuggestionsMap.put(TEST_PACKAGE_NAME_2, appInfo2);

        assertSerializeDeserialize(networkSuggestionsMap);
    }

    /**
     * Serialize/Deserialize multiple network suggestion from multiple apps.
     */
    @Test
    public void serializeDeserializeMultipleNetworkSuggestionFromMultipleApps() throws Exception {
        Map<String, PerAppInfo> networkSuggestionsMap = new HashMap<>();

        PerAppInfo appInfo1 = new PerAppInfo(TEST_UID_1, TEST_PACKAGE_NAME_1, TEST_FEATURE_ID);
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, true, true, true);
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true);
        appInfo1.hasUserApproved = true;
        appInfo1.extNetworkSuggestions.add(
                ExtendedWifiNetworkSuggestion.fromWns(networkSuggestion1, appInfo1, true));
        appInfo1.extNetworkSuggestions.add(
                ExtendedWifiNetworkSuggestion.fromWns(networkSuggestion2, appInfo1, true));
        networkSuggestionsMap.put(TEST_PACKAGE_NAME_1, appInfo1);

        PerAppInfo appInfo2 = new PerAppInfo(TEST_UID_2, TEST_PACKAGE_NAME_2, TEST_FEATURE_ID);
        WifiNetworkSuggestion networkSuggestion3 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true);
        WifiNetworkSuggestion networkSuggestion4 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, true, true, true);
        appInfo2.hasUserApproved = true;
        appInfo2.extNetworkSuggestions.add(
                ExtendedWifiNetworkSuggestion.fromWns(networkSuggestion3, appInfo2, true));
        appInfo2.extNetworkSuggestions.add(
                ExtendedWifiNetworkSuggestion.fromWns(networkSuggestion4, appInfo2, true));
        networkSuggestionsMap.put(TEST_PACKAGE_NAME_2, appInfo2);

        assertSerializeDeserialize(networkSuggestionsMap);
    }

    /**
     * Deserialize corrupt data and ensure that we gracefully handle any errors in the data.
     */
    @Test
    public void deserializeCorruptData() throws Exception {
        deserializeData(TEST_CORRUPT_DATA_INVALID_SSID.getBytes());
    }

    /**
     * Deserialize a single network suggestion from a single app using a predefined string
     * stored in the old/new XML format.
     */
    @Test
    public void deserializeFromPreRAndPostRFormat() throws Exception {
        ArgumentCaptor<HashMap> deserializedNetworkSuggestionsMap =
                ArgumentCaptor.forClass(HashMap.class);

        // Old format
        String preRFormatXml = String.format(
                TEST_PRE_R_STORE_FORMAT_XML_STRING, TEST_PACKAGE_NAME_1, TEST_UID_1);
        deserializeData(preRFormatXml.getBytes());

        // New format
        String postRFormatXml = String.format(
                TEST_POST_R_STORE_FORMAT_XML_STRING, TEST_PACKAGE_NAME_1, TEST_UID_1);
        deserializeData(postRFormatXml.getBytes());

        // Capture the deserialized data.
        verify(mDataSource, times(2)).fromDeserialized(deserializedNetworkSuggestionsMap.capture());
        Map<String, PerAppInfo> deserializedPerAppInfoMapPreRFormat =
                deserializedNetworkSuggestionsMap.getAllValues().get(0);
        Map<String, PerAppInfo> deserializedPerAppInfoMapPostRFormat =
                deserializedNetworkSuggestionsMap.getAllValues().get(1);

        // Ensure both formats produce the same parsed output.
        assertEquals(deserializedPerAppInfoMapPreRFormat, deserializedPerAppInfoMapPostRFormat);
    }

    /**
     * Deserialize no network suggestion from a single app using a predefined string stored in the
     * old/new XML format.
     */
    @Test
    public void deserializeEmptySuggestion() throws Exception {
        ArgumentCaptor<HashMap> deserializedNetworkSuggestionsMap =
                ArgumentCaptor.forClass(HashMap.class);

        // Old format with empty suggestion
        String preRFormatXml = String.format(
                TEST_PRE_R_APP_WITH_EMPTY_SUGGESTION, TEST_PACKAGE_NAME_1, TEST_UID_1);
        deserializeData(preRFormatXml.getBytes());

        // New format with empty suggestion
        String postRFormatXml = String.format(
                TEST_POST_R_APP_WITH_EMPTY_SUGGESTION, TEST_PACKAGE_NAME_1, TEST_UID_1);
        deserializeData(postRFormatXml.getBytes());

        // Capture the deserialized data.
        verify(mDataSource, times(2)).fromDeserialized(deserializedNetworkSuggestionsMap.capture());
        Map<String, PerAppInfo> deserializedPerAppInfoMapPreRFormat =
                deserializedNetworkSuggestionsMap.getAllValues().get(0);
        Map<String, PerAppInfo> deserializedPerAppInfoMapPostRFormat =
                deserializedNetworkSuggestionsMap.getAllValues().get(1);
        // Verify PerAppInfo is no
        assertNotNull(deserializedPerAppInfoMapPreRFormat.get(TEST_PACKAGE_NAME_1));
        assertNotNull(deserializedPerAppInfoMapPostRFormat.get(TEST_PACKAGE_NAME_1));
    }

    /**
     * Serialize/Deserialize a single Passpoint network suggestion from a single app.
     */
    @Test
    public void serializeDeserializeSinglePasspointSuggestionFromSingleApp() throws Exception {
        Map<String, PerAppInfo> networkSuggestionsMap = new HashMap<>();

        PerAppInfo appInfo = new PerAppInfo(TEST_UID_1, TEST_PACKAGE_NAME_1, TEST_FEATURE_ID);

        WifiNetworkSuggestion.Builder builder = new WifiNetworkSuggestion.Builder();
        builder.setPasspointConfig(
                createTestConfigWithUserCredential(TEST_FQDN, TEST_FRIENDLY_NAME));
        WifiNetworkSuggestion networkSuggestion = builder.build();
        appInfo.hasUserApproved = false;
        appInfo.extNetworkSuggestions.add(
                ExtendedWifiNetworkSuggestion.fromWns(networkSuggestion, appInfo, true));
        networkSuggestionsMap.put(TEST_PACKAGE_NAME_1, appInfo);

        Map<String, PerAppInfo> deserializedPerAppInfoMap =
                assertSerializeDeserialize(networkSuggestionsMap);
        ExtendedWifiNetworkSuggestion deserializedSuggestion =
                deserializedPerAppInfoMap.get(TEST_PACKAGE_NAME_1).extNetworkSuggestions.stream()
                        .findAny()
                        .orElse(null);
        assertEquals(networkSuggestion, deserializedSuggestion.wns);
    }

    @Test
    public void testDeserializeNullData() throws Exception {
        mNetworkSuggestionStoreData.deserializeData(null, 0,
                WifiConfigStore.ENCRYPT_CREDENTIALS_CONFIG_STORE_DATA_VERSION, null);
        verify(mDataSource).fromDeserialized(any());
    }

    private Map<String, PerAppInfo> assertSerializeDeserialize(
            Map<String, PerAppInfo> networkSuggestionsMap) throws Exception {
        // Setup the data to serialize.
        when(mDataSource.toSerialize()).thenReturn(networkSuggestionsMap);

        // Serialize/deserialize data.
        deserializeData(serializeData());

        // Verify the deserialized data.
        ArgumentCaptor<HashMap> deserializedNetworkSuggestionsMap =
                ArgumentCaptor.forClass(HashMap.class);
        verify(mDataSource).fromDeserialized(deserializedNetworkSuggestionsMap.capture());
        assertEquals(networkSuggestionsMap, deserializedNetworkSuggestionsMap.getValue());
        return deserializedNetworkSuggestionsMap.getValue();
    }

    private PasspointConfiguration createTestConfigWithUserCredential(String fqdn,
            String friendlyName) {
        PasspointConfiguration config = new PasspointConfiguration();
        HomeSp homeSp = new HomeSp();
        homeSp.setFqdn(fqdn);
        homeSp.setFriendlyName(friendlyName);
        config.setHomeSp(homeSp);
        Map<String, String> friendlyNames = new HashMap<>();
        friendlyNames.put("en", friendlyName);
        friendlyNames.put("kr", friendlyName + 1);
        friendlyNames.put("jp", friendlyName + 2);
        config.setServiceFriendlyNames(friendlyNames);
        Credential credential = new Credential();
        credential.setRealm(TEST_REALM);
        credential.setCaCertificate(FakeKeys.CA_CERT0);
        Credential.UserCredential userCredential = new Credential.UserCredential();
        userCredential.setUsername("username");
        userCredential.setPassword("password");
        userCredential.setEapType(EAPConstants.EAP_TTLS);
        userCredential.setNonEapInnerMethod(Credential.UserCredential.AUTH_METHOD_MSCHAP);
        credential.setUserCredential(userCredential);
        config.setCredential(credential);
        return config;
    }
}
