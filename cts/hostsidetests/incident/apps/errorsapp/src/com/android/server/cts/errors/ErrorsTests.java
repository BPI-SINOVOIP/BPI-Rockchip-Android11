/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.server.cts.errors;

import static org.junit.Assert.assertTrue;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.DropBoxManager;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Used by ErrorTest. Spawns misbehaving activities so reports will appear in Dropbox.
 */
@RunWith(AndroidJUnit4.class)
public class ErrorsTests {
    private static final String TAG = "ErrorsTests";

    private static final String CRASH_TAG = "data_app_crash";
    private static final String ANR_TAG = "data_app_anr";
    private static final String NATIVE_CRASH_TAG = "data_app_native_crash";
    private static final String TOMBSTONE_TAG = "SYSTEM_TOMBSTONE";

    private static final int TIMEOUT_SECS = 60 * 3;

    private CountDownLatch mResultsReceivedSignal;
    private DropBoxManager mDropbox;
    private long mStartMs;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
        mDropbox = (DropBoxManager) mContext.getSystemService(Context.DROPBOX_SERVICE);
        mResultsReceivedSignal = new CountDownLatch(1);
        mStartMs = System.currentTimeMillis();

        InstrumentationRegistry.getInstrumentation().getUiAutomation().executeShellCommand(
                "appops set " + mContext.getPackageName() + " android:get_usage_stats allow");
    }

    @Test
    public void testException() throws Exception {
        Log.i(TAG, "testException");

        registerReceiver(mContext, mResultsReceivedSignal, CRASH_TAG,
                 mContext.getPackageName() + ":TestProcess",
                "java.lang.RuntimeException: This is a test exception");
        Intent intent = new Intent();
        intent.setClass(mContext, ExceptionActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);

        assertTrue(mResultsReceivedSignal.await(TIMEOUT_SECS, TimeUnit.SECONDS));
    }

    @Test
    public void testANR() throws Exception {
        Log.i(TAG, "testANR");

        // Require that we get an ANR entry that shows that the activity is blocked in onCreate.
        registerReceiver(mContext, mResultsReceivedSignal, ANR_TAG,
                mContext.getPackageName() + ":TestProcess",
                "com.android.server.cts.errors.ANRActivity.onCreate");
        Intent intent = new Intent();
        intent.setClass(mContext, ANRActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);

        final Intent receiver = new Intent(mContext, Receiver.class);
        mContext.sendBroadcast(receiver);
        Log.i(TAG, "testANR -- sent broadcast to " + receiver);

        assertTrue(mResultsReceivedSignal.await(TIMEOUT_SECS, TimeUnit.SECONDS));
    }

    @Test
    public void testNativeCrash() throws Exception {
        Log.i(TAG, "testNativeCrash");

        registerReceiver(mContext, mResultsReceivedSignal, NATIVE_CRASH_TAG,
                mContext.getPackageName() + ":TestProcess", "backtrace:");
        Intent intent = new Intent();
        intent.setClass(mContext, NativeActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);

        assertTrue(mResultsReceivedSignal.await(TIMEOUT_SECS, TimeUnit.SECONDS));
    }

    @Test
    public void testTombstone() throws Exception {
        Log.i(TAG, "testTombstone");

        registerReceiver(mContext, mResultsReceivedSignal, TOMBSTONE_TAG,
                mContext.getPackageName() + ":TestProcess", "backtrace:");
        Intent intent = new Intent();
        intent.setClass(mContext, NativeActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);

        assertTrue(mResultsReceivedSignal.await(TIMEOUT_SECS, TimeUnit.SECONDS));
    }

    void registerReceiver(Context ctx, CountDownLatch onReceiveLatch, String wantTag,
            String... wantInStackTrace) {
        ctx.registerReceiver(new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                // DropBox might receive other entries while we're waiting for the error
                // entry, so we need to check the tag and stack trace before continuing.
                while (true) {
                    final DropBoxManager.Entry entry = mDropbox.getNextEntry(wantTag, mStartMs);
                    if (entry == null) {
                        break;
                    }
                    Log.d(TAG, "ErrorsTest got message from drobpox: " + entry.getTag());
                    mStartMs = entry.getTimeMillis();
                    String stackTrace = entry.getText(64 * 1024);
                    boolean allMatches = true;
                    for (String line : wantInStackTrace) {
                        boolean matched = stackTrace.contains(line);
                        Log.d(TAG, "   matched=" + matched + " line: " + line);
                        allMatches &= matched;
                    }
                    if (allMatches) {
                        onReceiveLatch.countDown();
                    }
                    entry.close();
                }
            }
        }, new IntentFilter(DropBoxManager.ACTION_DROPBOX_ENTRY_ADDED));
    }
}
