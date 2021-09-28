/*
 * Copyright 2019 The Android Open Source Project
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
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import android.content.Context;

import android.security.identity.EphemeralPublicKeyNotFoundException;
import android.security.identity.IdentityCredential;
import android.security.identity.IdentityCredentialException;
import android.security.identity.IdentityCredentialStore;
import android.security.identity.NoAuthenticationKeyAvailableException;
import android.security.identity.ResultData;
import androidx.test.InstrumentationRegistry;

import org.junit.Test;

import java.io.ByteArrayOutputStream;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.security.KeyPair;
import java.security.SignatureException;
import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.Map;

import javax.crypto.SecretKey;

import co.nstant.in.cbor.CborBuilder;
import co.nstant.in.cbor.CborEncoder;
import co.nstant.in.cbor.CborException;

public class DynamicAuthTest {
    private static final String TAG = "DynamicAuthTest";

    @Test
    public void dynamicAuthTest() throws Exception {
        assumeTrue("IC HAL is not implemented", Util.isHalImplemented());

        Context appContext = InstrumentationRegistry.getTargetContext();
        IdentityCredentialStore store = IdentityCredentialStore.getInstance(appContext);

        String credentialName = "test";

        store.deleteCredentialByName(credentialName);
        Collection<X509Certificate> certChain = ProvisioningTest.createCredential(store,
                credentialName);

        IdentityCredential credential = store.getCredentialByName(credentialName,
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);
        assertNotNull(credential);
        assertArrayEquals(new int[0], credential.getAuthenticationDataUsageCount());

        credential.setAvailableAuthenticationKeys(5, 3);
        assertArrayEquals(
                new int[]{0, 0, 0, 0, 0},
                credential.getAuthenticationDataUsageCount());

        // Getting data without device authentication should work even in the case where we haven't
        // provisioned any authentication keys. Check that.
        Map<String, Collection<String>> entriesToRequest = new LinkedHashMap<>();
        entriesToRequest.put("org.iso.18013-5.2019", Arrays.asList("First name", "Last name"));
        ResultData rd = credential.getEntries(
                Util.createItemsRequest(entriesToRequest, null),
                entriesToRequest,
                null, // sessionTranscript null indicates Device Authentication not requested.
                null);
        byte[] resultCbor = rd.getAuthenticatedData();
        try {
            String pretty = Util.cborPrettyPrint(Util.canonicalizeCbor(resultCbor));
            assertEquals("{\n"
                            + "  'org.iso.18013-5.2019' : {\n"
                            + "    'Last name' : 'Turing',\n"
                            + "    'First name' : 'Alan'\n"
                            + "  }\n"
                            + "}",
                    pretty);
        } catch (CborException e) {
            e.printStackTrace();
            assertTrue(false);
        }

        KeyPair readerEphemeralKeyPair = Util.createEphemeralKeyPair();

        credential = store.getCredentialByName(credentialName,
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);
        KeyPair ephemeralKeyPair = credential.createEphemeralKeyPair();
        credential.setReaderEphemeralPublicKey(readerEphemeralKeyPair.getPublic());

        byte[] sessionTranscript = Util.buildSessionTranscript(ephemeralKeyPair);
        // Then check that getEntries() throw NoAuthenticationKeyAvailableException (_even_ when
        // allowing using exhausted keys).
        try {
            rd = credential.getEntries(
                    Util.createItemsRequest(entriesToRequest, null),
                    entriesToRequest,
                    sessionTranscript,
                    null);
            assertTrue(false);
        } catch (NoAuthenticationKeyAvailableException e) {
            // This is the expected path...
        } catch (IdentityCredentialException e) {
            e.printStackTrace();
            assertTrue(false);
        }

        // Get auth keys needing certification. This should be all of them. Note that
        // this forces the creation of the authentication keys in the HAL.
        Collection<X509Certificate> certificates = null;
        certificates = credential.getAuthKeysNeedingCertification();
        assertEquals(5, certificates.size());

        // Do it one more time to check that an auth key is still pending even
        // when the corresponding key has been created.
        Collection<X509Certificate> certificates2 = null;
        certificates2 = credential.getAuthKeysNeedingCertification();
        assertArrayEquals(certificates.toArray(), certificates2.toArray());

        // Now set auth data for the *first* key (this is the act of certifying the key) and check
        // that one less key now needs certification.
        X509Certificate key0Cert = certificates.iterator().next();

        // Check key0Cert is signed by CredentialKey.
        try {
            key0Cert.verify(certChain.iterator().next().getPublicKey());
        } catch (CertificateException
                | InvalidKeyException
                | NoSuchAlgorithmException
                | NoSuchProviderException
                | SignatureException e) {
            e.printStackTrace();
            assertTrue(false);
        }

        try {
            credential.storeStaticAuthenticationData(key0Cert, new byte[]{42, 43, 44});
            certificates = credential.getAuthKeysNeedingCertification();
        } catch (IdentityCredentialException e) {
            e.printStackTrace();
            assertTrue(false);
        }
        assertEquals(4, certificates.size());

        // Now certify the *last* key.
        X509Certificate key4Cert = new ArrayList<X509Certificate>(certificates).get(
            certificates.size() - 1);
        try {
            key4Cert.verify(certChain.iterator().next().getPublicKey());
        } catch (CertificateException
                | InvalidKeyException
                | NoSuchAlgorithmException
                | NoSuchProviderException
                | SignatureException e) {
            e.printStackTrace();
            assertTrue(false);
        }

        try {
            credential.storeStaticAuthenticationData(key4Cert, new byte[]{43, 44, 45});
            certificates = credential.getAuthKeysNeedingCertification();
        } catch (IdentityCredentialException e) {
            e.printStackTrace();
            assertTrue(false);
        }
        assertEquals(3, certificates.size());

        // Now that we've provisioned authentication keys, presentation will no longer fail with
        // NoAuthenticationKeyAvailableException ... So now we can try a sessionTranscript without
        // the ephemeral public key that was created in the Secure Area and check it fails with
        // EphemeralPublicKeyNotFoundException instead...
        ByteArrayOutputStream stBaos = new ByteArrayOutputStream();
        try {
            new CborEncoder(stBaos).encode(new CborBuilder()
                    .addArray()
                    .add(new byte[]{0x01, 0x02})  // The device engagement structure, encoded
                    .add(new byte[]{0x03, 0x04})  // Reader ephemeral public key, encoded
                    .end()
                    .build());
        } catch (CborException e) {
            e.printStackTrace();
            assertTrue(false);
        }
        byte[] wrongSessionTranscript = stBaos.toByteArray();
        try {
            rd = credential.getEntries(
                    Util.createItemsRequest(entriesToRequest, null),
                    entriesToRequest,
                    wrongSessionTranscript,
                    null);
            assertTrue(false);
        } catch (EphemeralPublicKeyNotFoundException e) {
            // This is the expected path...
        } catch (IdentityCredentialException e) {
            e.printStackTrace();
            assertTrue(false);
        }

        // Now use one of the keys...
        entriesToRequest = new LinkedHashMap<>();
        entriesToRequest.put("org.iso.18013-5.2019",
                Arrays.asList("First name",
                        "Last name",
                        "Home address",
                        "Birth date",
                        "Cryptanalyst",
                        "Portrait image",
                        "Height"));
        credential = store.getCredentialByName(credentialName,
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);
        ephemeralKeyPair = credential.createEphemeralKeyPair();
        credential.setReaderEphemeralPublicKey(readerEphemeralKeyPair.getPublic());
        sessionTranscript = Util.buildSessionTranscript(ephemeralKeyPair);
        rd = credential.getEntries(
                Util.createItemsRequest(entriesToRequest, null),
                entriesToRequest,
                sessionTranscript,
                null);
        resultCbor = rd.getAuthenticatedData();
        try {
            String pretty = Util.cborPrettyPrint(Util.canonicalizeCbor(resultCbor));
            assertEquals("{\n"
                            + "  'org.iso.18013-5.2019' : {\n"
                            + "    'Height' : 180,\n"
                            + "    'Last name' : 'Turing',\n"
                            + "    'Birth date' : '19120623',\n"
                            + "    'First name' : 'Alan',\n"
                            + "    'Cryptanalyst' : true,\n"
                            + "    'Home address' : 'Maida Vale, London, England',\n"
                            + "    'Portrait image' : [0x01, 0x02]\n"
                            + "  }\n"
                            + "}",
                    pretty);
        } catch (CborException e) {
            e.printStackTrace();
            assertTrue(false);
        }

        byte[] deviceAuthenticationCbor = Util.buildDeviceAuthenticationCbor(
            "org.iso.18013-5.2019.mdl",
            sessionTranscript,
            resultCbor);

        // Calculate the MAC by deriving the key using ECDH and HKDF.
        SecretKey eMacKey = Util.calcEMacKeyForReader(
            key0Cert.getPublicKey(),
            readerEphemeralKeyPair.getPrivate(),
            sessionTranscript);
        byte[] deviceAuthenticationBytes =
                Util.prependSemanticTagForEncodedCbor(deviceAuthenticationCbor);
        byte[] expectedMac = Util.coseMac0(eMacKey,
                new byte[0],                 // payload
                deviceAuthenticationBytes);  // detached content

        // Then compare it with what the TA produced.
        assertArrayEquals(expectedMac, rd.getMessageAuthenticationCode());

        // Check that key0's static auth data is returned and that this
        // key has an increased use-count.
        assertArrayEquals(new byte[]{42, 43, 44}, rd.getStaticAuthenticationData());
        assertArrayEquals(new int[]{1, 0, 0, 0, 0}, credential.getAuthenticationDataUsageCount());


        // Now do this one more time.... this time key4 should have been used. Check this by
        // inspecting use-counts and the static authentication data.
        credential = store.getCredentialByName(credentialName,
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);
        ephemeralKeyPair = credential.createEphemeralKeyPair();
        credential.setReaderEphemeralPublicKey(readerEphemeralKeyPair.getPublic());
        sessionTranscript = Util.buildSessionTranscript(ephemeralKeyPair);
        rd = credential.getEntries(
                Util.createItemsRequest(entriesToRequest, null),
                entriesToRequest,
                sessionTranscript,
                null);
        assertArrayEquals(new byte[]{43, 44, 45}, rd.getStaticAuthenticationData());
        assertArrayEquals(new int[]{1, 0, 0, 0, 1}, credential.getAuthenticationDataUsageCount());

        // Verify MAC was made with key4.
        deviceAuthenticationCbor = Util.buildDeviceAuthenticationCbor(
            "org.iso.18013-5.2019.mdl",
            sessionTranscript,
            resultCbor);
        eMacKey = Util.calcEMacKeyForReader(
            key4Cert.getPublicKey(),
            readerEphemeralKeyPair.getPrivate(),
            sessionTranscript);
        deviceAuthenticationBytes =
                Util.prependSemanticTagForEncodedCbor(deviceAuthenticationCbor);
        expectedMac = Util.coseMac0(eMacKey,
                new byte[0],                 // payload
                deviceAuthenticationBytes);  // detached content
        assertArrayEquals(expectedMac, rd.getMessageAuthenticationCode());

        // And again.... this time key0 should have been used. Check it.
        credential = store.getCredentialByName(credentialName,
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);
        ephemeralKeyPair = credential.createEphemeralKeyPair();
        credential.setReaderEphemeralPublicKey(readerEphemeralKeyPair.getPublic());
        sessionTranscript = Util.buildSessionTranscript(ephemeralKeyPair);
        rd = credential.getEntries(
                Util.createItemsRequest(entriesToRequest, null),
                entriesToRequest,
                sessionTranscript,
                null);
        assertArrayEquals(new byte[]{42, 43, 44}, rd.getStaticAuthenticationData());
        assertArrayEquals(new int[]{2, 0, 0, 0, 1}, credential.getAuthenticationDataUsageCount());

        // And again.... this time key4 should have been used. Check it.
        credential = store.getCredentialByName(credentialName,
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);
        ephemeralKeyPair = credential.createEphemeralKeyPair();
        credential.setReaderEphemeralPublicKey(readerEphemeralKeyPair.getPublic());
        sessionTranscript = Util.buildSessionTranscript(ephemeralKeyPair);
        rd = credential.getEntries(
                Util.createItemsRequest(entriesToRequest, null),
                entriesToRequest,
                sessionTranscript,
                null);
        assertArrayEquals(new byte[]{43, 44, 45}, rd.getStaticAuthenticationData());
        assertArrayEquals(new int[]{2, 0, 0, 0, 2}, credential.getAuthenticationDataUsageCount());

        // We configured each key to have three uses only. So we have two more presentations
        // to go until we run out... first, check that only three keys need certifications
        certificates = credential.getAuthKeysNeedingCertification();
        assertEquals(3, certificates.size());

        // Then exhaust the two we've already configured.
        for (int n = 0; n < 2; n++) {
            credential = store.getCredentialByName(credentialName,
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);
            ephemeralKeyPair = credential.createEphemeralKeyPair();
            credential.setReaderEphemeralPublicKey(readerEphemeralKeyPair.getPublic());
            sessionTranscript = Util.buildSessionTranscript(ephemeralKeyPair);
            rd = credential.getEntries(
                    Util.createItemsRequest(entriesToRequest, null),
                    entriesToRequest,
                    sessionTranscript,
                    null);
            assertNotNull(rd);
        }
        assertArrayEquals(new int[]{3, 0, 0, 0, 3}, credential.getAuthenticationDataUsageCount());

        // Now we should have five certs needing certification.
        certificates = credential.getAuthKeysNeedingCertification();
        assertEquals(5, certificates.size());

        // We still have the two keys which have been exhausted.
        assertArrayEquals(new int[]{3, 0, 0, 0, 3}, credential.getAuthenticationDataUsageCount());

        // Check that we fail when running out of presentations (and explicitly don't allow
        // exhausting keys).
        try {
            credential = store.getCredentialByName(credentialName,
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);
            ephemeralKeyPair = credential.createEphemeralKeyPair();
            credential.setReaderEphemeralPublicKey(readerEphemeralKeyPair.getPublic());
            sessionTranscript = Util.buildSessionTranscript(ephemeralKeyPair);
            credential.setAllowUsingExhaustedKeys(false);
            rd = credential.getEntries(
                    Util.createItemsRequest(entriesToRequest, null),
                    entriesToRequest,
                    sessionTranscript,
                    null);
            assertTrue(false);
        } catch (IdentityCredentialException e) {
            assertTrue(e instanceof NoAuthenticationKeyAvailableException);
        }
        assertArrayEquals(new int[]{3, 0, 0, 0, 3}, credential.getAuthenticationDataUsageCount());

        // Now try with allowing using auth keys already exhausted... this should work!
        try {
            credential = store.getCredentialByName(credentialName,
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);
            ephemeralKeyPair = credential.createEphemeralKeyPair();
            credential.setReaderEphemeralPublicKey(readerEphemeralKeyPair.getPublic());
            sessionTranscript = Util.buildSessionTranscript(ephemeralKeyPair);
            rd = credential.getEntries(
                    Util.createItemsRequest(entriesToRequest, null),
                    entriesToRequest,
                    sessionTranscript,
                    null);
        } catch (IdentityCredentialException e) {
            e.printStackTrace();
            assertTrue(false);
        }
        assertArrayEquals(new int[]{4, 0, 0, 0, 3}, credential.getAuthenticationDataUsageCount());

        // Check that replenishing works...
        certificates = credential.getAuthKeysNeedingCertification();
        assertEquals(5, certificates.size());
        X509Certificate keyNewCert = certificates.iterator().next();
        try {
            credential.storeStaticAuthenticationData(keyNewCert, new byte[]{10, 11, 12});
            certificates = credential.getAuthKeysNeedingCertification();
        } catch (IdentityCredentialException e) {
            e.printStackTrace();
            assertTrue(false);
        }
        assertEquals(4, certificates.size());
        assertArrayEquals(new int[]{0, 0, 0, 0, 3}, credential.getAuthenticationDataUsageCount());
        credential = store.getCredentialByName(credentialName,
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);
        ephemeralKeyPair = credential.createEphemeralKeyPair();
        credential.setReaderEphemeralPublicKey(readerEphemeralKeyPair.getPublic());
        sessionTranscript = Util.buildSessionTranscript(ephemeralKeyPair);
        rd = credential.getEntries(
                Util.createItemsRequest(entriesToRequest, null),
                entriesToRequest,
                sessionTranscript,
                null);
        assertArrayEquals(new byte[]{10, 11, 12}, rd.getStaticAuthenticationData());
        assertArrayEquals(new int[]{1, 0, 0, 0, 3}, credential.getAuthenticationDataUsageCount());

        deviceAuthenticationCbor = Util.buildDeviceAuthenticationCbor(
            "org.iso.18013-5.2019.mdl",
            sessionTranscript,
            resultCbor);
        eMacKey = Util.calcEMacKeyForReader(
            keyNewCert.getPublicKey(),
            readerEphemeralKeyPair.getPrivate(),
            sessionTranscript);
        deviceAuthenticationBytes =
                Util.prependSemanticTagForEncodedCbor(deviceAuthenticationCbor);
        expectedMac = Util.coseMac0(eMacKey,
                new byte[0],                 // payload
                deviceAuthenticationBytes);  // detached content
        assertArrayEquals(expectedMac, rd.getMessageAuthenticationCode());

        // ... and we're done. Clean up after ourselves.
        store.deleteCredentialByName(credentialName);
    }

    // TODO: test storeStaticAuthenticationData() throwing UnknownAuthenticationKeyException
    // on an unknown auth key
}
