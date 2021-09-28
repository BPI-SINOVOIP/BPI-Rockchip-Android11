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

import static com.android.internal.net.eap.message.EapData.EAP_IDENTITY;
import static com.android.internal.net.eap.message.EapData.EAP_TYPE_SIM;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_FAILURE;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_REQUEST;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_SUCCESS;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.ID_INT;

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
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtPermanentIdReq;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtVersionList;
import com.android.internal.net.eap.message.simaka.EapSimAkaTypeData.DecodeResult;
import com.android.internal.net.eap.message.simaka.EapSimTypeData;
import com.android.internal.net.eap.statemachine.EapMethodStateMachine.FinalState;
import com.android.internal.net.eap.statemachine.EapSimMethodStateMachine.CreatedState;
import com.android.internal.net.eap.statemachine.EapSimMethodStateMachine.StartState;

import org.junit.Before;
import org.junit.Test;

import java.util.Arrays;
import java.util.List;

public class EapSimCreatedStateTest extends EapSimStateTest {
    private static final int EAP_SIM_START = 10;

    private CreatedState mCreatedState;

    @Before
    public void setUp() {
        super.setUp();
        mCreatedState = mEapSimMethodStateMachine.new CreatedState();
    }

    @Test
    public void testProcessSuccess() throws Exception {
        EapMessage input = new EapMessage(EAP_CODE_SUCCESS, ID_INT, null);
        EapResult result = mCreatedState.process(input);

        EapError eapError = (EapError) result;
        assertTrue(eapError.cause instanceof EapInvalidRequestException);
    }

    @Test
    public void testProcessFailure() throws Exception {
        EapMessage input = new EapMessage(EAP_CODE_FAILURE, ID_INT, null);
        EapResult result = mCreatedState.process(input);
        assertTrue(mEapSimMethodStateMachine.getState() instanceof FinalState);

        assertTrue(result instanceof EapFailure);
    }

    @Test
    public void testTransitionToStartState() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_SIM, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        List<EapSimAkaAttribute> attributes = Arrays.asList(
                new AtVersionList(8, 1), new AtPermanentIdReq());
        DecodeResult<EapSimTypeData> decodeResult =
                new DecodeResult<>(new EapSimTypeData(EAP_SIM_START, attributes));
        doReturn(decodeResult).when(mMockEapSimTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));

        mEapSimMethodStateMachine.process(eapMessage);
        assertTrue(mEapSimMethodStateMachine.getState() instanceof StartState);

        // decoded in CreatedState and StartState
        verify(mMockEapSimTypeDataDecoder, times(2)).decode(eq(DUMMY_EAP_TYPE_DATA));
        verifyNoMoreInteractions(mMockEapSimTypeDataDecoder);
    }

    @Test
    public void testProcessIncorrectEapMethodType() throws Exception {
        EapData eapData = new EapData(EAP_IDENTITY, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        EapResult result = mEapSimMethodStateMachine.process(eapMessage);
        EapError eapError = (EapError) result;
        assertTrue(eapError.cause instanceof EapInvalidRequestException);
    }
}
