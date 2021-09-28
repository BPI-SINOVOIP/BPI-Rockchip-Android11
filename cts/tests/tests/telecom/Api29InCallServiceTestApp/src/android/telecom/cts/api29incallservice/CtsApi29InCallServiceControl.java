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

package android.telecom.cts.api29incallservice;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.telecom.Call;
import android.util.Log;

public class CtsApi29InCallServiceControl extends Service {

    private static final String TAG = CtsApi29InCallServiceControl.class.getSimpleName();
    public static final String CONTROL_INTERFACE_ACTION =
            "android.telecom.cts.api29incallservice.ACTION_API29_CONTROL";

    private final IBinder mCtsCompanionAppControl = new ICtsApi29InCallServiceControl.Stub() {

        @Override
        public int getCallState(String callId) {
            return CtsApi29InCallService.sCalls.stream()
                    .filter(c -> c.getDetails().getTelecomCallId().equals(callId))
                    .findFirst().map(Call::getState).orElse(-1);
        }

        @Override
        public int getLocalCallCount() {
            return CtsApi29InCallService.getLocalCallCount();
        }

        @Override
        public int getHistoricalCallCount() {
            return CtsApi29InCallService.sHistoricalCallCount;
        }
    };

    @Override
    public IBinder onBind(Intent intent) {
        if (CONTROL_INTERFACE_ACTION.equals(intent.getAction())) {
            Log.d(TAG, "onBind: return control interface.");
            return mCtsCompanionAppControl;
        }
        Log.d(TAG, "onBind: invalid intent.");
        return null;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        CtsApi29InCallService.reset();
        return false;
    }

}
