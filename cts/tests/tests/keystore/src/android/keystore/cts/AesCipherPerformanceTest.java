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

import javax.crypto.Cipher;
import javax.crypto.SecretKey;

public class AesCipherPerformanceTest extends PerformanceTestBase {

    final int[] SUPPORTED_AES_KEY_SIZES = {128, 256};
    final int[] TEST_MESSAGE_SIZES = {1 << 6, 1 << 10, 1 << 17};

    public void testAES_CBC_NoPadding() throws Exception {
        testAesCipher("AES/CBC/NoPadding", SUPPORTED_AES_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testAES_CBC_PKCS7Padding() throws Exception {
        testAesCipher("AES/CBC/PKCS7Padding", SUPPORTED_AES_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testAES_CTR_NoPadding() throws Exception {
        testAesCipher("AES/CTR/NoPadding", SUPPORTED_AES_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testAES_ECB_NoPadding() throws Exception {
        testAesCipher("AES/ECB/NoPadding", SUPPORTED_AES_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testAES_ECB_PKCS7Padding() throws Exception {
        testAesCipher("AES/ECB/PKCS7Padding", SUPPORTED_AES_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testAES_GCM_NoPadding() throws Exception {
        testAesCipher("AES/GCM/NoPadding", SUPPORTED_AES_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    private void testAesCipher(String algorithm, int[] keySizes, int[] messageSizes)
            throws Exception {
        for (int keySize : keySizes) {
            KeystoreKeyGenerator androidKeystoreAesGenerator =
                    new AndroidKeystoreAesKeyGenerator(algorithm, keySize);
            KeystoreKeyGenerator defaultKeystoreAesGenerator =
                    new DefaultKeystoreSecretKeyGenerator(algorithm, keySize);
            for (int messageSize : messageSizes) {
                measure(
                        new KeystoreAesEncryptMeasurable(
                                androidKeystoreAesGenerator, keySize, messageSize),
                        new KeystoreAesEncryptMeasurable(
                                defaultKeystoreAesGenerator, keySize, messageSize),
                        new KeystoreAesDecryptMeasurable(
                                androidKeystoreAesGenerator, keySize, messageSize),
                        new KeystoreAesDecryptMeasurable(
                                defaultKeystoreAesGenerator, keySize, messageSize));
            }
        }
    }

    private class AndroidKeystoreAesKeyGenerator extends AndroidKeystoreKeyGenerator {
        AndroidKeystoreAesKeyGenerator(String algorithm, int keySize) throws Exception {
            super(algorithm);
            getSecretKeyGenerator()
                    .init(
                            getKeyGenParameterSpecBuilder(
                                            KeyProperties.PURPOSE_ENCRYPT
                                                    | KeyProperties.PURPOSE_DECRYPT)
                                    .setBlockModes(TestUtils.getCipherBlockMode(algorithm))
                                    .setEncryptionPaddings(
                                            TestUtils.getCipherEncryptionPadding(algorithm))
                                    .setRandomizedEncryptionRequired(false)
                                    .setKeySize(keySize)
                                    .build());
        }
    }

    private class KeystoreAesEncryptMeasurable extends KeystoreMeasurable {
        private final Cipher mCipher;
        private SecretKey mKey;

        KeystoreAesEncryptMeasurable(
                KeystoreKeyGenerator keyGenerator, int keySize, int messageSize) throws Exception {
            super(keyGenerator, "encrypt", keySize, messageSize);
            mCipher = Cipher.getInstance(getAlgorithm());
        }

        @Override
        public void initialSetUp() throws Exception {
            mKey = generateSecretKey();
        }

        @Override
        public void setUp() throws Exception {
            mCipher.init(Cipher.ENCRYPT_MODE, mKey);
        }

        @Override
        public void measure() throws Exception {
            mCipher.doFinal(getMessage());
        }
    }

    private class KeystoreAesDecryptMeasurable extends KeystoreMeasurable {
        private final Cipher mCipher;
        private byte[] mEncryptedMessage;
        private AlgorithmParameters mParameters;
        private SecretKey mKey;

        KeystoreAesDecryptMeasurable(
                KeystoreKeyGenerator keyGenerator, int keySize, int messageSize) throws Exception {
            super(keyGenerator, "decrypt", keySize, messageSize);
            mCipher = Cipher.getInstance(getAlgorithm());
        }

        @Override
        public void initialSetUp() throws Exception {
            mKey = generateSecretKey();
            mCipher.init(Cipher.ENCRYPT_MODE, mKey);
            mEncryptedMessage = mCipher.doFinal(getMessage());
            mParameters = mCipher.getParameters();
        }

        @Override
        public void setUp() throws Exception {
            mCipher.init(Cipher.DECRYPT_MODE, mKey, mParameters);
        }

        @Override
        public void measure() throws Exception {
            mCipher.doFinal(mEncryptedMessage);
        }
    }
}
