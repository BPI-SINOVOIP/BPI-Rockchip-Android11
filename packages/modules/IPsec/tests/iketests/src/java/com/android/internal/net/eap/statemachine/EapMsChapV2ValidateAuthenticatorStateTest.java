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
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_MSCHAP_V2_FAILURE_RESPONSE;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_MSCHAP_V2_SUCCESS_RESPONSE;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.ID_INT;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.INVALID_AUTHENTICATOR_RESPONSE;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_AUTHENTICATOR_CHALLENGE;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_AUTHENTICATOR_RESPONSE;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_ID_INT;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_NT_RESPONSE;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_PEER_CHALLENGE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.CHALLENGE_BYTES;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.ERROR_CODE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.MESSAGE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.PASSWORD_CHANGE_PROTOCOL;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.RETRY_BIT;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EAP_MSCHAP_V2_FAILURE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EAP_MSCHAP_V2_SUCCESS;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.android.internal.net.eap.EapResult;
import com.android.internal.net.eap.EapResult.EapError;
import com.android.internal.net.eap.EapResult.EapFailure;
import com.android.internal.net.eap.EapResult.EapResponse;
import com.android.internal.net.eap.exceptions.EapInvalidRequestException;
import com.android.internal.net.eap.message.EapData;
import com.android.internal.net.eap.message.EapMessage;
import com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EapMsChapV2FailureRequest;
import com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EapMsChapV2SuccessRequest;
import com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EapMsChapV2TypeDataDecoder.DecodeResult;
import com.android.internal.net.eap.statemachine.EapMethodStateMachine.FinalState;
import com.android.internal.net.eap.statemachine.EapMsChapV2MethodStateMachine.AwaitingEapFailureState;
import com.android.internal.net.eap.statemachine.EapMsChapV2MethodStateMachine.AwaitingEapSuccessState;

import org.junit.Before;
import org.junit.Test;

import java.nio.BufferUnderflowException;

public class EapMsChapV2ValidateAuthenticatorStateTest extends EapMsChapV2StateTest {
    private static final int INVALID_OP_CODE = -1;

    @Before
    @Override
    public void setUp() {
        super.setUp();

        mStateMachine.transitionTo(
                mStateMachine.new ValidateAuthenticatorState(
                        MSCHAP_V2_AUTHENTICATOR_CHALLENGE,
                        MSCHAP_V2_PEER_CHALLENGE,
                        MSCHAP_V2_NT_RESPONSE));
    }

    @Test
    public void testProcessSuccessRequest() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_MSCHAP_V2, DUMMY_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        EapMsChapV2SuccessRequest successRequest =
                new EapMsChapV2SuccessRequest(
                        MSCHAP_V2_ID_INT, 0, MSCHAP_V2_AUTHENTICATOR_RESPONSE, MESSAGE);

        doReturn(EAP_MSCHAP_V2_SUCCESS).when(mMockTypeDataDecoder).getOpCode(eq(DUMMY_TYPE_DATA));
        doReturn(new DecodeResult<>(successRequest))
                .when(mMockTypeDataDecoder)
                .decodeSuccessRequest(any(String.class), eq(DUMMY_TYPE_DATA));

        EapResult result = mStateMachine.process(eapMessage);
        EapResponse eapResponse = (EapResponse) result;
        assertArrayEquals(EAP_MSCHAP_V2_SUCCESS_RESPONSE, eapResponse.packet);
        assertTrue(mStateMachine.getState() instanceof AwaitingEapSuccessState);
        verify(mMockTypeDataDecoder).getOpCode(eq(DUMMY_TYPE_DATA));
        verify(mMockTypeDataDecoder).decodeSuccessRequest(any(String.class), eq(DUMMY_TYPE_DATA));
    }

    @Test
    public void testProcessInvalidSuccessRequest() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_MSCHAP_V2, DUMMY_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        EapMsChapV2SuccessRequest successRequest =
                new EapMsChapV2SuccessRequest(
                        MSCHAP_V2_ID_INT, 0, INVALID_AUTHENTICATOR_RESPONSE, MESSAGE);

        doReturn(EAP_MSCHAP_V2_SUCCESS).when(mMockTypeDataDecoder).getOpCode(eq(DUMMY_TYPE_DATA));
        doReturn(new DecodeResult<>(successRequest))
                .when(mMockTypeDataDecoder)
                .decodeSuccessRequest(any(String.class), eq(DUMMY_TYPE_DATA));

        EapResult result = mStateMachine.process(eapMessage);
        assertTrue(result instanceof EapFailure);
        assertTrue(mStateMachine.getState() instanceof FinalState);
        verify(mMockTypeDataDecoder).getOpCode(eq(DUMMY_TYPE_DATA));
        verify(mMockTypeDataDecoder).decodeSuccessRequest(any(String.class), eq(DUMMY_TYPE_DATA));
    }

    @Test
    public void testProcessFailureRequest() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_MSCHAP_V2, DUMMY_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        EapMsChapV2FailureRequest failureRequest =
                new EapMsChapV2FailureRequest(
                        MSCHAP_V2_ID_INT,
                        0,
                        ERROR_CODE,
                        RETRY_BIT,
                        CHALLENGE_BYTES,
                        PASSWORD_CHANGE_PROTOCOL,
                        MESSAGE);

        doReturn(EAP_MSCHAP_V2_FAILURE).when(mMockTypeDataDecoder).getOpCode(eq(DUMMY_TYPE_DATA));
        doReturn(new DecodeResult<>(failureRequest))
                .when(mMockTypeDataDecoder)
                .decodeFailureRequest(any(String.class), eq(DUMMY_TYPE_DATA));

        EapResult result = mStateMachine.process(eapMessage);
        EapResponse eapResponse = (EapResponse) result;
        assertArrayEquals(EAP_MSCHAP_V2_FAILURE_RESPONSE, eapResponse.packet);
        assertTrue(mStateMachine.getState() instanceof AwaitingEapFailureState);
        verify(mMockTypeDataDecoder).getOpCode(eq(DUMMY_TYPE_DATA));
        verify(mMockTypeDataDecoder).decodeFailureRequest(any(String.class), eq(DUMMY_TYPE_DATA));
    }

    @Test
    public void testProcessEmptyTypeData() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_MSCHAP_V2, new byte[0]);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        when(mMockTypeDataDecoder.getOpCode(eq(new byte[0])))
                .thenThrow(new BufferUnderflowException());

        EapResult result = mStateMachine.process(eapMessage);
        EapError eapError = (EapError) result;
        assertTrue(eapError.cause instanceof BufferUnderflowException);
        verify(mMockTypeDataDecoder).getOpCode(eq(new byte[0]));
    }

    @Test
    public void testProcessInvalidPacket() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_MSCHAP_V2, DUMMY_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        doReturn(INVALID_OP_CODE).when(mMockTypeDataDecoder).getOpCode(eq(DUMMY_TYPE_DATA));

        EapResult result = mStateMachine.process(eapMessage);
        EapError eapError = (EapError) result;
        assertTrue(eapError.cause instanceof EapInvalidRequestException);
        verify(mMockTypeDataDecoder).getOpCode(eq(DUMMY_TYPE_DATA));
    }
}
