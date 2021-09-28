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

import static android.provider.BlockedNumberContract.BlockedNumbers.COLUMN_ORIGINAL_NUMBER;
import static android.provider.BlockedNumberContract.BlockedNumbers.CONTENT_URI;

import android.app.IntentService;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Intent;
import android.database.ContentObserver;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.ResultReceiver;
import android.util.Log;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * A service to handle interactions with the BlockedNumberProvider. The BlockedNumberProvider
 * can only be accessed by the primary user. This service can be run as a singleton service
 * which will then be able to access the BlockedNumberProvider from a test running in a
 * secondary user.
 */
public class BlockedNumberService extends IntentService {

    static final String INSERT_ACTION = "android.telecom.cts.InsertBlockedNumber";
    static final String DELETE_ACTION = "android.telecom.cts.DeleteBlockedNumber";
    static final String PHONE_NUMBER_EXTRA = "number";
    static final String URI_EXTRA = "uri";
    static final String ROWS_EXTRA = "rows";
    static final String FAIL_EXTRA = "fail";
    static final String RESULT_RECEIVER_EXTRA = "resultReceiver";

    private static final String TAG = "CtsBlockNumberSvc";
    private static final int ASYNC_TIMEOUT = 10000;

    private ContentResolver mContentResolver;
    private Handler mHandler = new Handler();

    public BlockedNumberService() {
        super(BlockedNumberService.class.getName());
    }

    @Override
    public void onHandleIntent(Intent intent) {
        Log.i(TAG, "Starting BlockedNumberService service: " + intent);
        if (intent == null) {
            return;
        }
        Bundle bundle;
        mContentResolver = getContentResolver();
        switch (intent.getAction()) {
            case INSERT_ACTION:
                bundle = insertBlockedNumber(intent.getStringExtra(PHONE_NUMBER_EXTRA));
                break;
            case DELETE_ACTION:
                bundle = deleteBlockedNumber(Uri.parse(intent.getStringExtra(URI_EXTRA)));
                break;
            default:
                bundle = new Bundle();
                break;
        }
        ResultReceiver receiver = intent.getParcelableExtra(RESULT_RECEIVER_EXTRA);
        receiver.send(0, bundle);
    }

    private Bundle insertBlockedNumber(String number) {
        Log.i(TAG, "insertBlockedNumber: " + number);

        CountDownLatch blockedNumberLatch = getBlockedNumberLatch();
        ContentValues cv = new ContentValues();
        cv.put(COLUMN_ORIGINAL_NUMBER, number);
        Uri uri = mContentResolver.insert(CONTENT_URI, cv);
        Bundle bundle = new Bundle();
        bundle.putString(URI_EXTRA, uri.toString());

        // Wait for the content provider to be updated.
        try {
            if (!blockedNumberLatch.await(ASYNC_TIMEOUT, TimeUnit.MILLISECONDS)) {
                Log.e(TAG, "Timed out waiting for blocked number update");
                bundle.putBoolean(FAIL_EXTRA, true);
            }
        } catch (InterruptedException e) {
            Log.e(TAG, "Interrupted while waiting for blocked number update");
            bundle.putBoolean(FAIL_EXTRA, true);
        }
        return bundle;
    }

    private CountDownLatch getBlockedNumberLatch() {
        CountDownLatch changeLatch = new CountDownLatch(1);
        getContentResolver().registerContentObserver(
                CONTENT_URI, true,
                    new ContentObserver(mHandler) {
                        @Override
                        public void onChange(boolean selfChange, Uri uri) {
                            getContentResolver().unregisterContentObserver(this);
                            changeLatch.countDown();
                            super.onChange(selfChange);
                        }
                    });
        return changeLatch;
    }

    private Bundle deleteBlockedNumber(Uri uri) {
        Log.i(TAG, "deleteBlockedNumber: " + uri);

        int rows = mContentResolver.delete(uri, null, null);
        Bundle bundle = new Bundle();
        bundle.putInt(ROWS_EXTRA, rows);
        return bundle;
    }
}
