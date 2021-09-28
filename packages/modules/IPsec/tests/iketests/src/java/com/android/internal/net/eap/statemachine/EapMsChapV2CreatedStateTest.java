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

import static com.android.internal.net.eap.message.EapData.EAP_TYPE_MSCHAP_V2;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_REQUEST;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.ID_INT;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.CHALLENGE_BYTES;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.SERVER_NAME_BYTES;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.android.internal.net.eap.EapResult;
import com.android.internal.net.eap.EapResult.EapError;
import com.android.internal.net.eap.exceptions.mschapv2.EapMsChapV2ParsingException;
import com.android.internal.net.eap.message.EapData;
import com.android.internal.net.eap.message.EapMessage;
import com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EapMsChapV2ChallengeRequest;
import com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EapMsChapV2TypeDataDecoder.DecodeResult;
import com.android.internal.net.eap.statemachine.EapMsChapV2MethodStateMachine.ValidateAuthenticatorState;

import org.junit.Test;

public class EapMsChapV2CreatedStateTest extends EapMsChapV2StateTest {
    @Test
    public void testProcessChallengeRequest() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_MSCHAP_V2, DUMMY_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        when(mMockTypeDataDecoder.decodeChallengeRequest(any(String.class), eq(DUMMY_TYPE_DATA)))
                .thenReturn(
                        new DecodeResult<>(
                                new EapMsChapV2ChallengeRequest(
                                        0, 0, CHALLENGE_BYTES, SERVER_NAME_BYTES)));

        mStateMachine.process(eapMessage);

        assertTrue(mStateMachine.getState() instanceof ValidateAuthenticatorState);
        verify(mMockTypeDataDecoder, times(2))
                .decodeChallengeRequest(any(String.class), eq(DUMMY_TYPE_DATA));
    }

    @Test
    public void testIncorrectTypeData() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_MSCHAP_V2, DUMMY_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        when(mMockTypeDataDecoder.decodeChallengeRequest(
                        eq(CREATED_STATE_TAG), eq(DUMMY_TYPE_DATA)))
                .thenReturn(
                        new DecodeResult<>(
                                new EapError(
                                        new EapMsChapV2ParsingException("incorrect type data"))));

        EapResult result = mStateMachine.process(eapMessage);
        EapError eapError = (EapError) result;
        assertTrue(eapError.cause instanceof EapMsChapV2ParsingException);
        verify(mMockTypeDataDecoder)
                .decodeChallengeRequest(eq(CREATED_STATE_TAG), eq(DUMMY_TYPE_DATA));
    }
}
