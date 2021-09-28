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
 * limitations under the License
 */

package android.net.wifi.hotspot2;

import static android.net.wifi.WifiConfiguration.METERED_OVERRIDE_NONE;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.net.wifi.EAPConstants;
import android.net.wifi.FakeKeys;
import android.net.wifi.hotspot2.pps.Credential;
import android.net.wifi.hotspot2.pps.HomeSp;
import android.os.Parcel;

import androidx.test.filters.SmallTest;

import org.junit.Test;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.cert.CertificateEncodingException;
import java.security.cert.X509Certificate;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

/**
 * Unit tests for {@link android.net.wifi.hotspot2.PasspointConfiguration}.
 */
@SmallTest
public class PasspointConfigurationTest {
    private static final int MAX_URL_BYTES = 1023;
    private static final int CERTIFICATE_FINGERPRINT_BYTES = 32;

    /**
     * Verify parcel write and read consistency for the given configuration.
     *
     * @param writeConfig The configuration to verify
     * @throws Exception
     */
    private static void verifyParcel(PasspointConfiguration writeConfig) throws Exception {
        Parcel parcel = Parcel.obtain();
        writeConfig.writeToParcel(parcel, 0);

        parcel.setDataPosition(0);    // Rewind data position back to the beginning for read.
        PasspointConfiguration readConfig =
                PasspointConfiguration.CREATOR.createFromParcel(parcel);
        assertTrue(readConfig.equals(writeConfig));
    }

    /**
     * Verify parcel read/write for a default configuration.
     *
     * @throws Exception
     */
    @Test
    public void verifyParcelWithDefault() throws Exception {
        verifyParcel(new PasspointConfiguration());
    }

    /**
     * Verify parcel read/write for a configuration that contained the full configuration.
     *
     * @throws Exception
     */
    @Test
    public void verifyParcelWithFullConfiguration() throws Exception {
        verifyParcel(PasspointTestUtils.createConfig());
    }

    /**
     * Verify parcel read/write for a configuration that doesn't contain a list of service names.
     *
     * @throws Exception
     */
    @Test
    public void verifyParcelWithoutServiceNames() throws Exception {
        PasspointConfiguration config = PasspointTestUtils.createConfig();
        config.setServiceFriendlyNames(null);
        verifyParcel(config);
    }

    /**
     * Verify parcel read/write for a configuration that doesn't contain HomeSP.
     *
     * @throws Exception
     */
    @Test
    public void verifyParcelWithoutHomeSP() throws Exception {
        PasspointConfiguration config = PasspointTestUtils.createConfig();
        config.setHomeSp(null);
        verifyParcel(config);
    }

    /**
     * Verify parcel read/write for a configuration that doesn't contain Credential.
     *
     * @throws Exception
     */
    @Test
    public void verifyParcelWithoutCredential() throws Exception {
        PasspointConfiguration config = PasspointTestUtils.createConfig();
        config.setCredential(null);
        verifyParcel(config);
    }

    /**
     * Verify parcel read/write for a configuration that doesn't contain Policy.
     *
     * @throws Exception
     */
    @Test
    public void verifyParcelWithoutPolicy() throws Exception {
        PasspointConfiguration config = PasspointTestUtils.createConfig();
        config.setPolicy(null);
        verifyParcel(config);
    }

    /**
     * Verify parcel read/write for a configuration that doesn't contain subscription update.
     *
     * @throws Exception
     */
    @Test
    public void verifyParcelWithoutSubscriptionUpdate() throws Exception {
        PasspointConfiguration config = PasspointTestUtils.createConfig();
        config.setSubscriptionUpdate(null);
        verifyParcel(config);
    }

    /**
     * Verify parcel read/write for a configuration that doesn't contain trust root certificate
     * list.
     *
     * @throws Exception
     */
    @Test
    public void verifyParcelWithoutTrustRootCertList() throws Exception {
        PasspointConfiguration config = PasspointTestUtils.createConfig();
        config.setTrustRootCertList(null);
        verifyParcel(config);
    }

    /**
     * Verify parcel read/write for a configuration that doesn't contain AAA server trusted names
     * list.
     *
     * @throws Exception
     */
    @Test
    public void verifyParcelWithoutAaaServerTrustedNames() throws Exception {
        PasspointConfiguration config = PasspointTestUtils.createConfig();
        config.setAaaServerTrustedNames(null);
        verifyParcel(config);
    }

    /**
     * Verify that a default/empty configuration is invalid.
     *
     * @throws Exception
     */
    @Test
    public void validateDefaultConfig() throws Exception {
        PasspointConfiguration config = new PasspointConfiguration();

        assertFalse(config.validate());
        assertFalse(config.validateForR2());
        assertTrue(config.isAutojoinEnabled());
        assertTrue(config.isMacRandomizationEnabled());
        assertTrue(config.getMeteredOverride() == METERED_OVERRIDE_NONE);
    }

    /**
     * Verify that a configuration containing all fields is valid for R1/R2.
     *
     * @throws Exception
     */
    @Test
    public void validateFullConfig() throws Exception {
        PasspointConfiguration config = PasspointTestUtils.createConfig();

        assertTrue(config.validate());
        assertFalse(config.isOsuProvisioned());
    }

    /**
     * Verify that a configuration containing all fields except for UpdateIdentifier is valid for
     * R1, but invalid for R2.
     *
     * @throws Exception
     */
    @Test
    public void validateFullConfigWithoutUpdateIdentifier() throws Exception {
        PasspointConfiguration config = PasspointTestUtils.createConfig();
        config.setUpdateIdentifier(Integer.MIN_VALUE);

        assertTrue(config.validate());
        assertFalse(config.validateForR2());
    }

    /**
     * Verify that a configuration without Credential is invalid.
     *
     * @throws Exception
     */
    @Test
    public void validateConfigWithoutCredential() throws Exception {
        PasspointConfiguration config = PasspointTestUtils.createConfig();
        config.setCredential(null);

        assertFalse(config.validate());
        assertFalse(config.validateForR2());
    }

    /**
     * Verify that a configuration without HomeSP is invalid.
     *
     * @throws Exception
     */
    @Test
    public void validateConfigWithoutHomeSp() throws Exception {
        PasspointConfiguration config = PasspointTestUtils.createConfig();
        config.setHomeSp(null);

        assertFalse(config.validate());
        assertFalse(config.validateForR2());
    }

    /**
     * Verify that a configuration without Policy is valid, since Policy configurations
     * are optional for R1 and R2.
     *
     * @throws Exception
     */
    @Test
    public void validateConfigWithoutPolicy() throws Exception {
        PasspointConfiguration config = PasspointTestUtils.createConfig();
        config.setPolicy(null);

        assertTrue(config.validate());
    }

    /**
     * Verify that a configuration without subscription update is valid for R1 and invalid for R2,
     * since subscription update configuration is only applicable for R2.
     *
     * @throws Exception
     */
    @Test
    public void validateConfigWithoutSubscriptionUpdate() throws Exception {
        PasspointConfiguration config = PasspointTestUtils.createConfig();
        config.setSubscriptionUpdate(null);

        assertTrue(config.validate());
        assertFalse(config.validateForR2());
    }

    /**
     * Verify that a configuration without AAA server trusted names is valid for R1 and R2,
     * since AAA server trusted names are optional for R1 and R2.
     *
     * @throws Exception
     */
    @Test
    public void validateConfigWithoutAaaServerTrustedNames() throws Exception {
        PasspointConfiguration config = PasspointTestUtils.createConfig();
        config.setAaaServerTrustedNames(null);

        assertTrue(config.validate());
    }

    /**
     * Verify that a configuration with a trust root certificate URL exceeding the max size
     * is invalid.
     *
     * @throws Exception
     */
    @Test
    public void validateConfigWithInvalidTrustRootCertUrl() throws Exception {
        PasspointConfiguration config = PasspointTestUtils.createConfig();
        byte[] rawUrlBytes = new byte[MAX_URL_BYTES + 1];
        Map<String, byte[]> trustRootCertList = new HashMap<>();
        Arrays.fill(rawUrlBytes, (byte) 'a');
        trustRootCertList.put(new String(rawUrlBytes, StandardCharsets.UTF_8),
                new byte[CERTIFICATE_FINGERPRINT_BYTES]);
        config.setTrustRootCertList(trustRootCertList);

        assertFalse(config.validate());

        trustRootCertList = new HashMap<>();
        trustRootCertList.put(null, new byte[CERTIFICATE_FINGERPRINT_BYTES]);
        config.setTrustRootCertList(trustRootCertList);

        assertFalse(config.validate());
        assertFalse(config.validateForR2());
    }

    /**
     * Verify that a configuration with an invalid trust root certificate fingerprint is invalid.
     *
     * @throws Exception
     */
    @Test
    public void validateConfigWithInvalidTrustRootCertFingerprint() throws Exception {
        PasspointConfiguration config = PasspointTestUtils.createConfig();
        Map<String, byte[]> trustRootCertList = new HashMap<>();
        trustRootCertList.put("test.cert.com", new byte[CERTIFICATE_FINGERPRINT_BYTES + 1]);
        config.setTrustRootCertList(trustRootCertList);
        assertFalse(config.validate());

        trustRootCertList = new HashMap<>();
        trustRootCertList.put("test.cert.com", new byte[CERTIFICATE_FINGERPRINT_BYTES - 1]);
        config.setTrustRootCertList(trustRootCertList);
        assertFalse(config.validate());

        trustRootCertList = new HashMap<>();
        trustRootCertList.put("test.cert.com", null);
        config.setTrustRootCertList(trustRootCertList);
        assertFalse(config.validate());
        assertFalse(config.validateForR2());
    }

    /**
     * Verify that copy constructor works when pass in a null source.
     *
     * @throws Exception
     */
    @Test
    public void validateCopyConstructorWithNullSource() throws Exception {
        PasspointConfiguration copyConfig = new PasspointConfiguration(null);
        PasspointConfiguration defaultConfig = new PasspointConfiguration();
        assertTrue(copyConfig.equals(defaultConfig));
    }

    /**
     * Verify that copy constructor works when pass in a valid source.
     *
     * @throws Exception
     */
    @Test
    public void validateCopyConstructorWithValidSource() throws Exception {
        PasspointConfiguration sourceConfig = PasspointTestUtils.createConfig();
        PasspointConfiguration copyConfig = new PasspointConfiguration(sourceConfig);
        assertTrue(copyConfig.equals(sourceConfig));
    }

    /**
     * Verify that a configuration containing all fields is valid for R2.
     *
     * @throws Exception
     */
    @Test
    public void validateFullR2Config() throws Exception {
        PasspointConfiguration config = PasspointTestUtils.createR2Config();
        assertTrue(config.validate());
        assertTrue(config.validateForR2());
        assertTrue(config.isOsuProvisioned());
    }

    /**
     * Verify that the unique identifier generated is identical for two instances
     *
     * @throws Exception
     */
    @Test
    public void validateUniqueId() throws Exception {
        PasspointConfiguration config1 = PasspointTestUtils.createConfig();
        PasspointConfiguration config2 = PasspointTestUtils.createConfig();

        assertEquals(config1.getUniqueId(), config2.getUniqueId());
    }

    /**
     * Verify that the unique identifier generated is the same for two instances with different
     * HomeSp node but same FQDN
     *
     * @throws Exception
     */
    @Test
    public void validateUniqueIdDifferentHomeSpSameFqdn() throws Exception {
        PasspointConfiguration config1 = PasspointTestUtils.createConfig();

        // Modify config2's RCOIs and friendly name to a different set of values
        PasspointConfiguration config2 = PasspointTestUtils.createConfig();
        HomeSp homeSp = config2.getHomeSp();
        homeSp.setRoamingConsortiumOis(new long[] {0xaa, 0xbb});
        homeSp.setFriendlyName("Some other name");
        config2.setHomeSp(homeSp);

        assertEquals(config1.getUniqueId(), config2.getUniqueId());
    }

    /**
     * Verify that the unique identifier generated is different for two instances with the same
     * HomeSp node but different FQDN
     *
     * @throws Exception
     */
    @Test
    public void validateUniqueIdSameHomeSpDifferentFqdn() throws Exception {
        PasspointConfiguration config1 = PasspointTestUtils.createConfig();

        // Modify config2's FQDN to a different value
        PasspointConfiguration config2 = PasspointTestUtils.createConfig();
        HomeSp homeSp = config2.getHomeSp();
        homeSp.setFqdn("fqdn2.com");
        config2.setHomeSp(homeSp);

        assertNotEquals(config1.getUniqueId(), config2.getUniqueId());
    }

    /**
     * Verify that the unique identifier generated is different for two instances with different
     * SIM Credential node
     *
     * @throws Exception
     */
    @Test
    public void validateUniqueIdDifferentSimCredential() throws Exception {
        PasspointConfiguration config1 = PasspointTestUtils.createConfig();

        // Modify config2's realm and SIM credential to a different set of values
        PasspointConfiguration config2 = PasspointTestUtils.createConfig();
        Credential credential = config2.getCredential();
        credential.setRealm("realm2.example.com");
        credential.getSimCredential().setImsi("350460*");
        config2.setCredential(credential);

        assertNotEquals(config1.getUniqueId(), config2.getUniqueId());
    }

    /**
     * Verify that the unique identifier generated is different for two instances with different
     * Realm in the Credential node
     *
     * @throws Exception
     */
    @Test
    public void validateUniqueIdDifferentRealm() throws Exception {
        PasspointConfiguration config1 = PasspointTestUtils.createConfig();

        // Modify config2's realm to a different set of values
        PasspointConfiguration config2 = PasspointTestUtils.createConfig();
        Credential credential = config2.getCredential();
        credential.setRealm("realm2.example.com");
        config2.setCredential(credential);

        assertNotEquals(config1.getUniqueId(), config2.getUniqueId());
    }

    /**
     * Verify that the unique identifier generated is the same for two instances with different
     * password and same username in the User Credential node
     *
     * @throws Exception
     */
    @Test
    public void validateUniqueIdSameUserInUserCredential() throws Exception {
        PasspointConfiguration config1 = PasspointTestUtils.createConfig();
        Credential credential = createCredentialWithUserCredential("user", "passwd");
        config1.setCredential(credential);

        // Modify config2's Passpowrd to a different set of values
        PasspointConfiguration config2 = PasspointTestUtils.createConfig();
        credential = createCredentialWithUserCredential("user", "newpasswd");
        config2.setCredential(credential);

        assertEquals(config1.getUniqueId(), config2.getUniqueId());
    }

    /**
     * Verify that the unique identifier generated is different for two instances with different
     * username in the User Credential node
     *
     * @throws Exception
     */
    @Test
    public void validateUniqueIdDifferentUserCredential() throws Exception {
        PasspointConfiguration config1 = PasspointTestUtils.createConfig();
        Credential credential = createCredentialWithUserCredential("user", "passwd");
        config1.setCredential(credential);

        // Modify config2's username to a different value
        PasspointConfiguration config2 = PasspointTestUtils.createConfig();
        credential = createCredentialWithUserCredential("user2", "passwd");
        config2.setCredential(credential);

        assertNotEquals(config1.getUniqueId(), config2.getUniqueId());
    }

    /**
     * Verify that the unique identifier generated is different for two instances with different
     * Cert Credential node
     *
     * @throws Exception
     */
    @Test
    public void validateUniqueIdDifferentCertCredential() throws Exception {
        PasspointConfiguration config1 = PasspointTestUtils.createConfig();
        Credential credential = createCredentialWithCertificateCredential(true, true);
        config1.setCredential(credential);

        // Modify config2's cert credential to a different set of values
        PasspointConfiguration config2 = PasspointTestUtils.createConfig();
        credential = createCredentialWithCertificateCredential(false, false);
        config2.setCredential(credential);

        assertNotEquals(config1.getUniqueId(), config2.getUniqueId());
    }

    /**
     * Helper function for generating certificate credential for testing.
     *
     * @return {@link Credential}
     */
    private static Credential createCredentialWithCertificateCredential(Boolean useCaCert0,
            Boolean useCert0)
            throws NoSuchAlgorithmException, CertificateEncodingException {
        Credential.CertificateCredential certCred = new Credential.CertificateCredential();
        certCred.setCertType("x509v3");
        if (useCert0) {
            certCred.setCertSha256Fingerprint(
                    MessageDigest.getInstance("SHA-256").digest(FakeKeys.CLIENT_CERT.getEncoded()));
        } else {
            certCred.setCertSha256Fingerprint(MessageDigest.getInstance("SHA-256")
                    .digest(FakeKeys.CLIENT_SUITE_B_RSA3072_CERT.getEncoded()));
        }
        return createCredential(null, certCred, null, new X509Certificate[] {FakeKeys.CLIENT_CERT},
                FakeKeys.RSA_KEY1, useCaCert0 ? FakeKeys.CA_CERT0 : FakeKeys.CA_CERT1);
    }

    /**
     * Helper function for generating user credential for testing.
     *
     * @return {@link Credential}
     */
    private static Credential createCredentialWithUserCredential(String username, String password) {
        Credential.UserCredential userCred = new Credential.UserCredential();
        userCred.setUsername(username);
        userCred.setPassword(password);
        userCred.setMachineManaged(true);
        userCred.setAbleToShare(true);
        userCred.setSoftTokenApp("TestApp");
        userCred.setEapType(EAPConstants.EAP_TTLS);
        userCred.setNonEapInnerMethod("MS-CHAP");
        return createCredential(userCred, null, null, null, null, FakeKeys.CA_CERT0);
    }

    /**
     * Helper function for generating Credential for testing.
     *
     * @param userCred Instance of UserCredential
     * @param certCred Instance of CertificateCredential
     * @param simCred Instance of SimCredential
     * @param clientCertificateChain Chain of client certificates
     * @param clientPrivateKey Client private key
     * @param caCerts CA certificates
     * @return {@link Credential}
     */
    private static Credential createCredential(Credential.UserCredential userCred,
            Credential.CertificateCredential certCred,
            Credential.SimCredential simCred,
            X509Certificate[] clientCertificateChain, PrivateKey clientPrivateKey,
            X509Certificate... caCerts) {
        Credential cred = new Credential();
        cred.setCreationTimeInMillis(123455L);
        cred.setExpirationTimeInMillis(2310093L);
        cred.setRealm("realm");
        cred.setCheckAaaServerCertStatus(true);
        cred.setUserCredential(userCred);
        cred.setCertCredential(certCred);
        cred.setSimCredential(simCred);
        if (caCerts != null && caCerts.length == 1) {
            cred.setCaCertificate(caCerts[0]);
        } else {
            cred.setCaCertificates(caCerts);
        }
        cred.setClientCertificateChain(clientCertificateChain);
        cred.setClientPrivateKey(clientPrivateKey);
        return cred;
    }

    /**
     * Verify that the unique identifier API generates an exception if HomeSP is not initialized.
     *
     * @throws Exception
     */
    @Test (expected = IllegalStateException.class)
    public void validateUniqueIdExceptionWithEmptyHomeSp() throws Exception {
        PasspointConfiguration config = PasspointTestUtils.createConfig();
        config.setHomeSp(null);
        String uniqueId = config.getUniqueId();
    }

    /**
     * Verify that the unique identifier API generates an exception if Credential is not
     * initialized.
     *
     * @throws Exception
     */
    @Test (expected = IllegalStateException.class)
    public void validateUniqueIdExceptionWithEmptyCredential() throws Exception {
        PasspointConfiguration config = PasspointTestUtils.createConfig();
        config.setCredential(null);
        String uniqueId = config.getUniqueId();
    }
}
