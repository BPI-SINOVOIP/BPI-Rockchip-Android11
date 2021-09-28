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

import android.content.ContentResolver;
import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import android.test.suitebuilder.annotation.SmallTest;

import com.android.server.telecom.Call;
import com.android.server.telecom.TelecomSystem;
import com.android.server.telecom.Timeouts;
import com.android.server.telecom.callfiltering.CallFilter;
import com.android.server.telecom.callfiltering.CallFilterResultCallback;
import com.android.server.telecom.callfiltering.CallFilteringResult;
import com.android.server.telecom.callfiltering.IncomingCallFilterGraph;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.nullable;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.junit.Test;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CompletionStage;
import java.util.concurrent.TimeUnit;

@RunWith(JUnit4.class)
public class IncomingCallFilterGraphTest extends TelecomTestCase {
    @Mock private Call mCall;
    @Mock private Context mContext;
    @Mock private Timeouts.Adapter mTimeoutsAdapter;
    private TelecomSystem.SyncRoot mLock = new TelecomSystem.SyncRoot() {};

    private static final CallFilteringResult PASS_CALL_RESULT = new CallFilteringResult.Builder()
            .setShouldAllowCall(true)
            .setShouldReject(false)
            .setShouldSilence(false)
            .setShouldAddToCallLog(true)
            .setShouldShowNotification(true).build();
    private static final CallFilteringResult REJECT_CALL_RESULT = new CallFilteringResult.Builder()
            .setShouldAllowCall(false)
            .setShouldReject(true)
            .setShouldSilence(false)
            .setShouldAddToCallLog(true)
            .setShouldShowNotification(true).build();
    private final long FILTER_TIMEOUT = 5000;
    private final long TEST_TIMEOUT = 7000;
    private final long TIMEOUT_FILTER_SLEEP_TIME = 10000;

    private class AllowFilter extends CallFilter {
        @Override
        public CompletionStage<CallFilteringResult> startFilterLookup(
                CallFilteringResult priorStageResult) {
            return CompletableFuture.completedFuture(PASS_CALL_RESULT);
        }
    }

    private class DisallowFilter extends CallFilter {
        @Override
        public CompletionStage<CallFilteringResult> startFilterLookup(
                CallFilteringResult priorStageResult) {
            return CompletableFuture.completedFuture(REJECT_CALL_RESULT);
        }
    }

    private class TimeoutFilter extends CallFilter {
        @Override
        public CompletionStage<CallFilteringResult> startFilterLookup(
                CallFilteringResult priorStageResult) {
            HandlerThread handlerThread = new HandlerThread("TimeoutFilter");
            handlerThread.start();
            Handler handler = new Handler(handlerThread.getLooper());

            CompletableFuture<CallFilteringResult> resultFuture = new CompletableFuture<>();
            handler.postDelayed(() -> resultFuture.complete(PASS_CALL_RESULT),
                    TIMEOUT_FILTER_SLEEP_TIME);
            return CompletableFuture.completedFuture(PASS_CALL_RESULT);
        }
    }

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();
        when(mContext.getContentResolver()).thenReturn(null);
        when(mTimeoutsAdapter.getCallScreeningTimeoutMillis(nullable(ContentResolver.class)))
                .thenReturn(FILTER_TIMEOUT);

    }

    @SmallTest
    @Test
    public void testEmptyGraph() throws Exception {
        CompletableFuture<CallFilteringResult> testResult = new CompletableFuture<>();
        CallFilterResultCallback listener = (call, result) -> testResult.complete(result);

        IncomingCallFilterGraph graph = new IncomingCallFilterGraph(mCall, listener, mContext,
                mTimeoutsAdapter, mLock);
        graph.performFiltering();

        assertEquals(PASS_CALL_RESULT, testResult.get(TEST_TIMEOUT, TimeUnit.MILLISECONDS));
    }

    @SmallTest
    @Test
    public void testFiltersPerformOrder() throws Exception {
        CompletableFuture<CallFilteringResult> testResult = new CompletableFuture<>();
        CallFilterResultCallback listener = (call, result) -> testResult.complete(result);

        IncomingCallFilterGraph graph = new IncomingCallFilterGraph(mCall, listener, mContext,
                mTimeoutsAdapter, mLock);
        AllowFilter allowFilter = new AllowFilter();
        DisallowFilter disallowFilter = new DisallowFilter();
        graph.addFilter(allowFilter);
        graph.addFilter(disallowFilter);
        IncomingCallFilterGraph.addEdge(allowFilter, disallowFilter);
        graph.performFiltering();

        assertEquals(REJECT_CALL_RESULT, testResult.get(TEST_TIMEOUT, TimeUnit.MILLISECONDS));
    }

    @SmallTest
    @Test
    public void testFiltersPerformInParallel() throws Exception {
        CompletableFuture<CallFilteringResult> testResult = new CompletableFuture<>();
        CallFilterResultCallback listener = (call, result) -> testResult.complete(result);

        IncomingCallFilterGraph graph = new IncomingCallFilterGraph(mCall, listener, mContext,
                mTimeoutsAdapter, mLock);
        AllowFilter allowFilter1 = new AllowFilter();
        AllowFilter allowFilter2 = new AllowFilter();
        DisallowFilter disallowFilter = new DisallowFilter();
        graph.addFilter(allowFilter1);
        graph.addFilter(allowFilter2);
        graph.addFilter(disallowFilter);
        graph.performFiltering();

        assertEquals(REJECT_CALL_RESULT, testResult.get(TEST_TIMEOUT, TimeUnit.MILLISECONDS));
    }

    @SmallTest
    @Test
    public void testFiltersTimeout() throws Exception {
        CompletableFuture<CallFilteringResult> testResult = new CompletableFuture<>();
        CallFilterResultCallback listener = (call, result) -> testResult.complete(result);

        IncomingCallFilterGraph graph = new IncomingCallFilterGraph(mCall, listener, mContext,
                mTimeoutsAdapter, mLock);
        DisallowFilter disallowFilter = new DisallowFilter();
        TimeoutFilter timeoutFilter = new TimeoutFilter();
        graph.addFilter(disallowFilter);
        graph.addFilter(timeoutFilter);
        IncomingCallFilterGraph.addEdge(disallowFilter, timeoutFilter);
        graph.performFiltering();

        assertEquals(REJECT_CALL_RESULT, testResult.get(TEST_TIMEOUT, TimeUnit.MILLISECONDS));
    }
}
