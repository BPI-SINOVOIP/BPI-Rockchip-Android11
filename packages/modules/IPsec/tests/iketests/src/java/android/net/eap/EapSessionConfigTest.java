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
 * limitations under the License.
 */

package android.net.eap;

import static android.net.eap.EapSessionConfig.DEFAULT_IDENTITY;
import static android.telephony.TelephonyManager.APPTYPE_USIM;

import static com.android.internal.net.eap.message.EapData.EAP_TYPE_AKA;
import static com.android.internal.net.eap.message.EapData.EAP_TYPE_AKA_PRIME;
import static com.android.internal.net.eap.message.EapData.EAP_TYPE_MSCHAP_V2;
import static com.android.internal.net.eap.message.EapData.EAP_TYPE_SIM;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.net.eap.EapSessionConfig.EapAkaConfig;
import android.net.eap.EapSessionConfig.EapAkaPrimeConfig;
import android.net.eap.EapSessionConfig.EapMethodConfig;
import android.net.eap.EapSessionConfig.EapMsChapV2Config;
import android.net.eap.EapSessionConfig.EapSimConfig;

import org.junit.Test;

import java.nio.charset.StandardCharsets;

public class EapSessionConfigTest {
    private static final byte[] EAP_IDENTITY =
            "test@android.net".getBytes(StandardCharsets.US_ASCII);
    private static final int SUB_ID = 1;
    private static final String NETWORK_NAME = "android.net";
    private static final boolean ALLOW_MISMATCHED_NETWORK_NAMES = true;
    private static final String USERNAME = "username";
    private static final String PASSWORD = "password";

    @Test
    public void testBuildEapSim() {
        EapSessionConfig result = new EapSessionConfig.Builder()
                .setEapIdentity(EAP_IDENTITY)
                .setEapSimConfig(SUB_ID, APPTYPE_USIM)
                .build();

        assertArrayEquals(EAP_IDENTITY, result.eapIdentity);

        EapMethodConfig eapMethodConfig = result.eapConfigs.get(EAP_TYPE_SIM);
        assertEquals(EAP_TYPE_SIM, eapMethodConfig.methodType);
        EapSimConfig eapSimConfig = (EapSimConfig) eapMethodConfig;
        assertEquals(SUB_ID, eapSimConfig.subId);
        assertEquals(APPTYPE_USIM, eapSimConfig.apptype);
    }

    @Test
    public void testBuildEapAka() {
        EapSessionConfig result = new EapSessionConfig.Builder()
                .setEapAkaConfig(SUB_ID, APPTYPE_USIM)
                .build();

        assertArrayEquals(DEFAULT_IDENTITY, result.eapIdentity);
        EapMethodConfig eapMethodConfig = result.eapConfigs.get(EAP_TYPE_AKA);
        assertEquals(EAP_TYPE_AKA, eapMethodConfig.methodType);
        EapAkaConfig eapAkaConfig = (EapAkaConfig) eapMethodConfig;
        assertEquals(SUB_ID, eapAkaConfig.subId);
        assertEquals(APPTYPE_USIM, eapAkaConfig.apptype);
    }

    @Test
    public void testBuildEapAkaPrime() {
        EapSessionConfig result =
                new EapSessionConfig.Builder()
                        .setEapAkaPrimeConfig(
                                SUB_ID, APPTYPE_USIM, NETWORK_NAME, ALLOW_MISMATCHED_NETWORK_NAMES)
                        .build();

        assertEquals(DEFAULT_IDENTITY, result.eapIdentity);
        EapMethodConfig eapMethodConfig = result.eapConfigs.get(EAP_TYPE_AKA_PRIME);
        assertEquals(EAP_TYPE_AKA_PRIME, eapMethodConfig.methodType);
        EapAkaPrimeConfig eapAkaPrimeConfig = (EapAkaPrimeConfig) eapMethodConfig;
        assertEquals(SUB_ID, eapAkaPrimeConfig.subId);
        assertEquals(APPTYPE_USIM, eapAkaPrimeConfig.apptype);
        assertEquals(NETWORK_NAME, eapAkaPrimeConfig.networkName);
        assertTrue(eapAkaPrimeConfig.allowMismatchedNetworkNames);
    }

    @Test
    public void testBuildEapMsChapV2() {
        EapSessionConfig result =
                new EapSessionConfig.Builder().setEapMsChapV2Config(USERNAME, PASSWORD).build();

        EapMsChapV2Config config = (EapMsChapV2Config) result.eapConfigs.get(EAP_TYPE_MSCHAP_V2);
        assertEquals(EAP_TYPE_MSCHAP_V2, config.methodType);
        assertEquals(USERNAME, config.username);
        assertEquals(PASSWORD, config.password);
    }

    @Test
    public void testBuildWithoutConfigs() {
        try {
            new EapSessionConfig.Builder().build();
            fail("build() should throw an IllegalStateException if no EAP methods are configured");
        } catch (IllegalStateException expected) {
        }
    }
}
