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
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_FAILURE;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_REQUEST;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_SUCCESS;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.ID_INT;
import static com.android.internal.net.eap.message.simaka.EapAkaTypeData.EAP_AKA_CHALLENGE;
import static com.android.internal.net.eap.message.simaka.EapAkaTypeData.EAP_AKA_IDENTITY;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import com.android.internal.net.eap.EapResult;
import com.android.internal.net.eap.EapResult.EapError;
import com.android.internal.net.eap.EapResult.EapFailure;
import com.android.internal.net.eap.exceptions.EapInvalidRequestException;
import com.android.internal.net.eap.message.EapData;
import com.android.internal.net.eap.message.EapMessage;
import com.android.internal.net.eap.message.simaka.EapAkaTypeData;
import com.android.internal.net.eap.message.simaka.EapSimAkaTypeData.DecodeResult;
import com.android.internal.net.eap.statemachine.EapAkaMethodStateMachine.ChallengeState;
import com.android.internal.net.eap.statemachine.EapAkaMethodStateMachine.IdentityState;
import com.android.internal.net.eap.statemachine.EapMethodStateMachine.FinalState;

import org.junit.Test;

import java.util.LinkedHashMap;

public class EapAkaCreatedStateTest extends EapAkaStateTest {
    @Test
    public void testProcessTransitionToIdentityState() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_AKA, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        // Don't actually need any attributes in the attributeMap, since we only care about the
        // state transition here.
        DecodeResult<EapAkaTypeData> decodeResult =
                new DecodeResult<>(new EapAkaTypeData(EAP_AKA_IDENTITY, new LinkedHashMap<>()));
        doReturn(decodeResult).when(mMockEapAkaTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));

        mEapAkaMethodStateMachine.process(eapMessage);

        assertTrue(mEapAkaMethodStateMachine.getState() instanceof IdentityState);

        // decoded in CreatedState and IdentityState
        verify(mMockEapAkaTypeDataDecoder, times(2)).decode(eq(DUMMY_EAP_TYPE_DATA));
        verifyNoMoreInteractions(mMockTelephonyManager, mMockEapAkaTypeDataDecoder);
    }

    @Test
    public void testProcessTransitionToChallengeState() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_AKA, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        // Don't actually need any attributes in the attributeMap, since we only care about the
        // state transition here.
        DecodeResult<EapAkaTypeData> decodeResult =
                new DecodeResult<>(new EapAkaTypeData(EAP_AKA_CHALLENGE, new LinkedHashMap<>()));
        doReturn(decodeResult).when(mMockEapAkaTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));

        mEapAkaMethodStateMachine.process(eapMessage);

        ChallengeState challengeState = (ChallengeState) mEapAkaMethodStateMachine.getState();
        assertArrayEquals(EAP_IDENTITY_BYTES, challengeState.mIdentity);

        // decoded in CreatedState and ChallengeState
        verify(mMockEapAkaTypeDataDecoder, times(2)).decode(DUMMY_EAP_TYPE_DATA);
        verifyNoMoreInteractions(mMockTelephonyManager, mMockEapAkaTypeDataDecoder);
    }

    @Test
    public void testProcessSuccess() throws Exception {
        EapMessage input = new EapMessage(EAP_CODE_SUCCESS, ID_INT, null);
        EapResult result = mEapAkaMethodStateMachine.process(input);

        EapError eapError = (EapError) result;
        assertTrue(eapError.cause instanceof EapInvalidRequestException);
    }

    @Test
    public void testProcessFailure() throws Exception {
        EapMessage input = new EapMessage(EAP_CODE_FAILURE, ID_INT, null);
        EapResult result = mEapAkaMethodStateMachine.process(input);
        assertTrue(mEapAkaMethodStateMachine.getState() instanceof FinalState);

        assertTrue(result instanceof EapFailure);
    }
}
