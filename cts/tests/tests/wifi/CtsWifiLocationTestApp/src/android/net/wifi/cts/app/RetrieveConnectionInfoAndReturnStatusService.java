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

import android.app.job.JobParameters;
import android.app.job.JobService;
import android.net.wifi.WifiManager;
import android.os.ResultReceiver;
import android.util.Log;

/**
 * A service that retrieves scan results and returns status.
 */
public class RetrieveConnectionInfoAndReturnStatusService extends JobService {
    private static final String TAG = "RetrieveConnectionInfoAndReturnStatusService";
    private static final String RESULT_RECEIVER_EXTRA =
            "android.net.wifi.cts.app.extra.RESULT_RECEIVER";

    @Override
    public boolean onStartJob(JobParameters jobParameters) {
        ResultReceiver resultReceiver =
                jobParameters.getTransientExtras().getParcelable(RESULT_RECEIVER_EXTRA);
        WifiManager wifiManager = getSystemService(WifiManager.class);
        boolean succeeded;
        try {
            succeeded = !wifiManager.getConnectionInfo().getSSID().equals(WifiManager.UNKNOWN_SSID);
        } catch (SecurityException e) {
            succeeded = false;
        }
        if (succeeded) {
            Log.v(TAG, "SSID from connection info retrieval succeeded");
        } else {
            Log.v(TAG, "Failed to retrieve SSID from connection info");
        }
        resultReceiver.send(succeeded ? 1 : 0, null);
        return false;
    }

    @Override
    public boolean onStopJob(JobParameters jobParameters) {
        return false;
    }
}
