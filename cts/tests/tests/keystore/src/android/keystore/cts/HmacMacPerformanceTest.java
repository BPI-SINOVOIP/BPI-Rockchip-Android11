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

import javax.crypto.Mac;
import javax.crypto.SecretKey;

public class HmacMacPerformanceTest extends PerformanceTestBase {

    final int[] SUPPORTED_KEY_SIZES = {64, 128, 256, 512};
    final int[] TEST_MESSAGE_SIZES = {1 << 6, 1 << 10, 1 << 17};

    public void testHmacSHA1() throws Exception {
        testHmac("HmacSHA1", SUPPORTED_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testHmacSHA224() throws Exception {
        testHmac("HmacSHA224", SUPPORTED_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testHmacSHA256() throws Exception {
        testHmac("HmacSHA256", SUPPORTED_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testHmacSHA384() throws Exception {
        testHmac("HmacSHA384", SUPPORTED_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    public void testHmacSHA512() throws Exception {
        testHmac("HmacSHA512", SUPPORTED_KEY_SIZES, TEST_MESSAGE_SIZES);
    }

    private void testHmac(String algorithm, int[] keySizes, int[] messageSizes) throws Exception {
        for (int keySize : keySizes) {
            KeystoreKeyGenerator androidKeystoreHmacKeyGenerator =
                    new AndroidKeystoreHmacKeyGenerator(algorithm, keySize);
            KeystoreKeyGenerator defaultKeystoreHmacGenerator =
                    new DefaultKeystoreSecretKeyGenerator(algorithm, keySize);
            for (int messageSize : messageSizes) {
                measure(
                        new KeystoreHmacMacMeasurable(
                                androidKeystoreHmacKeyGenerator, keySize, messageSize),
                        new KeystoreHmacMacMeasurable(
                                defaultKeystoreHmacGenerator, keySize, messageSize));
            }
        }
    }

    private class AndroidKeystoreHmacKeyGenerator extends AndroidKeystoreKeyGenerator {

        AndroidKeystoreHmacKeyGenerator(String algorithm, int keySize) throws Exception {
            super(algorithm);
            getSecretKeyGenerator()
                    .init(
                            getKeyGenParameterSpecBuilder(
                                            KeyProperties.PURPOSE_SIGN
                                                    | KeyProperties.PURPOSE_VERIFY)
                                    .setKeySize(keySize)
                                    .build());
        }
    }

    private class KeystoreHmacMacMeasurable extends KeystoreMeasurable {
        private final Mac mMac;
        private SecretKey mKey;

        public KeystoreHmacMacMeasurable(
                KeystoreKeyGenerator generator, int keySize, int messageSize) throws Exception {
            super(generator, "sign", keySize, messageSize);
            mMac = Mac.getInstance(getAlgorithm());
        }

        @Override
        public void initialSetUp() throws Exception {
            mKey = generateSecretKey();
        }

        @Override
        public void setUp() throws Exception {
            mMac.init(mKey);
        }

        @Override
        public void measure() throws Exception {
            mMac.update(getMessage());
            mMac.doFinal();
        }
    }
}
