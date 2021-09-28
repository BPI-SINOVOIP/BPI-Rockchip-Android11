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

package com.android.compatibility.common.util;

import static com.android.compatibility.common.util.BlockedNumberService.DELETE_ACTION;
import static com.android.compatibility.common.util.BlockedNumberService.INSERT_ACTION;
import static com.android.compatibility.common.util.BlockedNumberService.PHONE_NUMBER_EXTRA;
import static com.android.compatibility.common.util.BlockedNumberService.RESULT_RECEIVER_EXTRA;
import static com.android.compatibility.common.util.BlockedNumberService.ROWS_EXTRA;
import static com.android.compatibility.common.util.BlockedNumberService.URI_EXTRA;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ResultReceiver;

import junit.framework.TestCase;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * Utility for starting the blocked number service.
 */
public class BlockedNumberUtil {

    private static final int TIMEOUT = 2;

    private BlockedNumberUtil() {}

    /** Insert a phone number into the blocked number provider and returns the resulting Uri. */
    public static Uri insertBlockedNumber(Context context, String phoneNumber) {
        Intent intent = new Intent(INSERT_ACTION);
        intent.putExtra(PHONE_NUMBER_EXTRA, phoneNumber);

        Bundle result = runBlockedNumberService(context, intent);
        if (result.getBoolean(BlockedNumberService.FAIL_EXTRA)) {
            return null;
        }
        return Uri.parse(result.getString(URI_EXTRA));
    }

    /** Remove a number from the blocked number provider and returns the number of rows deleted. */
    public static int deleteBlockedNumber(Context context, Uri uri) {
        Intent intent = new Intent(DELETE_ACTION);
        intent.putExtra(URI_EXTRA, uri.toString());

        return runBlockedNumberService(context, intent).getInt(ROWS_EXTRA);
    }

    /** Start the blocked number service. */
    static Bundle runBlockedNumberService(Context context, Intent intent) {
        // Temporarily allow background service
        SystemUtil.runShellCommand("cmd deviceidle tempwhitelist " + context.getPackageName());

        final Semaphore semaphore = new Semaphore(0);
        final Bundle result = new Bundle();

        ResultReceiver receiver = new ResultReceiver(new Handler(Looper.getMainLooper())) {
            @Override
            protected void onReceiveResult(int resultCode, Bundle resultData) {
                result.putAll(resultData);
                semaphore.release();
            }
        };
        intent.putExtra(RESULT_RECEIVER_EXTRA, receiver);
        intent.setComponent(new ComponentName(context, BlockedNumberService.class));

        context.startService(intent);

        try {
            TestCase.assertTrue(semaphore.tryAcquire(TIMEOUT, TimeUnit.SECONDS));
        } catch (InterruptedException e) {
            TestCase.fail("Timed out waiting for result from BlockedNumberService");
        }
        return result;
    }
}
