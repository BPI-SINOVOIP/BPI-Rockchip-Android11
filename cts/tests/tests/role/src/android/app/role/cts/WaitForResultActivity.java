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

package android.app.role.cts;

import android.app.Activity;
import android.content.Intent;
import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * An Activity that can start another Activity and wait for its result.
 */
public class WaitForResultActivity extends Activity {

    private static final int REQUEST_CODE_WAIT_FOR_RESULT = 1;

    private CountDownLatch mLatch;
    private int mResultCode;
    private Intent mData;

    public void startActivityToWaitForResult(@NonNull Intent intent) {
        mLatch = new CountDownLatch(1);
        startActivityForResult(intent, REQUEST_CODE_WAIT_FOR_RESULT);
    }

    @NonNull
    public Pair<Integer, Intent> waitForActivityResult(long timeoutMillis)
            throws InterruptedException {
        mLatch.await(timeoutMillis, TimeUnit.MILLISECONDS);
        return new Pair<>(mResultCode, mData);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        if (requestCode == REQUEST_CODE_WAIT_FOR_RESULT) {
            mResultCode = resultCode;
            mData = data;
            mLatch.countDown();
        } else {
            super.onActivityResult(requestCode, resultCode, data);
        }
    }
}
