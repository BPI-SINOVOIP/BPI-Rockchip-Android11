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

import java.security.AlgorithmParameters;
import java.security.KeyPair;

import javax.crypto.Cipher;

public class RsaCipherPerformanceTest extends PerformanceTestBase {

    final int[] SUPPORTED_RSA_KEY_SIZES = {2048, 3072, 4096};
    final int[] TEST_MESSAGE_SIZES = {1 << 6, 1 << 10};

    public void testRSA_ECB_NoPadding() throws Exception {
        testRsaCipher("RSA/ECB/NoPadding", SUPPORTED_RSA_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testRSA_ECB_PKCS1Padding() throws Exception {
        testRsaCipher("RSA/ECB/PKCS1Padding", SUPPORTED_RSA_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testRSA_ECB_OAEPWithSHA_1AndMGF1Padding() throws Exception {
        testRsaCipher(
                "RSA/ECB/OAEPWithSHA-1AndMGF1Padding", SUPPORTED_RSA_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testRSA_ECB_OAEPPadding() throws Exception {
        testRsaCipher("RSA/ECB/OAEPPadding", SUPPORTED_RSA_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    private void testRsaCipher(String algorithm, int[] keySizes, int[] messageSizes)
            throws Exception {
        for (int keySize : keySizes) {
            int maxMessageSize =
                    TestUtils.getMaxSupportedPlaintextInputSizeBytes(algorithm, keySize);
            for (int messageSize : messageSizes) {
                if (messageSize > maxMessageSize) {
                    continue;
                }
                measure(
                        new KeystoreRsaEncryptMeasurable(
                                new AndroidKeystoreRsaKeyGenerator(algorithm, keySize),
                                keySize,
                                messageSize),
                        new KeystoreRsaDecryptMeasurable(
                                new AndroidKeystoreRsaKeyGenerator(algorithm, keySize),
                                keySize,
                                messageSize),
                        new KeystoreRsaEncryptMeasurable(
                                new DefaultKeystoreKeyPairGenerator(algorithm, keySize),
                                keySize,
                                messageSize),
                        new KeystoreRsaDecryptMeasurable(
                                new DefaultKeystoreKeyPairGenerator(algorithm, keySize),
                                keySize,
                                messageSize));
            }
        }
    }

    private class AndroidKeystoreRsaKeyGenerator extends AndroidKeystoreKeyGenerator {

        AndroidKeystoreRsaKeyGenerator(String algorithm, int keySize) throws Exception {
            super(algorithm);
            String digest = TestUtils.getCipherDigest(algorithm);
            getKeyPairGenerator()
                    .initialize(
                            getKeyGenParameterSpecBuilder(
                                            KeyProperties.PURPOSE_ENCRYPT
                                                    | KeyProperties.PURPOSE_DECRYPT)
                                    .setBlockModes(TestUtils.getCipherBlockMode(algorithm))
                                    .setEncryptionPaddings(
                                            TestUtils.getCipherEncryptionPadding(algorithm))
                                    .setRandomizedEncryptionRequired(false)
                                    .setKeySize(keySize)
                                    .setDigests(
                                            (digest != null)
                                                    ? new String[] {digest}
                                                    : EmptyArray.STRING)
                                    .build());
        }
    }

    private class KeystoreRsaEncryptMeasurable extends KeystoreMeasurable {
        private final Cipher mCipher;
        private KeyPair mKey;

        KeystoreRsaEncryptMeasurable(
                KeystoreKeyGenerator keyGenerator, int keySize, int messageSize) throws Exception {
            super(keyGenerator, "encrypt", keySize, messageSize);
            mCipher = Cipher.getInstance(getAlgorithm());
        }

        @Override
        public void initialSetUp() throws Exception {
            mKey = generateKeyPair();
        }

        @Override
        public void setUp() throws Exception {
            mCipher.init(Cipher.ENCRYPT_MODE, mKey.getPublic());
        }

        @Override
        public void measure() throws Exception {
            mCipher.doFinal(getMessage());
        }
    }

    private class KeystoreRsaDecryptMeasurable extends KeystoreMeasurable {
        private final Cipher mCipher;
        private byte[] mEncryptedMessage;
        private AlgorithmParameters mAlgorithmParams;
        private KeyPair mKey;

        KeystoreRsaDecryptMeasurable(
                KeystoreKeyGenerator keyGenerator, int keySize, int messageSize) throws Exception {
            super(keyGenerator, "decrypt", keySize, messageSize);
            mCipher = Cipher.getInstance(getAlgorithm());
        }

        @Override
        public void initialSetUp() throws Exception {
            mKey = generateKeyPair();
            mCipher.init(Cipher.ENCRYPT_MODE, mKey.getPublic());
            mEncryptedMessage = mCipher.doFinal(getMessage());
            mAlgorithmParams = mCipher.getParameters();
        }

        @Override
        public void setUp() throws Exception {
            mCipher.init(Cipher.DECRYPT_MODE, mKey.getPrivate(), mAlgorithmParams);
        }

        @Override
        public void measure() throws Exception {
            mCipher.doFinal(mEncryptedMessage);
        }
    }
}
