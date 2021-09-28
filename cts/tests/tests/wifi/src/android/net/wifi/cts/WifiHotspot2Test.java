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

import static android.net.wifi.WifiConfiguration.METERED_OVERRIDE_NONE;

import android.net.Uri;
import android.net.wifi.hotspot2.OsuProvider;
import android.net.wifi.hotspot2.PasspointConfiguration;
import android.net.wifi.hotspot2.pps.Credential;
import android.net.wifi.hotspot2.pps.HomeSp;
import android.test.AndroidTestCase;
import android.text.TextUtils;

import java.lang.reflect.Constructor;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.cert.CertificateEncodingException;
import java.security.cert.X509Certificate;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

public class WifiHotspot2Test extends WifiJUnit3TestBase {
    static final int SIM_CREDENTIAL = 0;
    static final int USER_CREDENTIAL = 1;
    static final int CERT_CREDENTIAL = 2;
    private static final String TEST_SSID = "TEST SSID";
    private static final String TEST_FRIENDLY_NAME = "Friendly Name";
    private static final Map<String, String> TEST_FRIENDLY_NAMES =
            new HashMap<String, String>() {
                {
                    put("en", TEST_FRIENDLY_NAME);
                    put("kr", TEST_FRIENDLY_NAME + 2);
                    put("jp", TEST_FRIENDLY_NAME + 3);
                }
            };
    private static final String TEST_SERVICE_DESCRIPTION = "Dummy Service";
    private static final Uri TEST_SERVER_URI = Uri.parse("https://test.com");
    private static final String TEST_NAI = "test.access.com";
    private static final List<Integer> TEST_METHOD_LIST =
            Arrays.asList(1 /* METHOD_SOAP_XML_SPP */);
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
        super.tearDown();
    }

    /**
     * Tests {@link PasspointConfiguration#getMeteredOverride()} method.
     * <p>
     * Test default value
     */
    public void testGetMeteredOverride() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        PasspointConfiguration passpointConfiguration = new PasspointConfiguration();
        assertEquals(METERED_OVERRIDE_NONE, passpointConfiguration.getMeteredOverride());
    }

    /**
     * Tests {@link PasspointConfiguration#getSubscriptionExpirationTimeMillis()} method.
     * <p>
     * Test default value
     */
    public void testGetSubscriptionExpirationTimeMillis() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        PasspointConfiguration passpointConfiguration = new PasspointConfiguration();
        assertEquals(Long.MIN_VALUE,
                passpointConfiguration.getSubscriptionExpirationTimeMillis());
    }

    /**
     * Tests {@link PasspointConfiguration#getUniqueId()} method.
     * <p>
     * Test unique identifier is not null
     */
    public void testGetUniqueId() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }

        // Create a configuration and make sure the unique ID is not null
        PasspointConfiguration passpointConfiguration1 = createConfig(SIM_CREDENTIAL, "123456*",
                18 /* EAP_SIM */);
        String uniqueId1 = passpointConfiguration1.getUniqueId();
        assertNotNull(uniqueId1);

        // Create another configuration and make sure the unique ID is not null
        PasspointConfiguration passpointConfiguration2 = createConfig(SIM_CREDENTIAL, "567890*",
                23 /* EAP_AKA */);
        String uniqueId2 = passpointConfiguration2.getUniqueId();
        assertNotNull(uniqueId2);

        // Make sure the IDs are not equal
        assertFalse(uniqueId1.equals(uniqueId2));

        passpointConfiguration2 = createConfig(USER_CREDENTIAL);
        assertFalse(uniqueId1.equals(passpointConfiguration2.getUniqueId()));

        passpointConfiguration2 = createConfig(CERT_CREDENTIAL);
        assertFalse(uniqueId1.equals(passpointConfiguration2.getUniqueId()));
    }

    /**
     * Tests {@link PasspointConfiguration#isAutojoinEnabled()} method.
     * <p>
     * Test default value
     */
    public void testIsAutojoinEnabled() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        PasspointConfiguration passpointConfiguration = new PasspointConfiguration();
        assertTrue(passpointConfiguration.isAutojoinEnabled());
    }

    /**
     * Tests {@link PasspointConfiguration#isMacRandomizationEnabled()} method.
     * <p>
     * Test default value
     */
    public void testIsMacRandomizationEnabled() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        PasspointConfiguration passpointConfiguration = new PasspointConfiguration();
        assertTrue(passpointConfiguration.isMacRandomizationEnabled());
    }

    /**
     * Tests {@link PasspointConfiguration#isOsuProvisioned()} method.
     * <p>
     * Test default value
     */
    public void testIsOsuProvisioned() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        PasspointConfiguration passpointConfiguration = createConfig(USER_CREDENTIAL);
        assertFalse(passpointConfiguration.isOsuProvisioned());
    }

    /**
     * Tests {@link PasspointConfiguration#PasspointConfiguration(PasspointConfiguration)} method.
     * <p>
     * Test the PasspointConfiguration copy constructor
     */
    public void testPasspointConfigurationCopyConstructor() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        PasspointConfiguration passpointConfiguration = createConfig(USER_CREDENTIAL);
        PasspointConfiguration copyOfPasspointConfiguration =
                new PasspointConfiguration(passpointConfiguration);
        assertEquals(passpointConfiguration, copyOfPasspointConfiguration);
    }

    /**
     * Tests {@link HomeSp#HomeSp(HomeSp)} method.
     * <p>
     * Test the HomeSp copy constructor
     */
    public void testHomeSpCopyConstructor() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        HomeSp homeSp = createHomeSp();
        HomeSp copyOfHomeSp = new HomeSp(homeSp);
        assertEquals(copyOfHomeSp, homeSp);
    }

    /**
     * Tests {@link Credential#Credential(Credential)} method.
     * <p>
     * Test the Credential copy constructor
     */
    public void testCredentialCopyConstructor() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        Credential credential = createCredentialWithSimCredential("123456*", 18 /* EAP_SIM */);
        Credential copyOfCredential = new Credential(credential);
        assertEquals(copyOfCredential, credential);
    }

    /**
     * Tests {@link Credential.UserCredential#UserCredential(Credential.UserCredential)} method.
     * <p>
     * Test the Credential.UserCredential copy constructor
     */
    public void testUserCredentialCopyConstructor() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        Credential.UserCredential userCredential = new Credential.UserCredential();
        userCredential.setUsername("username");
        userCredential.setPassword("password");
        userCredential.setEapType(21 /* EAP_TTLS */);
        userCredential.setNonEapInnerMethod("MS-CHAP");

        Credential.UserCredential copyOfUserCredential =
                new Credential.UserCredential(userCredential);
        assertEquals(copyOfUserCredential, userCredential);
    }

    /**
     * Tests
     * {@link Credential.CertificateCredential#CertificateCredential(Credential.CertificateCredential)}
     * method.
     * <p>
     * Test the Credential.CertificateCredential copy constructor
     */
    public void testCertCredentialCopyConstructor() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        Credential.CertificateCredential certCredential = new Credential.CertificateCredential();
        certCredential.setCertType("x509v3");

        Credential.CertificateCredential copyOfCertificateCredential =
                new Credential.CertificateCredential(certCredential);
        assertEquals(copyOfCertificateCredential, certCredential);
    }

    /**
     * Tests {@link Credential.SimCredential#SimCredential(Credential.SimCredential)} method.
     * <p>
     * Test the Credential.SimCredential copy constructor
     */
    public void testSimCredentialCopyConstructor() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        Credential.SimCredential simCredential = new Credential.SimCredential();
        simCredential.setImsi("1234*");
        simCredential.setEapType(18/* EAP_SIM */);

        Credential.SimCredential copyOfSimCredential = new Credential.SimCredential(simCredential);
        assertEquals(copyOfSimCredential, simCredential);
    }

    /**
     * Tests {@link Credential#getCaCertificate()}  method.
     * <p>
     * Test that getting a set certificate produces the same value
     */
    public void testCredentialGetCertificate() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        Credential credential = new Credential();
        credential.setCaCertificate(FakeKeys.CA_CERT0);

        assertEquals(FakeKeys.CA_CERT0, credential.getCaCertificate());
    }

    /**
     * Tests {@link Credential#getClientCertificateChain()} and {@link
     * Credential#setCaCertificates(X509Certificate[])} methods.
     * <p>
     * Test that getting a set client certificate chain produces the same value
     */
    public void testCredentialClientCertificateChain() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        Credential credential = new Credential();
        X509Certificate[] certificates = new X509Certificate[]{FakeKeys.CLIENT_CERT};
        credential.setClientCertificateChain(certificates);

        assertTrue(Arrays.equals(certificates, credential.getClientCertificateChain()));
    }

    /**
     * Tests {@link Credential#getClientPrivateKey()} and
     * {@link Credential#setClientPrivateKey(PrivateKey)}
     * methods.
     * <p>
     * Test that getting a set client private key produces the same value
     */
    public void testCredentialSetGetClientPrivateKey() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        Credential credential = new Credential();
        credential.setClientPrivateKey(FakeKeys.RSA_KEY1);

        assertEquals(FakeKeys.RSA_KEY1, credential.getClientPrivateKey());
    }

    /**
     * Tests {@link Credential#getClientPrivateKey()} and
     * {@link Credential#setClientPrivateKey(PrivateKey)}
     * methods.
     * <p>
     * Test that getting a set client private key produces the same value
     */
    public void testCredentialGetClientPrivateKey() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        Credential credential = new Credential();
        credential.setClientPrivateKey(FakeKeys.RSA_KEY1);

        assertEquals(FakeKeys.RSA_KEY1, credential.getClientPrivateKey());
    }

    private static PasspointConfiguration createConfig(int type) throws Exception {
        return createConfig(type, "123456*", 18 /* EAP_SIM */);
    }

    private static PasspointConfiguration createConfig(int type, String imsi, int eapType)
            throws Exception {
        PasspointConfiguration config = new PasspointConfiguration();
        config.setHomeSp(createHomeSp());
        switch (type) {
            default:
            case SIM_CREDENTIAL:
                config.setCredential(
                        createCredentialWithSimCredential(imsi, eapType));
                break;
            case USER_CREDENTIAL:
                config.setCredential(createCredentialWithUserCredential());
                break;
            case CERT_CREDENTIAL:
                config.setCredential(createCredentialWithCertificateCredential());
                break;
        }

        return config;
    }

    /**
     * Helper function for generating HomeSp for testing.
     *
     * @return {@link HomeSp}
     */
    private static HomeSp createHomeSp() {
        HomeSp homeSp = new HomeSp();
        homeSp.setFqdn("test.com");
        homeSp.setFriendlyName("friendly name");
        homeSp.setRoamingConsortiumOis(new long[]{0x55, 0x66});
        return homeSp;
    }

    /**
     * Helper function for generating Credential for testing.
     *
     * @param userCred               Instance of UserCredential
     * @param certCred               Instance of CertificateCredential
     * @param simCred                Instance of SimCredential
     * @param clientCertificateChain Chain of client certificates
     * @param clientPrivateKey       Client private key
     * @param caCerts                CA certificates
     * @return {@link Credential}
     */
    private static Credential createCredential(Credential.UserCredential userCred,
            Credential.CertificateCredential certCred,
            Credential.SimCredential simCred,
            X509Certificate[] clientCertificateChain, PrivateKey clientPrivateKey,
            X509Certificate... caCerts) {
        Credential cred = new Credential();
        cred.setRealm("realm");
        cred.setUserCredential(userCred);
        cred.setCertCredential(certCred);
        cred.setSimCredential(simCred);
        return cred;
    }

    /**
     * Helper function for generating certificate credential for testing.
     *
     * @return {@link Credential}
     */
    private static Credential createCredentialWithCertificateCredential()
            throws NoSuchAlgorithmException, CertificateEncodingException {
        Credential.CertificateCredential certCred = new Credential.CertificateCredential();
        certCred.setCertType("x509v3");
        certCred.setCertSha256Fingerprint(
                MessageDigest.getInstance("SHA-256").digest(
                        FakeKeys.CLIENT_CERT.getEncoded()));
        return createCredential(null, certCred, null, new X509Certificate[]{
                        FakeKeys.CLIENT_CERT},
                FakeKeys.RSA_KEY1, FakeKeys.CA_CERT0,
                FakeKeys.CA_CERT1);
    }

    /**
     * Helper function for generating SIM credential for testing.
     *
     * @return {@link Credential}
     */
    private static Credential createCredentialWithSimCredential(String imsi, int eapType) {
        Credential.SimCredential simCred = new Credential.SimCredential();
        simCred.setImsi(imsi);
        simCred.setEapType(eapType);
        return createCredential(null, null, simCred, null, null, (X509Certificate[]) null);
    }

    /**
     * Helper function for generating user credential for testing.
     *
     * @return {@link Credential}
     */
    private static Credential createCredentialWithUserCredential() {
        Credential.UserCredential userCred = new Credential.UserCredential();
        userCred.setUsername("username");
        userCred.setPassword("password");
        userCred.setEapType(21 /* EAP_TTLS */);
        userCred.setNonEapInnerMethod("MS-CHAP");
        return createCredential(userCred, null, null, null, null,
                FakeKeys.CA_CERT0);
    }

    /**
     * Tests {@link OsuProvider#getFriendlyName()} and {@link OsuProvider#getServerUri()} methods.
     * <p>
     * Test that getting a set friendly name and server URI produces the same value
     */
    public void testOsuProviderGetters() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }

        // Using Java reflection to construct an OsuProvider instance because its constructor is
        // hidden and not available to apps.
        Class<?> osuProviderClass = Class.forName("android.net.wifi.hotspot2.OsuProvider");
        Constructor<?> osuProviderClassConstructor = osuProviderClass.getConstructor(String.class,
                Map.class, String.class, Uri.class, String.class, List.class);

        OsuProvider osuProvider = (OsuProvider) osuProviderClassConstructor.newInstance(TEST_SSID,
                TEST_FRIENDLY_NAMES, TEST_SERVICE_DESCRIPTION, TEST_SERVER_URI, TEST_NAI,
                TEST_METHOD_LIST);
        String lang = Locale.getDefault().getLanguage();
        String friendlyName = TEST_FRIENDLY_NAMES.get(lang);
        if (TextUtils.isEmpty(friendlyName)) {
            friendlyName = TEST_FRIENDLY_NAMES.get("en");
        }
        assertEquals(friendlyName, osuProvider.getFriendlyName());
        assertEquals(TEST_SERVER_URI, osuProvider.getServerUri());
    }
}
