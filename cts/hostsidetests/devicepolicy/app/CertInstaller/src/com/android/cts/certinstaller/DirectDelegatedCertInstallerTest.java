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

package com.android.cts.certinstaller;

import static com.google.common.truth.Truth.assertWithMessage;
import static com.google.common.truth.Truth.assertThat;

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.security.AttestedKeyPair;
import android.security.KeyChain;
import android.security.KeyChainException;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.telephony.TelephonyManager;
import android.test.InstrumentationTestCase;
import android.util.Base64;
import android.util.Base64InputStream;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.security.KeyFactory;
import java.security.KeyStore;
import java.security.PrivateKey;
import java.security.cert.Certificate;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.spec.PKCS8EncodedKeySpec;
import java.util.List;

/*
 * Tests the delegated certificate installer functionality.
 *
 * This class is configured as DelegatedCertInstaller by the DelegatedCertInstallerHelper and is
 * invoked directly from the host class,
 * DeviceAndProfileOwnerTest#testDelegatedCertInstallerDirectly.
 *
 * TODO: this class is missing more functionality of the DelegatedCertInstaller tests.
 * When this class is done then the DelegatedCertInstallerTest can be deleted.
 */
public class DirectDelegatedCertInstallerTest extends InstrumentationTestCase {
    // Content from cacert.pem
    private static final String TEST_CA =
            "-----BEGIN CERTIFICATE-----\n" +
                    "MIIDXTCCAkWgAwIBAgIJAK9Tl/F9V8kSMA0GCSqGSIb3DQEBCwUAMEUxCzAJBgNV\n" +
                    "BAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX\n" +
                    "aWRnaXRzIFB0eSBMdGQwHhcNMTUwMzA2MTczMjExWhcNMjUwMzAzMTczMjExWjBF\n" +
                    "MQswCQYDVQQGEwJBVTETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UECgwYSW50\n" +
                    "ZXJuZXQgV2lkZ2l0cyBQdHkgTHRkMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIB\n" +
                    "CgKCAQEAvItOutsE75WBTgTyNAHt4JXQ3JoseaGqcC3WQij6vhrleWi5KJ0jh1/M\n" +
                    "Rpry7Fajtwwb4t8VZa0NuM2h2YALv52w1xivql88zce/HU1y7XzbXhxis9o6SCI+\n" +
                    "oVQSbPeXRgBPppFzBEh3ZqYTVhAqw451XhwdA4Aqs3wts7ddjwlUzyMdU44osCUg\n" +
                    "kVg7lfPf9sTm5IoHVcfLSCWH5n6Nr9sH3o2ksyTwxuOAvsN11F/a0mmUoPciYPp+\n" +
                    "q7DzQzdi7akRG601DZ4YVOwo6UITGvDyuAAdxl5isovUXqe6Jmz2/myTSpAKxGFs\n" +
                    "jk9oRoG6WXWB1kni490GIPjJ1OceyQIDAQABo1AwTjAdBgNVHQ4EFgQUH1QIlPKL\n" +
                    "p2OQ/AoLOjKvBW4zK3AwHwYDVR0jBBgwFoAUH1QIlPKLp2OQ/AoLOjKvBW4zK3Aw\n" +
                    "DAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAcMi4voMMJHeQLjtq8Oky\n" +
                    "Azpyk8moDwgCd4llcGj7izOkIIFqq/lyqKdtykVKUWz2bSHO5cLrtaOCiBWVlaCV\n" +
                    "DYAnnVLM8aqaA6hJDIfaGs4zmwz0dY8hVMFCuCBiLWuPfiYtbEmjHGSmpQTG6Qxn\n" +
                    "ZJlaK5CZyt5pgh5EdNdvQmDEbKGmu0wpCq9qjZImwdyAul1t/B0DrsWApZMgZpeI\n" +
                    "d2od0VBrCICB1K4p+C51D93xyQiva7xQcCne+TAnGNy9+gjQ/MyR8MRpwRLv5ikD\n" +
                    "u0anJCN8pXo6IMglfMAsoton1J6o5/ae5uhC6caQU8bNUsCK570gpNfjkzo6rbP0\n" +
                    "wQ==\n" +
                    "-----END CERTIFICATE-----";
    // Content from userkey.pem without the private key header and footer.
    private static final String TEST_KEY =
            "MIICdwIBADANBgkqhkiG9w0BAQEFAASCAmEwggJdAgEAAoGBALCYprGsTU+5L3KM\n" +
                    "fhkm0gXM2xjGUH+543YLiMPGVr3eVS7biue1/tQlL+fJsw3rqsPKJe71RbVWlpqU\n" +
                    "mhegxG4s3IvGYVB0KZoRIjDKmnnvlx6nngL2ZJ8O27U42pHsw4z4MKlcQlWkjL3T\n" +
                    "9sV6zW2Wzri+f5mvzKjhnArbLktHAgMBAAECgYBlfVVPhtZnmuXJzzQpAEZzTugb\n" +
                    "tN1OimZO0RIocTQoqj4KT+HkiJOLGFQPwbtFpMre+q4SRqNpM/oZnI1yRtKcCmIc\n" +
                    "mZgkwJ2k6pdSxqO0ofxFFTdT9czJ3rCnqBHy1g6BqUQFXT4olcygkxUpKYUwzlz1\n" +
                    "oAl487CoPxyr4sVEAQJBANwiUOHcdGd2RoRILDzw5WOXWBoWPOKzX/K9wt0yL+mO\n" +
                    "wlFNFSymqo9eLheHcEq/VD9qK9rT700dCewJfWj6+bECQQDNXmWNYIxGii5NJilT\n" +
                    "OBOHiMD/F0NE178j+/kmacbhDJwpkbLYXaP8rW4+Iswrm4ORJ59lvjNuXaZ28+sx\n" +
                    "fFp3AkA6Z7Bl/IO135+eATgbgx6ZadIqObQ1wbm3Qbmtzl7/7KyJvZXcnuup1icM\n" +
                    "fxa//jtwB89S4+Ad6ZJ0WaA4dj5BAkEAuG7V9KmIULE388EZy8rIfyepa22Q0/qN\n" +
                    "hdt8XasRGHsio5Jdc0JlSz7ViqflhCQde/aBh/XQaoVgQeO8jKyI8QJBAJHekZDj\n" +
                    "WA0w1RsBVVReN1dVXgjm1CykeAT8Qx8TUmBUfiDX6w6+eGQjKtS7f4KC2IdRTV6+\n" +
                    "bDzDoHBChHNC9ms=\n";

    // Content from usercert.pem without the header and footer.
    private static final String TEST_CERT =
            "MIIDEjCCAfqgAwIBAgIBATANBgkqhkiG9w0BAQsFADBFMQswCQYDVQQGEwJBVTET\n" +
                    "MBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UECgwYSW50ZXJuZXQgV2lkZ2l0cyBQ\n" +
                    "dHkgTHRkMB4XDTE1MDUwMTE2NTQwNVoXDTI1MDQyODE2NTQwNVowWzELMAkGA1UE\n" +
                    "BhMCQVUxEzARBgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoMGEludGVybmV0IFdp\n" +
                    "ZGdpdHMgUHR5IEx0ZDEUMBIGA1UEAwwLY2xpZW50IGNlcnQwgZ8wDQYJKoZIhvcN\n" +
                    "AQEBBQADgY0AMIGJAoGBALCYprGsTU+5L3KMfhkm0gXM2xjGUH+543YLiMPGVr3e\n" +
                    "VS7biue1/tQlL+fJsw3rqsPKJe71RbVWlpqUmhegxG4s3IvGYVB0KZoRIjDKmnnv\n" +
                    "lx6nngL2ZJ8O27U42pHsw4z4MKlcQlWkjL3T9sV6zW2Wzri+f5mvzKjhnArbLktH\n" +
                    "AgMBAAGjezB5MAkGA1UdEwQCMAAwLAYJYIZIAYb4QgENBB8WHU9wZW5TU0wgR2Vu\n" +
                    "ZXJhdGVkIENlcnRpZmljYXRlMB0GA1UdDgQWBBQ8GL+jKSarvTn9fVNA2AzjY7qq\n" +
                    "gjAfBgNVHSMEGDAWgBRzBBA5sNWyT/fK8GrhN3tOqO5tgjANBgkqhkiG9w0BAQsF\n" +
                    "AAOCAQEAgwQEd2bktIDZZi/UOwU1jJUgGq7NiuBDPHcqgzjxhGFLQ8SQAAP3v3PR\n" +
                    "mLzcfxsxnzGynqN5iHQT4rYXxxaqrp1iIdj9xl9Wl5FxjZgXITxhlRscOd/UOBvG\n" +
                    "oMrazVczjjdoRIFFnjtU3Jf0Mich68HD1Z0S3o7X6sDYh6FTVR5KbLcxbk6RcoG4\n" +
                    "VCI5boR5LUXgb5Ed5UxczxvN12S71fyxHYVpuuI0z0HTIbAxKeRw43I6HWOmR1/0\n" +
                    "G6byGCNL/1Fz7Y+264fGqABSNTKdZwIU2K4ANEH7F+9scnhoO6OBp+gjBe5O+7jb\n" +
                    "wZmUCAoTka4hmoaOCj7cqt/IkmxozQ==\n";

    private DevicePolicyManager mDpm;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mDpm = getContext().getSystemService(DevicePolicyManager.class);
    }

    @Override
    public void tearDown() throws Exception {
        mDpm.uninstallCaCert(null, TEST_CA.getBytes());
        super.tearDown();
    }

    public void testCaCertsOperations() throws GeneralSecurityException, IOException {
        final byte[] cert = TEST_CA.getBytes();
        final Certificate caCert = CertificateFactory.getInstance("X.509")
                .generateCertificate(new ByteArrayInputStream(cert));

        // Exercise installCaCert()
        KeyStore keyStore = KeyStore.getInstance("AndroidCAStore");
        keyStore.load(null, null);
        assertWithMessage("CA certificate must not be installed in KeyStore at the"
                + " beginning of the test").that(keyStore.getCertificateAlias(caCert)).isNull();
        assertWithMessage("CA certificate must not be installed in the DPM at the"
                + " beginning of the test").that(mDpm.hasCaCertInstalled(null, cert)).isFalse();


        assertWithMessage("Expecting CA certificate installation to succeed").that(
                mDpm.installCaCert(null, cert)).isTrue();
        assertWithMessage("Expecting CA cert to be installed").that(
                mDpm.hasCaCertInstalled(null, cert)).isTrue();

        // Exercise getInstalledCaCerts()
        assertWithMessage("Expecting CA cert to be in the list of installed CA certs").that(
                containsCertificate(mDpm.getInstalledCaCerts(null), cert)).isTrue();

        // Verify that the CA cert was marked as installed by the Device Owner / Profile Owner.
        assertWithMessage("CA cert should have a KeyStore alias").that(
                keyStore.getCertificateAlias(caCert)).isNotNull();

        mDpm.uninstallCaCert(null, cert);
        assertWithMessage("Expecting CA cert to no longer be installed").that(
                mDpm.hasCaCertInstalled(null, cert)).isFalse();
    }

    public void testInstallKeyPair()
            throws GeneralSecurityException, KeyChainException, InterruptedException {
        final String alias = "delegated-cert-installer-test-key";

        PKCS8EncodedKeySpec keySpec = new PKCS8EncodedKeySpec(
                Base64.decode(TEST_KEY, Base64.DEFAULT));
        PrivateKey privatekey = KeyFactory.getInstance("RSA").generatePrivate(keySpec);

        Certificate certificate = CertificateFactory.getInstance("X.509")
                .generateCertificate(
                        new Base64InputStream(new ByteArrayInputStream(TEST_CERT.getBytes()),
                                Base64.DEFAULT));
        assertThat(mDpm.installKeyPair(null, privatekey, new Certificate[]{certificate}, alias,
                true)).isTrue();

        // Test that the installed private key can be obtained.
        PrivateKey obtainedKey = KeyChain.getPrivateKey(getContext(), alias);
        assertThat(obtainedKey).isNotNull();
        assertThat(obtainedKey.getAlgorithm()).isEqualTo("RSA");

        // Test cleaning up the key.
        assertThat(mDpm.removeKeyPair(null, alias)).isTrue();
        assertThat(KeyChain.getPrivateKey(getContext(), alias)).isNull();
    }

    // Test that a key generation request succeeds when device identifiers are not requested.
    public void testGenerateKeyPairWithoutDeviceIdAttestation() {
        final String alias = "com.android.test.generated-rsa-1";
        try {
            KeyGenParameterSpec keySpec = new KeyGenParameterSpec.Builder(
                    alias,
                    KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
                    .setKeySize(2048)
                    .setDigests(KeyProperties.DIGEST_SHA256)
                    .setSignaturePaddings(KeyProperties.SIGNATURE_PADDING_RSA_PSS,
                            KeyProperties.SIGNATURE_PADDING_RSA_PKCS1)
                    .setIsStrongBoxBacked(false)
                    .setAttestationChallenge(new byte[]{'a', 'b', 'c'})
                    .build();

            AttestedKeyPair generated = mDpm.generateKeyPair(
                    null, "RSA", keySpec, 0);
            assertThat(generated).isNotNull();
        } finally {
            assertThat(mDpm.removeKeyPair(null, alias)).isTrue();
        }
    }

    public void testAccessToDeviceIdentifiers() {
        String serialNumber = Build.getSerial();
        assertThat(Build.getSerial()).doesNotMatch(Build.UNKNOWN);

        PackageManager pm = getContext().getPackageManager();
        if ((pm == null) || (!pm.hasSystemFeature(PackageManager.FEATURE_TELEPHONY))) {
            return;
        }

        TelephonyManager telephonyService = (TelephonyManager) getContext().getSystemService(
                Context.TELEPHONY_SERVICE);
        assertWithMessage("Telephony service must be available.")
                .that(telephonyService).isNotNull();

        try {
            telephonyService.getImei();
        } catch (SecurityException e) {
            fail("Should have permission to access IMEI: " + e);
        }
    }

    private static boolean containsCertificate(List<byte[]> certificates, byte[] toMatch)
            throws CertificateException {
        Certificate certificateToMatch = readCertificate(toMatch);
        for (byte[] certBuffer : certificates) {
            Certificate cert = readCertificate(certBuffer);
            if (certificateToMatch.equals(cert)) {
                return true;
            }
        }
        return false;
    }

    private static Certificate readCertificate(byte[] certBuffer) throws CertificateException {
        final CertificateFactory certFactory = CertificateFactory.getInstance("X.509");
        return certFactory.generateCertificate(new ByteArrayInputStream(certBuffer));
    }

    private Context getContext() {
        return getInstrumentation().getContext();
    }
}
