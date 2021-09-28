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

public class DesKeyGenPerformanceTest extends PerformanceTestBase {

    final int[] SUPPORTED_DES_KEY_SIZES = {168};

    public void testDesKeyGen() throws Exception {
        if (!TestUtils.supports3DES()) {
            return;
        }
        for (int keySize : SUPPORTED_DES_KEY_SIZES) {
            measure(
                    new KeystoreSecretKeyGenMeasurable(
                            new DefaultKeystoreSecretKeyGenerator("DESede", keySize), keySize),
                    new KeystoreSecretKeyGenMeasurable(
                            new AndroidKeystoreDesKeyGenerator("DESede", keySize), keySize));
        }
    }

    private class AndroidKeystoreDesKeyGenerator extends AndroidKeystoreKeyGenerator {

        AndroidKeystoreDesKeyGenerator(String algorithm, int keySize) throws Exception {
            super(algorithm);
            getSecretKeyGenerator()
                    .init(
                            getKeyGenParameterSpecBuilder(
                                            KeyProperties.PURPOSE_ENCRYPT
                                                    | KeyProperties.PURPOSE_DECRYPT)
                                    .setKeySize(keySize)
                                    .build());
        }
    }
}
