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
import static com.android.internal.net.eap.message.EapData.EAP_TYPE_AKA;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_REQUEST;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_AKA_CLIENT_ERROR_UNABLE_TO_PROCESS;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_AKA_NOTIFICATION_RESPONSE;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.ID_INT;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.IMSI;
import static com.android.internal.net.eap.message.simaka.EapAkaTypeData.EAP_AKA_IDENTITY;
import static com.android.internal.net.eap.message.simaka.EapAkaTypeData.EAP_AKA_NOTIFICATION;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtNotification.GENERAL_FAILURE_PRE_CHALLENGE;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.net.eap.EapSessionConfig.EapAkaConfig;
import android.telephony.TelephonyManager;

import com.android.internal.net.eap.EapResult.EapError;
import com.android.internal.net.eap.EapResult.EapResponse;
import com.android.internal.net.eap.exceptions.EapInvalidRequestException;
import com.android.internal.net.eap.message.EapData;
import com.android.internal.net.eap.message.EapMessage;
import com.android.internal.net.eap.message.simaka.EapAkaTypeData;
import com.android.internal.net.eap.message.simaka.EapAkaTypeData.EapAkaTypeDataDecoder;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtAnyIdReq;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtNotification;
import com.android.internal.net.eap.message.simaka.EapSimAkaTypeData.DecodeResult;
import com.android.internal.net.eap.statemachine.EapAkaMethodStateMachine.CreatedState;

import org.junit.Before;
import org.junit.Test;

import java.util.Arrays;

public class EapAkaMethodStateMachineTest {
    private static final int SUB_ID = 1;
    private static final byte[] DUMMY_EAP_TYPE_DATA = hexStringToByteArray("112233445566");

    // EAP-Identity = hex("test@android.net")
    protected static final byte[] EAP_IDENTITY_BYTES =
            hexStringToByteArray("7465737440616E64726F69642E6E6574");

    protected TelephonyManager mMockTelephonyManager;
    private EapAkaTypeDataDecoder mMockEapAkaTypeDataDecoder;

    private EapAkaConfig mEapAkaConfig = new EapAkaConfig(SUB_ID, APPTYPE_USIM);
    private EapAkaMethodStateMachine mEapAkaMethodStateMachine;

    @Before
    public void setUp() {
        mMockTelephonyManager = mock(TelephonyManager.class);
        mMockEapAkaTypeDataDecoder = mock(EapAkaTypeDataDecoder.class);

        TelephonyManager mockInitialTelephonyManager = mock(TelephonyManager.class);
        doReturn(mMockTelephonyManager)
                .when(mockInitialTelephonyManager)
                .createForSubscriptionId(SUB_ID);

        mEapAkaMethodStateMachine =
                new EapAkaMethodStateMachine(
                        mockInitialTelephonyManager,
                        EAP_IDENTITY_BYTES,
                        mEapAkaConfig,
                        mMockEapAkaTypeDataDecoder,
                        false);

        verify(mockInitialTelephonyManager).createForSubscriptionId(SUB_ID);
    }

    @Test
    public void testEapAkaMethodStateMachineStartState() {
        assertTrue(mEapAkaMethodStateMachine.getState() instanceof CreatedState);
    }

    @Test
    public void testGetEapMethod() {
        assertEquals(EAP_TYPE_AKA, mEapAkaMethodStateMachine.getEapMethod());
    }

    @Test
    public void testEapAkaFailsOnMultipleAkaNotifications() throws Exception {
        EapData eapData = new EapData(EAP_TYPE_AKA, DUMMY_EAP_TYPE_DATA);
        EapMessage eapMessage = new EapMessage(EAP_CODE_REQUEST, ID_INT, eapData);

        // First EAP-AKA/Notification
        EapAkaTypeData notificationTypeData =
                new EapAkaTypeData(
                        EAP_AKA_NOTIFICATION,
                        Arrays.asList(new AtNotification(GENERAL_FAILURE_PRE_CHALLENGE)));
        DecodeResult<EapAkaTypeData> decodeResult = new DecodeResult<>(notificationTypeData);
        doReturn(decodeResult).when(mMockEapAkaTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));

        EapResponse eapResponse = (EapResponse) mEapAkaMethodStateMachine.process(eapMessage);
        assertArrayEquals(EAP_AKA_NOTIFICATION_RESPONSE, eapResponse.packet);
        verify(mMockEapAkaTypeDataDecoder).decode(DUMMY_EAP_TYPE_DATA);
        verifyNoMoreInteractions(mMockTelephonyManager, mMockEapAkaTypeDataDecoder);

        // Transition to IdentityState
        decodeResult =
                new DecodeResult<>(
                        new EapAkaTypeData(EAP_AKA_IDENTITY, Arrays.asList(new AtAnyIdReq())));
        doReturn(decodeResult).when(mMockEapAkaTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));
        doReturn(IMSI).when(mMockTelephonyManager).getSubscriberId();

        eapResponse = (EapResponse) mEapAkaMethodStateMachine.process(eapMessage);
        assertFalse(
                "EAP-Request/AKA-Identity returned a Client-Error response",
                Arrays.equals(EAP_AKA_CLIENT_ERROR_UNABLE_TO_PROCESS, eapResponse.packet));

        // decoded in: previous 1 time + in CreatedState and IdentityState
        verify(mMockEapAkaTypeDataDecoder, times(3)).decode(eq(DUMMY_EAP_TYPE_DATA));
        verify(mMockTelephonyManager).getSubscriberId();
        verifyNoMoreInteractions(mMockTelephonyManager, mMockEapAkaTypeDataDecoder);

        // Second EAP-AKA/Notification
        decodeResult = new DecodeResult<>(notificationTypeData);
        doReturn(decodeResult).when(mMockEapAkaTypeDataDecoder).decode(eq(DUMMY_EAP_TYPE_DATA));

        EapError eapError = (EapError) mEapAkaMethodStateMachine.process(eapMessage);
        assertTrue(eapError.cause instanceof EapInvalidRequestException);

        // decoded previous 3 times + 1
        verify(mMockEapAkaTypeDataDecoder, times(4)).decode(DUMMY_EAP_TYPE_DATA);
        verifyNoMoreInteractions(mMockTelephonyManager, mMockEapAkaTypeDataDecoder);
    }
}
