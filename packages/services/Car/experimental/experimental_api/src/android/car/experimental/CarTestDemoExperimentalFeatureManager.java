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

package android.car.experimental;

import android.car.Car;
import android.car.CarManagerBase;
import android.car.annotation.RequiredFeature;
import android.os.IBinder;
import android.os.RemoteException;

/**
 * Demo CarManager API for demonstrating experimental feature / API usage. This will never go to
 * production.
 *
 * @hide
 */
@RequiredFeature(ExperimentalCar.TEST_EXPERIMENTAL_FEATURE_SERVICE)
public final class CarTestDemoExperimentalFeatureManager extends CarManagerBase {

    private final ITestDemoExperimental mService;

    /**
     * Constructor parameters should remain this way for Car library to construct this.
     */
    public CarTestDemoExperimentalFeatureManager(Car car, IBinder service) {
        super(car);
        mService = ITestDemoExperimental.Stub.asInterface(service);
    }

    /**
     * Send ping msg service. It will replay back with the same message.
     */
    public String ping(String msg) {
        try {
            return mService.ping(msg);
        } catch (RemoteException e) {
            // For experimental API, we just crash client.
            throw e.rethrowFromSystemServer();
        }
    }

    protected void onCarDisconnected() {
        // Nothing to do
    }
}
