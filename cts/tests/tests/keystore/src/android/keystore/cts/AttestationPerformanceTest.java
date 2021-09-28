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

import android.keystore.cts.PerformanceTestBase.AndroidKeystoreKeyGenerator;
import android.security.keystore.KeyProperties;

import org.junit.Test;

import java.security.spec.ECGenParameterSpec;

public class AttestationPerformanceTest extends PerformanceTestBase {

    private final int[] RSA_KEY_SIZES = {2048, 3072, 4096};
    private final int[] EC_CURVES = {224, 256, 384, 521};

    private final byte[][] ATTESTATION_CHALLENGES = {
        new byte[0], // empty challenge
        "challenge".getBytes(), // short challenge
        new byte[128], // long challenge
    };

    public void testRsaKeyAttestation() throws Exception {
        for (byte[] challenge : ATTESTATION_CHALLENGES) {
            for (int keySize : RSA_KEY_SIZES) {
                measure(new KeystoreAttestationMeasurable(
                        new AndroidKeystoreRsaKeyGenerator("SHA1withRSA", keySize, challenge),
                        keySize, challenge.length));
            }
        }
    }

    public void testEcKeyAttestation() throws Exception {
        for (byte[] challenge : ATTESTATION_CHALLENGES) {
            for (int curve : EC_CURVES) {
                measure(new KeystoreAttestationMeasurable(
                        new AndroidKeystoreEcKeyGenerator("SHA1withECDSA", curve, challenge),
                        curve,
                        challenge.length));
            }
        }
    }

    private class AndroidKeystoreRsaKeyGenerator extends AndroidKeystoreKeyGenerator {

        AndroidKeystoreRsaKeyGenerator(String algorithm, int keySize, byte[] challenge)
                throws Exception {
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
                                    .setAttestationChallenge(challenge)
                                    .build());
        }
    }

    private class AndroidKeystoreEcKeyGenerator extends AndroidKeystoreKeyGenerator {

        AndroidKeystoreEcKeyGenerator(String algorithm, int keySize, byte[] challenge)
                throws Exception {
            super(algorithm);
            getKeyPairGenerator()
                    .initialize(
                            getKeyGenParameterSpecBuilder(
                                            KeyProperties.PURPOSE_SIGN
                                                    | KeyProperties.PURPOSE_VERIFY)
                                    .setKeySize(keySize)
                                    .setDigests(TestUtils.getSignatureAlgorithmDigest(algorithm))
                                    .setAttestationChallenge(challenge)
                                    .build());
        }
    }

    private class KeystoreAttestationMeasurable extends KeystoreMeasurable {
        private final AndroidKeystoreKeyGenerator mKeyGen;

        KeystoreAttestationMeasurable(
                AndroidKeystoreKeyGenerator keyGen, int keySize, int challengeSize)
                throws Exception {
            super(keyGen, "attest", keySize, challengeSize);
            mKeyGen = keyGen;
        }

        @Override
        public void initialSetUp() throws Exception {
            mKeyGen.getKeyPairGenerator().generateKeyPair();
        }

        @Override
        public void measure() throws Exception {
            mKeyGen.getCertificateChain();
        }
    }
}
