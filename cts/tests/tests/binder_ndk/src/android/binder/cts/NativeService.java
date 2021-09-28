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

import android.app.Service;

import android.content.Intent;

import android.os.IBinder;

import android.util.Log;

public class NativeService extends Service {
    private final String TAG = "NativeService";
    private final IBinder mBinder;

    private NativeService() {
        this("binder_ndk_test_interface_new");
    }

    private NativeService(String libName) {
        System.loadLibrary(libName);
        mBinder = getBinder_native();
    }

    // the configuration of these services is done in AndroidManifest.xml
    public static class Local extends NativeService {}
    public static class Remote extends NativeService {}
    public static class RemoteOld extends NativeService {
        public RemoteOld() { super("binder_ndk_test_interface_old"); }
    }

    @Override
    public IBinder onBind(Intent intent) {
        Log.i(TAG, "Binding service");
        return mBinder;
    }

    private native IBinder getBinder_native();
}
