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

package android.wifibroadcasts.app;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

/**
 * Logs to Logcat when unexpected broadcasts are received
 */
public class WifiBroadcastsDeviceActivity extends Activity {

    private static final String TAG = WifiBroadcastsDeviceActivity.class.getSimpleName();

    private final Context mContext = this;

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            Toast.makeText(mContext, action, Toast.LENGTH_SHORT).show();
            if (WifiManager.RSSI_CHANGED_ACTION.equals(action)) {
                Log.i(TAG, "UNEXPECTED WIFI BROADCAST RECEIVED - " + action);
            }
        }
    };

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        final IntentFilter filter = new IntentFilter();
        String action = WifiManager.RSSI_CHANGED_ACTION;
        filter.addAction(action);
        mContext.registerReceiver(mReceiver, filter);
        Log.i(TAG, "Registered " + action);
        Toast.makeText(mContext, "Started", Toast.LENGTH_SHORT).show();
    }

}
