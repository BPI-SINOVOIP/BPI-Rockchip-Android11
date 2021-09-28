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

package com.android.keychain.tests;

import static com.android.keychain.KeyChainActivity.CertificateParametersFilter;
import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.platform.test.annotations.LargeTest;
import android.security.Credentials;
import android.security.KeyStore;
import android.util.Base64;
import androidx.test.runner.AndroidJUnit4;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.security.cert.Certificate;
import java.security.cert.CertificateEncodingException;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import javax.security.auth.x500.X500Principal;
import org.bouncycastle.util.io.pem.PemObject;
import org.bouncycastle.util.io.pem.PemWriter;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@LargeTest
@RunWith(AndroidJUnit4.class)
public final class KeyChainActivityTest {
    // Generated with:
    // openssl req -newkey rsa:2048 -nodes -keyout key.pem -x509 -days 3650
    //   -out root_ca_certificate.pem
    private static final String ROOT_CA_CERT_RSA =
            "MIIDazCCAlOgAwIBAgIUWQjj+9olDNtdjcSLzK2RpxI9j6UwDQYJKoZIhvcNAQELBQAwRTELMAkG" +
            "A1UEBhMCVUsxCzAJBgNVBAgMAk5BMQ8wDQYDVQQHDAZMb25kb24xGDAWBgNVBAoMD0FuZHJvaWQg" +
            "VGVzdCBDQTAeFw0yMDAzMTIxMTUxNDJaFw0zMDAzMTAxMTUxNDJaMEUxCzAJBgNVBAYTAlVLMQsw" +
            "CQYDVQQIDAJOQTEPMA0GA1UEBwwGTG9uZG9uMRgwFgYDVQQKDA9BbmRyb2lkIFRlc3QgQ0EwggEi" +
            "MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDGbsQO+LqPMr5Nr4Lq0B7C0th93phohSY6hb2w" +
            "MmZs3MRwamlw8FS64KEgmszX5lnyLNqRs91FOFNuq4f2A+TYQhawi9D2bHB7z2ishDM3SxNAqwQl" +
            "LzVNBJw7DAtimajy3VvXoprescFbsOZx8wPGGb2xMKqAXg4Yw9F6te4Y4BSIiwCWtammiSR8Ev0B" +
            "lcnMBrWmSZ4yYF+UgNgNiD/TVrTtRmzQlRhBo5n4F61SGeAxb5p0NRRGmAXKtx358HiLANzZSCiM" +
            "UE5IrgDvW8AKPn5InuYS1G1K2wG5ar1eanQahimtaIEugQxhqG0+/OiKKq2LGRiBpwV1OomXHNFr" +
            "AgMBAAGjUzBRMB0GA1UdDgQWBBRrxYWzKZpCHDi/NK4keXIU5iGukzAfBgNVHSMEGDAWgBRrxYWz" +
            "KZpCHDi/NK4keXIU5iGukzAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQBJTjUY" +
            "rfi3adJFpF2IFkSnavPRxi+NX6wxfKgQvcScO7sV18gAMG7r2NhjsVeScZ48mxsNkj99lHaVoNm6" +
            "c+sWmcb3LO7WCqmAgfJcHeQ5VuluwNoJWo+SGuKbh6/yRejNeQFf++uaEwXP3yNydwKJyQDyDwoG" +
            "vx0jvy8glkVl3fr6u0lGQqmubGU5Q1X6QyA0zJ/sSWBVorLCgk6KvPABQJhjoij+g/GOB1h4g6fb" +
            "bQ3xnek6TGwjQ1bB7rQlqBF7iP/9iUtuuDf0cR8LwMr1Z2OUEMDjRHQCZQJ3APc0kW1ewJ8nqQ+m" +
            "NsUKFRuThYtE/OFsV/TfXwMXbc2rMug+";

    // Generated with:
    // openssl genrsa  -out intermediate_key.pem 2048
    // openssl req -new -key intermediate_key.pem -out intermediate_ca.csr
    // openssl x509 -req -days 3650 -in intermediate_ca.csr -CA root_ca_certificate.pem
    //   -CAkey key.pem -CAcreateserial -out intermediate_ca.pem
    private static final String INTERMEDIATE_CA_CERT_RSA =
            "MIIDHjCCAgYCFHVrA0CEKcoc/C8BqKap3a1m0x8CMA0GCSqGSIb3DQEBCwUAMEUxCzAJBgNVBAYT" +
            "AlVLMQswCQYDVQQIDAJOQTEPMA0GA1UEBwwGTG9uZG9uMRgwFgYDVQQKDA9BbmRyb2lkIFRlc3Qg" +
            "Q0EwHhcNMjAwMzEyMTE1NzUwWhcNMzAwMzEwMTE1NzUwWjBSMQswCQYDVQQGEwJVSzELMAkGA1UE" +
            "CAwCTkExDzANBgNVBAcMBkxvbmRvbjElMCMGA1UECgwcQW5kcm9pZCBJbnRlcm1lZGlhdGUgVGVz" +
            "dCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAPlEIzApeTyKzQWTvv25z/KVEbsr" +
            "alrcto7mX56HV1VQ53cGqi4I7dso3cvpg0CYfcZ+mZh6Evkd+njkcc7Dh/nI0KBJIzGZuo2LB+0r" +
            "qT0RfvI/Xv7CqLO9KOjWJ3+HK3EhSXGvnLvSTsQD1LnE9HXKVdhdOUgLFjbcZrzH62mvTRAO6nhg" +
            "agWTzprTXOX8okaMJtJl9QGMG63Z/m5DONPPrgASW6X6wksGjyorEaakQTUGuPimapP5mk+Y31Se" +
            "pLDDumqRavLT2CpfjHfFq0iDmnnJjG5nz6oKlirhg9JjxHwuKm5jIdsO4dIgi1fJ8Goz2ODG6R6E" +
            "6CjitsQjoMMCAwEAATANBgkqhkiG9w0BAQsFAAOCAQEAdud6PahLSmDLOTr9t0Jqq1HKpeRsjSqn" +
            "JTpd/GkNUrxcctKLTzpAruMq6en8OfcSEa2s3HUMJ1LVMoO9pp4aQaadH7626Q4uqHbNGHWngVNX" +
            "lfBioxrH2QJ2wuKBjUipEGWaM3LY0wqNuBFd5qAVuBwQtZ1x/XH7/Y4l38Y5EGGEi4jSw8eCqiiQ" +
            "2UKItmK8byl3/T5SVgAMbFYz1WJN37EgETEcEgPlosSQ4pha6fVB3Oz6mSfzjGXqHKpHBPUn/N2d" +
            "Z2kxJG8IuwhhhyemhqJdCfOxT2WpemgLQQCCgqtM9O89peWL8AJUVT9cF9KySOvh/P9lTtSf5bJf" +
            "sAzfSg==";

    // Generated with:
    // openssl req -new -newkey rsa:2048 -nodes -keyout server.key -out server.csr
    // openssl x509 -req -days 3650 -in server.csr -CA intermediate_ca.pem
    //   -CAkey intermediate_key.pem -CAcreateserial -out server.pem
    private static final String LEAF_SERVER_CERT_RSA =
            "MIIDIjCCAgoCFGNstHCN7uzPtFlxnbu2FQ3+sc7rMA0GCSqGSIb3DQEBCwUAMFIxCzAJBgNVBAYT" +
            "AlVLMQswCQYDVQQIDAJOQTEPMA0GA1UEBwwGTG9uZG9uMSUwIwYDVQQKDBxBbmRyb2lkIEludGVy" +
            "bWVkaWF0ZSBUZXN0IENBMB4XDTIwMDMxMjEyMTQwMloXDTMwMDMxMDEyMTQwMlowSTELMAkGA1UE" +
            "BhMCVUsxCzAJBgNVBAgMAk5BMQ8wDQYDVQQHDAZMb25kb24xHDAaBgNVBAoME0FuZHJvaWQgU2Vy" +
            "dmVyIFRlc3QwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDCBdt0uV01nG2ULMThfnEx" +
            "CDoVvOSJJLcqKSMsoRUcEYvvqA70x7O2BR6+FZLilr09qAyK1ygyTh+Q6NcBxiB137khrygd/R3S" +
            "U9nNWI6Xj882EospCiPaewR3j49F6F45PvviAacx7v0mZpU1dVMP7ZvhJWb1AB675/379EFztYyU" +
            "ghM5ub8zuAgBuvH/O/dJpnmKUmO43n/qSHDM+Q+oDmLAkWD4gd3SOBZKcLgTyO/pD0pghT8m2t0t" +
            "9X/3KX+tQhif1pAemCOqvxr/HHjlLWxY+QWmcRIAMzTg6+h7sNSKAwwfulMomzMwzkV5sJAYUJOm" +
            "suhhyAXCNX5rWuqXAgMBAAEwDQYJKoZIhvcNAQELBQADggEBACdFD/uV4iT4jg3/rEtszPHkyP4b" +
            "zHaYJYpExoWNbJIz7djhycptdM7wjSoesWQMfpJz95aNWoVHrW85DSdGT+7HwZEsW1zUx3KkXURA" +
            "CdlbVBn1CS4vm0Xk7Rr3LayfhqdFALQVItvBr+LkJPiG/R1jQySp1qaw+NrwukFoepukeZxHH1bF" +
            "9zjGCLwOcfuRB9g8Gm45wRgoSUTDKbD1SMSLuPyllKeHLKE+chhYjm51Evy2xR0DLm0yaFmeyPPM" +
            "KQaZJmtkeAzk2uYYb1HsVDlnEoDoXpTKKN1j39qckpCHxF0X0KqbN7D0grDWIDIee5mnSrg3+Xq5" +
            "aCEDTUn6uU4=";

    private static final X500Principal LEAF_SUBJECT =
            new X500Principal("O=Android Server Test, L=London, ST=NA, C=UK");

    private static final X500Principal INTERMEDIATE_SUBJECT =
            new X500Principal("O=Android Intermediate Test CA, L=London, ST=NA, C=UK");

    private static final X500Principal ROOT_SUBJECT =
            new X500Principal("O=Android Test CA, L=London, ST=NA, C=UK");

    private byte[] mLeafRsaCertificate;
    private byte[] mIntermediateRsaCertificate;
    private byte[] mRootRsaCertificate;

    @Before
    public void setUp() {
        mLeafRsaCertificate = Base64.decode(LEAF_SERVER_CERT_RSA, Base64.DEFAULT);
        mIntermediateRsaCertificate = Base64.decode(INTERMEDIATE_CA_CERT_RSA, Base64.DEFAULT);
        mRootRsaCertificate = Base64.decode(ROOT_CA_CERT_RSA, Base64.DEFAULT);
    }

    @Test
    public void testCertificateParametersFilter_filtersByIntermediateIssuer()
            throws InterruptedException, ExecutionException, CancellationException,
            TimeoutException, IOException, CertificateEncodingException {
        KeyStore keyStore = prepareKeyStoreWithLongChainCertificates();

        assertThat(createCheckerForIssuer(keyStore, ROOT_SUBJECT)
                .shouldPresentCertificate("client")).isTrue();

        assertThat(createCheckerForIssuer(keyStore, INTERMEDIATE_SUBJECT)
                .shouldPresentCertificate("client")).isTrue();

        assertThat(createCheckerForIssuer(keyStore, LEAF_SUBJECT)
                .shouldPresentCertificate("client")).isFalse();
    }

    // Return a KeyStore instance that has both a client certificate as well as a certificate
    // chain associated with it.
    private KeyStore prepareKeyStoreWithLongChainCertificates()
            throws IOException, CertificateEncodingException {
        KeyStore keyStore = mock(KeyStore.class);
        when(keyStore.get(Credentials.USER_CERTIFICATE + "client")).thenReturn(mLeafRsaCertificate);
        Certificate[] intermediates = new Certificate[] {
            parseCertificate(mRootRsaCertificate), parseCertificate(mIntermediateRsaCertificate)};
        byte[] intermediatesPem = convertToPem(intermediates);
        assertThat(intermediatesPem).isNotNull();
        when(keyStore.get(Credentials.CA_CERTIFICATE + "client")).thenReturn(intermediatesPem);

        return keyStore;
    }

    // Create a CertificateParametersFilter instance that has the specified issuer as a requested
    // issuer.
    private static CertificateParametersFilter createCheckerForIssuer(
            KeyStore keyStore, X500Principal issuer) {
        return new CertificateParametersFilter(
                keyStore, new String[] {},
                new ArrayList<byte[]>(Arrays.asList(issuer.getEncoded())));
    }

    private static X509Certificate parseCertificate(byte[] certificateBytes) {
        InputStream in = new ByteArrayInputStream(certificateBytes);
        try {
            CertificateFactory cf = CertificateFactory.getInstance("X.509");
            return (X509Certificate)cf.generateCertificate(in);
        } catch (CertificateException e) {
            fail(String.format("Could not parse certificate: %s", e));
            return null;
        }
    }

    // Copied from android.security.Credentials, as that is a framework class.
    public static byte[] convertToPem(Certificate... objects)
            throws IOException, CertificateEncodingException {
        ByteArrayOutputStream bao = new ByteArrayOutputStream();
        Writer writer = new OutputStreamWriter(bao, StandardCharsets.US_ASCII);
        PemWriter pw = new PemWriter(writer);
        for (Certificate o : objects) {
            pw.writeObject(new PemObject("CERTIFICATE", o.getEncoded()));
        }
        pw.close();
        return bao.toByteArray();
    }
}
