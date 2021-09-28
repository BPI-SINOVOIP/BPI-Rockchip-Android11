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
import java.security.Signature;
import java.security.spec.ECGenParameterSpec;

public abstract class AbstractUserAuthenticationSignatureTest
        extends AbstractUserAuthenticationTest {
    private Signature mSignature;

    @Override
    void createUserAuthenticationKey(String keyName, int timeout, int authType,
            boolean useStrongBox) throws Exception {
        KeyGenParameterSpec.Builder builder = new KeyGenParameterSpec.Builder(
                keyName, KeyProperties.PURPOSE_SIGN);
        builder.setDigests(KeyProperties.DIGEST_SHA256, KeyProperties.DIGEST_SHA512)
                .setAlgorithmParameterSpec(new ECGenParameterSpec("secp256r1"))
                .setUserAuthenticationRequired(true)
                .setIsStrongBoxBacked(useStrongBox)
                .setUserAuthenticationParameters(timeout, authType);

        KeyPairGenerator keyPairGenerator = KeyPairGenerator.getInstance(
                KeyProperties.KEY_ALGORITHM_EC, "AndroidKeyStore");
        keyPairGenerator.initialize(builder.build());
        keyPairGenerator.generateKeyPair();
    }

    @Override
    void initializeKeystoreOperation(String keyName) throws Exception {
        mSignature = Utils.initSignature(keyName);
    }

    @Override
    BiometricPrompt.CryptoObject getCryptoObject() {
        return new BiometricPrompt.CryptoObject(mSignature);
    }

    @Override
    void doKeystoreOperation(byte[] payload) throws Exception {
        try {
            Utils.doSign(mSignature, payload);
        } finally {
            mSignature = null;
        }
    }
}
