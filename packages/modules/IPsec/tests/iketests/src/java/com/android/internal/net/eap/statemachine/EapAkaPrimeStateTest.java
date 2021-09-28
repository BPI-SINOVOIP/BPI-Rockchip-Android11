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

import static com.android.internal.net.eap.message.EapData.EAP_NOTIFICATION;
import static com.android.internal.net.eap.message.EapData.EAP_TYPE_AKA_PRIME;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_FAILURE;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_REQUEST;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_SUCCESS;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_AKA_PRIME_CLIENT_ERROR_UNABLE_TO_PROCESS;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_RESPONSE_NOTIFICATION_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.ID_INT;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import com.android.internal.net.eap.EapResult;
import com.android.internal.net.eap.EapResult.EapError;
import com.android.internal.net.eap.EapResult.EapFailure;
import com.android.internal.net.eap.EapResult.EapResponse;
import com.android.internal.net.eap.exceptions.EapInvalidRequestException;
import com.android.internal.net.eap.message.EapData;
import com.android.internal.net.eap.message.EapMessage;
import com.android.internal.net.eap.message.simaka.EapAkaTypeData;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtClientErrorCode;
import com.android.internal.net.eap.message.simaka.EapSimAkaTypeData.DecodeResult;
import com.android.internal.net.eap.statemachine.EapMethodStateMachine.EapMethodState;
import com.android.internal.net.eap.statemachine.EapMethodStateMachine.FinalState;

import org.junit.Test;

public class EapAkaPrimeStateTest extends EapAkaPrimeTest {
    protected static final String NOTIFICATION_MESSAGE = "test";

    @Test
    public void testProcessNotification() throws Exception {
        EapData eapData = new EapData(EAP_NOTIFICATION, NOTIFICATION_MESSAGE.getBytes());
        EapMessage notification = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);
        EapMethodState preNotification = (EapMethodState) mStateMachine.getState();

        EapResult result = mStateMachine.process(notification);
        assertEquals(preNotification, mStateMachine.getState());
        verifyNoMoreInteractions(mMockTelephonyManager, mMockTypeDataDecoder);

        EapResponse eapResponse = (EapResponse) result;
        assertArrayEquals(EAP_RESPONSE_NOTIFICATION_PACKET, eapResponse.packet);
    }

    @Test
    public void testProcessInvalidDecodeResult() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_AKA_PRIME, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);
        EapMethodState preProcess = (EapMethodState) mStateMachine.getState();

        AtClientErrorCode atClientErrorCode = AtClientErrorCode.UNABLE_TO_PROCESS;
        DecodeResult<EapAkaTypeData> decodeResult = new DecodeResult<>(atClientErrorCode);
        doReturn(decodeResult).when(mMockTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));

        EapResult result = mStateMachine.process(eapMessage);
        assertEquals(preProcess, mStateMachine.getState());
        verify(mMockTypeDataDecoder).decode(DUMMY_EAP_TYPE_DATA);
        verifyNoMoreInteractions(mMockTelephonyManager, mMockTypeDataDecoder);

        EapResponse eapResponse = (EapResponse) result;
        assertArrayEquals(EAP_AKA_PRIME_CLIENT_ERROR_UNABLE_TO_PROCESS, eapResponse.packet);
    }

    @Test
    public void testHandleEapFailure() throws Exception {
        EapResult result = mStateMachine.process(new EapMessage(EAP_CODE_FAILURE, ID_INT, null));
        assertTrue(result instanceof EapFailure);
        assertTrue(mStateMachine.getState() instanceof FinalState);
    }

    @Test
    public void testHandleEapSuccess() throws Exception {
        EapResult result = mStateMachine.process(new EapMessage(EAP_CODE_SUCCESS, ID_INT, null));
        EapError eapError = (EapError) result;
        assertTrue(eapError.cause instanceof EapInvalidRequestException);
    }
}
