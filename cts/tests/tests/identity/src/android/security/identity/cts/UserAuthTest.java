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

import static android.security.identity.IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256;

import android.security.identity.AccessControlProfile;
import android.security.identity.AccessControlProfileId;
import android.security.identity.AlreadyPersonalizedException;
import android.security.identity.PersonalizationData;
import android.security.identity.IdentityCredential;
import android.security.identity.IdentityCredentialException;
import android.security.identity.IdentityCredentialStore;
import android.security.identity.WritableIdentityCredential;
import android.security.identity.ResultData;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import android.content.Context;
import android.os.SystemClock;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import android.app.KeyguardManager;
import android.server.wm.ActivityManagerTestBase;

import org.junit.Test;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.KeyPair;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.security.Signature;
import java.security.SignatureException;
import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.LinkedList;
import java.util.Map;

import co.nstant.in.cbor.CborBuilder;
import co.nstant.in.cbor.CborEncoder;
import co.nstant.in.cbor.CborException;

import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.security.PrivateKey;
import java.security.UnrecoverableEntryException;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.KeyGenerator;
import javax.crypto.NoSuchPaddingException;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.security.keystore.UserNotAuthenticatedException;


public class UserAuthTest {
    private static final String TAG = "UserAuthTest";

    private class DeviceLockSession extends ActivityManagerTestBase implements AutoCloseable {

        private LockScreenSession mLockCredential;

        public DeviceLockSession() throws Exception {
            mLockCredential = new LockScreenSession();
            mLockCredential.setLockCredential();
        }

        public void performDeviceLock() {
            mLockCredential.sleepDevice();
            Context appContext = InstrumentationRegistry.getTargetContext();
            KeyguardManager keyguardManager = (KeyguardManager)appContext.
                                              getSystemService(Context.KEYGUARD_SERVICE);
            for (int i = 0; i < 25 && !keyguardManager.isDeviceLocked(); i++) {
                SystemClock.sleep(200);
            }
        }

        public void performDeviceUnlock() throws Exception {
            mLockCredential.gotoKeyguard();
            mLockCredential.enterAndConfirmLockCredential();
            launchHomeActivity();
        }

        @Override
        public void close() throws Exception {
            mLockCredential.close();
        }
    }

    private boolean checkAuthBoundKey(String alias) {
        // Unfortunately there are no APIs to tell if a key needs user authentication to work so
        // we check if the key is available by simply trying to encrypt some data.
        try {
            KeyStore ks = KeyStore.getInstance("AndroidKeyStore");
            ks.load(null);
            KeyStore.Entry entry = ks.getEntry(alias, null);
            SecretKey secretKey = ((KeyStore.SecretKeyEntry) entry).getSecretKey();

            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.ENCRYPT_MODE, secretKey);
            byte[] clearText = {0x01, 0x02};
            byte[] cipherText = cipher.doFinal(clearText);
            return true;
        } catch (UserNotAuthenticatedException e) {
            return false;
        } catch (Exception e) {
            throw new RuntimeException("Failed!", e);
        }
    }

    void createAuthBoundKey(String alias, int timeoutSeconds) {
        try {
            KeyGenerator kg = KeyGenerator.getInstance(
                KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
            KeyGenParameterSpec.Builder builder =
                    new KeyGenParameterSpec.Builder(
                        alias,
                        KeyProperties.PURPOSE_ENCRYPT| KeyProperties.PURPOSE_DECRYPT)
                            .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                            .setUserAuthenticationRequired(true)
                            .setUserAuthenticationValidityDurationSeconds(timeoutSeconds)
                            .setKeySize(128);
            kg.init(builder.build());
            kg.generateKey();
        } catch (InvalidAlgorithmParameterException
                | NoSuchAlgorithmException
                | NoSuchProviderException e) {
            throw new RuntimeException("Error creating auth-bound key", e);
        }
    }

    @Test
    public void testUserAuth() throws Exception {
        String alias = "authbound";

        try (DeviceLockSession dl = new DeviceLockSession()) {
            Context appContext = InstrumentationRegistry.getTargetContext();
            KeyguardManager keyguardManager = (KeyguardManager)appContext
                                              .getSystemService(Context.KEYGUARD_SERVICE);

            doTestUserAuth(dl, keyguardManager);
        } catch (org.junit.AssumptionViolatedException e) {
            /* do nothing */
        }
    }

    void doTestUserAuth(DeviceLockSession dl, KeyguardManager keyguardManager) throws Exception {
        assumeTrue("IC HAL is not implemented", Util.isHalImplemented());

        // This test creates two different access control profiles:
        //
        // - free for all
        // - user authentication with 10 second timeout
        //
        // and for each ACP, a single entry which is protected by only that ACP.
        //
        // Note that we cannot test authentication on every reader session (e.g. timeout set to 0)
        // due to limitations of testing harness (only BiometricPrompt takes a CryptoObject).
        //

        // Provision the credential.
        Context appContext = InstrumentationRegistry.getTargetContext();
        IdentityCredentialStore store = IdentityCredentialStore.getInstance(appContext);

        store.deleteCredentialByName("test");
        WritableIdentityCredential wc = store.createCredential("test",
                "org.iso.18013-5.2019.mdl");
        // Profile 1 (user auth with 10000 msec timeout)
        AccessControlProfile authTenSecTimeoutProfile =
                new AccessControlProfile.Builder(new AccessControlProfileId(1))
                        .setUserAuthenticationRequired(true)
                        .setUserAuthenticationTimeout(10000)
                        .build();
        // Profile 0 (free for all)
        AccessControlProfile freeForAllProfile =
                new AccessControlProfile.Builder(new AccessControlProfileId(0))
                        .setUserAuthenticationRequired(false)
                        .build();
        // (We add the profiles in this weird order - 1, 0 - to check below that the
        // provisioning receipt lists them in the same order.)
        Collection<AccessControlProfileId> idsProfile0 = new ArrayList<AccessControlProfileId>();
        idsProfile0.add(new AccessControlProfileId(0));
        Collection<AccessControlProfileId> idsProfile1 = new ArrayList<AccessControlProfileId>();
        idsProfile1.add(new AccessControlProfileId(1));

        String mdlNs = "org.iso.18013-5.2019";
        PersonalizationData personalizationData =
                new PersonalizationData.Builder()
                        .addAccessControlProfile(authTenSecTimeoutProfile)
                        .addAccessControlProfile(freeForAllProfile)
                        .putEntry(mdlNs, "Accessible to all (0)", idsProfile0,
                                Util.cborEncodeString("foo0"))
                        .putEntry(mdlNs, "Accessible to auth-with-10-sec-timeout (1)", idsProfile1,
                                Util.cborEncodeString("foo1"))
                        .build();
        byte[] proofOfProvisioningSignature = wc.personalize(personalizationData);
        byte[] proofOfProvisioning = Util.coseSign1GetData(proofOfProvisioningSignature);

        String pretty = Util.cborPrettyPrint(proofOfProvisioning);
        Log.d(TAG, "pretty: " + pretty);
        assertEquals("[\n"
                + "  'ProofOfProvisioning',\n"
                + "  'org.iso.18013-5.2019.mdl',\n"
                + "  [\n"
                + "    {\n"
                + "      'id' : 1,\n"
                + "      'userAuthenticationRequired' : true,\n"
                + "      'timeoutMillis' : 10000\n"
                + "    },\n"
                + "    {\n"
                + "      'id' : 0\n"
                + "    }\n"
                + "  ],\n"
                + "  {\n"
                + "    'org.iso.18013-5.2019' : [\n"
                + "      {\n"
                + "        'name' : 'Accessible to all (0)',\n"
                + "        'value' : 'foo0',\n"
                + "        'accessControlProfiles' : [0]\n"
                + "      },\n"
                + "      {\n"
                + "        'name' : 'Accessible to auth-with-10-sec-timeout (1)',\n"
                + "        'value' : 'foo1',\n"
                + "        'accessControlProfiles' : [1]\n"
                + "      }\n"
                + "    ]\n"
                + "  },\n"
                + "  false\n"
                + "]", pretty);

        // Get the credential we'll be reading from and provision it with a sufficient number
        // of dynamic auth keys
        IdentityCredential credential = store.getCredentialByName("test",
                CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);
        assertNotNull(credential);
        credential.setAvailableAuthenticationKeys(1, 10);
        Collection<X509Certificate> dynAuthKeyCerts = credential.getAuthKeysNeedingCertification();
        credential.storeStaticAuthenticationData(dynAuthKeyCerts.iterator().next(), new byte[0]);

        // Sleep at least the timeout to ensure that the deadline has been reached.
        //
        // Then we test that
        //
        //  "Accessible to auth-with-10-sec-timeout (1)"
        //
        // is not available.
        SystemClock.sleep(11 * 1000);

        Collection<String> entryNames;
        Collection<String> resultNamespaces;
        ResultData rd;

        Map<String, Collection<String>> entriesToRequest = new LinkedHashMap<>();
        entriesToRequest.put("org.iso.18013-5.2019",
                Arrays.asList("Accessible to all (0)",
                        "Accessible to auth-with-10-sec-timeout (1)"));

        rd = credential.getEntries(
            Util.createItemsRequest(entriesToRequest, null),
            entriesToRequest,
            null,  // sessionTranscript
            null); // readerSignature
        resultNamespaces = rd.getNamespaces();
        assertEquals(1, resultNamespaces.size());
        assertEquals("org.iso.18013-5.2019", resultNamespaces.iterator().next());
        entryNames = rd.getEntryNames("org.iso.18013-5.2019");
        assertEquals(2, entryNames.size());
        assertTrue(entryNames.contains("Accessible to all (0)"));
        assertTrue(entryNames.contains("Accessible to auth-with-10-sec-timeout (1)"));
        assertEquals(ResultData.STATUS_OK,
                rd.getStatus("org.iso.18013-5.2019", "Accessible to all (0)"));
        assertEquals(ResultData.STATUS_USER_AUTHENTICATION_FAILED,
                rd.getStatus("org.iso.18013-5.2019", "Accessible to auth-with-10-sec-timeout (1)"));
        assertEquals("foo0",
                Util.getStringEntry(rd, "org.iso.18013-5.2019", "Accessible to all (0)"));
        assertEquals(null,
                rd.getEntry("org.iso.18013-5.2019", "Accessible to auth-with-10-sec-timeout (1)"));

        // Now we lock and unlock the screen... this should make
        //
        //  Accessible to auth-with-10-sec-timeout (1)
        //
        // available. We check that.
        dl.performDeviceLock();
        assertTrue(keyguardManager.isDeviceLocked());
        dl.performDeviceUnlock();
        assertTrue(!keyguardManager.isDeviceLocked());

        credential = store.getCredentialByName("test",
                CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);
        assertNotNull(credential);
        rd = credential.getEntries(Util.createItemsRequest(entriesToRequest, null),
                entriesToRequest,
                null,  // sessionTranscript
                null); // readerSignature
        resultNamespaces = rd.getNamespaces();
        assertEquals(1, resultNamespaces.size());
        assertEquals("org.iso.18013-5.2019", resultNamespaces.iterator().next());
        entryNames = rd.getEntryNames("org.iso.18013-5.2019");
        assertEquals(2, entryNames.size());
        assertTrue(entryNames.contains("Accessible to all (0)"));
        assertTrue(entryNames.contains("Accessible to auth-with-10-sec-timeout (1)"));
        assertEquals(ResultData.STATUS_OK,
                rd.getStatus("org.iso.18013-5.2019", "Accessible to all (0)"));
        assertEquals(ResultData.STATUS_OK,
                rd.getStatus("org.iso.18013-5.2019", "Accessible to auth-with-10-sec-timeout (1)"));
        assertEquals("foo0",
                Util.getStringEntry(rd, "org.iso.18013-5.2019", "Accessible to all (0)"));
        assertEquals("foo1",
                Util.getStringEntry(rd, "org.iso.18013-5.2019",
                        "Accessible to auth-with-10-sec-timeout (1)"));

        // Now we again sleep at least the timeout to ensure that the deadline has been reached.
        //
        // Then we test that
        //
        //  "Accessible to auth-with-10-sec-timeout (1)"
        //
        // is not available.
        SystemClock.sleep(11 * 1000);

        credential = store.getCredentialByName("test",
                CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);
        assertNotNull(credential);
        rd = credential.getEntries(
            Util.createItemsRequest(entriesToRequest, null),
            entriesToRequest,
            null,  // sessionTranscript
            null); // readerSignature
        resultNamespaces = rd.getNamespaces();
        assertEquals(1, resultNamespaces.size());
        assertEquals("org.iso.18013-5.2019", resultNamespaces.iterator().next());
        entryNames = rd.getEntryNames("org.iso.18013-5.2019");
        assertEquals(2, entryNames.size());
        assertTrue(entryNames.contains("Accessible to all (0)"));
        assertTrue(entryNames.contains("Accessible to auth-with-10-sec-timeout (1)"));
        assertEquals(ResultData.STATUS_OK,
                rd.getStatus("org.iso.18013-5.2019", "Accessible to all (0)"));
        assertEquals(ResultData.STATUS_USER_AUTHENTICATION_FAILED,
                rd.getStatus("org.iso.18013-5.2019", "Accessible to auth-with-10-sec-timeout (1)"));
        assertEquals("foo0",
                Util.getStringEntry(rd, "org.iso.18013-5.2019", "Accessible to all (0)"));
        assertEquals(null,
                rd.getEntry("org.iso.18013-5.2019", "Accessible to auth-with-10-sec-timeout (1)"));
    }

}
