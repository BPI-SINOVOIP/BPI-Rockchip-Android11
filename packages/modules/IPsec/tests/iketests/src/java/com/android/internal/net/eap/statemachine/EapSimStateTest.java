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

import static android.telephony.TelephonyManager.APPTYPE_USIM;

import static com.android.internal.net.TestUtils.hexStringToByteArray;
import static com.android.internal.net.eap.message.EapData.EAP_NOTIFICATION;
import static com.android.internal.net.eap.message.EapData.EAP_TYPE_SIM;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_REQUEST;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_RESPONSE_NOTIFICATION_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_SIM_CLIENT_ERROR_INSUFFICIENT_CHALLENGES;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_SIM_NOTIFICATION_RESPONSE;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.ID_INT;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtNotification.GENERAL_FAILURE_PRE_CHALLENGE;
import static com.android.internal.net.eap.message.simaka.EapSimTypeData.EAP_SIM_NOTIFICATION;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.net.eap.EapSessionConfig.EapSimConfig;
import android.telephony.TelephonyManager;

import com.android.internal.net.eap.EapResult;
import com.android.internal.net.eap.EapResult.EapResponse;
import com.android.internal.net.eap.message.EapData;
import com.android.internal.net.eap.message.EapMessage;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtClientErrorCode;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtNotification;
import com.android.internal.net.eap.message.simaka.EapSimAkaTypeData.DecodeResult;
import com.android.internal.net.eap.message.simaka.EapSimTypeData;
import com.android.internal.net.eap.message.simaka.EapSimTypeData.EapSimTypeDataDecoder;
import com.android.internal.net.eap.statemachine.EapMethodStateMachine.EapMethodState;

import org.junit.Before;
import org.junit.Test;

import java.security.SecureRandom;
import java.util.Arrays;

public class EapSimStateTest {
    protected static final int SUB_ID = 1;
    protected static final String NOTIFICATION_MESSAGE = "test";
    protected static final byte[] DUMMY_EAP_TYPE_DATA = hexStringToByteArray("112233445566");

    // EAP-Identity = hex("test@android.net")
    protected static final byte[] EAP_IDENTITY_BYTES =
            hexStringToByteArray("7465737440616E64726F69642E6E6574");

    protected TelephonyManager mMockTelephonyManager;
    protected EapSimTypeDataDecoder mMockEapSimTypeDataDecoder;

    protected EapSimConfig mEapSimConfig = new EapSimConfig(SUB_ID, APPTYPE_USIM);
    protected EapSimMethodStateMachine mEapSimMethodStateMachine;

    @Before
    public void setUp() {
        mMockTelephonyManager = mock(TelephonyManager.class);
        mMockEapSimTypeDataDecoder = mock(EapSimTypeDataDecoder.class);

        TelephonyManager mockInitialTelephonyManager = mock(TelephonyManager.class);
        doReturn(mMockTelephonyManager)
                .when(mockInitialTelephonyManager)
                .createForSubscriptionId(SUB_ID);

        mEapSimMethodStateMachine =
                new EapSimMethodStateMachine(
                        mockInitialTelephonyManager,
                        EAP_IDENTITY_BYTES,
                        mEapSimConfig,
                        new SecureRandom(),
                        mMockEapSimTypeDataDecoder);

        verify(mockInitialTelephonyManager).createForSubscriptionId(SUB_ID);
    }

    @Test
    public void testProcessNotification() throws Exception {
        EapData eapData = new EapData(EAP_NOTIFICATION, NOTIFICATION_MESSAGE.getBytes());
        EapMessage notification = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);
        EapMethodState preNotification = (EapMethodState) mEapSimMethodStateMachine.getState();

        EapResult result = mEapSimMethodStateMachine.process(notification);
        assertEquals(preNotification, mEapSimMethodStateMachine.getState());
        verifyNoMoreInteractions(mMockTelephonyManager, mMockEapSimTypeDataDecoder);

        assertTrue(result instanceof EapResponse);
        EapResponse eapResponse = (EapResponse) result;
        assertArrayEquals(EAP_RESPONSE_NOTIFICATION_PACKET, eapResponse.packet);
    }

    @Test
    public void testProcessEapSimNotification() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_SIM, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);
        EapMethodState preProcess = (EapMethodState) mEapSimMethodStateMachine.getState();
        EapSimTypeData typeData =
                new EapSimTypeData(
                        EAP_SIM_NOTIFICATION,
                        Arrays.asList(new AtNotification(GENERAL_FAILURE_PRE_CHALLENGE)));

        DecodeResult<EapSimTypeData> decodeResult = new DecodeResult<>(typeData);
        doReturn(decodeResult).when(mMockEapSimTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));

        EapResponse eapResponse = (EapResponse) mEapSimMethodStateMachine.process(eapMessage);
        assertArrayEquals(EAP_SIM_NOTIFICATION_RESPONSE, eapResponse.packet);
        assertEquals(preProcess, mEapSimMethodStateMachine.getState());
        verify(mMockEapSimTypeDataDecoder).decode(DUMMY_EAP_TYPE_DATA);
        verifyNoMoreInteractions(mMockTelephonyManager, mMockEapSimTypeDataDecoder);
    }

    @Test
    public void testProcessInvalidDecodeResult() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_SIM, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);
        EapMethodState preProcess = (EapMethodState) mEapSimMethodStateMachine.getState();

        AtClientErrorCode atClientErrorCode = AtClientErrorCode.INSUFFICIENT_CHALLENGES;
        DecodeResult<EapSimTypeData> decodeResult = new DecodeResult<>(atClientErrorCode);
        doReturn(decodeResult).when(mMockEapSimTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));

        EapResult result = mEapSimMethodStateMachine.process(eapMessage);
        assertEquals(preProcess, mEapSimMethodStateMachine.getState());
        verify(mMockEapSimTypeDataDecoder).decode(DUMMY_EAP_TYPE_DATA);
        verifyNoMoreInteractions(mMockTelephonyManager, mMockEapSimTypeDataDecoder);

        assertTrue(result instanceof EapResponse);
        EapResponse eapResponse = (EapResponse) result;
        assertArrayEquals(EAP_SIM_CLIENT_ERROR_INSUFFICIENT_CHALLENGES, eapResponse.packet);
    }
}
