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

package android.net.wifi.cts.app;

import android.app.Activity;
import android.content.Intent;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.util.Log;

/**
 * An activity that triggers a wifi scan and returns status.
 */
public class TriggerScanAndReturnStatusActivity extends Activity {
    private static final String TAG = "TriggerScanAndReturnStatusActivity";
    private static final String SCAN_STATUS_EXTRA = "android.net.wifi.cts.app.extra.STATUS";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        WifiManager wifiManager = getSystemService(WifiManager.class);
        boolean succeeded;
        try {
            succeeded = wifiManager.startScan();
        } catch (SecurityException e) {
            succeeded = false;
        }
        if (succeeded) {
            Log.v(TAG, "Scan trigger succeeded");
        } else {
            Log.v(TAG, "Failed to trigger scan");
        }
        setResult(RESULT_OK, new Intent().putExtra(SCAN_STATUS_EXTRA, succeeded));
        finish();
    }
}
