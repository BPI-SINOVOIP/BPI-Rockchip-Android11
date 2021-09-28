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

import android.net.Uri;
import android.provider.CallLog;
import android.telecom.CallerInfo;
import android.telecom.Log;

import com.android.server.telecom.Call;
import com.android.server.telecom.CallerInfoLookupHelper;
import com.android.server.telecom.LogUtils;

import java.util.Objects;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CompletionStage;

public class DirectToVoicemailFilter extends CallFilter {
    private final Call mCall;
    private final CallerInfoLookupHelper mCallerInfoLookupHelper;

    public DirectToVoicemailFilter(Call call, CallerInfoLookupHelper callerInfoLookupHelper) {
        mCall = call;
        mCallerInfoLookupHelper = callerInfoLookupHelper;
    }

    @Override
    public CompletionStage<CallFilteringResult> startFilterLookup(CallFilteringResult result) {
        Log.addEvent(mCall, LogUtils.Events.DIRECT_TO_VM_INITIATED);
        CompletableFuture<CallFilteringResult> resultFuture = new CompletableFuture<>();
        mCallerInfoLookupHelper.startLookup(mCall.getHandle(),
                new CallerInfoLookupHelper.OnQueryCompleteListener() {
                    @Override
                    public void onCallerInfoQueryComplete(Uri handle, CallerInfo info) {
                        if ((handle != null) && Objects.equals(mCall.getHandle(), handle)
                                && (info != null)) {
                            resultFuture.complete(new CallFilteringResult.Builder()
                                    .setShouldAllowCall(!info.shouldSendToVoicemail)
                                    .setShouldReject(info.shouldSendToVoicemail)
                                    .setShouldAddToCallLog(true)
                                    .setShouldShowNotification(true)
                                    .setCallBlockReason(info.shouldSendToVoicemail ?
                                            CallLog.Calls.BLOCK_REASON_DIRECT_TO_VOICEMAIL
                                            : CallLog.Calls.BLOCK_REASON_NOT_BLOCKED)
                                    .setContactExists(info.contactExists)
                                    .build());
                        } else {
                            if (info == null) {
                                Log.w(this, "CallerInfo lookup returned a null " +
                                        "CallerInfo.");
                            } else {
                                Log.w(this, "CallerInfo lookup returned with "
                                                + "handle %s, should be %s", handle,
                                        mCall.getHandle());
                            }
                            resultFuture.complete(IncomingCallFilterGraph.DEFAULT_RESULT);
                        }
                        Log.addEvent(mCall, LogUtils.Events.DIRECT_TO_VM_FINISHED);
                    }

                    @Override
                    public void onContactPhotoQueryComplete(Uri handle, CallerInfo info) {
                        // Ignore
                    }
                });
        return resultFuture;
    }
}
