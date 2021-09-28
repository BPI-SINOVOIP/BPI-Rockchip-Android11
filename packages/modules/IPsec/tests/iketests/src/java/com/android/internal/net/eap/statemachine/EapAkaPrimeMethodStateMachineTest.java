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

package com.android.internal.net.eap.statemachine;

import static com.android.internal.net.TestUtils.hexStringToByteArray;
import static com.android.internal.net.eap.message.EapData.EAP_TYPE_AKA_PRIME;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_REQUEST;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.ID_INT;
import static com.android.internal.net.eap.message.simaka.EapAkaTypeData.EAP_AKA_CHALLENGE;
import static com.android.internal.net.eap.statemachine.EapAkaPrimeMethodStateMachine.K_AUT_LEN;
import static com.android.internal.net.eap.statemachine.EapAkaPrimeMethodStateMachine.K_RE_LEN;
import static com.android.internal.net.eap.statemachine.EapSimAkaMethodStateMachine.KEY_LEN;
import static com.android.internal.net.eap.statemachine.EapSimAkaMethodStateMachine.SESSION_KEY_LENGTH;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import com.android.internal.net.eap.message.EapData;
import com.android.internal.net.eap.message.EapMessage;
import com.android.internal.net.eap.message.simaka.EapAkaPrimeTypeData;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtMac;
import com.android.internal.net.eap.statemachine.EapAkaMethodStateMachine.CreatedState;

import org.junit.Test;

import java.util.Arrays;

public class EapAkaPrimeMethodStateMachineTest extends EapAkaPrimeTest {
    private static final String TAG = EapAkaPrimeMethodStateMachineTest.class.getSimpleName();
    private static final byte[] K_AUT =
            hexStringToByteArray(
                    "000102030405060708090A0B0C0D0E0F000102030405060708090A0B0C0D0E0F");
    private static final byte[] MAC = hexStringToByteArray("0322b08b59cae2df8f766162ac76f30b");

    @Test
    public void testEapAkaPrimeMethodStateMachineStartState() {
        assertTrue(mStateMachine.getState() instanceof CreatedState);
    }

    @Test
    public void testKeyLengths() {
        assertEquals(KEY_LEN, mStateMachine.getKEncrLength());
        assertEquals(K_AUT_LEN, mStateMachine.getKAutLength());
        assertEquals(K_RE_LEN, mStateMachine.getKReLen());
        assertEquals(SESSION_KEY_LENGTH, mStateMachine.getMskLength());
        assertEquals(SESSION_KEY_LENGTH, mStateMachine.getEmskLength());
    }

    @Test
    public void testIsValidMacUsesHmacSha256() throws Exception {
        System.arraycopy(K_AUT, 0, mStateMachine.mKAut, 0, K_AUT.length);

        EapData eapData = new EapData(EAP_TYPE_AKA_PRIME, new byte[0]);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);
        EapAkaPrimeTypeData eapAkaPrimeTypeData =
                new EapAkaPrimeTypeData(EAP_AKA_CHALLENGE, Arrays.asList(new AtMac(MAC)));

        assertTrue(mStateMachine.isValidMac(TAG, eapMessage, eapAkaPrimeTypeData, new byte[0]));
    }
}
