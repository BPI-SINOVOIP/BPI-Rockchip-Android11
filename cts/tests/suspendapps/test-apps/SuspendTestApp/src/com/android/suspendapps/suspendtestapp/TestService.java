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

package com.android.suspendapps.suspendtestapp;

import android.app.Service;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.ArrayMap;
import android.util.Log;

import androidx.annotation.GuardedBy;

import java.util.concurrent.atomic.AtomicInteger;

/**
 * Service used by the test to investigate this app's state.
 */
public class TestService extends Service {
    private static final String TAG = TestService.class.getSimpleName();

    private static final Object sLock = new Object();
    @GuardedBy("sLock")
    private static final ArrayMap<String, IBroadcastReporter> sRegisteredReporters =
            new ArrayMap<>();

    private final IBinder mBinder = new ITestService.Stub() {
        @Override
        public boolean isPackageSuspended() throws RemoteException {
            return getPackageManager().isPackageSuspended();
        }

        @Override
        public Bundle getSuspendedAppExtras() throws RemoteException {
            return getPackageManager().getSuspendedPackageAppExtras();
        }

        @Override
        public Intent waitForActivityStart(long timeoutMs) throws RemoteException {
            synchronized (SuspendTestActivity.sLock) {
                if (SuspendTestActivity.sStartedIntent == null) {
                    try {
                        SuspendTestActivity.sLock.wait(timeoutMs);
                    } catch (InterruptedException e) {
                        Log.e(TAG, "Interrupted while waiting for activity start", e);
                    }
                }
                return SuspendTestActivity.sStartedIntent;
            }
        }

        @Override
        public boolean waitForActivityStop(long timeoutMs) throws RemoteException {
            synchronized (SuspendTestActivity.sLock) {
                if (SuspendTestActivity.sStartedIntent != null) {
                    try {
                        SuspendTestActivity.sLock.wait(timeoutMs);
                    } catch (InterruptedException e) {
                        Log.e(TAG, "Interrupted while waiting for activity stop", e);
                    }
                }
                return SuspendTestActivity.sStartedIntent == null;
            }
        }

        /**
         * Registers a {@link com.android.suspendapps.suspendtestapp.IBroadcastReporter} to trigger
         * whenever this app receives a broadcast with the specified Intent action.
         *
         * <p> This will replace any existing registration present for the given {@code action} and
         * the {@code reporter} will get unregistered once it has been called.
         *
         * @param reporter
         * @param action
         * @throws RemoteException
         */
        @Override
        public void registerBroadcastReporter(String action, IBroadcastReporter reporter)
                throws RemoteException {
            synchronized (sLock) {
                sRegisteredReporters.put(action, reporter);
            }
        }

        @Override
        public void unregisterBroadcastReporter(String action)
                throws RemoteException {
            synchronized (sLock) {
                sRegisteredReporters.remove(action);
            }
        }
    };

    static void reportBroadcastIfNeeded(Intent intent) {
        IBroadcastReporter reporter;
        synchronized (sLock) {
            reporter = sRegisteredReporters.remove(intent.getAction());
        }
        if (reporter != null) {
            try {
                reporter.onBroadcastReceived(intent);
            } catch (RemoteException re) {
                Log.e(TAG, "Unable to report broadcast " + intent + " to instrumentation", re);
            }
        }
    }

    private static final AtomicInteger sNumBindings = new AtomicInteger(0);

    @Override
    public IBinder onBind(Intent intent) {
        final int numBindings = sNumBindings.incrementAndGet();
        if (numBindings > 1) {
            Log.w(TAG, "Too many bindings: " + numBindings);
        }
        return mBinder;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        sNumBindings.getAndDecrement();
        synchronized (sLock) {
            sRegisteredReporters.clear();
        }
        return super.onUnbind(intent);
    }
}
