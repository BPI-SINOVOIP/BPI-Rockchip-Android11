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

package android.app.cts;

import android.app.ActivityManager;
import android.app.cts.android.app.cts.tools.WatchUidRunner;
import android.content.ContentResolver;
import android.content.pm.ApplicationInfo;
import android.net.Uri;
import android.os.HandlerThread;
import android.os.Handler;
import android.os.SystemClock;
import android.test.AndroidTestCase;

import androidx.test.InstrumentationRegistry;

/**
 * Test system behavior of a bad provider.
 */
public class BadProviderTest extends AndroidTestCase {
    private static final String AUTHORITY = "com.android.cts.stubbad.badprovider";
    private static final String TEST_PACKAGE_NAME = "com.android.cts.stubbad";
    private static final int WAIT_TIME = 2000;

    public void testExitOnCreate() {
        WatchUidRunner uidWatcher = null;
        ContentResolver res = mContext.getContentResolver();
        HandlerThread worker = new HandlerThread("work");
        worker.start();
        Handler handler = new Handler(worker.getLooper());
        try {
            ApplicationInfo appInfo = mContext.getPackageManager().getApplicationInfo(
                    TEST_PACKAGE_NAME, 0);
            uidWatcher = new WatchUidRunner(InstrumentationRegistry.getInstrumentation(),
                    appInfo.uid, WAIT_TIME);
            long startTs = SystemClock.uptimeMillis();
            handler.post(()->
                res.query(Uri.parse("content://" + AUTHORITY), null, null, null, null)
            );
            // Ensure the system will try at least 3 times for a bad content provider.
            uidWatcher.waitFor(WatchUidRunner.CMD_GONE, null);
            uidWatcher.waitFor(WatchUidRunner.CMD_GONE, null);
            uidWatcher.waitFor(WatchUidRunner.CMD_GONE, null);
            // Finish the watcher
            uidWatcher.finish();
            // Sleep for 10 seconds and initialize the watcher again
            // (content provider publish timeout is 10 seconds)
            Thread.sleep(Math.max(0, 10000 - (SystemClock.uptimeMillis() - startTs)));
            uidWatcher = new WatchUidRunner(InstrumentationRegistry.getInstrumentation(),
                    appInfo.uid, WAIT_TIME);
            // By now we shouldn't see it's retrying again.
            try {
                uidWatcher.waitFor(WatchUidRunner.CMD_GONE, null);
                fail("Excessive attempts to bring up a provider");
            } catch (IllegalStateException e) {
            }
        } catch (Exception e) {
            fail("Unexpected exception while query provider: " + e.getMessage());
        } finally {
            if (uidWatcher != null) {
                uidWatcher.finish();
            }
            worker.quitSafely();
        }
    }
}
