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

import android.app.Service;
import android.car.Car;
import android.content.Intent;
import android.os.IBinder;

import java.io.FileDescriptor;
import java.io.PrintWriter;

/**
 * Top class to keep all experimental features.
 */
public class ExperimentalCarService extends Service {

    private Car mCar;
    private final IExperimentalCarImpl mIExperimentalCarImpl = new IExperimentalCarImpl(this);

    @Override
    public void onCreate() {
        super.onCreate();
        // This is for crashing this service when car service crashes.
        mCar = Car.createCar(this);
    }

    @Override
    public void onDestroy() {
        mIExperimentalCarImpl.release();
        if (mCar != null && mCar.isConnected()) {
            mCar.disconnect();
            mCar = null;
        }
        super.onDestroy();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // keep it alive.
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mIExperimentalCarImpl;
    }

    @Override
    protected void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
        mIExperimentalCarImpl.dump(fd, writer, args);
    }
}
