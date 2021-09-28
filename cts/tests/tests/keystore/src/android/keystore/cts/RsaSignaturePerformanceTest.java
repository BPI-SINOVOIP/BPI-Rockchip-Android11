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
import java.util.Arrays;

public class RsaSignaturePerformanceTest extends PerformanceTestBase {

    final int[] SUPPORTED_RSA_KEY_SIZES = {2048, 3072, 4096};
    final int[] TEST_MESSAGE_SIZES = {1 << 6, 1 << 10, 1 << 17};

    public void testNONEwithRSA() throws Exception {
        for (int keySize : SUPPORTED_RSA_KEY_SIZES) {
            int modulusSizeBytes = (keySize + 7) / 8;
            int[] messageSizes =
                    Arrays.stream(TEST_MESSAGE_SIZES).filter(x -> modulusSizeBytes > x).toArray();
            testRsaSign("NONEwithRSA", new int[] {keySize}, messageSizes);
        }
    }

    public void testMD5withRSA() throws Exception {
        testRsaSign("MD5withRSA", SUPPORTED_RSA_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testSHA1withRSA() throws Exception {
        testRsaSign("SHA1withRSA", SUPPORTED_RSA_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testSHA224withRSA() throws Exception {
        testRsaSign("SHA224withRSA", SUPPORTED_RSA_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testSHA256withRSA() throws Exception {
        testRsaSign("SHA256withRSA", SUPPORTED_RSA_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testSHA384withRSA() throws Exception {
        testRsaSign("SHA384withRSA", SUPPORTED_RSA_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testSHA512withRSA() throws Exception {
        testRsaSign("SHA512withRSA", SUPPORTED_RSA_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testSHA1withRSA_PSS() throws Exception {
        testRsaSign("SHA1withRSA/PSS", SUPPORTED_RSA_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testSHA224withRSA_PSS() throws Exception {
        testRsaSign("SHA224withRSA/PSS", SUPPORTED_RSA_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testSHA256withRSA_PSS() throws Exception {
        testRsaSign("SHA256withRSA/PSS", SUPPORTED_RSA_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testSHA384withRSA_PSS() throws Exception {
        testRsaSign("SHA384withRSA/PSS", SUPPORTED_RSA_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testSHA512withRSA_PSS() throws Exception {
        testRsaSign("SHA512withRSA/PSS", SUPPORTED_RSA_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    private void testRsaSign(String algorithm, int[] keySizes, int[] messageSizes)
            throws Exception {
        for (int keySize : keySizes) {
            if (!TestUtils.isKeyLongEnoughForSignatureAlgorithm(algorithm, keySize)) {
                continue;
            }
            KeystoreKeyGenerator androidKeystoreRsaGenerator =
                    new AndroidKeystoreRsaKeyGenerator(algorithm, keySize);
            KeystoreKeyGenerator defaultKeystoreRsaGenerator =
                    new DefaultKeystoreKeyPairGenerator(algorithm, keySize);
            for (int messageSize : messageSizes) {
                measure(
                        new KeystoreRsaSignMeasurable(
                                androidKeystoreRsaGenerator, keySize, messageSize),
                        new KeystoreRsaSignMeasurable(
                                defaultKeystoreRsaGenerator, keySize, messageSize),
                        new KeystoreRsaVerifyMeasurable(
                                androidKeystoreRsaGenerator, keySize, messageSize),
                        new KeystoreRsaVerifyMeasurable(
                                defaultKeystoreRsaGenerator, keySize, messageSize));
            }
        }
    }

    private class AndroidKeystoreRsaKeyGenerator extends AndroidKeystoreKeyGenerator {

        AndroidKeystoreRsaKeyGenerator(String algorithm, int keySize) throws Exception {
            super(algorithm);
            getKeyPairGenerator()
                    .initialize(
                            getKeyGenParameterSpecBuilder(
                                            KeyProperties.PURPOSE_SIGN
                                                    | KeyProperties.PURPOSE_VERIFY)
                                    .setKeySize(keySize)
                                    .setSignaturePaddings(
                                            TestUtils.getSignatureAlgorithmPadding(algorithm))
                                    .setDigests(TestUtils.getSignatureAlgorithmDigest(algorithm))
                                    .build());
        }
    }

    private class KeystoreRsaSignMeasurable extends KeystoreMeasurable {
        private final Signature mSignature;
        private KeyPair mKey;

        KeystoreRsaSignMeasurable(KeystoreKeyGenerator keyGen, int keySize, int messageSize)
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

    private class KeystoreRsaVerifyMeasurable extends KeystoreMeasurable {
        private final Signature mSignature;
        private byte[] mMessageSignature;
        private KeyPair mKey;

        KeystoreRsaVerifyMeasurable(KeystoreKeyGenerator keyGen, int keySize, int messageSize)
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
