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

package com.android.server.telecom.tests;

import static junit.framework.Assert.assertEquals;

import static org.mockito.ArgumentMatchers.nullable;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.net.Uri;
import android.provider.CallLog;
import android.telecom.CallerInfo;
import android.test.suitebuilder.annotation.SmallTest;

import com.android.server.telecom.Call;
import com.android.server.telecom.CallerInfoLookupHelper;
import com.android.server.telecom.callfiltering.CallFilter;
import com.android.server.telecom.callfiltering.CallFilteringResult;
import com.android.server.telecom.callfiltering.DirectToVoicemailFilter;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;

import java.util.concurrent.CompletionStage;
import java.util.concurrent.TimeUnit;


@RunWith(JUnit4.class)
public class DirectToVoicemailFilterTest extends TelecomTestCase {
    @Mock private CallerInfoLookupHelper mCallerInfoLookupHelper;
    @Mock private Call mCall;

    private final CallFilteringResult PASS_RESULT = new CallFilteringResult.Builder()
            .setShouldAllowCall(true)
            .setShouldReject(false)
            .setShouldAddToCallLog(true)
            .setShouldShowNotification(true)
            .build();
    private final CallFilteringResult DIRECT_TO_VOICEMAIL_RESULT = new CallFilteringResult.Builder()
            .setShouldAllowCall(false)
            .setShouldReject(true)
            .setShouldAddToCallLog(true)
            .setShouldShowNotification(true)
            .setCallBlockReason(CallLog.Calls.BLOCK_REASON_DIRECT_TO_VOICEMAIL)
            .setCallScreeningAppName(null)
            .setCallScreeningComponentName(null)
            .build();
    private final long CALLER_INFO_TIMEOUT = 2000;

    private Uri mTestHandle;
    private DirectToVoicemailFilter mFilter;
    private CallerInfo mCallerInfo;

    @Override
    @Before
    public void setUp() throws Exception {
        super.setUp();
        mTestHandle = Uri.parse("tel:1235551234");
        mFilter = new DirectToVoicemailFilter(mCall, mCallerInfoLookupHelper);
        mCallerInfo = new CallerInfo();
        when(mCall.getHandle()).thenReturn(mTestHandle);
    }

    @SmallTest
    @Test
    public void testSendToVoicemail() throws Exception {
        mCallerInfo.shouldSendToVoicemail = true;

        CompletionStage<CallFilteringResult> resultFuture = mFilter.startFilterLookup(PASS_RESULT);
        CallerInfoLookupHelper.OnQueryCompleteListener listener = verifyLookupStart();
        listener.onCallerInfoQueryComplete(mTestHandle, mCallerInfo);
        assertEquals(DIRECT_TO_VOICEMAIL_RESULT, resultFuture.toCompletableFuture()
                .get(CALLER_INFO_TIMEOUT, TimeUnit.MILLISECONDS));
    }

    @SmallTest
    @Test
    public void testDontSendToVoicemail() throws Exception {
        mCallerInfo.shouldSendToVoicemail = false;

        CompletionStage<CallFilteringResult> resultFuture = mFilter.startFilterLookup(PASS_RESULT);
        CallerInfoLookupHelper.OnQueryCompleteListener listener = verifyLookupStart();
        listener.onCallerInfoQueryComplete(mTestHandle, mCallerInfo);
        assertEquals(PASS_RESULT, resultFuture.toCompletableFuture()
                .get(CALLER_INFO_TIMEOUT, TimeUnit.MILLISECONDS));
    }

    @SmallTest
    @Test
    public void testNullResponseAndDifferentHandleFromLookupHelper() throws Exception {
        mTestHandle = null;

        CompletionStage<CallFilteringResult> resultFuture = mFilter.startFilterLookup(PASS_RESULT);
        CallerInfoLookupHelper.OnQueryCompleteListener listener = verifyLookupStart();
        listener.onCallerInfoQueryComplete(null, null);
        assertEquals(PASS_RESULT, resultFuture.toCompletableFuture()
                .get(CALLER_INFO_TIMEOUT, TimeUnit.MILLISECONDS));
    }

    @SmallTest
    @Test
    public void testNullResponseAndSameHandleFromLookupHelper() throws Exception {
        CompletionStage<CallFilteringResult> resultFuture = mFilter.startFilterLookup(PASS_RESULT);
        CallerInfoLookupHelper.OnQueryCompleteListener listener = verifyLookupStart();
        listener.onCallerInfoQueryComplete(mTestHandle, null);
        assertEquals(PASS_RESULT, resultFuture.toCompletableFuture()
                .get(CALLER_INFO_TIMEOUT, TimeUnit.MILLISECONDS));
    }

    private CallerInfoLookupHelper.OnQueryCompleteListener verifyLookupStart() {
        ArgumentCaptor<CallerInfoLookupHelper.OnQueryCompleteListener> captor =
                ArgumentCaptor.forClass(CallerInfoLookupHelper.OnQueryCompleteListener.class);
        verify(mCallerInfoLookupHelper, timeout(CALLER_INFO_TIMEOUT))
                .startLookup(nullable(Uri.class), captor.capture());
        return captor.getValue();
    }
}
