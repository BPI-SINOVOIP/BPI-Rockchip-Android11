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

import static com.android.internal.net.eap.message.EapData.EAP_TYPE_AKA_PRIME;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_REQUEST;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_AKA_PRIME_IDENTITY_RESPONSE;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.ID_INT;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.IMSI;
import static com.android.internal.net.eap.message.simaka.EapAkaTypeData.EAP_AKA_CHALLENGE;
import static com.android.internal.net.eap.message.simaka.EapAkaTypeData.EAP_AKA_IDENTITY;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import com.android.internal.net.eap.EapResult.EapResponse;
import com.android.internal.net.eap.message.EapData;
import com.android.internal.net.eap.message.EapMessage;
import com.android.internal.net.eap.message.simaka.EapAkaPrimeTypeData;
import com.android.internal.net.eap.message.simaka.EapAkaTypeData;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtAnyIdReq;
import com.android.internal.net.eap.message.simaka.EapSimAkaTypeData.DecodeResult;
import com.android.internal.net.eap.statemachine.EapAkaPrimeMethodStateMachine.ChallengeState;

import org.junit.Before;
import org.junit.Test;

import java.util.ArrayList;
import java.util.Arrays;

public class EapAkaPrimeIdentityStateTest extends EapAkaPrimeStateTest {
    @Before
    public void setUp() {
        super.setUp();

        mStateMachine.transitionTo(mStateMachine.new IdentityState());
    }

    @Test
    public void testProcessIdentityRequest() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_AKA_PRIME, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        DecodeResult<EapAkaTypeData> decodeResult =
                new DecodeResult<>(
                        new EapAkaPrimeTypeData(EAP_AKA_IDENTITY, Arrays.asList(new AtAnyIdReq())));
        doReturn(decodeResult).when(mMockTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));
        doReturn(IMSI).when(mMockTelephonyManager).getSubscriberId();

        EapResponse eapResponse = (EapResponse) mStateMachine.process(eapMessage);
        assertArrayEquals(EAP_AKA_PRIME_IDENTITY_RESPONSE, eapResponse.packet);

        verify(mMockTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));
        verify(mMockTelephonyManager).getSubscriberId();
        verifyNoMoreInteractions(mMockTypeDataDecoder, mMockTelephonyManager);
    }

    @Test
    public void testProcessTransitionToChallengeState() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_AKA_PRIME, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        // Don't actually need any attributes in the attributeMap, since we only care about the
        // state transition here.
        DecodeResult<EapAkaTypeData> decodeResult =
                new DecodeResult<>(new EapAkaPrimeTypeData(EAP_AKA_CHALLENGE, new ArrayList<>()));
        doReturn(decodeResult).when(mMockTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));

        mStateMachine.process(eapMessage);

        assertTrue(mStateMachine.getState() instanceof ChallengeState);

        // decoded in IdentityState and ChallengeState
        verify(mMockTypeDataDecoder, times(2)).decode(DUMMY_EAP_TYPE_DATA);
        verifyNoMoreInteractions(mMockTelephonyManager, mMockTypeDataDecoder);
    }
}
