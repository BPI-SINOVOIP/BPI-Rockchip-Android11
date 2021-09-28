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

package android.location.cts.common;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Looper;

import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

public class BroadcastCapture extends BroadcastReceiver implements AutoCloseable {

    protected final Context mContext;
    private final LinkedBlockingQueue<Intent> mIntents;

    public BroadcastCapture(Context context, String action) {
        this(context);
        register(action);
    }

    protected BroadcastCapture(Context context) {
        mContext = context;
        mIntents = new LinkedBlockingQueue<>();
    }

    protected void register(String action) {
        mContext.registerReceiver(this, new IntentFilter(action));
    }

    public Intent getNextIntent(long timeoutMs) throws InterruptedException {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            throw new AssertionError("getNextLocation() called from main thread");
        }

        return mIntents.poll(timeoutMs, TimeUnit.MILLISECONDS);
    }

    @Override
    public void close() {
        mContext.unregisterReceiver(this);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        mIntents.add(intent);
    }
}