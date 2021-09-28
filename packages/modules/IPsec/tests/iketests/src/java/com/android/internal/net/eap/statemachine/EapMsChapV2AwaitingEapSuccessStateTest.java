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

import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_SUCCESS;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.ID_INT;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_MSK;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.MSCHAP_V2_NT_RESPONSE;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertTrue;

import com.android.internal.net.eap.EapResult;
import com.android.internal.net.eap.EapResult.EapSuccess;
import com.android.internal.net.eap.message.EapMessage;
import com.android.internal.net.eap.statemachine.EapMethodStateMachine.FinalState;

import org.junit.Before;
import org.junit.Test;

public class EapMsChapV2AwaitingEapSuccessStateTest extends EapMsChapV2StateTest {
    @Before
    @Override
    public void setUp() {
        super.setUp();

        mStateMachine.transitionTo(
                mStateMachine.new AwaitingEapSuccessState(MSCHAP_V2_NT_RESPONSE));
    }

    @Test
    @Override
    public void testHandleEapSuccess() throws Exception {
        EapResult result = mStateMachine.process(new EapMessage(EAP_CODE_SUCCESS, ID_INT, null));
        EapSuccess eapSuccess = (EapSuccess) result;
        assertArrayEquals(MSCHAP_V2_MSK, eapSuccess.msk);
        assertArrayEquals(new byte[0], eapSuccess.emsk);
        assertTrue(mStateMachine.getState() instanceof FinalState);
    }
}
