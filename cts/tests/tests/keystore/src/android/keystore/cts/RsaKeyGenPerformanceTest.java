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

public class RsaKeyGenPerformanceTest extends PerformanceTestBase {

    private final int[] SUPPORTED_RSA_KEY_SIZES = {2048, 3072, 4096};

    public void testRsaKeyGen() throws Exception {
        for (int keySize : SUPPORTED_RSA_KEY_SIZES) {
            measure(
                    new KeystoreKeyPairGenMeasurable(
                            new AndroidKeystoreRsaKeyenerator("RSA", keySize),
                            keySize),
                    new KeystoreKeyPairGenMeasurable(
                            new DefaultKeystoreKeyPairGenerator("RSA", keySize),
                            keySize));
        }
    }

    private class AndroidKeystoreRsaKeyenerator extends AndroidKeystoreKeyGenerator {

        AndroidKeystoreRsaKeyenerator(String algorithm, int keySize) throws Exception {
            super(algorithm);
            getKeyPairGenerator()
                    .initialize(
                            getKeyGenParameterSpecBuilder(
                                            KeyProperties.PURPOSE_SIGN
                                                    | KeyProperties.PURPOSE_VERIFY)
                                    .setKeySize(keySize)
                                    .build());
        }
    }
}
