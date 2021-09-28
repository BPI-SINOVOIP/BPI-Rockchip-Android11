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

package com.android.cts.verifier.biometrics;

import android.hardware.biometrics.BiometricPrompt;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;

import java.security.KeyPairGenerator;
import java.security.KeyStore;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;

public abstract class AbstractUserAuthenticationCipherTest extends AbstractUserAuthenticationTest {
    private Cipher mCipher;

    @Override
    void createUserAuthenticationKey(String keyName, int timeout, int authType,
            boolean useStrongBox) throws Exception {
        KeyGenParameterSpec.Builder builder = new KeyGenParameterSpec.Builder(
                keyName, KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT);
        builder.setBlockModes(KeyProperties.BLOCK_MODE_CBC)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_PKCS7)
                .setUserAuthenticationRequired(true)
                .setUserAuthenticationParameters(timeout, authType)
                .setIsStrongBoxBacked(useStrongBox);

        KeyGenerator keyGenerator = KeyGenerator.getInstance(
                KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
        keyGenerator.init(builder.build());
        keyGenerator.generateKey();
    }

    @Override
    void initializeKeystoreOperation(String keyName) throws Exception {
        mCipher = Utils.initCipher(keyName);
    }

    @Override
    BiometricPrompt.CryptoObject getCryptoObject() {
        return new BiometricPrompt.CryptoObject(mCipher);
    }

    @Override
    void doKeystoreOperation(byte[] payload) throws Exception {
        try {
            Utils.doEncrypt(mCipher, payload);
        } finally {
            mCipher = null;
        }
    }
}
