/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.keystore.cts;

import static android.app.admin.DevicePolicyManager.ID_TYPE_SERIAL;

import static com.google.common.truth.Truth.assertThat;
import static org.testng.Assert.assertThrows;

import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.security.AttestedKeyPair;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;

public class KeyGenerationUtils {
    private static final String ALIAS = "com.android.test.generated-rsa-1";

    private static KeyGenParameterSpec buildRsaKeySpecWithKeyAttestation(String alias) {
        return new KeyGenParameterSpec.Builder(
                alias,
                KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
                .setKeySize(2048)
                .setDigests(KeyProperties.DIGEST_SHA256)
                .setSignaturePaddings(KeyProperties.SIGNATURE_PADDING_RSA_PSS,
                        KeyProperties.SIGNATURE_PADDING_RSA_PKCS1)
                .setIsStrongBoxBacked(false)
                .setAttestationChallenge(new byte[]{'a', 'b', 'c'})
                .build();
    }

    private static AttestedKeyPair generateRsaKeyPair(DevicePolicyManager dpm, ComponentName admin,
            int deviceIdAttestationFlags, String alias) {
        return  dpm.generateKeyPair(
                admin, "RSA", buildRsaKeySpecWithKeyAttestation(alias),
                deviceIdAttestationFlags);
    }

    private static void generateKeyWithIdFlagsExpectingSuccess(DevicePolicyManager dpm,
            ComponentName admin, int deviceIdAttestationFlags) {
        try {
            AttestedKeyPair generated =
                    generateRsaKeyPair(dpm, admin, deviceIdAttestationFlags, ALIAS);
            assertThat(generated).isNotNull();
        } finally {
            assertThat(dpm.removeKeyPair(admin, ALIAS)).isTrue();
        }
    }

    public static void generateRsaKey(DevicePolicyManager dpm, ComponentName admin, String alias) {
        assertThat(generateRsaKeyPair(dpm, admin, 0, alias)).isNotNull();
    }

    public static void generateKeyWithDeviceIdAttestationExpectingSuccess(DevicePolicyManager dpm,
            ComponentName admin) {
        generateKeyWithIdFlagsExpectingSuccess(dpm, admin, ID_TYPE_SERIAL);
    }

    public static void generateKeyWithDeviceIdAttestationExpectingFailure(DevicePolicyManager dpm,
            ComponentName admin) {
        assertThrows(SecurityException.class,
                () -> dpm.generateKeyPair(admin, "RSA", buildRsaKeySpecWithKeyAttestation(ALIAS),
                        ID_TYPE_SERIAL));
    }
}
