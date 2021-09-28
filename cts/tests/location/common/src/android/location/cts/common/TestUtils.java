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

package android.location.cts.common;

import android.content.Context;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.util.Log;

import com.android.compatibility.common.util.SystemUtil;

import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class TestUtils {
    private static final String TAG = "LocationTestUtils";

    private static final long STANDARD_WAIT_TIME_MS = 50;
    private static final long STANDARD_SLEEP_TIME_MS = 50;

    private static final int DATA_CONNECTION_CHECK_INTERVAL_MS = 500;
    private static final int DATA_CONNECTION_CHECK_COUNT = 10; // 500 * 10 - Roughly 5 secs wait

    public static boolean waitFor(CountDownLatch latch, int timeInSec) throws InterruptedException {
        // Since late 2014, if the main thread has been occupied for long enough, Android will
        // increase its priority. Such new behavior can causes starvation to the background thread -
        // even if the main thread has called await() to yield its execution, the background thread
        // still can't get scheduled.
        //
        // Here we're trying to wait on the main thread for a PendingIntent from a background
        // thread. Because of the starvation problem, the background thread may take up to 5 minutes
        // to deliver the PendingIntent if we simply call await() on the main thread. In order to
        // give the background thread a chance to run, we call Thread.sleep() in a loop. Such dirty
        // hack isn't ideal, but at least it can work.
        //
        // See also: b/17423027
        long waitTimeRounds = (TimeUnit.SECONDS.toMillis(timeInSec)) /
                (STANDARD_WAIT_TIME_MS + STANDARD_SLEEP_TIME_MS);
        for (int i = 0; i < waitTimeRounds; ++i) {
            Thread.sleep(STANDARD_SLEEP_TIME_MS);
            if (latch.await(STANDARD_WAIT_TIME_MS, TimeUnit.MILLISECONDS)) {
                return true;
            }
        }
        return false;
    }

    public static boolean waitForWithCondition(int timeInSec, Callable<Boolean> callback)
        throws Exception {
        long waitTimeRounds = (TimeUnit.SECONDS.toMillis(timeInSec)) / STANDARD_SLEEP_TIME_MS;
        for (int i = 0; i < waitTimeRounds; ++i) {
            Thread.sleep(STANDARD_SLEEP_TIME_MS);
            if(callback.call()) return true;
        }
        return false;
    }

    public static boolean deviceHasGpsFeature(Context context) {
        // If device does not have a GPS, skip the test.
        if (context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_LOCATION_GPS)) {
            return true;
        }
        Log.w(TAG, "GPS feature not present on device, skipping GPS test.");
        return false;
    }

    /**
     * Returns whether the device is currently connected to a wifi or cellular.
     *
     * @param context {@link Context} object
     * @return {@code true} if connected to Wifi or Cellular; {@code false} otherwise
     */
    public static boolean isConnectedToWifiOrCellular(Context context) {
        NetworkInfo info = getActiveNetworkInfo(context);
        return info != null
                && info.isConnected()
                && (info.getType() == ConnectivityManager.TYPE_WIFI
                || info.getType() == ConnectivityManager.TYPE_MOBILE);
    }

    /**
     * Gets the active network info.
     *
     * @param context {@link Context} object
     * @return {@link NetworkInfo}
     */
    private static NetworkInfo getActiveNetworkInfo(Context context) {
        ConnectivityManager cm = getConnectivityManager(context);
        if (cm != null) {
            return cm.getActiveNetworkInfo();
        }
        return null;
    }

    /**
     * Gets the connectivity manager.
     *
     * @param context {@link Context} object
     * @return {@link ConnectivityManager}
     */
    public static ConnectivityManager getConnectivityManager(Context context) {
        return (ConnectivityManager) context.getApplicationContext()
                .getSystemService(Context.CONNECTIVITY_SERVICE);
    }

    /**
     * Returns {@code true} if the setting {@code airplane_mode_on} is set to 1.
     */
    public static boolean isAirplaneModeOn() {
        return SystemUtil.runShellCommand("settings get global airplane_mode_on")
                .trim().equals("1");
    }

    /**
     * Changes the setting {@code airplane_mode_on} to 1 if {@code enableAirplaneMode}
     * is {@code true}. Otherwise, it is set to 0.
     *
     * <p>Waits for a certain time duration for network connections to turn on/off based on
     * {@code enableAirplaneMode}.
     */
    public static void setAirplaneModeOn(Context context,
            boolean enableAirplaneMode) throws InterruptedException {
        Log.i(TAG, "Setting airplane_mode_on to " + enableAirplaneMode);
        SystemUtil.runShellCommand("cmd connectivity airplane-mode "
                + (enableAirplaneMode ? "enable" : "disable"));

        // Wait for a few seconds until the airplane mode changes take effect. The airplane mode on
        // state and the WiFi/cell connected state are opposite. So, we wait while they are the
        // same or until the specified time interval expires.
        //
        // Note that in unusual cases where the WiFi/cell are not in a connected state before
        // turning on airplane mode, then turning off airplane mode won't restore either of
        // these connections, and then the wait time below will be wasteful.
        int dataConnectionCheckCount = DATA_CONNECTION_CHECK_COUNT;
        while (enableAirplaneMode == isConnectedToWifiOrCellular(context)) {
            if (--dataConnectionCheckCount <= 0) {
                Log.w(TAG, "Airplane mode " + (enableAirplaneMode ? "on" : "off")
                        + " setting did not take effect on WiFi/cell connected state.");
                return;
            }
            Thread.sleep(DATA_CONNECTION_CHECK_INTERVAL_MS);
        }
    }
}
