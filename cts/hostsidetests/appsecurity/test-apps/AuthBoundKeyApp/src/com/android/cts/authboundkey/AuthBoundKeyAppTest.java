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

package com.android.cts.authboundkeyapp;

import static junit.framework.Assert.assertFalse;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyInfo;
import android.security.keystore.KeyPermanentlyInvalidatedException;
import android.security.keystore.KeyProperties;
import android.test.AndroidTestCase;
import android.test.MoreAsserts;
import android.util.Log;

import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.KeyStore;
import java.security.PrivateKey;
import java.security.UnrecoverableKeyException;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.SecretKeyFactory;

public class AuthBoundKeyAppTest extends AndroidTestCase {
    private static final String KEY_NAME = "nice_key";
    private static final String KEYSTORE = "AndroidKeyStore";
    public void testGenerateAuthBoundKey() throws Exception {
        KeyStore keyStore = KeyStore.getInstance(KEYSTORE);
        keyStore.load(null);
        KeyGenerator keyGenerator = KeyGenerator.getInstance(
                KeyProperties.KEY_ALGORITHM_AES, KEYSTORE);
        KeyGenParameterSpec spec = new KeyGenParameterSpec.Builder(
                        KEY_NAME, KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                        .setBlockModes(KeyProperties.BLOCK_MODE_CBC)
                        .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_PKCS7)
                        .setUserAuthenticationRequired(true)
                        .setUserAuthenticationParameters(0, KeyProperties.AUTH_DEVICE_CREDENTIAL)
                        .build();
        assertEquals(spec.getUserAuthenticationType(), KeyProperties.AUTH_DEVICE_CREDENTIAL);
        keyGenerator.init(spec);
        SecretKey key = keyGenerator.generateKey();
        SecretKeyFactory keyFactory = SecretKeyFactory.getInstance(key.getAlgorithm(),
                                                                   "AndroidKeyStore");
        KeyInfo info = (KeyInfo) keyFactory.getKeySpec(key, KeyInfo.class);
        assertEquals(0, info.getUserAuthenticationValidityDurationSeconds());
        assertEquals(KeyProperties.AUTH_DEVICE_CREDENTIAL, info.getUserAuthenticationType());
    }

    public void testUseKey() throws Exception {
        KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
        keyStore.load(null);
        try {
            SecretKey secretKey = (SecretKey) keyStore.getKey(KEY_NAME, null);
        } catch (UnrecoverableKeyException e) {
            // This is correct behavior
            return;
        }
        fail("Expected an UnrecoverableKeyException");
    }

}
