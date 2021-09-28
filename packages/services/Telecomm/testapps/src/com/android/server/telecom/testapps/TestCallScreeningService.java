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
 * limitations under the License
 */

package com.android.server.telecom.testapps;

import android.os.SystemProperties;
import android.telecom.Call;
import android.telecom.CallScreeningService;
import android.telecom.Log;

/**
 * To use this while testing, use:
 * adb shell setprop com.android.server.telecom.testapps.callscreeningresult n,
 * where n is one of the codes defined below.
 */
public class TestCallScreeningService extends CallScreeningService {
    private Call.Details mDetails;
    private static TestCallScreeningService sTestCallScreeningService;

    public static TestCallScreeningService getInstance() {
        return sTestCallScreeningService;
    }

    private static final int ALLOW_CALL = 0;
    private static final int BLOCK_CALL = 1;
    private static final int SCREEN_CALL_FURTHER = 2;

    private static final String SCREENING_RESULT_KEY =
            TestCallScreeningService.class.getPackage().getName() + ".callscreeningresult";

    /**
     * Handles request from the system to screen an incoming call.
     * @param callDetails Information about a new incoming call, see {@link Call.Details}.
     */
    @Override
    public void onScreenCall(Call.Details callDetails) {
        Log.i(this, "onScreenCall: received call %s", callDetails);
        sTestCallScreeningService = this;

        mDetails = callDetails;

        if (callDetails.getCallDirection() == Call.Details.DIRECTION_INCOMING) {
            Log.i(this, "%s = %d", SCREENING_RESULT_KEY,
                    SystemProperties.getInt(SCREENING_RESULT_KEY, 0));
            switch (SystemProperties.getInt(SCREENING_RESULT_KEY, 0)) {
                case ALLOW_CALL:
                    allowCall();
                    break;
                case BLOCK_CALL:
                    blockCall();
                    break;
                case SCREEN_CALL_FURTHER:
                    screenCallFurther();
                    break;
            }
        }
    }

    public void blockCall() {
        CallScreeningService.CallResponse
                response = new CallScreeningService.CallResponse.Builder()
                .setDisallowCall(true)
                .setRejectCall(true)
                .setSkipCallLog(false)
                .setSkipNotification(true)
                .build();
        respondToCall(mDetails, response);
    }

    public void allowCall() {
        CallScreeningService.CallResponse
                response = new CallScreeningService.CallResponse.Builder()
                .setDisallowCall(false)
                .setRejectCall(false)
                .setSkipCallLog(false)
                .setSkipNotification(false)
                .build();
        respondToCall(mDetails, response);
    }

    void screenCallFurther() {
        CallScreeningService.CallResponse
                response = new CallScreeningService.CallResponse.Builder()
                .setDisallowCall(false)
                .setRejectCall(false)
                .setSkipCallLog(false)
                .setSkipNotification(false)
                .setShouldScreenCallViaAudioProcessing(true)
                .build();
        respondToCall(mDetails, response);
    }
}
