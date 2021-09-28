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

package com.android.cts.certinstaller;

import static com.google.common.truth.Truth.assertThat;

import android.security.KeyChain;
import android.security.KeyChainException;
import android.content.Context;
import android.test.InstrumentationTestCase;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.Signature;
import java.security.SignatureException;
import java.security.cert.X509Certificate;

public class PreSelectedKeyAccessTest extends InstrumentationTestCase {
    private static final String PRE_SELECTED_ALIAS = "pre-selected-rsa";
    @Override
    public void setUp() throws Exception {
        super.setUp();
    }

    // Test that this app can access pre-granted alias
    public void testAccessingPreSelectedAliasExpectingSuccess() throws
            KeyChainException, NoSuchAlgorithmException, InvalidKeyException, SignatureException,
            InterruptedException {
        PrivateKey privateKey = KeyChain.getPrivateKey(getContext(), PRE_SELECTED_ALIAS);
        assertThat(privateKey).isNotNull();
        String algoIdentifier = "SHA256withRSA";

        byte[] data = new String("hello").getBytes();
        Signature sign = Signature.getInstance(algoIdentifier);
        sign.initSign(privateKey);
        sign.update(data);
        byte[] signature = sign.sign();

        X509Certificate[] certs = KeyChain.getCertificateChain(getContext(), PRE_SELECTED_ALIAS);
        assertThat(certs).isNotEmpty();

        PublicKey publicKey = certs[0].getPublicKey();
        Signature verify = Signature.getInstance(algoIdentifier);
        verify.initVerify(publicKey);
        verify.update(data);
        assertThat(verify.verify(signature)).isTrue();
    }

    public void testAccessingPreSelectedAliasWithoutGrant() throws
            KeyChainException, InterruptedException {
        assertThat(KeyChain.getPrivateKey(getContext(), PRE_SELECTED_ALIAS)).isNull();
    }

    private Context getContext() {
        return getInstrumentation().getContext();
    }
}
