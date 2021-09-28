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

import static com.android.internal.net.eap.EapTestUtils.getDummyEapSessionConfig;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_IDENTITY;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_REQUEST_IDENTITY_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_REQUEST_NOTIFICATION_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_REQUEST_SIM_START_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_RESPONSE_IDENTITY_DEFAULT_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_RESPONSE_IDENTITY_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_RESPONSE_NOTIFICATION_PACKET;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import com.android.internal.net.eap.EapResult;
import com.android.internal.net.eap.EapResult.EapResponse;
import com.android.internal.net.eap.statemachine.EapStateMachine.MethodState;

import org.junit.Before;
import org.junit.Test;

import java.security.SecureRandom;

public class IdentityStateTest extends EapStateTest {
    private EapStateMachine mEapStateMachineSpy;

    @Before
    @Override
    public void setUp() {
        super.setUp();

        mEapStateMachineSpy = spy(mEapStateMachine);
        mEapState = mEapStateMachineSpy.new IdentityState();
    }

    @Test
    public void testProcess() {
        mEapSessionConfig = getDummyEapSessionConfig(EAP_IDENTITY);
        mEapStateMachineSpy = spy(
                new EapStateMachine(mContext, mEapSessionConfig, new SecureRandom()));
        mEapState = mEapStateMachineSpy.new IdentityState();

        EapResult eapResult = mEapState.process(EAP_REQUEST_IDENTITY_PACKET);

        assertTrue(eapResult instanceof EapResponse);
        EapResponse eapResponse = (EapResponse) eapResult;
        assertArrayEquals(EAP_RESPONSE_IDENTITY_PACKET, eapResponse.packet);
        verify(mEapStateMachineSpy, never()).transitionAndProcess(any(), any());
    }

    @Test
    public void testProcessDefaultIdentity() {
        EapResult eapResult = mEapState.process(EAP_REQUEST_IDENTITY_PACKET);

        assertTrue(eapResult instanceof EapResponse);
        EapResponse eapResponse = (EapResponse) eapResult;
        assertArrayEquals(EAP_RESPONSE_IDENTITY_DEFAULT_PACKET, eapResponse.packet);
        verify(mEapStateMachineSpy, never()).transitionAndProcess(any(), any());
    }

    @Test
    public void testProcessNotificationRequest() {
        EapResult eapResult = mEapState.process(EAP_REQUEST_NOTIFICATION_PACKET);

        // state shouldn't change after Notification request
        assertTrue(eapResult instanceof EapResponse);
        EapResponse eapResponse = (EapResponse) eapResult;
        assertArrayEquals(EAP_RESPONSE_NOTIFICATION_PACKET, eapResponse.packet);
        verify(mEapStateMachineSpy, never()).transitionAndProcess(any(), any());
    }

    @Test
    public void testProcessSimStart() {
        mEapState.process(EAP_REQUEST_SIM_START_PACKET);

        // EapStateMachine should change to MethodState for method-type packet
        verify(mEapStateMachineSpy).transitionAndProcess(
                any(MethodState.class), eq(EAP_REQUEST_SIM_START_PACKET));
    }
}
