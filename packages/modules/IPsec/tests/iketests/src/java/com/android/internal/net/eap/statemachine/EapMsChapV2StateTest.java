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
import static com.android.internal.net.eap.message.EapData.EAP_NOTIFICATION;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_FAILURE;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_REQUEST;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_SUCCESS;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_RESPONSE_NOTIFICATION_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.ID_INT;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_PASSWORD;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_USERNAME;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;

import android.net.eap.EapSessionConfig.EapMsChapV2Config;

import com.android.internal.net.eap.EapResult;
import com.android.internal.net.eap.EapResult.EapError;
import com.android.internal.net.eap.EapResult.EapFailure;
import com.android.internal.net.eap.EapResult.EapResponse;
import com.android.internal.net.eap.exceptions.EapInvalidRequestException;
import com.android.internal.net.eap.message.EapData;
import com.android.internal.net.eap.message.EapMessage;
import com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EapMsChapV2TypeDataDecoder;
import com.android.internal.net.eap.statemachine.EapMethodStateMachine.EapMethodState;
import com.android.internal.net.eap.statemachine.EapMethodStateMachine.FinalState;

import org.junit.Before;
import org.junit.Test;

import java.security.SecureRandom;

public class EapMsChapV2StateTest {
    protected static final String CREATED_STATE_TAG = "CreatedState";
    protected static final String NOTIFICATION_MESSAGE = "test";
    protected static final byte[] DUMMY_TYPE_DATA = hexStringToByteArray("00112233");

    protected SecureRandom mMockSecureRandom;
    protected EapMsChapV2TypeDataDecoder mMockTypeDataDecoder;

    protected EapMsChapV2Config mEapMsChapV2Config;
    protected EapMsChapV2MethodStateMachine mStateMachine;

    @Before
    public void setUp() {
        mMockSecureRandom = mock(SecureRandom.class);
        mMockTypeDataDecoder = mock(EapMsChapV2TypeDataDecoder.class);

        mEapMsChapV2Config = new EapMsChapV2Config(MSCHAP_V2_USERNAME, MSCHAP_V2_PASSWORD);
        mStateMachine =
                new EapMsChapV2MethodStateMachine(
                        mEapMsChapV2Config, mMockSecureRandom, mMockTypeDataDecoder);
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

    @Test
    public void testHandleEapNotification() throws Exception {
        EapData eapData = new EapData(EAP_NOTIFICATION, NOTIFICATION_MESSAGE.getBytes());
        EapMessage notification = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);
        EapMethodState preNotification = (EapMethodState) mStateMachine.getState();

        EapResult result = mStateMachine.process(notification);
        assertEquals(preNotification, mStateMachine.getState());

        EapResponse eapResponse = (EapResponse) result;
        assertArrayEquals(EAP_RESPONSE_NOTIFICATION_PACKET, eapResponse.packet);
    }
}
