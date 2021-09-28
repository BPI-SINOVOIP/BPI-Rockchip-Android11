/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.net.eap.cts;

import static android.telephony.TelephonyManager.APPTYPE_USIM;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.net.eap.EapSessionConfig;
import android.net.eap.EapSessionConfig.EapAkaConfig;
import android.net.eap.EapSessionConfig.EapAkaPrimeConfig;
import android.net.eap.EapSessionConfig.EapMsChapV2Config;
import android.net.eap.EapSessionConfig.EapSimConfig;
import android.net.eap.EapSessionConfig.EapUiccConfig;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class EapSessionConfigTest {
    // These constants are IANA-defined values and are copies of hidden constants in
    // frameworks/opt/net/ike/src/java/com/android/internal/net/eap/message/EapData.java.
    private static final int EAP_TYPE_SIM = 18;
    private static final int EAP_TYPE_AKA = 23;
    private static final int EAP_TYPE_MSCHAP_V2 = 26;
    private static final int EAP_TYPE_AKA_PRIME = 50;

    private static final int SUB_ID = 1;
    private static final byte[] EAP_IDENTITY = "test@android.net".getBytes();
    private static final String NETWORK_NAME = "android.net";
    private static final String EAP_MSCHAPV2_USERNAME = "username";
    private static final String EAP_MSCHAPV2_PASSWORD = "password";

    @Test
    public void testBuildWithAllEapMethods() {
        EapSessionConfig result =
                new EapSessionConfig.Builder()
                        .setEapIdentity(EAP_IDENTITY)
                        .setEapSimConfig(SUB_ID, APPTYPE_USIM)
                        .setEapAkaConfig(SUB_ID, APPTYPE_USIM)
                        .setEapAkaPrimeConfig(
                                SUB_ID,
                                APPTYPE_USIM,
                                NETWORK_NAME,
                                true /* allowMismatchedNetworkNames */)
                        .setEapMsChapV2Config(EAP_MSCHAPV2_USERNAME, EAP_MSCHAPV2_PASSWORD)
                        .build();

        assertArrayEquals(EAP_IDENTITY, result.getEapIdentity());

        EapSimConfig eapSimConfig = result.getEapSimConfig();
        assertNotNull(eapSimConfig);
        assertEquals(EAP_TYPE_SIM, eapSimConfig.getMethodType());
        verifyEapUiccConfigCommon(eapSimConfig);

        EapAkaConfig eapAkaConfig = result.getEapAkaConfig();
        assertNotNull(eapAkaConfig);
        assertEquals(EAP_TYPE_AKA, eapAkaConfig.getMethodType());
        verifyEapUiccConfigCommon(eapAkaConfig);

        EapAkaPrimeConfig eapAkaPrimeConfig = result.getEapAkaPrimeConfig();
        assertNotNull(eapAkaPrimeConfig);
        assertEquals(EAP_TYPE_AKA_PRIME, eapAkaPrimeConfig.getMethodType());
        assertEquals(NETWORK_NAME, eapAkaPrimeConfig.getNetworkName());
        assertTrue(NETWORK_NAME, eapAkaPrimeConfig.allowsMismatchedNetworkNames());
        verifyEapUiccConfigCommon(eapAkaPrimeConfig);

        EapMsChapV2Config eapMsChapV2Config = result.getEapMsChapV2onfig();
        assertNotNull(eapMsChapV2Config);
        assertEquals(EAP_TYPE_MSCHAP_V2, eapMsChapV2Config.getMethodType());
        assertEquals(EAP_MSCHAPV2_USERNAME, eapMsChapV2Config.getUsername());
        assertEquals(EAP_MSCHAPV2_PASSWORD, eapMsChapV2Config.getPassword());
    }

    private void verifyEapUiccConfigCommon(EapUiccConfig config) {
        assertEquals(SUB_ID, config.getSubId());
        assertEquals(APPTYPE_USIM, config.getAppType());
    }
}
