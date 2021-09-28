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

import static android.provider.BlockedNumberContract.STATUS_BLOCKED_IN_LIST;
import static android.provider.BlockedNumberContract.STATUS_NOT_BLOCKED;

import static junit.framework.TestCase.assertEquals;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.os.PersistableBundle;
import android.provider.CallLog;
import android.telecom.CallerInfo;
import android.telecom.TelecomManager;
import android.telephony.CarrierConfigManager;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Pair;

import com.android.server.telecom.Call;
import com.android.server.telecom.CallerInfoLookupHelper;
import com.android.server.telecom.callfiltering.BlockCheckerAdapter;
import com.android.server.telecom.callfiltering.BlockCheckerFilter;
import com.android.server.telecom.callfiltering.CallFilteringResult;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CompletionStage;
import java.util.concurrent.TimeUnit;

@RunWith(JUnit4.class)
public class BlockCheckerFilterTest extends TelecomTestCase {
    @Mock Call mCall;
    @Mock private BlockCheckerAdapter mBlockCheckerAdapter;
    @Mock private CallerInfoLookupHelper mCallerInfoLookupHelper;
    @Mock private CarrierConfigManager mCarrierConfigManager;

    private BlockCheckerFilter mFilter;
    private static final CallFilteringResult PASS_RESULT = new CallFilteringResult.Builder()
            .setShouldAllowCall(true)
            .setShouldReject(false)
            .setShouldAddToCallLog(true)
            .setShouldShowNotification(true)
            .build();
    private static final CallFilteringResult BLOCK_RESULT = new CallFilteringResult.Builder()
            .setShouldAllowCall(false)
            .setShouldReject(true)
            .setShouldAddToCallLog(true)
            .setShouldShowNotification(false)
            .setCallBlockReason(CallLog.Calls.BLOCK_REASON_BLOCKED_NUMBER)
            .setCallScreeningAppName(null)
            .setCallScreeningComponentName(null)
            .build();

    private static final Uri TEST_HANDLE = Uri.parse("tel:1235551234");

    @Override
    @Before
    public void setUp() throws Exception {
        super.setUp();
        when(mCall.getHandle()).thenReturn(TEST_HANDLE);
        mFilter = new BlockCheckerFilter(mContext, mCall, mCallerInfoLookupHelper,
                mBlockCheckerAdapter);
    }

    @SmallTest
    @Test
    public void testBlockNumber() throws Exception {
        when(mBlockCheckerAdapter.getBlockStatus(any(Context.class),
                eq(TEST_HANDLE.getSchemeSpecificPart()), any(Bundle.class)))
                .thenReturn(STATUS_BLOCKED_IN_LIST);

        setEnhancedBlockingEnabled(false);
        assertEquals(BLOCK_RESULT, mFilter.startFilterLookup(PASS_RESULT).toCompletableFuture()
                .get(BlockCheckerFilter.CALLER_INFO_QUERY_TIMEOUT, TimeUnit.MILLISECONDS));
    }

    @SmallTest
    @Test
    public void testBlockNumberWhenEnhancedBlockingEnabled() throws Exception {
        when(mBlockCheckerAdapter.getBlockStatus(any(Context.class),
                eq(TEST_HANDLE.getSchemeSpecificPart()), any(Bundle.class)))
                .thenReturn(STATUS_BLOCKED_IN_LIST);

        setEnhancedBlockingEnabled(true);
        CompletionStage<CallFilteringResult> resultFuture = mFilter.startFilterLookup(PASS_RESULT);
        verifyEnhancedLookupStart();
        assertEquals(BLOCK_RESULT, resultFuture.toCompletableFuture()
                .get(BlockCheckerFilter.CALLER_INFO_QUERY_TIMEOUT, TimeUnit.MILLISECONDS));
    }

    @SmallTest
    @Test
    public void testDontBlockNumber() throws Exception {
        when(mBlockCheckerAdapter.getBlockStatus(any(Context.class),
                eq(TEST_HANDLE.getSchemeSpecificPart()), any(Bundle.class)))
                .thenReturn(STATUS_NOT_BLOCKED);

        setEnhancedBlockingEnabled(false);
        assertEquals(PASS_RESULT, mFilter.startFilterLookup(PASS_RESULT).toCompletableFuture()
                .get(BlockCheckerFilter.CALLER_INFO_QUERY_TIMEOUT, TimeUnit.MILLISECONDS));
    }

    @SmallTest
    @Test
    public void testDontBlockNumberWhenEnhancedBlockingEnabled() throws Exception {
        when(mBlockCheckerAdapter.getBlockStatus(any(Context.class),
                eq(TEST_HANDLE.getSchemeSpecificPart()), any(Bundle.class)))
                .thenReturn(STATUS_NOT_BLOCKED);

        setEnhancedBlockingEnabled(true);
        CompletionStage<CallFilteringResult> resultFuture = mFilter.startFilterLookup(PASS_RESULT);
        verifyEnhancedLookupStart();
        assertEquals(PASS_RESULT, resultFuture.toCompletableFuture()
                .get(BlockCheckerFilter.CALLER_INFO_QUERY_TIMEOUT, TimeUnit.MILLISECONDS));
    }

    private void setEnhancedBlockingEnabled(boolean value) {
        PersistableBundle bundle = new PersistableBundle();
        bundle.putBoolean(CarrierConfigManager.KEY_SUPPORT_ENHANCED_CALL_BLOCKING_BOOL, value);
        when(mContext.getSystemService(Context.CARRIER_CONFIG_SERVICE))
                .thenReturn(mCarrierConfigManager);
        when(mCarrierConfigManager.getConfig()).thenReturn(bundle);
        if (value) {
            when(mCall.getHandlePresentation()).thenReturn(TelecomManager.PRESENTATION_ALLOWED);
        }
    }

    private void verifyEnhancedLookupStart() {
        ArgumentCaptor<CallerInfoLookupHelper.OnQueryCompleteListener> captor =
                ArgumentCaptor.forClass(CallerInfoLookupHelper.OnQueryCompleteListener.class);
        verify(mCallerInfoLookupHelper, timeout(BlockCheckerFilter.CALLER_INFO_QUERY_TIMEOUT))
                .startLookup(eq(TEST_HANDLE), captor.capture());
        captor.getValue().onCallerInfoQueryComplete(TEST_HANDLE, null);
    }
}
