/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.car;

import android.app.Service;
import android.car.ICarBluetoothUserService;
import android.car.ILocationManagerProxy;
import android.car.IPerUserCarService;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

/**
 * {@link CarService} process runs as the System User. When logged in as a different user, some
 * services do not provide an API to register or bind as a User, hence CarService doesn't receive
 * the events from services/processes running as a non-system user.
 *
 * This Service is run as the Current User on every User Switch and components of CarService can
 * use this service to communicate with services/processes running as the current (non-system) user.
 */
public class PerUserCarService extends Service {
    private static final boolean DBG = true;
    private static final String TAG = "PerUserCarService";
    private volatile CarBluetoothUserService mCarBluetoothUserService;
    private volatile LocationManagerProxy mLocationManagerProxy;
    private PerUserCarServiceBinder mPerUserCarServiceBinder;

    @Override
    public IBinder onBind(Intent intent) {
        if (DBG) {
            Log.d(TAG, "onBind()");
        }
        if (mPerUserCarServiceBinder == null) {
            Log.e(TAG, "UserSvcBinder null");
        }
        return mPerUserCarServiceBinder;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (DBG) {
            Log.d(TAG, "onStart()");
        }
        return START_STICKY;
    }

    @Override
    public void onCreate() {
        if (DBG) {
            Log.d(TAG, "onCreate()");
        }
        mPerUserCarServiceBinder = new PerUserCarServiceBinder();
        mCarBluetoothUserService = new CarBluetoothUserService(this);
        mLocationManagerProxy = new LocationManagerProxy(this);
        super.onCreate();
    }

    @Override
    public void onDestroy() {
        if (DBG) {
            Log.d(TAG, "onDestroy()");
        }
        mPerUserCarServiceBinder = null;
    }

    /**
     * Other Services in CarService can create their own Binder interface and receive that interface
     * through this PerUserCarService binder.
     */
    private final class PerUserCarServiceBinder extends IPerUserCarService.Stub {
        @Override
        public ICarBluetoothUserService getBluetoothUserService() {
            return mCarBluetoothUserService;
        }

        @Override
        public ILocationManagerProxy getLocationManagerProxy() {
            return mLocationManagerProxy;
        }
    }
}
