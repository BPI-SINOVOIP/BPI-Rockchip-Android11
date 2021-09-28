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

package com.android.experimentalcar;

import android.car.experimental.DriverAwarenessEvent;
import android.car.experimental.DriverAwarenessSupplierService;
import android.content.Intent;
import android.os.IBinder;
import android.os.SystemClock;
import android.util.Log;

/**
 * Simple example of how an external driver awareness supplier service could be implemented.
 */
public class SampleExternalDriverAwarenessSupplier extends DriverAwarenessSupplierService {

    private static final String TAG = "SampleExternalDriverAwarenessSupplier";
    private static final float INITIAL_DRIVER_AWARENESS_VALUE = 1.0f;
    private static final long MAX_STALENESS_MILLIS = 100L;

    @Override
    public long getMaxStalenessMillis() {
        return MAX_STALENESS_MILLIS;
    }

    @Override
    public void onReady() {
        // send an initial event, as required by the IDriverAwarenessSupplierCallback spec
        onDriverAwarenessUpdated(new DriverAwarenessEvent(SystemClock.elapsedRealtime(),
                INITIAL_DRIVER_AWARENESS_VALUE));
    }

    @Override
    public IBinder onBind(Intent intent) {
        logd("onBind, intent: " + intent);
        return super.onBind(intent);
    }

    private static void logd(String message) {
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, message);
        }
    }
}
