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

import static android.net.wifi.WifiEnterpriseConfig.Eap.AKA;
import static android.net.wifi.WifiEnterpriseConfig.Eap.WAPI_CERT;

import android.net.MacAddress;
import android.net.wifi.WifiEnterpriseConfig;
import android.net.wifi.WifiNetworkSuggestion;
import android.net.wifi.hotspot2.PasspointConfiguration;
import android.net.wifi.hotspot2.pps.Credential;
import android.net.wifi.hotspot2.pps.HomeSp;
import android.platform.test.annotations.AppModeFull;
import android.telephony.TelephonyManager;
import android.test.AndroidTestCase;

import androidx.test.filters.SmallTest;

@AppModeFull(reason = "Cannot get WifiManager in instant app mode")
@SmallTest
public class WifiNetworkSuggestionTest extends WifiJUnit3TestBase {
    private static final String TEST_SSID = "testSsid";
    private static final String TEST_BSSID = "00:df:aa:bc:12:23";
    private static final String TEST_PASSPHRASE = "testPassword";

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

    private WifiNetworkSuggestion.Builder createBuilderWithCommonParams() {
        return createBuilderWithCommonParams(false);
    }

    private WifiNetworkSuggestion.Builder createBuilderWithCommonParams(boolean isPasspoint) {
        WifiNetworkSuggestion.Builder builder = new WifiNetworkSuggestion.Builder();
        if (!isPasspoint) {
            builder.setSsid(TEST_SSID);
            builder.setBssid(MacAddress.fromString(TEST_BSSID));
            builder.setIsEnhancedOpen(false);
            builder.setIsHiddenSsid(true);
        }
        builder.setPriority(0);
        builder.setIsAppInteractionRequired(true);
        builder.setIsUserInteractionRequired(true);
        builder.setIsMetered(true);
        builder.setCarrierId(TelephonyManager.UNKNOWN_CARRIER_ID);
        builder.setCredentialSharedWithUser(true);
        builder.setIsInitialAutojoinEnabled(true);
        builder.setUntrusted(false);
        return builder;
    }

    private void validateCommonParams(WifiNetworkSuggestion suggestion) {
        validateCommonParams(suggestion, false);
    }

    private void validateCommonParams(WifiNetworkSuggestion suggestion, boolean isPasspoint) {
        assertNotNull(suggestion);
        assertNotNull(suggestion.getWifiConfiguration());
        if (!isPasspoint) {
            assertEquals(TEST_SSID, suggestion.getSsid());
            assertEquals(TEST_BSSID, suggestion.getBssid().toString());
            assertFalse(suggestion.isEnhancedOpen());
            assertTrue(suggestion.isHiddenSsid());
        }
        assertEquals(0, suggestion.getPriority());
        assertTrue(suggestion.isAppInteractionRequired());
        assertTrue(suggestion.isUserInteractionRequired());
        assertTrue(suggestion.isMetered());
        assertTrue(suggestion.isCredentialSharedWithUser());
        assertTrue(suggestion.isInitialAutojoinEnabled());
        assertFalse(suggestion.isUntrusted());
    }

    /**
     * Tests {@link android.net.wifi.WifiNetworkSuggestion.Builder} class.
     */
    public void testBuilderWithWpa2Passphrase() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        WifiNetworkSuggestion suggestion =
                createBuilderWithCommonParams()
                .setWpa2Passphrase(TEST_PASSPHRASE)
                .build();
        validateCommonParams(suggestion);
        assertEquals(TEST_PASSPHRASE, suggestion.getPassphrase());
        assertNull(suggestion.getPasspointConfig());
    }

    /**
     * Tests {@link android.net.wifi.WifiNetworkSuggestion.Builder} class.
     */
    public void testBuilderWithWpa3Passphrase() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        WifiNetworkSuggestion suggestion =
                createBuilderWithCommonParams()
                        .setWpa3Passphrase(TEST_PASSPHRASE)
                        .build();
        validateCommonParams(suggestion);
        assertEquals(TEST_PASSPHRASE, suggestion.getPassphrase());
        assertNull(suggestion.getPasspointConfig());
    }

    /**
     * Tests {@link android.net.wifi.WifiNetworkSuggestion.Builder} class.
     */
    public void testBuilderWithWapiPassphrase() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        WifiNetworkSuggestion suggestion =
                createBuilderWithCommonParams()
                        .setWapiPassphrase(TEST_PASSPHRASE)
                        .build();
        validateCommonParams(suggestion);
        assertEquals(TEST_PASSPHRASE, suggestion.getPassphrase());
        assertNull(suggestion.getPasspointConfig());
    }

    private static WifiEnterpriseConfig createEnterpriseConfig() {
        WifiEnterpriseConfig config = new WifiEnterpriseConfig();
        config.setEapMethod(AKA);
        return config;
    }

    /**
     * Tests {@link android.net.wifi.WifiNetworkSuggestion.Builder} class.
     */
    public void testBuilderWithWpa2Enterprise() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        WifiEnterpriseConfig enterpriseConfig = createEnterpriseConfig();
        WifiNetworkSuggestion suggestion =
                createBuilderWithCommonParams()
                        .setWpa2EnterpriseConfig(enterpriseConfig)
                        .build();
        validateCommonParams(suggestion);
        assertNull(suggestion.getPassphrase());
        assertNotNull(suggestion.getEnterpriseConfig());
        assertEquals(enterpriseConfig.getEapMethod(),
                suggestion.getEnterpriseConfig().getEapMethod());
        assertNull(suggestion.getPasspointConfig());
    }

    /**
     * Tests {@link android.net.wifi.WifiNetworkSuggestion.Builder} class.
     */
    public void testBuilderWithWpa3Enterprise() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        WifiEnterpriseConfig enterpriseConfig = createEnterpriseConfig();
        WifiNetworkSuggestion suggestion =
                createBuilderWithCommonParams()
                        .setWpa3EnterpriseConfig(enterpriseConfig)
                        .build();
        validateCommonParams(suggestion);
        assertNull(suggestion.getPassphrase());
        assertNotNull(suggestion.getEnterpriseConfig());
        assertEquals(enterpriseConfig.getEapMethod(),
                suggestion.getEnterpriseConfig().getEapMethod());
        assertNull(suggestion.getPasspointConfig());
    }

    /**
     * Tests {@link android.net.wifi.WifiNetworkSuggestion.Builder} class.
     */
    public void testBuilderWithWapiEnterprise() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        WifiEnterpriseConfig enterpriseConfig = new WifiEnterpriseConfig();
        enterpriseConfig.setEapMethod(WAPI_CERT);
        WifiNetworkSuggestion suggestion =
                createBuilderWithCommonParams()
                        .setWapiEnterpriseConfig(enterpriseConfig)
                        .build();
        validateCommonParams(suggestion);
        assertNull(suggestion.getPassphrase());
        assertNotNull(suggestion.getEnterpriseConfig());
        assertEquals(enterpriseConfig.getEapMethod(),
                suggestion.getEnterpriseConfig().getEapMethod());
        assertNull(suggestion.getPasspointConfig());
    }

    /**
     * Helper function for creating a {@link PasspointConfiguration} for testing.
     *
     * @return {@link PasspointConfiguration}
     */
    private static PasspointConfiguration createPasspointConfig() {
        HomeSp homeSp = new HomeSp();
        homeSp.setFqdn("fqdn");
        homeSp.setFriendlyName("friendly name");
        homeSp.setRoamingConsortiumOis(new long[] {0x55, 0x66});
        Credential cred = new Credential();
        cred.setRealm("realm");
        cred.setUserCredential(null);
        cred.setCertCredential(null);
        cred.setSimCredential(new Credential.SimCredential());
        cred.getSimCredential().setImsi("1234*");
        cred.getSimCredential().setEapType(23); // EAP-AKA
        cred.setCaCertificate(null);
        cred.setClientCertificateChain(null);
        cred.setClientPrivateKey(null);
        PasspointConfiguration config = new PasspointConfiguration();
        config.setHomeSp(homeSp);
        config.setCredential(cred);
        return config;
    }

    /**
     * Tests {@link android.net.wifi.WifiNetworkSuggestion.Builder} class.
     */
    public void testBuilderWithPasspointConfig() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        PasspointConfiguration passpointConfig = createPasspointConfig();
        WifiNetworkSuggestion suggestion =
                createBuilderWithCommonParams(true)
                        .setPasspointConfig(passpointConfig)
                        .build();
        validateCommonParams(suggestion, true);
        assertNull(suggestion.getPassphrase());
        assertEquals(passpointConfig, suggestion.getPasspointConfig());
    }
}
