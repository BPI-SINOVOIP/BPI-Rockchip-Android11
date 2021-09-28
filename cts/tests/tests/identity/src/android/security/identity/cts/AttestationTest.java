/*
 * Copyright 2020 The Android Open Source Project
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

package android.security.identity.cts;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertTrue;

import android.content.Context;

import android.security.identity.IdentityCredential;
import android.security.identity.IdentityCredentialStore;
import androidx.test.InstrumentationRegistry;

import org.junit.Test;
import static org.junit.Assume.assumeTrue;

import com.google.common.primitives.Bytes;

import java.security.SecureRandom;
import java.security.cert.Certificate;
import java.security.cert.X509Certificate;
import java.util.Collection;

import java.util.Optional;

public class AttestationTest {
    private static final String TAG = "AttestationTest";

    // The subset of Keymaster tags we care about. See
    //
    //   https://source.android.com/security/keystore/tags
    //
    // for a list of known tags.
    //
    public static final int KM_TAG_ATTESTATION_APPLICATION_ID = 709;
    public static final int KM_TAG_IDENTITY_CREDENTIAL_KEY = 721;

    @Test
    public void attestationTest() throws Exception {
        assumeTrue("IC HAL is not implemented", Util.isHalImplemented());

        Context appContext = InstrumentationRegistry.getTargetContext();
        IdentityCredentialStore store = IdentityCredentialStore.getInstance(appContext);

        // Use a random challenge of length between 16 and 32.
        SecureRandom random = new SecureRandom();
        int challengeLength = 16 + random.nextInt(16);
        byte[] challenge = new byte[challengeLength];
        random.nextBytes(challenge);

        String credentialName = "test";

        store.deleteCredentialByName(credentialName);
        Collection<X509Certificate> certChain = ProvisioningTest.createCredentialWithChallenge(
            store, credentialName, challenge);
        store.deleteCredentialByName("test");

        // Check that each certificate in the chain isn't expired and also that it's signed by the
        // next one.
        assertTrue(verifyCertificateChain(certChain));

        // Parse the attestation record... Identity Credential uses standard KeyMaster-style
        // attestation with only a few differences:
        //
        // - Tag::IDENTITY_CREDENTIAL_KEY (boolean) must be set to identify the key as an Identity
        //   Credential key.
        //
        // - the KeyMaster version and security-level should be set to that of Identity Credential
        //
        // In this test we only test for these two things and a) the attestationApplicationId
        // matches the calling application; and b) the given challenge matches what was sent.
        //
        // We could test for all the other expected attestation records but currently we assume
        // this is already tested by Keymaster/Android Keystore tests since the TA is expected to
        // re-use the same infrastructure.
        //
        X509Certificate cert = certChain.iterator().next();
        ParsedAttestationRecord record = new ParsedAttestationRecord(cert);

        // Need at least attestation version 3 and attestations to be done in either the TEE
        // or in a StrongBox.
        assertTrue(record.getAttestationVersion() >= 3);

        // Also per the IC HAL, the keymasterSecurityLevel field is used as the securityLevel of
        // the IC TA and this must be either TEE or in a StrongBox... except we specifically
        // allow our software implementation to report Software here.
        //
        // Since we cannot get the implementation name or author at this layer, we can't test for
        // it. This can be tested for in the VTS test, however.

        // Check that the challenge we passed in, is in fact in the attestation record.
        assertArrayEquals(challenge, record.getAttestationChallenge());

        // Tag::ATTESTATION_APPLICATION_ID is used to identify the set of possible applications of
        // which one has initiated a key attestation. This is complicated ASN.1 which we don't want
        // to parse and we don't need to [1].. we can however easily check that our applicationId
        // is appearing as a substring somewhere in this blob.
        //
        // [1] : and the point of this test isn't to verify that attestations are done correctly,
        // that's tested elsewhere in e.g. KM and Android Keystore.
        //
        Optional<byte[]> attestationApplicationId =
                record.getSoftwareAuthorizationByteString(KM_TAG_ATTESTATION_APPLICATION_ID);
        assertTrue(attestationApplicationId.isPresent());
        String appId = appContext.getPackageName();
        assertTrue(Bytes.indexOf(attestationApplicationId.get(), appId.getBytes()) != -1);

        // Tag::IDENTITY_CREDENTIAL_KEY is used in attestations produced by the Identity Credential
        // HAL when that HAL attests to Credential Keys.
        boolean isIdentityCredentialKey =
                record.getTeeAuthorizationBoolean(KM_TAG_IDENTITY_CREDENTIAL_KEY);
        assertTrue(isIdentityCredentialKey);
    }

    // This only verifies each cert hasn't expired and signed by the next one.
    private static boolean verifyCertificateChain(Collection<X509Certificate> certChain) {
        X509Certificate[] certs = new X509Certificate[certChain.size()];
        certs = certChain.toArray(certs);
        X509Certificate parent = certs[certs.length - 1];
        for (int i = certs.length - 1; i >= 0; i--) {
            X509Certificate cert = certs[i];
            try {
                cert.checkValidity();
                cert.verify(parent.getPublicKey());
            } catch (Exception e) {
                e.printStackTrace();
                return false;
            }
            parent = cert;
        }
        return true;
    }
}
