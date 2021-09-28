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

package com.android.tv.settings.users;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.UserHandle;
import android.util.Log;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

class RestrictedProfilePinServiceConnection implements ServiceConnection {
    private static final String TAG = RestrictedProfilePinServiceConnection.class.getSimpleName();
    public static final long CONNECTION_TIMEOUT_MILLIS = TimeUnit.SECONDS.toMillis(5);

    private Context mContext;

    private final AtomicReference<IRestrictedProfilePinService> mPinServiceRef =
            new AtomicReference<>();

    RestrictedProfilePinServiceConnection(Context context) {
        mContext = context;
    }

    @Override
    public void onServiceConnected(ComponentName name, IBinder binder) {
        synchronized (mPinServiceRef) {
            mPinServiceRef.set(IRestrictedProfilePinService.Stub.asInterface(binder));
            mPinServiceRef.notifyAll();
        }
        Log.d(TAG, "RestrictedProfilePinService connected");
    }

    @Override
    public void onServiceDisconnected(ComponentName name) {
        synchronized (mPinServiceRef) {
            mPinServiceRef.set(null);
        }
        Log.e(TAG, "RestrictedProfilePinService disconnected");
    }

    @Override
    public void onNullBinding(ComponentName name) {
        mPinServiceRef.set(null);
        Log.w(TAG, "Could not bind to RestrictedProfilePinService");
    }

    @Override
    public void onBindingDied(ComponentName name) {
        mPinServiceRef.set(null);
        Log.e(TAG, "Connection to RestrictedProfilePinService died");
    }

    IRestrictedProfilePinService getPinService() throws RemoteException {
        synchronized (mPinServiceRef) {
            if (!waitForService()) {
                throw new RemoteException("Connecting to RestrictedProfilePinService failed.");
            }

            return mPinServiceRef.get();
        }
    }

    void bindPinService() {
        if (mPinServiceRef.get() == null) {
            Intent intent = new Intent(mContext, RestrictedProfilePinService.class);
            mContext.bindServiceAsUser(intent, this, Context.BIND_AUTO_CREATE, UserHandle.SYSTEM);
        }
    }

    void unbindPinService() {
        mContext.unbindService(this);
    }

    private boolean waitForService() {
        try {
            synchronized (mPinServiceRef) {
                if (mPinServiceRef.get() == null) {
                    Log.w(TAG, "No connection to pin service. Waiting for connection...");
                    mPinServiceRef.wait(CONNECTION_TIMEOUT_MILLIS);
                }
            }
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        return mPinServiceRef.get() != null;
    }
}
