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
 * limitations under the License.
 */

package com.android.test.util.esimutility;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.os.Bundle;
import android.telephony.TelephonyManager;
import android.util.Log;

public class ESimUtilityInstrumentation extends Instrumentation {
    private static final String TAG = ESimUtilityInstrumentation.class.getCanonicalName();
    private Bundle mArguments;

    @Override
    public void onCreate(Bundle arguments) {
        super.onCreate(arguments);
        mArguments = arguments;
        start();
    }

    @Override
    public void onStart() {
        super.onStart();
        try {
            Log.d(TAG, "starting instrumentation");
            TelephonyManager telephonyManager = (TelephonyManager) getContext().getSystemService(Context.TELEPHONY_SERVICE);
            updateStart();
            telephonyManager.setSimPowerState(getState());
            Log.d(TAG, "ending instrumentation");
            updateAllGood();
        } catch (Exception e) {
            updateWithException(e);
        }
    }

    private void updateAllGood() {
        Bundle allGood = new Bundle();
        allGood.putString("all_is", "good");
        finish(Activity.RESULT_OK, allGood);
    }

    private void updateWithException(Exception e) {
        Bundle result = new Bundle();
        result.putString("sim_utility_exception", e.getMessage());
        StackTraceElement[] elements = e.getStackTrace();
        StringBuilder builder = new StringBuilder();
        for (StackTraceElement element: elements) {
            builder.append(System.lineSeparator());
            builder.append("        ").append(element.toString());
        }
        result.putString("sim_utility_exception_stack_trace", builder.toString());
        finish(Activity.RESULT_CANCELED, result);
    }

    private void updateStart() {
        Bundle result = new Bundle();
        result.putString("setting_state_to", getRawState());
        sendStatus(Activity.RESULT_OK, result);
    }

    public String getRawState() {
        return mArguments.getString("state", "no-provided");
    }

    public int getState() {
        switch (getRawState()) {
            case "down":
                return TelephonyManager.CARD_POWER_DOWN;
            case "up":
                return TelephonyManager.CARD_POWER_UP;
            case "pass-through":
                return TelephonyManager.CARD_POWER_UP_PASS_THROUGH;
        }

        throw new IllegalArgumentException(
                "Invalid or missing state option. Use as: -e state " + "up/down/pass-through");
    }
}
