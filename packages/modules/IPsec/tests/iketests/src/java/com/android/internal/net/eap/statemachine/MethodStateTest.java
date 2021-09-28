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

import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_FAILURE;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_SUCCESS;
import static com.android.internal.net.eap.message.EapMessage.EAP_HEADER_LENGTH;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_AKA_PRIME_REQUEST;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_FAILURE_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_REQUEST_AKA;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_REQUEST_IDENTITY_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_REQUEST_MSCHAP_V2;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_REQUEST_NOTIFICATION_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_REQUEST_SIM_START_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_RESPONSE_NAK_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_RESPONSE_NOTIFICATION_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_SUCCESS_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EMSK;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.ID_INT;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSK;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.net.eap.EapSessionConfig;

import com.android.internal.net.eap.EapResult;
import com.android.internal.net.eap.EapResult.EapError;
import com.android.internal.net.eap.EapResult.EapFailure;
import com.android.internal.net.eap.EapResult.EapResponse;
import com.android.internal.net.eap.EapResult.EapSuccess;
import com.android.internal.net.eap.exceptions.EapInvalidRequestException;
import com.android.internal.net.eap.message.EapMessage;
import com.android.internal.net.eap.statemachine.EapStateMachine.FailureState;
import com.android.internal.net.eap.statemachine.EapStateMachine.MethodState;
import com.android.internal.net.eap.statemachine.EapStateMachine.SuccessState;

import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentMatcher;

import java.security.SecureRandom;

public class MethodStateTest extends EapStateTest {
    private static final String USERNAME = "username";
    private static final String PASSWORD = "password";
    private static final String NETWORK_NAME = "android.net";
    private static final boolean ALLOW_MISMATCHED_NETWORK_NAMES = true;

    @Before
    @Override
    public void setUp() {
        super.setUp();
        mEapState = mEapStateMachine.new MethodState();
        mEapStateMachine.transitionTo(mEapState);
    }

    @Test
    public void testProcessUnsupportedEapType() {
        mEapState = mEapStateMachine.new MethodState();
        EapResult result = mEapState.process(EAP_REQUEST_IDENTITY_PACKET);

        EapResponse eapResponse = (EapResponse) result;
        assertArrayEquals(EAP_RESPONSE_NAK_PACKET, eapResponse.packet);
    }

    @Test
    public void testProcessTransitionsToEapSim() {
        mEapStateMachine.process(EAP_REQUEST_SIM_START_PACKET);

        assertTrue(mEapStateMachine.getState() instanceof MethodState);
        MethodState methodState = (MethodState) mEapStateMachine.getState();
        assertTrue(methodState.mEapMethodStateMachine instanceof EapSimMethodStateMachine);
    }

    @Test
    public void testProcessTransitionToEapAka() {
        // make EapStateMachine with EAP-AKA configurations
        EapSessionConfig eapSessionConfig = new EapSessionConfig.Builder()
                .setEapAkaConfig(0, APPTYPE_USIM).build();
        mEapStateMachine = new EapStateMachine(mContext, eapSessionConfig, new SecureRandom());

        mEapStateMachine.process(EAP_REQUEST_AKA);

        assertTrue(mEapStateMachine.getState() instanceof MethodState);
        MethodState methodState = (MethodState) mEapStateMachine.getState();
        assertTrue(methodState.mEapMethodStateMachine instanceof EapAkaMethodStateMachine);
    }

    @Test
    public void testProcessTransitionToEapAkaPrime() {
        // make EapStateMachine with EAP-AKA' configurations
        EapSessionConfig eapSessionConfig =
                new EapSessionConfig.Builder()
                        .setEapAkaPrimeConfig(
                                0, APPTYPE_USIM, NETWORK_NAME, ALLOW_MISMATCHED_NETWORK_NAMES)
                        .build();
        mEapStateMachine = new EapStateMachine(mContext, eapSessionConfig, new SecureRandom());

        mEapStateMachine.process(EAP_AKA_PRIME_REQUEST);

        assertTrue(mEapStateMachine.getState() instanceof MethodState);
        MethodState methodState = (MethodState) mEapStateMachine.getState();
        assertTrue(methodState.mEapMethodStateMachine instanceof EapAkaPrimeMethodStateMachine);
    }

    @Test
    public void testProcessTransitionToEapMsChapV2() {
        // make EapStateMachine with EAP MSCHAPv2 configurations
        EapSessionConfig eapSessionConfig =
                new EapSessionConfig.Builder().setEapMsChapV2Config(USERNAME, PASSWORD).build();
        mEapStateMachine = new EapStateMachine(mContext, eapSessionConfig, new SecureRandom());

        mEapStateMachine.process(EAP_REQUEST_MSCHAP_V2);

        assertTrue(mEapStateMachine.getState() instanceof MethodState);
        MethodState methodState = (MethodState) mEapStateMachine.getState();
        assertTrue(methodState.mEapMethodStateMachine instanceof EapMsChapV2MethodStateMachine);
    }

    @Test
    public void testProcessTransitionToSuccessState() {
        EapSuccess eapSuccess = new EapSuccess(MSK, EMSK);

        ArgumentMatcher<EapMessage> eapSuccessMatcher = msg ->
                msg.eapCode == EAP_CODE_SUCCESS
                        && msg.eapIdentifier == ID_INT
                        && msg.eapLength == EAP_HEADER_LENGTH
                        && msg.eapData == null;

        EapMethodStateMachine mockEapMethodStateMachine = mock(EapMethodStateMachine.class);
        doReturn(eapSuccess).when(mockEapMethodStateMachine).process(argThat(eapSuccessMatcher));
        ((MethodState) mEapState).mEapMethodStateMachine = mockEapMethodStateMachine;

        mEapState.process(EAP_SUCCESS_PACKET);
        verify(mockEapMethodStateMachine).process(argThat(eapSuccessMatcher));
        assertTrue(mEapStateMachine.getState() instanceof SuccessState);
        verifyNoMoreInteractions(mockEapMethodStateMachine);
    }

    @Test
    public void testProcessTransitionToFailureState() {
        EapFailure eapFailure = new EapFailure();

        ArgumentMatcher<EapMessage> eapSuccessMatcher = msg ->
                msg.eapCode == EAP_CODE_FAILURE
                        && msg.eapIdentifier == ID_INT
                        && msg.eapLength == EAP_HEADER_LENGTH
                        && msg.eapData == null;

        EapMethodStateMachine mockEapMethodStateMachine = mock(EapMethodStateMachine.class);
        doReturn(eapFailure).when(mockEapMethodStateMachine).process(argThat(eapSuccessMatcher));
        ((MethodState) mEapState).mEapMethodStateMachine = mockEapMethodStateMachine;

        mEapState.process(EAP_FAILURE_PACKET);
        verify(mockEapMethodStateMachine).process(argThat(eapSuccessMatcher));
        assertTrue(mEapStateMachine.getState() instanceof FailureState);
        verifyNoMoreInteractions(mockEapMethodStateMachine);
    }

    @Test
    public void testProcessEapFailureWithNoEapMethodState() {
        EapResult result = mEapStateMachine.process(EAP_FAILURE_PACKET);
        assertTrue(result instanceof EapFailure);
        assertTrue(mEapStateMachine.getState() instanceof FailureState);
    }

    @Test
    public void testProcessEapSuccessWithNoEapMethodState() {
        EapResult result = mEapStateMachine.process(EAP_SUCCESS_PACKET);
        EapError eapError = (EapError) result;
        assertTrue(eapError.cause instanceof EapInvalidRequestException);
        assertTrue(mEapStateMachine.getState() instanceof MethodState);
    }

    @Test
    public void testProcessEapNotificationWithNoEapMethodState() {
        EapResult result = mEapStateMachine.process(EAP_REQUEST_NOTIFICATION_PACKET);
        EapResponse eapResponse = (EapResponse) result;
        assertArrayEquals(EAP_RESPONSE_NOTIFICATION_PACKET, eapResponse.packet);
        assertTrue(mEapStateMachine.getState() instanceof MethodState);
    }
}
