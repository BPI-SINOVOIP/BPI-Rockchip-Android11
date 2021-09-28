/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.car.dialer.livedata;

import android.telecom.Call;
import android.telecom.InCallService;

import androidx.annotation.Nullable;
import androidx.lifecycle.LiveData;

import com.android.car.telephony.common.CallDetail;

import java.util.List;

/**
 * Represents the details of an active phone call.
 */
public class CallDetailLiveData extends LiveData<CallDetail> {

    private Call mTelecomCall;

    private Call.Callback mCallback = new Call.Callback() {
        @Override
        public void onStateChanged(Call telecomCall, int state) {
            // no ops
        }

        @Override
        public void onParentChanged(Call telecomCall, Call parent) {
            // no ops
        }

        @Override
        public void onCallDestroyed(Call telecomCall) {
            // no ops
        }

        @Override
        public void onDetailsChanged(Call telecomCall, Call.Details details) {
            setTelecomCallDetail(mTelecomCall);
        }

        @Override
        public void onVideoCallChanged(Call telecomCall, InCallService.VideoCall videoCall) {
            // no ops
        }

        @Override
        public void onCannedTextResponsesLoaded(Call telecomCall,
                List<String> cannedTextResponses) {
            // no ops
        }

        @Override
        public void onChildrenChanged(Call telecomCall, List<Call> children) {
            // no ops
        }
    };

    /**
     * Sets the {@link Call} of which this live data sources.
     */
    public void setTelecomCall(Call telecomCall) {
        if (mTelecomCall != null) {
            mTelecomCall.unregisterCallback(mCallback);
        }
        mTelecomCall = telecomCall;
        setTelecomCallDetail(mTelecomCall);
        if (mTelecomCall != null) {
            mTelecomCall.registerCallback(mCallback);
        }
    }

    private void setTelecomCallDetail(@Nullable Call telecomCall) {
        setValue(telecomCall != null ? CallDetail.fromTelecomCallDetail(telecomCall.getDetails())
                : null);
    }
}
