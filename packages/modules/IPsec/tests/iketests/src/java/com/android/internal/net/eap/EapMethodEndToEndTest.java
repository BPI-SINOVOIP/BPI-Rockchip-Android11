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

package com.android.internal.net.eap;

import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_FAILURE_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_REQUEST_NOTIFICATION_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_RESPONSE_NOTIFICATION_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_SUCCESS;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.Context;
import android.net.eap.EapSessionConfig;
import android.os.test.TestLooper;

import com.android.internal.net.eap.exceptions.EapInvalidRequestException;

import org.junit.Before;

import java.security.SecureRandom;

public class EapMethodEndToEndTest {
    protected Context mMockContext;
    protected SecureRandom mMockSecureRandom;
    protected IEapCallback mMockCallback;

    protected TestLooper mTestLooper;
    protected EapSessionConfig mEapSessionConfig;
    protected EapAuthenticator mEapAuthenticator;

    @Before
    public void setUp() {
        mMockContext = mock(Context.class);
        mMockSecureRandom = mock(SecureRandom.class);
        mMockCallback = mock(IEapCallback.class);

        mTestLooper = new TestLooper();
    }

    protected void verifyUnsupportedType(byte[] invalidMessageType, byte[] nakResponse) {
        mEapAuthenticator.processEapMessage(invalidMessageType);
        mTestLooper.dispatchAll();

        // verify EAP-Response/Nak returned
        verify(mMockCallback).onResponse(eq(nakResponse));
        verifyNoMoreInteractions(mMockCallback);
    }

    protected void verifyEapNotification(int callsToVerify) {
        mEapAuthenticator.processEapMessage(EAP_REQUEST_NOTIFICATION_PACKET);
        mTestLooper.dispatchAll();

        verify(mMockCallback, times(callsToVerify))
                .onResponse(eq(EAP_RESPONSE_NOTIFICATION_PACKET));
        verifyNoMoreInteractions(mMockCallback);
    }

    protected void verifyEapSuccess(byte[] msk, byte[] emsk) {
        // EAP-Success
        mEapAuthenticator.processEapMessage(EAP_SUCCESS);
        mTestLooper.dispatchAll();

        // verify that onSuccess callback made
        verify(mMockCallback).onSuccess(eq(msk), eq(emsk));
        verifyNoMoreInteractions(mMockContext, mMockSecureRandom, mMockCallback);
    }

    protected void verifyEapFailure() {
        mEapAuthenticator.processEapMessage(EAP_FAILURE_PACKET);
        mTestLooper.dispatchAll();

        verify(mMockCallback).onFail();
        verifyNoMoreInteractions(mMockCallback);
    }

    /**
     * To be used for verifying that non-EAP-Failure messages receive EapError responses after
     * EAP-Failure messages are expected.
     */
    protected void verifyExpectsEapFailure(byte[] incomingEapPacket) {
        mEapAuthenticator.processEapMessage(incomingEapPacket);
        mTestLooper.dispatchAll();

        verify(mMockCallback).onError(any(EapInvalidRequestException.class));
    }
}
