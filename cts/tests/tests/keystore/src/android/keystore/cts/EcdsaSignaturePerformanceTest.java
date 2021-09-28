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
 * limitations under the License
 */

package android.keystore.cts;

import android.security.keystore.KeyProperties;

import org.junit.Test;

import java.security.KeyPair;
import java.security.Signature;
import java.security.spec.ECGenParameterSpec;

public class EcdsaSignaturePerformanceTest extends PerformanceTestBase {

    final int[] SUPPORTED_KEY_SIZES = {224, 256, 384, 521};
    final int[] TEST_MESSAGE_SIZES = {1 << 6, 1 << 10, 1 << 17};

    public void testNONEwithECDSA() throws Exception {
        testEcdsaSign("NONEwithECDSA", TEST_MESSAGE_SIZES);
    }

    public void testSHA1withECDSA() throws Exception {
        testEcdsaSign("SHA1withECDSA", TEST_MESSAGE_SIZES);
    }

    public void testSHA224withECDSA() throws Exception {
        testEcdsaSign("SHA224withECDSA", TEST_MESSAGE_SIZES);
    }

    public void testSHA256withECDSA() throws Exception {
        testEcdsaSign("SHA256withECDSA", TEST_MESSAGE_SIZES);
    }

    public void testSHA384withECDSA() throws Exception {
        testEcdsaSign("SHA384withECDSA", TEST_MESSAGE_SIZES);
    }

    public void testSHA512withECDSA() throws Exception {
        testEcdsaSign("SHA512withECDSA", TEST_MESSAGE_SIZES);
    }

    private void testEcdsaSign(String algorithm, int[] messageSizes) throws Exception {
        for (int keySize : SUPPORTED_KEY_SIZES) {
            KeystoreKeyGenerator androidKeystoreEcGenerator =
                    new AndroidKeystoreEcKeyGenerator(algorithm, keySize);
            KeystoreKeyGenerator defaultKeystoreEcGenerator =
                    new DefaultKeystoreEcKeyGenerator(algorithm, keySize);
            for (int messageSize : messageSizes) {
                measure(
                        new KeystoreEcSignMeasurable(
                                androidKeystoreEcGenerator, keySize, messageSize),
                        new KeystoreEcVerifyMeasurable(
                                androidKeystoreEcGenerator, keySize, messageSize),
                        new KeystoreEcSignMeasurable(
                                defaultKeystoreEcGenerator, keySize, messageSize),
                        new KeystoreEcVerifyMeasurable(
                                defaultKeystoreEcGenerator, keySize, messageSize));
            }
        }
    }

    private class DefaultKeystoreEcKeyGenerator extends KeystoreKeyGenerator {

        DefaultKeystoreEcKeyGenerator(String algorithm, int keySize) throws Exception {
            super(algorithm);
            getKeyPairGenerator().initialize(keySize);
        }
    }

    private class AndroidKeystoreEcKeyGenerator extends AndroidKeystoreKeyGenerator {

        AndroidKeystoreEcKeyGenerator(String algorithm, int keySize) throws Exception {
            super(algorithm);
            getKeyPairGenerator()
                    .initialize(
                            getKeyGenParameterSpecBuilder(
                                            KeyProperties.PURPOSE_SIGN
                                                    | KeyProperties.PURPOSE_VERIFY)
                                    .setKeySize(keySize)
                                    .setDigests(TestUtils.getSignatureAlgorithmDigest(algorithm))
                                    .build());
        }
    }

    private class KeystoreEcSignMeasurable extends KeystoreMeasurable {
        private final Signature mSignature;
        private KeyPair mKey;

        KeystoreEcSignMeasurable(KeystoreKeyGenerator keyGen, int keySize, int messageSize)
                throws Exception {
            super(keyGen, "sign", keySize, messageSize);
            mSignature = Signature.getInstance(getAlgorithm());
        }

        @Override
        public void initialSetUp() throws Exception {
            mKey = generateKeyPair();
        }

        @Override
        public void setUp() throws Exception {
            mSignature.initSign(mKey.getPrivate());
        }

        @Override
        public void measure() throws Exception {
            mSignature.update(getMessage());
            mSignature.sign();
        }
    }

    private class KeystoreEcVerifyMeasurable extends KeystoreMeasurable {
        private byte[] mMessageSignature;
        private final Signature mSignature;
        private KeyPair mKey;

        KeystoreEcVerifyMeasurable(KeystoreKeyGenerator keyGen, int keySize, int messageSize)
                throws Exception {
            super(keyGen, "verify", keySize, messageSize);
            mSignature = Signature.getInstance(getAlgorithm());
        }

        @Override
        public void initialSetUp() throws Exception {
            mKey = generateKeyPair();
            mSignature.initSign(mKey.getPrivate());
            mSignature.update(getMessage());
            mMessageSignature = mSignature.sign();
        }

        @Override
        public void setUp() throws Exception {
            mSignature.initVerify(mKey.getPublic());
        }

        @Override
        public void measure() throws Exception {
            mSignature.update(getMessage());
            mSignature.verify(mMessageSignature);
        }
    }
}
