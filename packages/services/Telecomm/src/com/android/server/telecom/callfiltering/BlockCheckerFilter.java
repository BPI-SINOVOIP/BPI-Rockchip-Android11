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

package com.android.server.telecom.callfiltering;

import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.provider.BlockedNumberContract;
import android.provider.CallLog;
import android.telecom.CallerInfo;
import android.telecom.Log;
import android.telecom.TelecomManager;

import com.android.server.telecom.Call;
import com.android.server.telecom.CallerInfoLookupHelper;
import com.android.server.telecom.LogUtils;
import com.android.server.telecom.LoggedHandlerExecutor;
import com.android.server.telecom.settings.BlockedNumbersUtil;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CompletionStage;

public class BlockCheckerFilter extends CallFilter {
    private final Call mCall;
    private final Context mContext;
    private final CallerInfoLookupHelper mCallerInfoLookupHelper;
    private final BlockCheckerAdapter mBlockCheckerAdapter;
    private final String TAG = "BlockCheckerFilter";
    private boolean mContactExists;
    private HandlerThread mHandlerThread;
    private Handler mHandler;

    public static final long CALLER_INFO_QUERY_TIMEOUT = 5000;

    public BlockCheckerFilter(Context context, Call call,
            CallerInfoLookupHelper callerInfoLookupHelper,
            BlockCheckerAdapter blockCheckerAdapter) {
        mCall = call;
        mContext = context;
        mCallerInfoLookupHelper = callerInfoLookupHelper;
        mBlockCheckerAdapter = blockCheckerAdapter;
        mContactExists = false;
        mHandlerThread = new HandlerThread(TAG);
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
    }

    @Override
    public CompletionStage<CallFilteringResult> startFilterLookup(CallFilteringResult result) {
        Log.addEvent(mCall, LogUtils.Events.BLOCK_CHECK_INITIATED);
        CompletableFuture<CallFilteringResult> resultFuture = new CompletableFuture<>();
        Bundle extras = new Bundle();
        if (BlockedNumbersUtil.isEnhancedCallBlockingEnabledByPlatform(mContext)) {
            int presentation = mCall.getHandlePresentation();
            extras.putInt(BlockedNumberContract.EXTRA_CALL_PRESENTATION, presentation);
            if (presentation == TelecomManager.PRESENTATION_ALLOWED) {
                mCallerInfoLookupHelper.startLookup(mCall.getHandle(),
                        new CallerInfoLookupHelper.OnQueryCompleteListener() {
                            @Override
                            public void onCallerInfoQueryComplete(Uri handle, CallerInfo info) {
                                if (info != null && info.contactExists) {
                                    mContactExists = true;
                                }
                                getBlockStatus(resultFuture);
                            }

                            @Override
                            public void onContactPhotoQueryComplete(Uri handle, CallerInfo info) {
                                // Ignore
                            }
                        });
            } else {
                getBlockStatus(resultFuture);
            }
        } else {
            getBlockStatus(resultFuture);
        }
        return resultFuture;
    }

    private void getBlockStatus(
            CompletableFuture<CallFilteringResult> resultFuture) {
        // Set extras
        Bundle extras = new Bundle();
        if (BlockedNumbersUtil.isEnhancedCallBlockingEnabledByPlatform(mContext)) {
            int presentation = mCall.getHandlePresentation();
            extras.putInt(BlockedNumberContract.EXTRA_CALL_PRESENTATION, presentation);
            if (presentation == TelecomManager.PRESENTATION_ALLOWED) {
                extras.putBoolean(BlockedNumberContract.EXTRA_CONTACT_EXIST, mContactExists);
            }
        }

        // Set number
        final String number = mCall.getHandle() == null ? null :
                mCall.getHandle().getSchemeSpecificPart();

        CompletableFuture.supplyAsync(
                () -> mBlockCheckerAdapter.getBlockStatus(mContext, number, extras),
                new LoggedHandlerExecutor(mHandler, "BCF.gBS", null))
                .thenApplyAsync((x) -> completeResult(resultFuture, x),
                        new LoggedHandlerExecutor(mHandler, "BCF.gBS", null));
    }

    private int completeResult(CompletableFuture<CallFilteringResult> resultFuture,
            int blockStatus) {
        CallFilteringResult result;
        if (blockStatus != BlockedNumberContract.STATUS_NOT_BLOCKED) {
            result = new CallFilteringResult.Builder()
                    .setShouldAllowCall(false)
                    .setShouldReject(true)
                    .setShouldAddToCallLog(true)
                    .setShouldShowNotification(false)
                    .setCallBlockReason(getBlockReason(blockStatus))
                    .setCallScreeningAppName(null)
                    .setCallScreeningComponentName(null)
                    .setContactExists(mContactExists)
                    .build();
        } else {
            result = new CallFilteringResult.Builder()
                    .setShouldAllowCall(true)
                    .setShouldReject(false)
                    .setShouldSilence(false)
                    .setShouldAddToCallLog(true)
                    .setShouldShowNotification(true)
                    .setContactExists(mContactExists)
                    .build();
        }
        Log.addEvent(mCall, LogUtils.Events.BLOCK_CHECK_FINISHED,
                BlockedNumberContract.SystemContract.blockStatusToString(blockStatus) + " "
                        + result);
        resultFuture.complete(result);
        mHandlerThread.quitSafely();
        return blockStatus;
    }

    private int getBlockReason(int blockStatus) {
        switch (blockStatus) {
            case BlockedNumberContract.STATUS_BLOCKED_IN_LIST:
                return CallLog.Calls.BLOCK_REASON_BLOCKED_NUMBER;

            case BlockedNumberContract.STATUS_BLOCKED_UNKNOWN_NUMBER:
                return CallLog.Calls.BLOCK_REASON_UNKNOWN_NUMBER;

            case BlockedNumberContract.STATUS_BLOCKED_RESTRICTED:
                return CallLog.Calls.BLOCK_REASON_RESTRICTED_NUMBER;

            case BlockedNumberContract.STATUS_BLOCKED_PAYPHONE:
                return CallLog.Calls.BLOCK_REASON_PAY_PHONE;

            case BlockedNumberContract.STATUS_BLOCKED_NOT_IN_CONTACTS:
                return CallLog.Calls.BLOCK_REASON_NOT_IN_CONTACTS;

            default:
                Log.w(this,
                        "There's no call log block reason can be converted");
                return CallLog.Calls.BLOCK_REASON_BLOCKED_NUMBER;
        }
    }
}
