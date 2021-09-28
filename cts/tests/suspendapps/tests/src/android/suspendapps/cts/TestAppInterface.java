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

package android.suspendapps.cts;

import static android.suspendapps.cts.Constants.TEST_APP_PACKAGE_NAME;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

import com.android.suspendapps.suspendtestapp.IBroadcastReporter;
import com.android.suspendapps.suspendtestapp.ITestService;
import com.android.suspendapps.suspendtestapp.TestService;

import java.util.Objects;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Interface that communicates with the {@linkplain Constants#TEST_APP_PACKAGE_NAME test app}
 * using a service binding.
 */
public class TestAppInterface {
    private static final String TAG = TestAppInterface.class.getSimpleName();
    private static final long DEFAULT_REMOTE_WAIT_MS = 30_000;
    private Context mContext;
    private volatile ITestService mRemote;
    private volatile CountDownLatch mConnectionLatch;
    private BroadcastLatch mBroadcastLatch;

    private final ServiceConnection mRemoteConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            mRemote = ITestService.Stub.asInterface(service);
            mConnectionLatch.countDown();
            if (mRemote == null) {
                Log.e(TAG, "Test app returned null binder");
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            Log.e(TAG, "Disconnected from test app service");
            mRemote = null;
        }

        @Override
        public void onBindingDied(ComponentName name) {
            Log.e(TAG, "Binding to " + name + " died unexpectedly");
        }
    };

    TestAppInterface(Context context) {
        mContext = context;
        mConnectionLatch = new CountDownLatch(1);
        final Intent serviceIntent = new Intent()
                .setClassName(TEST_APP_PACKAGE_NAME, TestService.class.getName());
        context.bindService(serviceIntent, mRemoteConnection, Context.BIND_AUTO_CREATE);
        try {
            mConnectionLatch.await(5, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Log.e(TAG, "Interrupted while waiting for service connection", e);
        }
        assertNotNull("Could not connect to remote service", mRemote);
        mBroadcastLatch = new BroadcastLatch(mRemote);
    }

    void disconnect() {
        try {
            mBroadcastLatch.unregister();
        } catch (RemoteException e) {
            Log.w(TAG, "Cannot talk to remote service before unbinding", e);
        }
        mContext.unbindService(mRemoteConnection);
    }

    boolean isTestAppSuspended() throws RemoteException {
        return mRemote.isPackageSuspended();
    }

    Bundle getTestAppSuspendedExtras() throws RemoteException {
        return mRemote.getSuspendedAppExtras();
    }

    boolean awaitTestActivityStop() throws RemoteException {
        return mRemote.waitForActivityStop(DEFAULT_REMOTE_WAIT_MS);
    }

    Intent awaitTestActivityStart() throws RemoteException {
        return mRemote.waitForActivityStart(DEFAULT_REMOTE_WAIT_MS);
    }

    Intent awaitTestActivityStart(long timeoutMs) throws RemoteException {
        return mRemote.waitForActivityStart(timeoutMs);
    }

    void startListeningForBroadcast(String action) throws RemoteException {
        mBroadcastLatch.register(action);
    }

    Intent awaitBroadcast(long timeoutMs) {
        return mBroadcastLatch.awaitIntent(timeoutMs);
    }

    Intent awaitBroadcast() {
        return mBroadcastLatch.awaitIntent(DEFAULT_REMOTE_WAIT_MS);
    }

    private static class BroadcastLatch {
        private String mAction;
        private Intent mReportedIntent;
        private ITestService mRemoteService;

        BroadcastLatch(ITestService remoteService) {
            mRemoteService = Objects.requireNonNull(remoteService);
        }

        synchronized void register(String action) throws RemoteException {
            mAction = Objects.requireNonNull(action);
            mReportedIntent = null;
            mRemoteService.registerBroadcastReporter(mAction, new IBroadcastReporter.Stub() {
                @Override
                public void onBroadcastReceived(Intent intent) throws RemoteException {
                    synchronized (BroadcastLatch.this) {
                        if (mAction != null && mAction.equals(intent.getAction())) {
                            mReportedIntent = intent;
                            BroadcastLatch.this.notifyAll();
                        }
                    }
                }
            });
        }

        synchronized void unregister() throws RemoteException {
            if (mAction != null) {
                mRemoteService.unregisterBroadcastReporter(mAction);
                mAction = null;
                mReportedIntent = null;
            }
        }

        /**
         * Returns the intent reported by the test app after the latest call to {@link #register},
         * waiting if necessary.
         *
         * @param timeoutMs milliseconds to wait for the intent if it hasn't been reported yet.
         */
        synchronized Intent awaitIntent(long timeoutMs) {
            if (mAction == null) {
                throw new IllegalStateException("Not registered for any action");
            }
            if (mReportedIntent == null) {
                try {
                    wait(timeoutMs);
                } catch (InterruptedException e) {
                    Log.e(TAG, "Interrupted while waiting for broadcast reporter", e);
                }
            }
            if (mReportedIntent != null) {
                assertEquals(mAction, mReportedIntent.getAction());
            }
            return mReportedIntent;
        }
    }
}
