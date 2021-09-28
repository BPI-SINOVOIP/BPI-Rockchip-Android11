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

package android.net.cts.api23test;

import static android.content.pm.PackageManager.FEATURE_WIFI;

import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.cts.util.CtsNetUtils;
import android.os.Looper;
import android.test.AndroidTestCase;
import android.util.Log;

import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

public class ConnectivityManagerApi23Test extends AndroidTestCase {
    private static final String TAG = ConnectivityManagerApi23Test.class.getSimpleName();
    private static final int SEND_BROADCAST_TIMEOUT = 30000;
    // Intent string to get the number of wifi CONNECTIVITY_ACTION callbacks the test app has seen
    public static final String GET_WIFI_CONNECTIVITY_ACTION_COUNT =
            "android.net.cts.appForApi23.getWifiConnectivityActionCount";
    // Action sent to ConnectivityActionReceiver when a network callback is sent via PendingIntent.

    private Context mContext;
    private PackageManager mPackageManager;
    private CtsNetUtils mCtsNetUtils;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        Looper.prepare();
        mContext = getContext();
        mPackageManager = mContext.getPackageManager();
        mCtsNetUtils = new CtsNetUtils(mContext);
    }

    /**
     * Tests reporting of connectivity changed.
     */
    public void testConnectivityChanged_manifestRequestOnly_shouldNotReceiveIntent() {
        if (!mPackageManager.hasSystemFeature(FEATURE_WIFI)) {
            Log.i(TAG, "testConnectivityChanged_manifestRequestOnly_shouldNotReceiveIntent cannot execute unless device supports WiFi");
            return;
        }
        ConnectivityReceiver.prepare();

        mCtsNetUtils.toggleWifi();

        // The connectivity broadcast has been sent; push through a terminal broadcast
        // to wait for in the receive to confirm it didn't see the connectivity change.
        Intent finalIntent = new Intent(ConnectivityReceiver.FINAL_ACTION);
        finalIntent.setClass(mContext, ConnectivityReceiver.class);
        mContext.sendBroadcast(finalIntent);
        assertFalse(ConnectivityReceiver.waitForBroadcast());
    }

    public void testConnectivityChanged_manifestRequestOnlyPreN_shouldReceiveIntent()
            throws InterruptedException {
        if (!mPackageManager.hasSystemFeature(FEATURE_WIFI)) {
            Log.i(TAG, "testConnectivityChanged_manifestRequestOnlyPreN_shouldReceiveIntent cannot"
                    + "execute unless device supports WiFi");
            return;
        }
        mContext.startActivity(new Intent()
                .setComponent(new ComponentName("android.net.cts.appForApi23",
                        "android.net.cts.appForApi23.ConnectivityListeningActivity"))
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        Thread.sleep(200);

        mCtsNetUtils.toggleWifi();

        Intent getConnectivityCount = new Intent(GET_WIFI_CONNECTIVITY_ACTION_COUNT);
        assertEquals(2, sendOrderedBroadcastAndReturnResultCode(
                getConnectivityCount, SEND_BROADCAST_TIMEOUT));
    }

    public void testConnectivityChanged_whenRegistered_shouldReceiveIntent() {
        if (!mPackageManager.hasSystemFeature(FEATURE_WIFI)) {
            Log.i(TAG, "testConnectivityChanged_whenRegistered_shouldReceiveIntent cannot execute unless device supports WiFi");
            return;
        }
        ConnectivityReceiver.prepare();
        ConnectivityReceiver receiver = new ConnectivityReceiver();
        IntentFilter filter = new IntentFilter();
        filter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);
        mContext.registerReceiver(receiver, filter);

        mCtsNetUtils.toggleWifi();
        Intent finalIntent = new Intent(ConnectivityReceiver.FINAL_ACTION);
        finalIntent.setClass(mContext, ConnectivityReceiver.class);
        mContext.sendBroadcast(finalIntent);

        assertTrue(ConnectivityReceiver.waitForBroadcast());
    }

    private int sendOrderedBroadcastAndReturnResultCode(
            Intent intent, int timeoutMs) throws InterruptedException {
        final LinkedBlockingQueue<Integer> result = new LinkedBlockingQueue<>(1);
        mContext.sendOrderedBroadcast(intent, null, new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                result.offer(getResultCode());
            }
        }, null, 0, null, null);

        Integer resultCode = result.poll(timeoutMs, TimeUnit.MILLISECONDS);
        assertNotNull("Timed out (more than " + timeoutMs +
                " milliseconds) waiting for result code for broadcast", resultCode);
        return resultCode;
    }

}