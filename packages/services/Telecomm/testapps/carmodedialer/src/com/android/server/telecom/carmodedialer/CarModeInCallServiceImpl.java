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
 * limitations under the License
 */

package com.android.server.telecom.carmodedialer;

import android.content.Intent;
import android.telecom.Call;
import android.telecom.CallAudioState;
import android.telecom.InCallService;
import android.telecom.Phone;
import android.util.Log;

import java.lang.Override;
import java.lang.String;

/**
 * Test In-Call service implementation.  Logs incoming events.  Mainly used to test binding to
 * multiple {@link InCallService} implementations.
 */
public class CarModeInCallServiceImpl extends InCallService {
    private static final String TAG = "TestInCallServiceImpl";
    public static com.android.server.telecom.carmodedialer.CarModeInCallServiceImpl sInstance;

    private Phone mPhone;

    private Phone.Listener mPhoneListener = new Phone.Listener() {
        @Override
        public void onCallAdded(Phone phone, Call call) {
            Log.i(TAG, "onCallAdded: " + call.toString());
            CarModeCallList callList = CarModeCallList.getInstance();
            callList.addCall(call);

            if (callList.size() == 1) {
                startInCallUI();
            }
        }

        @Override
        public void onCallRemoved(Phone phone, Call call) {
            Log.i(TAG, "onCallRemoved: "+call.toString());
            CarModeCallList.getInstance().removeCall(call);
        }
    };

    @Override
    public void onPhoneCreated(Phone phone) {
        Log.i(TAG, "onPhoneCreated");
        mPhone = phone;
        mPhone.addListener(mPhoneListener);
        CarModeCallList.getInstance().clearCalls();
    }

    @Override
    public boolean onUnbind(Intent intent) {
        Log.i(TAG, "onPhoneDestroyed");
        mPhone.removeListener(mPhoneListener);
        mPhone = null;
        CarModeCallList.getInstance().clearCalls();
        sInstance = null;
        return super.onUnbind(intent);
    }

    @Override
    public void onCallAudioStateChanged(CallAudioState cas) {
        if (CarModeInCallUI.sInstance != null) {
            CarModeInCallUI.sInstance.updateCallAudioState(cas);
        }
    }

    private void startInCallUI() {
        sInstance = this;
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setFlags(Intent.FLAG_ACTIVITY_NO_USER_ACTION | Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setClass(this, CarModeInCallUI.class);
        startActivity(intent);
    }
}
