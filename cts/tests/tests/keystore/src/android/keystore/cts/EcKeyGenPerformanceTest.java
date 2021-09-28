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

import java.security.spec.ECGenParameterSpec;

public class EcKeyGenPerformanceTest extends PerformanceTestBase {

    final int[] SUPPORTED_CURVES = {224, 256, 384, 521};

    public void testEcKeyGen() throws Exception {
        for (int curve : SUPPORTED_CURVES) {
            measure(
                    new KeystoreKeyPairGenMeasurable(
                            new AndroidKeystoreEcKeyGenerator("EC", curve), curve),
                    new KeystoreKeyPairGenMeasurable(
                            new DefaultKeystoreEcKeyGenerator("EC", curve), curve));
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
                                    .build());
        }
    }

    private class DefaultKeystoreEcKeyGenerator extends KeystoreKeyGenerator {

        DefaultKeystoreEcKeyGenerator(String algorithm, int curve) throws Exception {
            super(algorithm);
            getKeyPairGenerator().initialize(curve);
        }
    }
}
