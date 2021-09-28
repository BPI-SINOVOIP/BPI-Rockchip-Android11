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

package android.telecom.cts.carmodetestapp;

import android.content.Intent;
import android.telecom.Call;
import android.telecom.InCallService;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;

/**
 * A simple car-mode InCallService implementation.
 */
public class CtsCarModeInCallService extends InCallService {
    private static final String TAG = CtsCarModeInCallService.class.getSimpleName();
    private static boolean sIsServiceBound = false;
    private static boolean sIsServiceUnbound = false;
    private static CtsCarModeInCallService sInstance = null;
    private int mCallCount = 0;
    private List<Call> mCalls = new ArrayList<>();

    @Override
    public android.os.IBinder onBind(Intent intent) {
        sIsServiceBound = true;
        sIsServiceUnbound = false;
        sInstance = this;
        Log.i(TAG, "InCallService on bind");
        return super.onBind(intent);
    }

    @Override
    public boolean onUnbind(Intent intent) {
        sIsServiceBound = false;
        sIsServiceUnbound = true;
        sInstance = null;
        Log.i(TAG, "InCallService on unbind");
        return super.onUnbind(intent);
    }

    @Override
    public void onCallAdded(Call call) {
        Log.i(TAG, "onCallAdded - " + call);
        mCallCount++;
        mCalls.add(call);
    }

    @Override
    public void onCallRemoved(Call call) {
        Log.i(TAG, "onCallRemoved - " + call);
        mCallCount--;
        mCalls.remove(call);
    }

    public static boolean isBound() {
        return sIsServiceBound;
    }

    public static boolean isUnbound() {
        return sIsServiceUnbound;
    }

    public static CtsCarModeInCallService getInstance() {
        return sInstance;
    }

    public static void reset() {
        sIsServiceUnbound = false;
        sIsServiceBound = false;
    }

    public int getCallCount() {
        return mCallCount;
    }

    public void disconnectCalls() {
        for (Call call : mCalls) {
            call.disconnect();
        }
    }
}
