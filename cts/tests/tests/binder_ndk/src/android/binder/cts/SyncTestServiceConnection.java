/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.binder.cts;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;

import android.os.IBinder;

import android.util.Log;

import test_package.ITest;

/**
 * This enables connecting to a service synchronously easily for testing purposes.
 */
public class SyncTestServiceConnection implements ServiceConnection {
    private final String TAG = "SyncServiceConnection";

    private Class mServiceProviderClass;
    private Context mContext;

    private ITest mInterface;
    private boolean mInvalid = false;  // if the service has disconnected abrubtly

    public SyncTestServiceConnection(Context context, Class serviceClass) {
        mContext = context;
        mServiceProviderClass = serviceClass;
    }

    public void onServiceConnected(ComponentName className, IBinder service) {
        synchronized (this) {
            mInterface = ITest.Stub.asInterface(service);
            Log.e(TAG, "Service has connected: " + mServiceProviderClass);
            this.notify();
        }
    }

    public void onServiceDisconnected(ComponentName className) {
        Log.e(TAG, "Service has disconnected: " + mServiceProviderClass);
        synchronized (this) {
            mInterface = null;
            mInvalid = true;
            this.notify();
        }
    }

    ITest get() {
        synchronized(this) {
            if (!mInvalid && mInterface == null) {
                Intent intent = new Intent(mContext, mServiceProviderClass);
                intent.setAction(ITest.class.getName());
                mContext.bindService(intent, this, Context.BIND_AUTO_CREATE);

                try {
                    this.wait(5000);
                } catch (InterruptedException e) {}
            }
        }
        return mInterface;
    }
}
