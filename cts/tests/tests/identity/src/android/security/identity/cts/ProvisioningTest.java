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

import static android.security.identity.ResultData.STATUS_NOT_REQUESTED;
import static android.security.identity.ResultData.STATUS_NO_SUCH_ENTRY;
import static android.security.identity.ResultData.STATUS_OK;
import static android.security.identity.ResultData.STATUS_NOT_IN_REQUEST_MESSAGE;
import static android.security.identity.ResultData.STATUS_NO_ACCESS_CONTROL_PROFILES;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import android.content.Context;

import android.security.identity.AccessControlProfile;
import android.security.identity.AccessControlProfileId;
import android.security.identity.AlreadyPersonalizedException;
import android.security.identity.PersonalizationData;
import android.security.identity.IdentityCredential;
import android.security.identity.IdentityCredentialException;
import android.security.identity.IdentityCredentialStore;
import android.security.identity.ResultData;
import android.security.identity.WritableIdentityCredential;
import android.security.identity.SessionTranscriptMismatchException;
import androidx.test.InstrumentationRegistry;

import org.junit.Test;

import java.io.ByteArrayOutputStream;
import java.security.KeyPair;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.cert.CertificateEncodingException;
import java.security.cert.X509Certificate;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.LinkedHashMap;
import java.util.Map;

import co.nstant.in.cbor.CborBuilder;
import co.nstant.in.cbor.CborEncoder;
import co.nstant.in.cbor.CborException;
import co.nstant.in.cbor.model.UnicodeString;
import co.nstant.in.cbor.model.UnsignedInteger;

/**
 * Instrumented test, which will execute on an Android device.
 *
 * @see <a href="http://d.android.com/tools/testing">Testing documentation</a>
 */
public class ProvisioningTest {
    private static final String TAG = "ProvisioningTest";

    private static byte[] getExampleDrivingPrivilegesCbor() {
        // As per 7.4.4 of ISO 18013-5, driving privileges are defined with the following CDDL:
        //
        // driving_privileges = [
        //     * driving_privilege
        // ]
        //
        // driving_privilege = {
        //     vehicle_category_code: tstr ; Vehicle category code as per ISO 18013-2 Annex A
        //     ? issue_date: #6.0(tstr)    ; Date of issue encoded as full-date per RFC 3339
        //     ? expiry_date: #6.0(tstr)   ; Date of expiry encoded as full-date per RFC 3339
        //     ? code: tstr                ; Code as per ISO 18013-2 Annex A
        //     ? sign: tstr                ; Sign as per ISO 18013-2 Annex A
        //     ? value: int                ; Value as per ISO 18013-2 Annex A
        // }
        //
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        try {
            new CborEncoder(baos).encode(new CborBuilder()
                    .addArray()
                    .addMap()
                    .put(new UnicodeString("vehicle_category_code"), new UnicodeString("TODO"))
                    .put(new UnicodeString("value"), new UnsignedInteger(42))
                    .end()
                    .end()
                    .build());
        } catch (CborException e) {
            assertTrue(false);
        }
        return baos.toByteArray();
    }

    static Collection<X509Certificate> createCredential(IdentityCredentialStore store,
            String credentialName) throws IdentityCredentialException {
        return createCredentialWithChallengeAndAcpId(store, credentialName, "SomeChallenge".getBytes(), 0);
    }

    static Collection<X509Certificate> createCredentialWithAcpId(IdentityCredentialStore store,
            String credentialName,
            int accessControlProfileId) throws IdentityCredentialException {
        return createCredentialWithChallengeAndAcpId(store, credentialName, "SomeChallenge".getBytes(),
                                                     accessControlProfileId);
    }

    static Collection<X509Certificate> createCredentialWithChallenge(IdentityCredentialStore store,
            String credentialName,
            byte[] challenge) throws IdentityCredentialException {
        return createCredentialWithChallengeAndAcpId(store, credentialName, challenge, 0);
    }

    static Collection<X509Certificate> createCredentialWithChallengeAndAcpId(IdentityCredentialStore store,
            String credentialName,
            byte[] challenge,
            int id) throws IdentityCredentialException {
        WritableIdentityCredential wc = null;
        wc = store.createCredential(credentialName, "org.iso.18013-5.2019.mdl");

        Collection<X509Certificate> certificateChain =
                wc.getCredentialKeyCertificateChain(challenge);
        // TODO: inspect cert-chain

        // Profile X (no authentication)
        AccessControlProfile noAuthProfile =
                new AccessControlProfile.Builder(new AccessControlProfileId(id))
                        .setUserAuthenticationRequired(false)
                        .build();

        byte[] drivingPrivileges = getExampleDrivingPrivilegesCbor();

        Collection<AccessControlProfileId> idsNoAuth = new ArrayList<AccessControlProfileId>();
        idsNoAuth.add(new AccessControlProfileId(id));
        Collection<AccessControlProfileId> idsNoAcp = new ArrayList<AccessControlProfileId>();
        String mdlNs = "org.iso.18013-5.2019";
        PersonalizationData personalizationData =
                new PersonalizationData.Builder()
                        .addAccessControlProfile(noAuthProfile)
                        .putEntry(mdlNs, "First name", idsNoAuth, Util.cborEncodeString("Alan"))
                        .putEntry(mdlNs, "Last name", idsNoAuth, Util.cborEncodeString("Turing"))
                        .putEntry(mdlNs, "Home address", idsNoAuth,
                                Util.cborEncodeString("Maida Vale, London, England"))
                        .putEntry(mdlNs, "Birth date", idsNoAuth, Util.cborEncodeString("19120623"))
                        .putEntry(mdlNs, "Cryptanalyst", idsNoAuth, Util.cborEncodeBoolean(true))
                        .putEntry(mdlNs, "Portrait image", idsNoAuth, Util.cborEncodeBytestring(
                            new byte[]{0x01, 0x02}))
                        .putEntry(mdlNs, "Height", idsNoAuth, Util.cborEncodeInt(180))
                        .putEntry(mdlNs, "Neg Item", idsNoAuth, Util.cborEncodeInt(-42))
                        .putEntry(mdlNs, "Int Two Bytes", idsNoAuth, Util.cborEncodeInt(0x101))
                        .putEntry(mdlNs, "Int Four Bytes", idsNoAuth, Util.cborEncodeInt(0x10001))
                        .putEntry(mdlNs, "Int Eight Bytes", idsNoAuth,
                                Util.cborEncodeInt(0x100000001L))
                        .putEntry(mdlNs, "driving_privileges", idsNoAuth, drivingPrivileges)
                        .putEntry(mdlNs, "No Access", idsNoAcp,
                                Util.cborEncodeString("Cannot be retrieved"))
                        .build();

        byte[] proofOfProvisioningSignature = wc.personalize(personalizationData);
        byte[] proofOfProvisioning = Util.coseSign1GetData(proofOfProvisioningSignature);

        String pretty = "";
        try {
            pretty = Util.cborPrettyPrint(proofOfProvisioning);
        } catch (CborException e) {
            e.printStackTrace();
            assertTrue(false);
        }
        // Checks that order of elements is the order it was added, using the API.
        assertEquals("[\n"
                + "  'ProofOfProvisioning',\n"
                + "  'org.iso.18013-5.2019.mdl',\n"
                + "  [\n"
                + "    {\n"
                + "      'id' : " + id + "\n"
                + "    }\n"
                + "  ],\n"
                + "  {\n"
                + "    'org.iso.18013-5.2019' : [\n"
                + "      {\n"
                + "        'name' : 'First name',\n"
                + "        'value' : 'Alan',\n"
                + "        'accessControlProfiles' : [" + id + "]\n"
                + "      },\n"
                + "      {\n"
                + "        'name' : 'Last name',\n"
                + "        'value' : 'Turing',\n"
                + "        'accessControlProfiles' : [" + id + "]\n"
                + "      },\n"
                + "      {\n"
                + "        'name' : 'Home address',\n"
                + "        'value' : 'Maida Vale, London, England',\n"
                + "        'accessControlProfiles' : [" + id + "]\n"
                + "      },\n"
                + "      {\n"
                + "        'name' : 'Birth date',\n"
                + "        'value' : '19120623',\n"
                + "        'accessControlProfiles' : [" + id + "]\n"
                + "      },\n"
                + "      {\n"
                + "        'name' : 'Cryptanalyst',\n"
                + "        'value' : true,\n"
                + "        'accessControlProfiles' : [" + id + "]\n"
                + "      },\n"
                + "      {\n"
                + "        'name' : 'Portrait image',\n"
                + "        'value' : [0x01, 0x02],\n"
                + "        'accessControlProfiles' : [" + id + "]\n"
                + "      },\n"
                + "      {\n"
                + "        'name' : 'Height',\n"
                + "        'value' : 180,\n"
                + "        'accessControlProfiles' : [" + id + "]\n"
                + "      },\n"
                + "      {\n"
                + "        'name' : 'Neg Item',\n"
                + "        'value' : -42,\n"
                + "        'accessControlProfiles' : [" + id + "]\n"
                + "      },\n"
                + "      {\n"
                + "        'name' : 'Int Two Bytes',\n"
                + "        'value' : 257,\n"
                + "        'accessControlProfiles' : [" + id + "]\n"
                + "      },\n"
                + "      {\n"
                + "        'name' : 'Int Four Bytes',\n"
                + "        'value' : 65537,\n"
                + "        'accessControlProfiles' : [" + id + "]\n"
                + "      },\n"
                + "      {\n"
                + "        'name' : 'Int Eight Bytes',\n"
                + "        'value' : 4294967297,\n"
                + "        'accessControlProfiles' : [" + id + "]\n"
                + "      },\n"
                + "      {\n"
                + "        'name' : 'driving_privileges',\n"
                + "        'value' : [\n"
                + "          {\n"
                + "            'value' : 42,\n"
                + "            'vehicle_category_code' : 'TODO'\n"
                + "          }\n"
                + "        ],\n"
                + "        'accessControlProfiles' : [" + id + "]\n"
                + "      },\n"
                + "      {\n"
                + "        'name' : 'No Access',\n"
                + "        'value' : 'Cannot be retrieved',\n"
                + "        'accessControlProfiles' : []\n"
                + "      }\n"
                + "    ]\n"
                + "  },\n"
                + "  false\n"
                + "]", pretty);

        try {
            assertTrue(Util.coseSign1CheckSignature(
                proofOfProvisioningSignature,
                new byte[0], // Additional data
                certificateChain.iterator().next().getPublicKey()));
        } catch (NoSuchAlgorithmException | InvalidKeyException e) {
            e.printStackTrace();
            assertTrue(false);
        }

        // TODO: Check challenge is in certificatechain

        // TODO: Check each cert signs the next one

        // TODO: Check bottom cert is the Google well-know cert

        // TODO: need to also get and check SecurityStatement

        return certificateChain;
    }

    static Collection<X509Certificate> createCredentialMultipleNamespaces(
            IdentityCredentialStore store,
            String credentialName) throws IdentityCredentialException {
        WritableIdentityCredential wc = null;
        wc = store.createCredential(credentialName, "org.iso.18013-5.2019.mdl");

        Collection<X509Certificate> certificateChain =
                wc.getCredentialKeyCertificateChain("SomeChallenge".getBytes());

        // Profile 0 (no authentication)
        AccessControlProfile noAuthProfile =
                new AccessControlProfile.Builder(new AccessControlProfileId(0))
                        .setUserAuthenticationRequired(false)
                        .build();

        Collection<AccessControlProfileId> idsNoAuth = new ArrayList<AccessControlProfileId>();
        idsNoAuth.add(new AccessControlProfileId(0));
        PersonalizationData personalizationData =
                new PersonalizationData.Builder()
                        .addAccessControlProfile(noAuthProfile)
                        .putEntry("org.example.barfoo", "Bar", idsNoAuth,
                                Util.cborEncodeString("Foo"))
                        .putEntry("org.example.barfoo", "Foo", idsNoAuth,
                                Util.cborEncodeString("Bar"))
                        .putEntry("org.example.foobar", "Foo", idsNoAuth,
                                Util.cborEncodeString("Bar"))
                        .putEntry("org.example.foobar", "Bar", idsNoAuth,
                                Util.cborEncodeString("Foo"))
                        .build();

        try {
            byte[] proofOfProvisioningSignature = wc.personalize(personalizationData);
            byte[] proofOfProvisioning = Util.coseSign1GetData(proofOfProvisioningSignature);
            String pretty = Util.cborPrettyPrint(proofOfProvisioning);
            // Checks that order of elements is the order it was added, using the API.
            assertEquals("[\n"
                    + "  'ProofOfProvisioning',\n"
                    + "  'org.iso.18013-5.2019.mdl',\n"
                    + "  [\n"
                    + "    {\n"
                    + "      'id' : 0\n"
                    + "    }\n"
                    + "  ],\n"
                    + "  {\n"
                    + "    'org.example.barfoo' : [\n"
                    + "      {\n"
                    + "        'name' : 'Bar',\n"
                    + "        'value' : 'Foo',\n"
                    + "        'accessControlProfiles' : [0]\n"
                    + "      },\n"
                    + "      {\n"
                    + "        'name' : 'Foo',\n"
                    + "        'value' : 'Bar',\n"
                    + "        'accessControlProfiles' : [0]\n"
                    + "      }\n"
                    + "    ],\n"
                    + "    'org.example.foobar' : [\n"
                    + "      {\n"
                    + "        'name' : 'Foo',\n"
                    + "        'value' : 'Bar',\n"
                    + "        'accessControlProfiles' : [0]\n"
                    + "      },\n"
                    + "      {\n"
                    + "        'name' : 'Bar',\n"
                    + "        'value' : 'Foo',\n"
                    + "        'accessControlProfiles' : [0]\n"
                    + "      }\n"
                    + "    ]\n"
                    + "  },\n"
                    + "  false\n"
                    + "]", pretty);

        } catch (CborException e) {
            e.printStackTrace();
            assertTrue(false);
        }

        return certificateChain;
    }

    @Test
    public void alreadyPersonalized() throws IdentityCredentialException {
        assumeTrue("IC HAL is not implemented", Util.isHalImplemented());

        Context appContext = InstrumentationRegistry.getTargetContext();
        IdentityCredentialStore store = IdentityCredentialStore.getInstance(appContext);

        store.deleteCredentialByName("test");
        createCredential(store, "test");
        try {
            createCredential(store, "test");
            assertTrue(false);
        } catch (AlreadyPersonalizedException e) {
            // The expected path.
        }
        store.deleteCredentialByName("test");
        // TODO: check retutrned |proofOfDeletion|
    }

    @Test
    public void nonExistent() throws IdentityCredentialException {
        assumeTrue("IC HAL is not implemented", Util.isHalImplemented());

        Context appContext = InstrumentationRegistry.getTargetContext();
        IdentityCredentialStore store = IdentityCredentialStore.getInstance(appContext);

        store.deleteCredentialByName("test");
        IdentityCredential credential = store.getCredentialByName("test",
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);
        assertNull(credential);
    }

    @Test
    public void defaultStoreSupportsAnyDocumentType() throws IdentityCredentialException {
        assumeTrue("IC HAL is not implemented", Util.isHalImplemented());

        Context appContext = InstrumentationRegistry.getTargetContext();
        IdentityCredentialStore store = IdentityCredentialStore.getInstance(appContext);

        String[] supportedDocTypes = store.getSupportedDocTypes();
        assertEquals(0, supportedDocTypes.length);
    }

    @Test
    public void deleteCredential()
            throws IdentityCredentialException, CborException, CertificateEncodingException {
        assumeTrue("IC HAL is not implemented", Util.isHalImplemented());

        Context appContext = InstrumentationRegistry.getTargetContext();
        IdentityCredentialStore store = IdentityCredentialStore.getInstance(appContext);

        store.deleteCredentialByName("test");
        assertNull(store.deleteCredentialByName("test"));
        Collection<X509Certificate> certificateChain = createCredential(store, "test");

        // Deleting the credential involves destroying the keys referenced in the returned
        // certificateChain... so get an encoded blob we can turn into a X509 cert when
        // checking the deletion receipt below, post-deletion.
        byte[] encodedCredentialCert = certificateChain.iterator().next().getEncoded();

        byte[] proofOfDeletionSignature = store.deleteCredentialByName("test");
        byte[] proofOfDeletion = Util.coseSign1GetData(proofOfDeletionSignature);

        // Check the returned CBOR is what is expected.
        String pretty = Util.cborPrettyPrint(proofOfDeletion);
        assertEquals("['ProofOfDeletion', 'org.iso.18013-5.2019.mdl', false]", pretty);

        try {
            assertTrue(Util.coseSign1CheckSignature(
                proofOfDeletionSignature,
                new byte[0], // Additional data
                certificateChain.iterator().next().getPublicKey()));
        } catch (NoSuchAlgorithmException | InvalidKeyException e) {
            e.printStackTrace();
            assertTrue(false);
        }

        // Finally, check deleting an already deleted credential returns the expected.
        assertNull(store.deleteCredentialByName("test"));
    }

    @Test
    public void testProvisionAndRetrieve() throws IdentityCredentialException, CborException {
        assumeTrue("IC HAL is not implemented", Util.isHalImplemented());

        Context appContext = InstrumentationRegistry.getTargetContext();
        IdentityCredentialStore store = IdentityCredentialStore.getInstance(appContext);

        store.deleteCredentialByName("test");
        Collection<X509Certificate> certChain = createCredential(store, "test");

        IdentityCredential credential = store.getCredentialByName("test",
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);

        // Check that the read-back certChain matches the created one.
        Collection<X509Certificate> readBackCertChain =
                credential.getCredentialKeyCertificateChain();
        assertEquals(certChain.size(), readBackCertChain.size());
        Iterator<X509Certificate> it = readBackCertChain.iterator();
        for (X509Certificate expectedCert : certChain) {
            X509Certificate readBackCert = it.next();
            assertEquals(expectedCert, readBackCert);
        }

        Map<String, Collection<String>> entriesToRequest = new LinkedHashMap<>();
        entriesToRequest.put("org.iso.18013-5.2019",
                Arrays.asList("First name",
                        "Last name",
                        "Home address",
                        "Birth date",
                        "Cryptanalyst",
                        "Portrait image",
                        "Height",
                        "Neg Item",
                        "Int Two Bytes",
                        "Int Eight Bytes",
                        "Int Four Bytes",
                        "driving_privileges"));
        ResultData rd = credential.getEntries(
                Util.createItemsRequest(entriesToRequest, null),
                entriesToRequest,
                null,
                null);

        Collection<String> resultNamespaces = rd.getNamespaces();
        assertEquals(resultNamespaces.size(), 1);
        assertEquals("org.iso.18013-5.2019", resultNamespaces.iterator().next());
        assertEquals(12, rd.getEntryNames("org.iso.18013-5.2019").size());

        String ns = "org.iso.18013-5.2019";
        assertEquals("Alan", Util.getStringEntry(rd, ns, "First name"));
        assertEquals("Turing", Util.getStringEntry(rd, ns, "Last name"));
        assertEquals("Maida Vale, London, England", Util.getStringEntry(rd, ns, "Home address"));
        assertEquals("19120623", Util.getStringEntry(rd, ns, "Birth date"));
        assertEquals(true, Util.getBooleanEntry(rd, ns, "Cryptanalyst"));
        assertArrayEquals(new byte[]{0x01, 0x02},
                Util.getBytestringEntry(rd, ns, "Portrait image"));
        assertEquals(180, Util.getIntegerEntry(rd, ns, "Height"));
        assertEquals(-42, Util.getIntegerEntry(rd, ns, "Neg Item"));
        assertEquals(0x101, Util.getIntegerEntry(rd, ns, "Int Two Bytes"));
        assertEquals(0x10001, Util.getIntegerEntry(rd, ns, "Int Four Bytes"));
        assertEquals(0x100000001L, Util.getIntegerEntry(rd, ns, "Int Eight Bytes"));
        byte[] drivingPrivileges = getExampleDrivingPrivilegesCbor();
        assertArrayEquals(drivingPrivileges, rd.getEntry(ns, "driving_privileges"));

        assertEquals("{\n"
                + "  'org.iso.18013-5.2019' : {\n"
                + "    'Height' : 180,\n"
                + "    'Neg Item' : -42,\n"
                + "    'Last name' : 'Turing',\n"
                + "    'Birth date' : '19120623',\n"
                + "    'First name' : 'Alan',\n"
                + "    'Cryptanalyst' : true,\n"
                + "    'Home address' : 'Maida Vale, London, England',\n"
                + "    'Int Two Bytes' : 257,\n"
                + "    'Int Four Bytes' : 65537,\n"
                + "    'Portrait image' : [0x01, 0x02],\n"
                + "    'Int Eight Bytes' : 4294967297,\n"
                + "    'driving_privileges' : [\n"
                + "      {\n"
                + "        'value' : 42,\n"
                + "        'vehicle_category_code' : 'TODO'\n"
                + "      }\n"
                + "    ]\n"
                + "  }\n"
                + "}", Util.cborPrettyPrint(Util.canonicalizeCbor(rd.getAuthenticatedData())));

        store.deleteCredentialByName("test");
    }

    @Test
    public void testProvisionAndRetrieveMultipleTimes() throws IdentityCredentialException,
            InvalidKeyException {
        assumeTrue("IC HAL is not implemented", Util.isHalImplemented());

        Context appContext = InstrumentationRegistry.getTargetContext();
        IdentityCredentialStore store = IdentityCredentialStore.getInstance(appContext);

        // This checks we can do multiple getEntries() calls

        store.deleteCredentialByName("test");
        Collection<X509Certificate> certChain = createCredential(store, "test");

        IdentityCredential credential = store.getCredentialByName("test",
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);

        // We're going to need some authentication keys for this so create some dummy ones.
        credential.setAvailableAuthenticationKeys(5, 1);
        Collection<X509Certificate> authKeys = credential.getAuthKeysNeedingCertification();
        for (X509Certificate authKey : authKeys) {
            byte[] staticAuthData = new byte[5];
            credential.storeStaticAuthenticationData(authKey, staticAuthData);
        }

        KeyPair ephemeralKeyPair = credential.createEphemeralKeyPair();
        KeyPair readerEphemeralKeyPair = Util.createEphemeralKeyPair();
        credential.setReaderEphemeralPublicKey(readerEphemeralKeyPair.getPublic());
        byte[] sessionTranscript = Util.buildSessionTranscript(ephemeralKeyPair);

        Map<String, Collection<String>> entriesToRequest = new LinkedHashMap<>();
        entriesToRequest.put("org.iso.18013-5.2019", Arrays.asList("First name", "Last name"));

        for (int n = 0; n < 3; n++) {
            ResultData rd = credential.getEntries(
                Util.createItemsRequest(entriesToRequest, null),
                entriesToRequest,
                sessionTranscript,
                null);
            assertEquals("Alan", Util.getStringEntry(rd, "org.iso.18013-5.2019", "First name"));
            assertEquals("Turing", Util.getStringEntry(rd, "org.iso.18013-5.2019", "Last name"));
            assertNotNull(rd.getMessageAuthenticationCode());
        }

        // Now try with a different (but still valid) sessionTranscript - this should fail with
        // SessionTranscriptMismatchException
        KeyPair otherEphemeralKeyPair = Util.createEphemeralKeyPair();
        byte[] otherSessionTranscript = Util.buildSessionTranscript(otherEphemeralKeyPair);
        try {
            ResultData rd = credential.getEntries(
                Util.createItemsRequest(entriesToRequest, null),
                entriesToRequest,
                otherSessionTranscript,
                null);
            assertTrue(false);
        } catch (SessionTranscriptMismatchException e) {
            // This is the expected path...
        } catch (IdentityCredentialException e) {
            e.printStackTrace();
            assertTrue(false);
        }

        store.deleteCredentialByName("test");
    }

    @Test
    public void testProvisionAndRetrieveWithFiltering() throws IdentityCredentialException {
        assumeTrue("IC HAL is not implemented", Util.isHalImplemented());

        Context appContext = InstrumentationRegistry.getTargetContext();
        IdentityCredentialStore store = IdentityCredentialStore.getInstance(appContext);

        store.deleteCredentialByName("test");
        Collection<X509Certificate> certChain = createCredential(store, "test");

        IdentityCredential credential = store.getCredentialByName("test",
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);

        Map<String, Collection<String>> entriesToRequest = new LinkedHashMap<>();
        entriesToRequest.put("org.iso.18013-5.2019",
                Arrays.asList("First name",
                        "Last name",
                        "Home address",
                        "Birth date",
                        "Cryptanalyst",
                        "Portrait image",
                        "Height"));
        Map<String, Collection<String>> entriesToRequestWithoutHomeAddress = new LinkedHashMap<>();
        entriesToRequestWithoutHomeAddress.put("org.iso.18013-5.2019",
                Arrays.asList("First name",
                        "Last name",
                        "Birth date",
                        "Cryptanalyst",
                        "Portrait image",
                        "Height"));
        ResultData rd = credential.getEntries(
                Util.createItemsRequest(entriesToRequest, null),
                entriesToRequestWithoutHomeAddress,
                null,
                null);

        Collection<String> resultNamespaces = rd.getNamespaces();
        assertEquals(resultNamespaces.size(), 1);
        assertEquals("org.iso.18013-5.2019", resultNamespaces.iterator().next());
        assertEquals(6, rd.getEntryNames("org.iso.18013-5.2019").size());

        String ns = "org.iso.18013-5.2019";
        assertEquals("Alan", Util.getStringEntry(rd, ns, "First name"));
        assertEquals("Turing", Util.getStringEntry(rd, ns, "Last name"));
        assertEquals("19120623", Util.getStringEntry(rd, ns, "Birth date"));
        assertEquals(true, Util.getBooleanEntry(rd, ns, "Cryptanalyst"));
        assertArrayEquals(new byte[]{0x01, 0x02},
                Util.getBytestringEntry(rd, ns, "Portrait image"));
        assertEquals(180, Util.getIntegerEntry(rd, ns, "Height"));

        store.deleteCredentialByName("test");
    }

    @Test
    public void testProvisionAndRetrieveElementWithNoACP() throws IdentityCredentialException {
        assumeTrue("IC HAL is not implemented", Util.isHalImplemented());

        Context appContext = InstrumentationRegistry.getTargetContext();
        IdentityCredentialStore store = IdentityCredentialStore.getInstance(appContext);

        store.deleteCredentialByName("test");
        Collection<X509Certificate> certChain = createCredential(store, "test");

        IdentityCredential credential = store.getCredentialByName("test",
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);

        Map<String, Collection<String>> entriesToRequest = new LinkedHashMap<>();
        entriesToRequest.put("org.iso.18013-5.2019", Arrays.asList("No Access"));
        ResultData rd = credential.getEntries(
                Util.createItemsRequest(entriesToRequest, null),
                entriesToRequest,
                null,
                null);

        Collection<String> resultNamespaces = rd.getNamespaces();
        assertEquals(resultNamespaces.size(), 1);
        assertEquals("org.iso.18013-5.2019", resultNamespaces.iterator().next());
        assertEquals(1, rd.getEntryNames("org.iso.18013-5.2019").size());
        assertEquals(0, rd.getRetrievedEntryNames("org.iso.18013-5.2019").size());

        String ns = "org.iso.18013-5.2019";
        assertEquals(STATUS_NO_ACCESS_CONTROL_PROFILES, rd.getStatus(ns, "No Access"));

        store.deleteCredentialByName("test");
    }

    // TODO: Make sure we test retrieving an entry with multiple ACPs and test all four cases:
    //
    // - ACP1 bad,  ACP2 bad   -> NOT OK
    // - ACP1 good, ACP2 bad   -> OK
    // - ACP1 bad,  ACP2 good  -> OK
    // - ACP1 good, ACP2 good  -> OK
    //

    @Test
    public void testProvisionAndRetrieveWithEntryNotInRequest() throws IdentityCredentialException {
        assumeTrue("IC HAL is not implemented", Util.isHalImplemented());

        Context appContext = InstrumentationRegistry.getTargetContext();
        IdentityCredentialStore store = IdentityCredentialStore.getInstance(appContext);

        store.deleteCredentialByName("test");
        Collection<X509Certificate> certChain = createCredential(store, "test");

        IdentityCredential credential = store.getCredentialByName("test",
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);

        Map<String, Collection<String>> entriesToRequest = new LinkedHashMap<>();
        entriesToRequest.put("org.iso.18013-5.2019",
                Arrays.asList("First name",
                        "Last name",
                        "Home address",
                        "Birth date",
                        "Cryptanalyst",
                        "Portrait image",
                        "Height"));
        Map<String, Collection<String>> entriesToRequestWithoutHomeAddress = new LinkedHashMap<>();
        entriesToRequestWithoutHomeAddress.put("org.iso.18013-5.2019",
                Arrays.asList("First name",
                        "Last name",
                        "Birth date",
                        "Cryptanalyst",
                        "Portrait image",
                        "Height"));
        ResultData rd = credential.getEntries(
                Util.createItemsRequest(entriesToRequestWithoutHomeAddress, null),
                entriesToRequest,
                null,
                null);

        Collection<String> resultNamespaces = rd.getNamespaces();
        assertEquals(resultNamespaces.size(), 1);
        assertEquals("org.iso.18013-5.2019", resultNamespaces.iterator().next());
        assertEquals(7, rd.getEntryNames("org.iso.18013-5.2019").size());
        assertEquals(6, rd.getRetrievedEntryNames("org.iso.18013-5.2019").size());

        String ns = "org.iso.18013-5.2019";
        assertEquals(STATUS_NOT_IN_REQUEST_MESSAGE, rd.getStatus(ns, "Home address"));

        assertEquals("Alan", Util.getStringEntry(rd, ns, "First name"));
        assertEquals("Turing", Util.getStringEntry(rd, ns, "Last name"));
        assertEquals("19120623", Util.getStringEntry(rd, ns, "Birth date"));
        assertEquals(true, Util.getBooleanEntry(rd, ns, "Cryptanalyst"));
        assertArrayEquals(new byte[]{0x01, 0x02},
                Util.getBytestringEntry(rd, ns, "Portrait image"));
        assertEquals(180, Util.getIntegerEntry(rd, ns, "Height"));

        store.deleteCredentialByName("test");
    }

    @Test
    public void nonExistentEntries() throws IdentityCredentialException {
        assumeTrue("IC HAL is not implemented", Util.isHalImplemented());

        Context appContext = InstrumentationRegistry.getTargetContext();
        IdentityCredentialStore store = IdentityCredentialStore.getInstance(appContext);

        store.deleteCredentialByName("test");
        Collection<X509Certificate> certChain = createCredential(store, "test");

        IdentityCredential credential = store.getCredentialByName("test",
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);

        Map<String, Collection<String>> entriesToRequest = new LinkedHashMap<>();
        entriesToRequest.put("org.iso.18013-5.2019",
                Arrays.asList("First name",
                        "Last name",
                        "Non-existent Entry"));
        ResultData rd = credential.getEntries(
                null,
                entriesToRequest,
                null,
                null);

        Collection<String> resultNamespaces = rd.getNamespaces();
        assertEquals(resultNamespaces.size(), 1);
        assertEquals("org.iso.18013-5.2019", resultNamespaces.iterator().next());
        assertEquals(3, rd.getEntryNames("org.iso.18013-5.2019").size());
        assertEquals(2, rd.getRetrievedEntryNames("org.iso.18013-5.2019").size());

        String ns = "org.iso.18013-5.2019";

        assertEquals(STATUS_OK, rd.getStatus(ns, "First name"));
        assertEquals(STATUS_OK, rd.getStatus(ns, "Last name"));
        assertEquals(STATUS_NO_SUCH_ENTRY, rd.getStatus(ns, "Non-existent Entry"));
        assertEquals(STATUS_NOT_REQUESTED, rd.getStatus(ns, "Entry not even requested"));

        assertEquals("Alan", Util.getStringEntry(rd, ns, "First name"));
        assertEquals("Turing", Util.getStringEntry(rd, ns, "Last name"));
        assertNull(rd.getEntry(ns, "Non-existent Entry"));
        assertNull(rd.getEntry(ns, "Entry not even requested"));

        store.deleteCredentialByName("test");
    }

    @Test
    public void multipleNamespaces() throws IdentityCredentialException, CborException {
        assumeTrue("IC HAL is not implemented", Util.isHalImplemented());

        Context appContext = InstrumentationRegistry.getTargetContext();
        IdentityCredentialStore store = IdentityCredentialStore.getInstance(appContext);

        store.deleteCredentialByName("test");
        Collection<X509Certificate> certChain = createCredentialMultipleNamespaces(
                store, "test");

        IdentityCredential credential = store.getCredentialByName("test",
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);

        // Request these in different order than they are stored
        Map<String, Collection<String>> entriesToRequest = new LinkedHashMap<>();
        entriesToRequest.put("org.example.foobar", Arrays.asList("Foo", "Bar", "Non-exist"));
        entriesToRequest.put("org.example.barfoo", Arrays.asList("Bar", "Non-exist", "Foo"));
        entriesToRequest.put("org.example.foofoo", Arrays.asList("Bar", "Foo", "Non-exist"));

        ResultData rd = credential.getEntries(
                null,
                entriesToRequest,
                null,
                null);

        // We should get the same number of namespaces back, as we requested - even for namespaces
        // that do not exist in the credential.
        //
        // Additionally, each namespace should have exactly the items requested, in the same order.
        Collection<String> resultNamespaces = rd.getNamespaces();
        assertEquals(resultNamespaces.size(), 3);


        // First requested namespace - org.example.foobar
        String ns = "org.example.foobar";
        assertArrayEquals(new String[]{"Foo", "Bar", "Non-exist"}, rd.getEntryNames(ns).toArray());
        assertArrayEquals(new String[]{"Foo", "Bar"}, rd.getRetrievedEntryNames(ns).toArray());

        assertEquals(STATUS_OK, rd.getStatus(ns, "Foo"));
        assertEquals(STATUS_OK, rd.getStatus(ns, "Bar"));
        assertEquals(STATUS_NO_SUCH_ENTRY, rd.getStatus(ns, "Non-exist"));
        assertEquals(STATUS_NOT_REQUESTED, rd.getStatus(ns, "Entry not even requested"));

        assertEquals("Bar", Util.getStringEntry(rd, ns, "Foo"));
        assertEquals("Foo", Util.getStringEntry(rd, ns, "Bar"));
        assertNull(rd.getEntry(ns, "Non-exist"));
        assertNull(rd.getEntry(ns, "Entry not even requested"));

        // Second requested namespace - org.example.barfoo
        ns = "org.example.barfoo";
        assertArrayEquals(new String[]{"Bar", "Non-exist", "Foo"}, rd.getEntryNames(ns).toArray());
        assertArrayEquals(new String[]{"Bar", "Foo"}, rd.getRetrievedEntryNames(ns).toArray());

        assertEquals(STATUS_OK, rd.getStatus(ns, "Foo"));
        assertEquals(STATUS_OK, rd.getStatus(ns, "Bar"));
        assertEquals(STATUS_NO_SUCH_ENTRY, rd.getStatus(ns, "Non-exist"));
        assertEquals(STATUS_NOT_REQUESTED, rd.getStatus(ns, "Entry not even requested"));

        assertEquals("Bar", Util.getStringEntry(rd, ns, "Foo"));
        assertEquals("Foo", Util.getStringEntry(rd, ns, "Bar"));
        assertNull(rd.getEntry(ns, "Non-exist"));
        assertNull(rd.getEntry(ns, "Entry not even requested"));

        // Third requested namespace - org.example.foofoo
        ns = "org.example.foofoo";
        assertArrayEquals(new String[]{"Bar", "Foo", "Non-exist"}, rd.getEntryNames(ns).toArray());
        assertEquals(0, rd.getRetrievedEntryNames(ns).size());
        assertEquals(STATUS_NO_SUCH_ENTRY, rd.getStatus(ns, "Foo"));
        assertEquals(STATUS_NO_SUCH_ENTRY, rd.getStatus(ns, "Bar"));
        assertEquals(STATUS_NO_SUCH_ENTRY, rd.getStatus(ns, "Non-exist"));
        assertEquals(STATUS_NOT_REQUESTED, rd.getStatus(ns, "Entry not even requested"));

        // Now check the returned CBOR ... note how it only has entries _and_ namespaces
        // for data that was returned.
        //
        // Importantly, this is unlike the returned ResultData which mirrors one to one the passed
        // in Map<String,Collection<String>> structure, _including_ ensuring the order is the same
        // ... (which we - painfully - test for just above.)
        byte[] resultCbor = rd.getAuthenticatedData();
        String pretty = Util.cborPrettyPrint(Util.canonicalizeCbor(resultCbor));
        assertEquals("{\n"
                + "  'org.example.barfoo' : {\n"
                + "    'Bar' : 'Foo',\n"
                + "    'Foo' : 'Bar'\n"
                + "  },\n"
                + "  'org.example.foobar' : {\n"
                + "    'Bar' : 'Foo',\n"
                + "    'Foo' : 'Bar'\n"
                + "  }\n"
                + "}", pretty);

        store.deleteCredentialByName("test");
    }

    @Test
    public void testProvisionAcpIdNotInValidRange() throws IdentityCredentialException {
        assumeTrue("IC HAL is not implemented", Util.isHalImplemented());

        Context appContext = InstrumentationRegistry.getTargetContext();
        IdentityCredentialStore store = IdentityCredentialStore.getInstance(appContext);

        WritableIdentityCredential wc =
                store.createCredential("testAcpNotInValidRange", "org.iso.18013-5.2019.mdl");

        Collection<X509Certificate> certificateChain =
                wc.getCredentialKeyCertificateChain("SomeChallenge".getBytes());

        // Profile 32 (no authentication) - invalid profile id
        AccessControlProfile noAuthProfile =
                new AccessControlProfile.Builder(new AccessControlProfileId(32))
                        .setUserAuthenticationRequired(false)
                        .build();

        Collection<AccessControlProfileId> idsNoAuth = new ArrayList<AccessControlProfileId>();
        idsNoAuth.add(new AccessControlProfileId(32));
        String mdlNs = "org.iso.18013-5.2019";
        PersonalizationData personalizationData =
                new PersonalizationData.Builder()
                        .addAccessControlProfile(noAuthProfile)
                        .putEntry("com.example.ns", "Name", idsNoAuth, Util.cborEncodeString("Alan"))
                        .build();

        // personalize() should fail because of the invalid profile id
        try {
            byte[] proofOfProvisioningSignature = wc.personalize(personalizationData);
            assertTrue(false);
        } catch (Exception e) {
            // This is the expected path...
        }
    }

    @Test
    public void testProvisionAcpIdNotStartingAtZero() throws
            IdentityCredentialException, CborException {
        assumeTrue("IC HAL is not implemented", Util.isHalImplemented());

        Context appContext = InstrumentationRegistry.getTargetContext();
        IdentityCredentialStore store = IdentityCredentialStore.getInstance(appContext);

        store.deleteCredentialByName("test");
        Collection<X509Certificate> certChain = createCredentialWithAcpId(store, "test", 1);

        IdentityCredential credential = store.getCredentialByName("test",
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);

        // Check that the read-back certChain matches the created one.
        Collection<X509Certificate> readBackCertChain =
                credential.getCredentialKeyCertificateChain();
        assertEquals(certChain.size(), readBackCertChain.size());
        Iterator<X509Certificate> it = readBackCertChain.iterator();
        for (X509Certificate expectedCert : certChain) {
            X509Certificate readBackCert = it.next();
            assertEquals(expectedCert, readBackCert);
        }

        Map<String, Collection<String>> entriesToRequest = new LinkedHashMap<>();
        entriesToRequest.put("org.iso.18013-5.2019",
                Arrays.asList("First name",
                        "Last name",
                        "Home address",
                        "Birth date",
                        "Cryptanalyst",
                        "Portrait image",
                        "Height",
                        "Neg Item",
                        "Int Two Bytes",
                        "Int Eight Bytes",
                        "Int Four Bytes",
                        "driving_privileges"));
        ResultData rd = credential.getEntries(
                Util.createItemsRequest(entriesToRequest, null),
                entriesToRequest,
                null,
                null);

        Collection<String> resultNamespaces = rd.getNamespaces();
        assertEquals(resultNamespaces.size(), 1);
        assertEquals("org.iso.18013-5.2019", resultNamespaces.iterator().next());
        assertEquals(12, rd.getEntryNames("org.iso.18013-5.2019").size());

        String ns = "org.iso.18013-5.2019";
        assertEquals("Alan", Util.getStringEntry(rd, ns, "First name"));
        assertEquals("Turing", Util.getStringEntry(rd, ns, "Last name"));
        assertEquals("Maida Vale, London, England", Util.getStringEntry(rd, ns, "Home address"));
        assertEquals("19120623", Util.getStringEntry(rd, ns, "Birth date"));
        assertEquals(true, Util.getBooleanEntry(rd, ns, "Cryptanalyst"));
        assertArrayEquals(new byte[]{0x01, 0x02},
                Util.getBytestringEntry(rd, ns, "Portrait image"));
        assertEquals(180, Util.getIntegerEntry(rd, ns, "Height"));
        assertEquals(-42, Util.getIntegerEntry(rd, ns, "Neg Item"));
        assertEquals(0x101, Util.getIntegerEntry(rd, ns, "Int Two Bytes"));
        assertEquals(0x10001, Util.getIntegerEntry(rd, ns, "Int Four Bytes"));
        assertEquals(0x100000001L, Util.getIntegerEntry(rd, ns, "Int Eight Bytes"));
        byte[] drivingPrivileges = getExampleDrivingPrivilegesCbor();
        assertArrayEquals(drivingPrivileges, rd.getEntry(ns, "driving_privileges"));

        assertEquals("{\n"
                + "  'org.iso.18013-5.2019' : {\n"
                + "    'Height' : 180,\n"
                + "    'Neg Item' : -42,\n"
                + "    'Last name' : 'Turing',\n"
                + "    'Birth date' : '19120623',\n"
                + "    'First name' : 'Alan',\n"
                + "    'Cryptanalyst' : true,\n"
                + "    'Home address' : 'Maida Vale, London, England',\n"
                + "    'Int Two Bytes' : 257,\n"
                + "    'Int Four Bytes' : 65537,\n"
                + "    'Portrait image' : [0x01, 0x02],\n"
                + "    'Int Eight Bytes' : 4294967297,\n"
                + "    'driving_privileges' : [\n"
                + "      {\n"
                + "        'value' : 42,\n"
                + "        'vehicle_category_code' : 'TODO'\n"
                + "      }\n"
                + "    ]\n"
                + "  }\n"
                + "}", Util.cborPrettyPrint(Util.canonicalizeCbor(rd.getAuthenticatedData())));

        store.deleteCredentialByName("test");
    }

}
