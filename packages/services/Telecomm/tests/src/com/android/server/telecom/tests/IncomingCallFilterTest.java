/*
 * Copyright (C) 2016 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.server.telecom.tests;

import android.content.ContentResolver;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.provider.CallLog.Calls;
import android.test.suitebuilder.annotation.SmallTest;

import com.android.server.telecom.Call;
import com.android.server.telecom.Timeouts;
import com.android.server.telecom.callfiltering.CallFilterResultCallback;
import com.android.server.telecom.callfiltering.CallFilteringResult;
import com.android.server.telecom.callfiltering.CallFilteringResult.Builder;
import com.android.server.telecom.callfiltering.IncomingCallFilter;
import com.android.server.telecom.TelecomSystem;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import static org.junit.Assert.assertEquals;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.atMost;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

@RunWith(JUnit4.class)
public class IncomingCallFilterTest extends TelecomTestCase {
    @Mock private CallFilterResultCallback mResultCallback;
    @Mock private Call mCall;
    private final TelecomSystem.SyncRoot mLock = new TelecomSystem.SyncRoot() {};

    @Mock private IncomingCallFilter.CallFilter mFilter1;
    @Mock private IncomingCallFilter.CallFilter mFilter2;
    @Mock private IncomingCallFilter.CallFilter mFilter3;
    @Mock private IncomingCallFilter.CallFilter mFilter4;

    @Mock private Timeouts.Adapter mTimeoutsAdapter;

    private static final Uri TEST_HANDLE = Uri.parse("tel:1235551234");
    private static final long LONG_TIMEOUT = 1000000;
    private static final long SHORT_TIMEOUT = 100;

    private static final CallFilteringResult PASS_CALL_RESULT =
            new Builder()
                    .setShouldAllowCall(true)
                    .setShouldReject(false)
                    .setShouldAddToCallLog(true)
                    .setShouldShowNotification(true)
                    .build();

    private static final CallFilteringResult ASYNC_BLOCK_CHECK_BLOCK_RESULT =
            new Builder()
                    .setShouldAllowCall(false)
                    .setShouldReject(true)
                    .setShouldAddToCallLog(true)
                    .setShouldShowNotification(false)
                    .setCallBlockReason(Calls.BLOCK_REASON_BLOCKED_NUMBER)
                    .setCallScreeningAppName(null)
                    .setCallScreeningComponentName(null)
                    .build();

    private static final CallFilteringResult DIRECT_TO_VOICEMAIL_CALL_BLOCK_RESULT =
            new Builder()
                    .setShouldAllowCall(false)
                    .setShouldReject(true)
                    .setShouldAddToCallLog(true)
                    .setShouldShowNotification(true)
                    .setCallBlockReason(Calls.BLOCK_REASON_DIRECT_TO_VOICEMAIL)
                    .setCallScreeningAppName(null)
                    .setCallScreeningComponentName(null)
                    .build();

    private static final CallFilteringResult CALL_SCREENING_SERVICE_BLOCK_RESULT =
            new Builder()
                    .setShouldAllowCall(false)
                    .setShouldReject(true)
                    .setShouldAddToCallLog(false)
                    .setShouldShowNotification(true)
                    .setCallBlockReason(Calls.BLOCK_REASON_CALL_SCREENING_SERVICE)
                    .setCallScreeningAppName("com.android.thirdparty")
                    .setCallScreeningComponentName(
                            "com.android.thirdparty/"
                                    + "com.android.thirdparty.callscreeningserviceimpl")
                    .build();

    private static final CallFilteringResult DEFAULT_RESULT = PASS_CALL_RESULT;
    private Handler mHandler = new Handler(Looper.getMainLooper());

    @Override
    @Before
    public void setUp() throws Exception {
        super.setUp();
        mContext = mComponentContextFixture.getTestDouble().getApplicationContext();
        when(mCall.getHandle()).thenReturn(TEST_HANDLE);
        setTimeoutLength(LONG_TIMEOUT);
    }

    @Override
    @After
    public void tearDown() throws Exception {
        mHandler.removeCallbacksAndMessages(null);
        waitForHandlerAction(mHandler, 1000);
        super.tearDown();
    }

    @SmallTest
    @Test
    public void testAsyncBlockCallResultFilter() {
        IncomingCallFilter testFilter = new IncomingCallFilter(mContext, mResultCallback, mCall,
                mLock, mTimeoutsAdapter, Collections.singletonList(mFilter1), mHandler);
        testFilter.performFiltering();
        verify(mFilter1).startFilterLookup(mCall, testFilter);

        testFilter.onCallFilteringComplete(mCall, ASYNC_BLOCK_CHECK_BLOCK_RESULT);
        waitForHandlerAction(testFilter.getHandler(), SHORT_TIMEOUT * 2);
        verify(mResultCallback).onCallFilteringComplete(eq(mCall), eq
                (ASYNC_BLOCK_CHECK_BLOCK_RESULT));
    }

    @SmallTest
    @Test
    public void testDirectToVoiceMailCallResultFilter() {
        IncomingCallFilter testFilter = new IncomingCallFilter(mContext, mResultCallback, mCall,
                mLock, mTimeoutsAdapter, Collections.singletonList(mFilter1), mHandler);
        testFilter.performFiltering();
        verify(mFilter1).startFilterLookup(mCall, testFilter);

        testFilter.onCallFilteringComplete(mCall, DIRECT_TO_VOICEMAIL_CALL_BLOCK_RESULT);
        waitForHandlerAction(testFilter.getHandler(), SHORT_TIMEOUT * 2);
        verify(mResultCallback).onCallFilteringComplete(eq(mCall), eq
                (DIRECT_TO_VOICEMAIL_CALL_BLOCK_RESULT));
    }

    @SmallTest
    @Test
    public void testCallScreeningServiceBlockCallResultFilter() {
        IncomingCallFilter testFilter = new IncomingCallFilter(mContext, mResultCallback, mCall,
                mLock, mTimeoutsAdapter, Collections.singletonList(mFilter1), mHandler);
        testFilter.performFiltering();
        verify(mFilter1).startFilterLookup(mCall, testFilter);

        testFilter.onCallFilteringComplete(mCall, CALL_SCREENING_SERVICE_BLOCK_RESULT);
        waitForHandlerAction(testFilter.getHandler(), SHORT_TIMEOUT * 2);
        verify(mResultCallback).onCallFilteringComplete(eq(mCall), eq
                (CALL_SCREENING_SERVICE_BLOCK_RESULT));
    }

    @SmallTest
    @Test
    public void testPassCallResultFilter() {
        IncomingCallFilter testFilter = new IncomingCallFilter(mContext, mResultCallback, mCall,
                mLock, mTimeoutsAdapter, Collections.singletonList(mFilter1), mHandler);
        testFilter.performFiltering();
        verify(mFilter1).startFilterLookup(mCall, testFilter);

        testFilter.onCallFilteringComplete(mCall, PASS_CALL_RESULT);
        waitForHandlerAction(testFilter.getHandler(), SHORT_TIMEOUT * 2);
        verify(mResultCallback).onCallFilteringComplete(eq(mCall), eq(PASS_CALL_RESULT));
    }

    @SmallTest
    @Test
    public void testMultipleFiltersForAsyncBlockCheckFilter() {
        List<IncomingCallFilter.CallFilter> filters =
                new ArrayList<IncomingCallFilter.CallFilter>() {{
                    add(mFilter1);
                    add(mFilter2);
                    add(mFilter3);
                    add(mFilter4);
                }};
        IncomingCallFilter testFilter = new IncomingCallFilter(mContext, mResultCallback, mCall,
                mLock, mTimeoutsAdapter, filters, mHandler);
        testFilter.performFiltering();
        verify(mFilter1).startFilterLookup(mCall, testFilter);
        verify(mFilter2).startFilterLookup(mCall, testFilter);
        verify(mFilter3).startFilterLookup(mCall, testFilter);
        verify(mFilter4).startFilterLookup(mCall, testFilter);

        testFilter.onCallFilteringComplete(mCall, PASS_CALL_RESULT);
        testFilter.onCallFilteringComplete(mCall, ASYNC_BLOCK_CHECK_BLOCK_RESULT);
        testFilter.onCallFilteringComplete(mCall, DIRECT_TO_VOICEMAIL_CALL_BLOCK_RESULT);
        testFilter.onCallFilteringComplete(mCall, CALL_SCREENING_SERVICE_BLOCK_RESULT);
        waitForHandlerAction(testFilter.getHandler(), SHORT_TIMEOUT * 2);
        verify(mResultCallback).onCallFilteringComplete(eq(mCall), eq(new Builder()
                .setShouldAllowCall(false)
                .setShouldReject(true)
                .setShouldAddToCallLog(false)
                .setShouldShowNotification(false)
                .setCallBlockReason(Calls.BLOCK_REASON_BLOCKED_NUMBER)
                .setCallScreeningAppName(null)
                .setCallScreeningComponentName(null)
                .build()));
    }

    @SmallTest
    @Test
    public void testMultipleFiltersForDirectToVoicemailCallFilter() {
        List<IncomingCallFilter.CallFilter> filters =
                new ArrayList<IncomingCallFilter.CallFilter>() {{
                    add(mFilter1);
                    add(mFilter2);
                    add(mFilter3);
                }};
        IncomingCallFilter testFilter = new IncomingCallFilter(mContext, mResultCallback, mCall,
                mLock, mTimeoutsAdapter, filters, mHandler);
        testFilter.performFiltering();
        verify(mFilter1).startFilterLookup(mCall, testFilter);
        verify(mFilter2).startFilterLookup(mCall, testFilter);
        verify(mFilter3).startFilterLookup(mCall, testFilter);

        testFilter.onCallFilteringComplete(mCall, PASS_CALL_RESULT);
        testFilter.onCallFilteringComplete(mCall, DIRECT_TO_VOICEMAIL_CALL_BLOCK_RESULT);
        testFilter.onCallFilteringComplete(mCall, CALL_SCREENING_SERVICE_BLOCK_RESULT);
        waitForHandlerAction(testFilter.getHandler(), SHORT_TIMEOUT * 2);
        verify(mResultCallback).onCallFilteringComplete(eq(mCall), eq(new Builder()
                .setShouldAllowCall(false)
                .setShouldReject(true)
                .setShouldAddToCallLog(false)
                .setShouldShowNotification(true)
                .setCallBlockReason(Calls.BLOCK_REASON_DIRECT_TO_VOICEMAIL)
                .setCallScreeningAppName(null)
                .setCallScreeningComponentName(null)
                .build()));
    }

    @SmallTest
    @Test
    public void testMultipleFiltersForCallScreeningServiceFilter() {
        List<IncomingCallFilter.CallFilter> filters =
                new ArrayList<IncomingCallFilter.CallFilter>() {{
                    add(mFilter1);
                    add(mFilter2);
                }};
        IncomingCallFilter testFilter = new IncomingCallFilter(mContext, mResultCallback, mCall,
                mLock, mTimeoutsAdapter, filters, mHandler);
        testFilter.performFiltering();
        verify(mFilter1).startFilterLookup(mCall, testFilter);
        verify(mFilter2).startFilterLookup(mCall, testFilter);

        testFilter.onCallFilteringComplete(mCall, PASS_CALL_RESULT);
        testFilter.onCallFilteringComplete(mCall, CALL_SCREENING_SERVICE_BLOCK_RESULT);
        waitForHandlerAction(testFilter.getHandler(), SHORT_TIMEOUT * 2);
        verify(mResultCallback).onCallFilteringComplete(eq(mCall), eq(new Builder()
                .setShouldAllowCall(false)
                .setShouldReject(true)
                .setShouldAddToCallLog(false)
                .setShouldShowNotification(true)
                .setCallBlockReason(Calls.BLOCK_REASON_CALL_SCREENING_SERVICE)
                .setCallScreeningAppName("com.android.thirdparty")
                .setCallScreeningComponentName(
                        "com.android.thirdparty/com.android.thirdparty.callscreeningserviceimpl")
                .build()));
    }

    @SmallTest
    @Test
    public void testFilterTimeout() throws Exception {
        setTimeoutLength(SHORT_TIMEOUT);
        IncomingCallFilter testFilter = new IncomingCallFilter(mContext, mResultCallback, mCall,
                mLock, mTimeoutsAdapter, Collections.singletonList(mFilter1), mHandler);
        testFilter.performFiltering();
        verify(mResultCallback, timeout((int) SHORT_TIMEOUT * 2)).onCallFilteringComplete(eq(mCall),
                eq(DEFAULT_RESULT));
        testFilter.onCallFilteringComplete(mCall, PASS_CALL_RESULT);
        waitForHandlerAction(testFilter.getHandler(), SHORT_TIMEOUT * 2);
        // verify that we don't report back again with the result
        verify(mResultCallback, atMost(1)).onCallFilteringComplete(any(Call.class),
                any(CallFilteringResult.class));
    }

    @SmallTest
    @Test
    public void testFilterTimeoutDoesntTrip() throws Exception {
        setTimeoutLength(SHORT_TIMEOUT);
        IncomingCallFilter testFilter = new IncomingCallFilter(mContext, mResultCallback, mCall,
                mLock, mTimeoutsAdapter, Collections.singletonList(mFilter1), mHandler);
        testFilter.performFiltering();
        testFilter.onCallFilteringComplete(mCall, PASS_CALL_RESULT);
        waitForHandlerAction(testFilter.getHandler(), SHORT_TIMEOUT * 2);
        Thread.sleep(SHORT_TIMEOUT);
        verify(mResultCallback, atMost(1)).onCallFilteringComplete(any(Call.class),
                any(CallFilteringResult.class));
    }

    @SmallTest
    @Test
    public void testToString() {
        assertEquals("[Allow, logged, notified]", PASS_CALL_RESULT.toString());
        assertEquals("[Reject, notified, mCallBlockReason = 1, mCallScreeningAppName = com" +
                ".android.thirdparty, mCallScreeningComponentName = com.android.thirdparty/com" +
                ".android.thirdparty.callscreeningserviceimpl]",
            CALL_SCREENING_SERVICE_BLOCK_RESULT.toString());
        assertEquals("[Reject, logged, mCallBlockReason = 3]",
            ASYNC_BLOCK_CHECK_BLOCK_RESULT.toString());
    }

    private void setTimeoutLength(long length) throws Exception {
        when(mTimeoutsAdapter.getCallScreeningTimeoutMillis(any(ContentResolver.class)))
                .thenReturn(length);
    }
}
