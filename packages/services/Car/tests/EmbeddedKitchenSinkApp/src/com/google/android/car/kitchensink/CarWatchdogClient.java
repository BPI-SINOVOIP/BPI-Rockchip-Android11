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

package com.google.android.car.kitchensink;

import android.annotation.NonNull;
import android.car.Car;
import android.car.watchdog.CarWatchdogManager;
import android.car.watchdog.CarWatchdogManager.CarWatchdogClientCallback;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class CarWatchdogClient {
    private static final String TAG = CarWatchdogClient.class.getSimpleName();
    private static final String TIMEOUT_CRITICAL = "critical";
    private static final String TIMEOUT_MODERATE = "moderate";
    private static final String TIMEOUT_NORMAL = "normal";
    private static CarWatchdogClient sCarWatchdogClient;

    private final CarWatchdogManager mCarWatchdogManager;
    private final CarWatchdogClientCallback mClientCallback = new CarWatchdogClientCallback() {
        @Override
        public boolean onCheckHealthStatus(int sessionId, int timeout) {
            if (mClientConfig.verbose) {
                Log.i(TAG, "onCheckHealthStatus: session id =  " + sessionId);
            }
            long currentUptime = SystemClock.uptimeMillis();
            return mClientConfig.notRespondAfterInMs < 0
                    || mClientConfig.notRespondAfterInMs > currentUptime - mClientStartTime;
        }

        @Override
        public void onPrepareProcessTermination() {
            Log.w(TAG, "This process is being terminated by car watchdog");
        }
    };
    private final ExecutorService mCallbackExecutor = Executors.newFixedThreadPool(1);
    private ClientConfig mClientConfig;
    private long mClientStartTime;

    // This method is not intended for multi-threaded calls.
    public static void start(Car car, @NonNull String command) {
        if (sCarWatchdogClient != null) {
            Log.w(TAG, "Car watchdog client already started");
            return;
        }
        ClientConfig config;
        try {
            config = parseCommand(command);
        } catch (IllegalArgumentException e) {
            Log.w(TAG, "Watchdog command error: " + e);
            return;
        }
        sCarWatchdogClient = new CarWatchdogClient(car, config);
        sCarWatchdogClient.registerAndGo();
    }

    private static ClientConfig parseCommand(String command) {
        String[] tokens = command.split("[ ]+");
        int paramCount = tokens.length;
        if (paramCount != 3 && paramCount != 4) {
            throw new IllegalArgumentException("invalid command syntax");
        }
        int timeout;
        int inactiveMainAfterInSec;
        int notRespondAfterInSec;
        switch (tokens[0]) {
            case TIMEOUT_CRITICAL:
                timeout = CarWatchdogManager.TIMEOUT_CRITICAL;
                break;
            case TIMEOUT_MODERATE:
                timeout = CarWatchdogManager.TIMEOUT_MODERATE;
                break;
            case TIMEOUT_NORMAL:
                timeout = CarWatchdogManager.TIMEOUT_NORMAL;
                break;
            default:
                throw new IllegalArgumentException("invalid timeout value");
        }
        try {
            notRespondAfterInSec = Integer.parseInt(tokens[1]);
        } catch (NumberFormatException e) {
            throw new IllegalArgumentException("time for \"not responding after\" is not number");
        }
        try {
            inactiveMainAfterInSec = Integer.parseInt(tokens[2]);
        } catch (NumberFormatException e) {
            throw new IllegalArgumentException("time for \"inactive main after\" is not number");
        }
        boolean verbose = false;
        if (paramCount == 4) {
            switch (tokens[3]) {
                case "true":
                    verbose = true;
                    break;
                case "false":
                    verbose = false;
                    break;
                default:
                    throw new IllegalArgumentException("invalid verbose value: " + tokens[3]);
            }
        }
        Log.i(TAG, "CarWatchdogClient command: timeout = " + tokens[0] + ", notRespondingAfter = "
                + notRespondAfterInSec + ", inactiveMainAfter = " + inactiveMainAfterInSec
                + ", verbose = " + verbose);
        return new ClientConfig(timeout, inactiveMainAfterInSec, notRespondAfterInSec, verbose);
    }

    private CarWatchdogClient(Car car, ClientConfig config) {
        mClientConfig = config;
        mCarWatchdogManager = (CarWatchdogManager) car.getCarManager(Car.CAR_WATCHDOG_SERVICE);
    }

    private void registerAndGo() {
        mClientStartTime = SystemClock.uptimeMillis();
        mCarWatchdogManager.registerClient(mCallbackExecutor, mClientCallback,
                mClientConfig.timeout);
        // Post a runnable which takes long time to finish to the main thread if inactive_main_after
        // is no less than 0
        if (mClientConfig.inactiveMainAfterInMs >= 0) {
            Handler handler = new Handler(Looper.getMainLooper());
            handler.postDelayed(() -> {
                try {
                    if (mClientConfig.verbose) {
                        Log.i(TAG, "Main thread gets inactive");
                    }
                    Thread.sleep(getTimeForInactiveMain(mClientConfig.timeout));
                } catch (InterruptedException e) {
                    // Ignore
                }
            }, mClientConfig.inactiveMainAfterInMs);
        }
    }

    // The waiting time = (timeout * 2) + 50 milliseconds.
    private long getTimeForInactiveMain(int timeout) {
        switch (timeout) {
            case CarWatchdogManager.TIMEOUT_CRITICAL:
                return 6050L;
            case CarWatchdogManager.TIMEOUT_MODERATE:
                return 10050L;
            case CarWatchdogManager.TIMEOUT_NORMAL:
                return 20050L;
            default:
                Log.w(TAG, "Invalid timeout");
                return 20050L;
        }
    }

    private static final class ClientConfig {
        public int timeout;
        public long inactiveMainAfterInMs;
        public long notRespondAfterInMs;
        public boolean verbose;

        ClientConfig(int timeout, int inactiveMainAfterInSec, int notRespondAfterInSec,
                boolean verbose) {
            this.timeout = timeout;
            inactiveMainAfterInMs = inactiveMainAfterInSec * 1000L;
            notRespondAfterInMs = notRespondAfterInSec * 1000L;
            this.verbose = verbose;
        }
    }
}
