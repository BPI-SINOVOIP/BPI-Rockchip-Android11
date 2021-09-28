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

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.validateMockitoUsage;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiEnterpriseConfig;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.security.KeyStore;
import java.security.cert.X509Certificate;

/**
 * Unit tests for {@link com.android.server.wifi.WifiConfigManager}.
 */
@SmallTest
public class WifiKeyStoreTest extends WifiBaseTest {
    @Mock private WifiEnterpriseConfig mWifiEnterpriseConfig;
    @Mock private KeyStore mKeyStore;

    private WifiKeyStore mWifiKeyStore;
    private static final String TEST_KEY_ID = "blah";
    private static final String USER_CERT_ALIAS = "aabbccddee";
    private static final String USER_CA_CERT_ALIAS = "aacccddd";
    private static final String [] USER_CA_CERT_ALIASES = {"aacccddd", "bbbccccaaa"};
    private static final String TEST_PACKAGE_NAME = "TestApp";

    /**
     * Setup the mocks and an instance of WifiConfigManager before each test.
     */
    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mWifiKeyStore = new WifiKeyStore(mKeyStore);

        when(mWifiEnterpriseConfig.getClientCertificateAlias()).thenReturn(USER_CERT_ALIAS);
        when(mWifiEnterpriseConfig.getCaCertificateAlias()).thenReturn(USER_CA_CERT_ALIAS);
        when(mWifiEnterpriseConfig.getCaCertificateAliases())
                .thenReturn(USER_CA_CERT_ALIASES);
        when(mWifiEnterpriseConfig.getClientPrivateKey()).thenReturn(FakeKeys.RSA_KEY1);
        when(mWifiEnterpriseConfig.getClientCertificate()).thenReturn(FakeKeys.CLIENT_CERT);
        when(mWifiEnterpriseConfig.getCaCertificate()).thenReturn(FakeKeys.CA_CERT0);
        when(mWifiEnterpriseConfig.getClientCertificateChain())
                .thenReturn(new X509Certificate[] {FakeKeys.CLIENT_CERT});
        when(mWifiEnterpriseConfig.getCaCertificates())
                .thenReturn(new X509Certificate[] {FakeKeys.CA_CERT0});
        when(mWifiEnterpriseConfig.getKeyId(any())).thenReturn(TEST_KEY_ID);
    }

    /**
     * Called after each test
     */
    @After
    public void cleanup() {
        validateMockitoUsage();
    }

    /**
     * Verifies that keys and certs are removed when they were installed by an app.
     */
    @Test
    public void testRemoveKeysForAppInstalledCerts() throws Exception {
        when(mWifiEnterpriseConfig.isAppInstalledDeviceKeyAndCert()).thenReturn(true);
        when(mWifiEnterpriseConfig.isAppInstalledCaCert()).thenReturn(true);
        mWifiKeyStore.removeKeys(mWifiEnterpriseConfig);

        // Method calls the KeyStore#delete method 4 times, user key, user cert, and 2 CA cert
        verify(mKeyStore).deleteEntry(USER_CERT_ALIAS);
        verify(mKeyStore).deleteEntry(USER_CA_CERT_ALIASES[0]);
    }

    /**
     * Verifies that keys and certs are removed when they were installed by an app and not removed
     * when CA certs are installed by the user.
     */
    @Test
    public void testRemoveKeysForMixedInstalledCerts1() throws Exception {
        when(mWifiEnterpriseConfig.isAppInstalledDeviceKeyAndCert()).thenReturn(true);
        when(mWifiEnterpriseConfig.isAppInstalledCaCert()).thenReturn(false);
        mWifiKeyStore.removeKeys(mWifiEnterpriseConfig);

        // Method calls the KeyStore#deleteEntry method: user key and user cert
        verify(mKeyStore).deleteEntry(USER_CERT_ALIAS);
        verifyNoMoreInteractions(mKeyStore);
    }

    /**
     * Verifies that keys and certs are not removed when they were installed by the user and
     * removed when CA certs are installed by the app.
     */
    @Test
    public void testRemoveKeysForMixedInstalledCerts2() throws Exception {
        when(mWifiEnterpriseConfig.isAppInstalledDeviceKeyAndCert()).thenReturn(false);
        when(mWifiEnterpriseConfig.isAppInstalledCaCert()).thenReturn(true);
        mWifiKeyStore.removeKeys(mWifiEnterpriseConfig);

        // Method calls the KeyStore#delete method 2 times: 2 CA certs
        verify(mKeyStore).deleteEntry(USER_CA_CERT_ALIASES[0]);
        verify(mKeyStore).deleteEntry(USER_CA_CERT_ALIASES[1]);
        verifyNoMoreInteractions(mKeyStore);
    }

    /**
     * Verifies that keys and certs are not removed when they were installed by the user.
     */
    @Test
    public void testRemoveKeysForUserInstalledCerts() {
        when(mWifiEnterpriseConfig.isAppInstalledDeviceKeyAndCert()).thenReturn(false);
        when(mWifiEnterpriseConfig.isAppInstalledCaCert()).thenReturn(false);
        mWifiKeyStore.removeKeys(mWifiEnterpriseConfig);
        verifyNoMoreInteractions(mKeyStore);
    }

    /**
     * Verifies that keys and certs are added when they were installed by an app and verifies the
     * alias used.
     */
    @Test
    public void testAddKeysForAppInstalledCerts() throws Exception {
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork();
        config.enterpriseConfig = mWifiEnterpriseConfig;
        assertTrue(mWifiKeyStore.updateNetworkKeys(config, null));

        String expectedAlias = config.getKeyIdForCredentials(null);
        String expectedCaAlias = expectedAlias + "_0";
        // Method calls the KeyStore#delete method 4 times, user key, user cert, and 2 CA cert
        verify(mKeyStore).setKeyEntry(
                eq(expectedAlias), eq(FakeKeys.RSA_KEY1), eq(null),
                aryEq(new X509Certificate[] {FakeKeys.CLIENT_CERT}));
        verify(mKeyStore).setCertificateEntry(eq(expectedCaAlias), eq(FakeKeys.CA_CERT0));
        verify(mWifiEnterpriseConfig).setClientCertificateAlias(eq(expectedAlias));
        verify(mWifiEnterpriseConfig).setCaCertificateAliases(
                aryEq(new String[] {expectedCaAlias}));
    }

    /**
     * Add two same network credential one is from user saved, the other is from suggestion.
     * Both oh them should be installed successfully and has different alias, and will not override
     * each other.
     */
    @Test
    public void testAddRemoveFromBothSavedAndSuggestionNetwork() throws Exception {
        WifiConfiguration savedNetwork = WifiConfigurationTestUtil.createEapNetwork();
        WifiConfiguration suggestionNetwork = new WifiConfiguration(savedNetwork);
        savedNetwork.enterpriseConfig = mWifiEnterpriseConfig;
        suggestionNetwork.enterpriseConfig = mWifiEnterpriseConfig;
        suggestionNetwork.fromWifiNetworkSuggestion = true;
        suggestionNetwork.creatorName = TEST_PACKAGE_NAME;

        assertTrue(mWifiKeyStore.updateNetworkKeys(savedNetwork, null));
        assertTrue(mWifiKeyStore.updateNetworkKeys(suggestionNetwork, null));

        String savedNetworkAlias = savedNetwork.getKeyIdForCredentials(null);
        String savedNetworkCaAlias = savedNetworkAlias + "_0";

        String suggestionNetworkAlias = suggestionNetwork.getKeyIdForCredentials(null);
        String suggestionNetworkCaAlias = suggestionNetworkAlias + "_0";

        assertNotEquals(savedNetworkAlias, suggestionNetworkAlias);

        verify(mKeyStore).setKeyEntry(
                eq(savedNetworkAlias), eq(FakeKeys.RSA_KEY1), eq(null),
                aryEq(new X509Certificate[] {FakeKeys.CLIENT_CERT}));
        verify(mKeyStore).setCertificateEntry(eq(savedNetworkCaAlias), eq(FakeKeys.CA_CERT0));
        verify(mWifiEnterpriseConfig).setClientCertificateAlias(eq(savedNetworkAlias));
        verify(mWifiEnterpriseConfig).setCaCertificateAliases(
                aryEq(new String[] {savedNetworkCaAlias}));

        verify(mKeyStore).setKeyEntry(
                eq(suggestionNetworkAlias), eq(FakeKeys.RSA_KEY1), eq(null),
                aryEq(new X509Certificate[] {FakeKeys.CLIENT_CERT}));
        verify(mKeyStore).setCertificateEntry(eq(suggestionNetworkCaAlias), eq(FakeKeys.CA_CERT0));
        verify(mWifiEnterpriseConfig).setClientCertificateAlias(eq(suggestionNetworkAlias));
        verify(mWifiEnterpriseConfig).setCaCertificateAliases(
                aryEq(new String[] {suggestionNetworkCaAlias}));
    }

    @Test
    public void test_remove_empty_alias_enterprise_config() throws Exception {
        WifiConfiguration savedNetwork = WifiConfigurationTestUtil.createEapNetwork();
        WifiConfiguration suggestionNetwork = new WifiConfiguration(savedNetwork);
        suggestionNetwork.fromWifiNetworkSuggestion = true;
        suggestionNetwork.creatorName = TEST_PACKAGE_NAME;
        mWifiKeyStore.removeKeys(savedNetwork.enterpriseConfig);
        mWifiKeyStore.removeKeys(suggestionNetwork.enterpriseConfig);
        verify(mKeyStore, never()).deleteEntry(any());
    }

    /**
     * Test configuring WPA3-Enterprise in 192-bit mode for RSA 3072 correctly when CA and client
     * certificates are of RSA 3072 type and the network is Suite-B.
     */
    @Test
    public void testConfigureSuiteBRsa3072() throws Exception {
        when(mWifiEnterpriseConfig.getCaCertificateAliases())
                .thenReturn(new String[]{USER_CA_CERT_ALIAS});
        when(mWifiEnterpriseConfig.getClientPrivateKey())
                .thenReturn(FakeKeys.CLIENT_SUITE_B_RSA3072_KEY);
        when(mWifiEnterpriseConfig.getClientCertificate()).thenReturn(
                FakeKeys.CLIENT_SUITE_B_RSA3072_CERT);
        when(mWifiEnterpriseConfig.getCaCertificate()).thenReturn(FakeKeys.CA_SUITE_B_RSA3072_CERT);
        when(mWifiEnterpriseConfig.getClientCertificateChain())
                .thenReturn(new X509Certificate[]{FakeKeys.CLIENT_SUITE_B_RSA3072_CERT});
        when(mWifiEnterpriseConfig.getCaCertificates())
                .thenReturn(new X509Certificate[]{FakeKeys.CA_SUITE_B_RSA3072_CERT});
        when(mKeyStore.getCertificate(eq(USER_CERT_ALIAS))).thenReturn(
                FakeKeys.CLIENT_SUITE_B_RSA3072_CERT);
        when(mKeyStore.getCertificate(eq(USER_CA_CERT_ALIASES[0]))).thenReturn(
                FakeKeys.CA_SUITE_B_RSA3072_CERT);
        WifiConfiguration savedNetwork = WifiConfigurationTestUtil.createEapSuiteBNetwork(
                WifiConfiguration.SuiteBCipher.ECDHE_RSA);
        savedNetwork.enterpriseConfig = mWifiEnterpriseConfig;
        assertTrue(mWifiKeyStore.updateNetworkKeys(savedNetwork, null));
        assertTrue(savedNetwork.allowedSuiteBCiphers.get(WifiConfiguration.SuiteBCipher.ECDHE_RSA));
    }

    /**
     * Test configuring WPA3-Enterprise in 192-bit mode for ECDSA correctly when CA and client
     * certificates are of ECDSA type and the network is Suite-B.
     */
    @Test
    public void testConfigureSuiteBEcdsa() throws Exception {
        when(mWifiEnterpriseConfig.getCaCertificateAliases())
                .thenReturn(new String[]{USER_CA_CERT_ALIAS});
        when(mWifiEnterpriseConfig.getClientPrivateKey())
                .thenReturn(FakeKeys.CLIENT_SUITE_B_ECC_KEY);
        when(mWifiEnterpriseConfig.getClientCertificate()).thenReturn(
                FakeKeys.CLIENT_SUITE_B_ECDSA_CERT);
        when(mWifiEnterpriseConfig.getCaCertificate()).thenReturn(FakeKeys.CA_SUITE_B_ECDSA_CERT);
        when(mWifiEnterpriseConfig.getClientCertificateChain())
                .thenReturn(new X509Certificate[]{FakeKeys.CLIENT_SUITE_B_ECDSA_CERT});
        when(mWifiEnterpriseConfig.getCaCertificates())
                .thenReturn(new X509Certificate[]{FakeKeys.CA_SUITE_B_ECDSA_CERT});
        when(mKeyStore.getCertificate(eq(USER_CERT_ALIAS))).thenReturn(
                FakeKeys.CLIENT_SUITE_B_ECDSA_CERT);
        when(mKeyStore.getCertificate(eq(USER_CA_CERT_ALIASES[0]))).thenReturn(
                FakeKeys.CA_SUITE_B_ECDSA_CERT);
        WifiConfiguration savedNetwork = WifiConfigurationTestUtil.createEapSuiteBNetwork(
                WifiConfiguration.SuiteBCipher.ECDHE_ECDSA);
        savedNetwork.enterpriseConfig = mWifiEnterpriseConfig;
        assertTrue(mWifiKeyStore.updateNetworkKeys(savedNetwork, null));
        assertTrue(
                savedNetwork.allowedSuiteBCiphers.get(WifiConfiguration.SuiteBCipher.ECDHE_ECDSA));
    }

    /**
     * Test configuring WPA3-Enterprise in 192-bit mode for RSA 3072 fails when CA and client
     * certificates are not of the same type.
     */
    @Test
    public void testConfigurationFailureSuiteB() throws Exception {
        // Create a configuration with RSA client cert and ECDSA CA cert
        when(mWifiEnterpriseConfig.getClientPrivateKey())
                .thenReturn(FakeKeys.CLIENT_SUITE_B_RSA3072_KEY);
        when(mWifiEnterpriseConfig.getClientCertificate()).thenReturn(
                FakeKeys.CLIENT_SUITE_B_RSA3072_CERT);
        when(mWifiEnterpriseConfig.getCaCertificate()).thenReturn(FakeKeys.CA_SUITE_B_ECDSA_CERT);
        when(mWifiEnterpriseConfig.getClientCertificateChain())
                .thenReturn(new X509Certificate[]{FakeKeys.CLIENT_SUITE_B_RSA3072_CERT});
        when(mWifiEnterpriseConfig.getCaCertificates())
                .thenReturn(new X509Certificate[]{FakeKeys.CA_SUITE_B_ECDSA_CERT});
        when(mKeyStore.getCertificate(eq(USER_CERT_ALIAS))).thenReturn(
                FakeKeys.CLIENT_SUITE_B_RSA3072_CERT);
        when(mKeyStore.getCertificate(eq(USER_CA_CERT_ALIASES[0]))).thenReturn(
                FakeKeys.CA_SUITE_B_ECDSA_CERT);
        WifiConfiguration savedNetwork = WifiConfigurationTestUtil.createEapSuiteBNetwork(
                WifiConfiguration.SuiteBCipher.ECDHE_ECDSA);
        savedNetwork.enterpriseConfig = mWifiEnterpriseConfig;
        assertFalse(mWifiKeyStore.updateNetworkKeys(savedNetwork, null));
    }

    /**
     * Test configuring WPA3-Enterprise in 192-bit mode for RSA 3072 fails when CA is RSA but not
     * with the required security
     */
    @Test
    public void testConfigurationFailureSuiteBNon3072Rsa() throws Exception {
        // Create a configuration with RSA client cert and weak RSA CA cert
        when(mWifiEnterpriseConfig.getClientPrivateKey())
                .thenReturn(FakeKeys.CLIENT_SUITE_B_RSA3072_KEY);
        when(mWifiEnterpriseConfig.getClientCertificate()).thenReturn(
                FakeKeys.CLIENT_SUITE_B_RSA3072_CERT);
        when(mWifiEnterpriseConfig.getCaCertificate()).thenReturn(FakeKeys.CA_CERT0);
        when(mWifiEnterpriseConfig.getClientCertificateChain())
                .thenReturn(new X509Certificate[]{FakeKeys.CLIENT_SUITE_B_RSA3072_CERT});
        when(mWifiEnterpriseConfig.getCaCertificates())
                .thenReturn(new X509Certificate[]{FakeKeys.CA_CERT0});
        when(mKeyStore.getCertificate(eq(USER_CERT_ALIAS))).thenReturn(
                FakeKeys.CLIENT_SUITE_B_RSA3072_CERT);
        when(mKeyStore.getCertificate(eq(USER_CA_CERT_ALIASES[0]))).thenReturn(
                FakeKeys.CA_CERT0);
        WifiConfiguration savedNetwork = WifiConfigurationTestUtil.createEapSuiteBNetwork(
                WifiConfiguration.SuiteBCipher.ECDHE_RSA);
        savedNetwork.enterpriseConfig = mWifiEnterpriseConfig;
        assertFalse(mWifiKeyStore.updateNetworkKeys(savedNetwork, null));
    }

    /**
     * Test configuring WPA3-Enterprise in 192-bit mode for RSA 3072 fails when one CA in the list
     * is RSA but not with the required security
     */
    @Test
    public void testConfigurationFailureSuiteBNon3072RsaInList() throws Exception {
        // Create a configuration with RSA client cert and weak RSA CA cert
        when(mWifiEnterpriseConfig.getClientPrivateKey())
                .thenReturn(FakeKeys.CLIENT_SUITE_B_RSA3072_KEY);
        when(mWifiEnterpriseConfig.getClientCertificate()).thenReturn(
                FakeKeys.CLIENT_SUITE_B_RSA3072_CERT);
        when(mWifiEnterpriseConfig.getCaCertificate()).thenReturn(FakeKeys.CA_SUITE_B_RSA3072_CERT);
        when(mWifiEnterpriseConfig.getClientCertificateChain())
                .thenReturn(new X509Certificate[]{FakeKeys.CLIENT_SUITE_B_RSA3072_CERT});
        when(mWifiEnterpriseConfig.getCaCertificates())
                .thenReturn(
                        new X509Certificate[]{FakeKeys.CA_SUITE_B_RSA3072_CERT, FakeKeys.CA_CERT0});
        when(mKeyStore.getCertificate(eq(USER_CERT_ALIAS))).thenReturn(
                FakeKeys.CLIENT_SUITE_B_RSA3072_CERT);
        when(mKeyStore.getCertificate(eq(USER_CA_CERT_ALIASES[0]))).thenReturn(
                FakeKeys.CA_SUITE_B_RSA3072_CERT);
        when(mKeyStore.getCertificate(eq(USER_CA_CERT_ALIASES[1]))).thenReturn(
                FakeKeys.CA_CERT0);
        when(mWifiEnterpriseConfig.getCaCertificateAliases())
                .thenReturn(USER_CA_CERT_ALIASES);
        WifiConfiguration savedNetwork = WifiConfigurationTestUtil.createEapSuiteBNetwork(
                WifiConfiguration.SuiteBCipher.ECDHE_RSA);
        savedNetwork.enterpriseConfig = mWifiEnterpriseConfig;
        assertFalse(mWifiKeyStore.updateNetworkKeys(savedNetwork, null));
    }

    /**
     * Test configuring WPA3-Enterprise in 192-bit mode for RSA 3072 fails when one CA in the list
     * is RSA and the other is ECDSA
     */
    @Test
    public void testConfigurationFailureSuiteBRsaAndEcdsaInList() throws Exception {
        // Create a configuration with RSA client cert and weak RSA CA cert
        when(mWifiEnterpriseConfig.getClientPrivateKey())
                .thenReturn(FakeKeys.CLIENT_SUITE_B_RSA3072_KEY);
        when(mWifiEnterpriseConfig.getClientCertificate()).thenReturn(
                FakeKeys.CLIENT_SUITE_B_RSA3072_CERT);
        when(mWifiEnterpriseConfig.getCaCertificate()).thenReturn(FakeKeys.CA_SUITE_B_RSA3072_CERT);
        when(mWifiEnterpriseConfig.getClientCertificateChain())
                .thenReturn(new X509Certificate[]{FakeKeys.CLIENT_SUITE_B_RSA3072_CERT});
        when(mWifiEnterpriseConfig.getCaCertificates())
                .thenReturn(
                        new X509Certificate[]{FakeKeys.CA_SUITE_B_RSA3072_CERT,
                                FakeKeys.CA_SUITE_B_ECDSA_CERT});
        when(mKeyStore.getCertificate(eq(USER_CERT_ALIAS))).thenReturn(
                FakeKeys.CLIENT_SUITE_B_RSA3072_CERT);
        when(mKeyStore.getCertificate(eq(USER_CA_CERT_ALIASES[0]))).thenReturn(
                FakeKeys.CA_SUITE_B_RSA3072_CERT);
        when(mKeyStore.getCertificate(eq(USER_CA_CERT_ALIASES[1]))).thenReturn(
                FakeKeys.CA_SUITE_B_ECDSA_CERT);
        when(mWifiEnterpriseConfig.getCaCertificateAliases())
                .thenReturn(USER_CA_CERT_ALIASES);
        WifiConfiguration savedNetwork = WifiConfigurationTestUtil.createEapSuiteBNetwork(
                WifiConfiguration.SuiteBCipher.ECDHE_RSA);
        savedNetwork.enterpriseConfig = mWifiEnterpriseConfig;
        assertFalse(mWifiKeyStore.updateNetworkKeys(savedNetwork, null));
    }
}
