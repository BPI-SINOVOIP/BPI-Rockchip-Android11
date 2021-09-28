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

import static com.android.internal.net.eap.message.EapData.EAP_TYPE_AKA;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_REQUEST;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_AKA_CLIENT_ERROR_UNABLE_TO_PROCESS;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_AKA_IDENTITY_RESPONSE;
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

import com.android.internal.net.eap.EapResult.EapError;
import com.android.internal.net.eap.EapResult.EapResponse;
import com.android.internal.net.eap.exceptions.simaka.EapSimAkaIdentityUnavailableException;
import com.android.internal.net.eap.message.EapData;
import com.android.internal.net.eap.message.EapMessage;
import com.android.internal.net.eap.message.simaka.EapAkaTypeData;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtAnyIdReq;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtPermanentIdReq;
import com.android.internal.net.eap.message.simaka.EapSimAkaTypeData.DecodeResult;
import com.android.internal.net.eap.statemachine.EapAkaMethodStateMachine.ChallengeState;
import com.android.internal.net.eap.statemachine.EapAkaMethodStateMachine.IdentityState;

import org.junit.Before;
import org.junit.Test;

import java.util.Arrays;
import java.util.LinkedHashMap;

public class EapAkaIdentityStateTest extends EapAkaStateTest {
    private IdentityState mIdentityState;

    @Before
    public void setUp() {
        super.setUp();

        mIdentityState = mEapAkaMethodStateMachine.new IdentityState();
        mEapAkaMethodStateMachine.transitionTo(mIdentityState);
    }

    @Test
    public void testProcessIdentityRequest() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_AKA, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        DecodeResult<EapAkaTypeData> decodeResult =
                new DecodeResult<>(
                        new EapAkaTypeData(EAP_AKA_IDENTITY, Arrays.asList(new AtAnyIdReq())));
        doReturn(decodeResult).when(mMockEapAkaTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));
        doReturn(IMSI).when(mMockTelephonyManager).getSubscriberId();

        EapResponse eapResponse = (EapResponse) mEapAkaMethodStateMachine.process(eapMessage);
        assertArrayEquals(EAP_AKA_IDENTITY_RESPONSE, eapResponse.packet);

        verify(mMockEapAkaTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));
        verify(mMockTelephonyManager).getSubscriberId();
        verifyNoMoreInteractions(mMockEapAkaTypeDataDecoder, mMockTelephonyManager);
    }

    @Test
    public void testProcessWithoutIdentityRequestAttributes() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_AKA, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        DecodeResult<EapAkaTypeData> decodeResult =
                new DecodeResult<>(new EapAkaTypeData(EAP_AKA_IDENTITY, new LinkedHashMap<>()));
        doReturn(decodeResult).when(mMockEapAkaTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));

        EapResponse eapResponse = (EapResponse) mEapAkaMethodStateMachine.process(eapMessage);
        assertArrayEquals(EAP_AKA_CLIENT_ERROR_UNABLE_TO_PROCESS, eapResponse.packet);

        verify(mMockEapAkaTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));
        verifyNoMoreInteractions(mMockEapAkaTypeDataDecoder, mMockTelephonyManager);
    }

    @Test
    public void testProcessMultipleIdentityRequestAttributes() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_AKA, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        DecodeResult<EapAkaTypeData> decodeResult =
                new DecodeResult<>(
                        new EapAkaTypeData(
                                EAP_AKA_IDENTITY,
                                Arrays.asList(new AtAnyIdReq(), new AtPermanentIdReq())));
        doReturn(decodeResult).when(mMockEapAkaTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));

        EapResponse eapResponse = (EapResponse) mEapAkaMethodStateMachine.process(eapMessage);
        assertArrayEquals(EAP_AKA_CLIENT_ERROR_UNABLE_TO_PROCESS, eapResponse.packet);

        verify(mMockEapAkaTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));
        verifyNoMoreInteractions(mMockEapAkaTypeDataDecoder, mMockTelephonyManager);
    }

    @Test
    public void testProcessImsiUnavailable() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_AKA, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        DecodeResult<EapAkaTypeData> decodeResult =
                new DecodeResult<>(
                        new EapAkaTypeData(EAP_AKA_IDENTITY, Arrays.asList(new AtAnyIdReq())));
        doReturn(decodeResult).when(mMockEapAkaTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));
        doReturn(null).when(mMockTelephonyManager).getSubscriberId();

        EapError eapError = (EapError) mEapAkaMethodStateMachine.process(eapMessage);
        assertTrue(eapError.cause instanceof EapSimAkaIdentityUnavailableException);

        verify(mMockEapAkaTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));
        verify(mMockTelephonyManager).getSubscriberId();
        verifyNoMoreInteractions(mMockEapAkaTypeDataDecoder, mMockTelephonyManager);
    }

    @Test
    public void testProcessTransitionToChallengeState() throws Exception {
        // transition to IdentityState so we can verify the transition to ChallengeState
        IdentityState identityState = mEapAkaMethodStateMachine.new IdentityState();
        mEapAkaMethodStateMachine.transitionTo(identityState);

        EapData eapData = new EapData(EAP_TYPE_AKA, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        // Don't actually need any attributes in the attributeMap, since we only care about the
        // state transition here.
        DecodeResult<EapAkaTypeData> decodeResult =
                new DecodeResult<>(new EapAkaTypeData(EAP_AKA_CHALLENGE, new LinkedHashMap<>()));
        doReturn(decodeResult).when(mMockEapAkaTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));

        mEapAkaMethodStateMachine.process(eapMessage);

        assertTrue(mEapAkaMethodStateMachine.getState() instanceof ChallengeState);

        // decoded in IdentityState and ChallengeState
        verify(mMockEapAkaTypeDataDecoder, times(2)).decode(DUMMY_EAP_TYPE_DATA);
        verifyNoMoreInteractions(mMockTelephonyManager, mMockEapAkaTypeDataDecoder);
    }
}
