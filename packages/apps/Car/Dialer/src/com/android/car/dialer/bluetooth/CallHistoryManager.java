/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.car.dialer.bluetooth;

import android.content.Context;

import androidx.lifecycle.LiveData;
import androidx.lifecycle.Observer;

import com.android.car.arch.common.LiveDataFunctions;
import com.android.car.dialer.livedata.CallHistoryLiveData;
import com.android.car.dialer.log.L;
import com.android.car.telephony.common.PhoneCallLog;

import java.util.List;

/**
 * A class that monitors the call history data change.
 */
public class CallHistoryManager {
    private static final String TAG = "CD.CallHistoryManager";

    private static CallHistoryManager sCallHistoryManager;

    private LiveData<List<PhoneCallLog>> mCallHistoryLiveData;

    private Observer mCallHistoryObserver;

    /**
     * Initializes a globally accessible {@link CallHistoryManager} which can be retrieved by {@link
     * #get}. Must be called after {@link UiBluetoothMonitor} has been initiated.
     *
     * @param applicationContext Application context.
     */
    public static CallHistoryManager init(Context applicationContext) {
        if (sCallHistoryManager == null) {
            sCallHistoryManager = new CallHistoryManager(applicationContext);
        }

        return sCallHistoryManager;
    }

    /**
     * Gets the global {@link CallHistoryManager} instance. Make sure {@link #init(Context)} is
     * called before calling this method.
     */
    public static CallHistoryManager get() {
        if (sCallHistoryManager == null) {
            L.e(TAG, "Call CallHistoryManager.init(Context) before calling this function");
        }
        return sCallHistoryManager;
    }

    private CallHistoryManager(Context applicationContext) {
        mCallHistoryLiveData = LiveDataFunctions.switchMapNonNull(
                UiBluetoothMonitor.get().getFirstHfpConnectedDevice(),
                device -> CallHistoryLiveData.newInstance(applicationContext, device.getAddress()));

        mCallHistoryObserver = o -> L.i(TAG, "Call history is updated");

        // Call history live data is observed forever to avoid race condition between new call
        // log insertion timing and data change registering.
        mCallHistoryLiveData.observeForever(mCallHistoryObserver);
    }

    /**
     * Tears down the {@link CallHistoryManager}. Calling this function will null out the global
     * accessible {@link CallHistoryManager} instance.
     */
    public void tearDown() {
        if (mCallHistoryLiveData != null && mCallHistoryLiveData.hasObservers()) {
            mCallHistoryLiveData.removeObserver(mCallHistoryObserver);
        }

        sCallHistoryManager = null;
    }

    /**
     * Returns a LiveData which monitors the call history data for the first connected device.
     */
    public LiveData<List<PhoneCallLog>> getCallHistoryLiveData() {
        return mCallHistoryLiveData;
    }
}
