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
package android.car.test.util;

import android.annotation.Nullable;
import android.os.Bundle;
import android.util.Log;

import com.android.internal.os.IResultReceiver;
import com.android.internal.util.Preconditions;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Implementation of {@link IResultReceiver} that blocks waiting for the result.
 */
public final class BlockingResultReceiver extends IResultReceiver.Stub {

    private static final String TAG = BlockingResultReceiver.class.getSimpleName();

    private final CountDownLatch mLatch = new CountDownLatch(1);
    private final long mTimeoutMs;

    private int mResultCode;
    @Nullable private Bundle mResultData;

    /**
     * Default constructor.
     *
     * @param timeoutMs how long to wait for before failing.
     */
    public BlockingResultReceiver(long timeoutMs) {
        mTimeoutMs = timeoutMs;
    }

    @Override
    public void send(int resultCode, Bundle resultData) {
        Log.d(TAG, "send() received: code=" + resultCode + ", data=" + resultData + ", count="
                + mLatch.getCount());
        Preconditions.checkState(mLatch.getCount() == 1,
                "send() already called (code=" + mResultCode + ", data=" + mResultData);
        mResultCode = resultCode;
        mResultData = resultData;
        mLatch.countDown();
    }

    private void assertCalled() throws InterruptedException {
        boolean called = mLatch.await(mTimeoutMs, TimeUnit.MILLISECONDS);
        Log.d(TAG, "assertCalled(): " + called);
        Preconditions.checkState(called, "receiver not called in " + mTimeoutMs + " ms");
    }

    /**
     * Gets the {@code resultCode} or fails if it times out before {@code send()} is called.
     */
    public int getResultCode() throws InterruptedException {
        assertCalled();
        return mResultCode;
    }

    /**
     * Gets the {@code resultData} or fails if it times out before {@code send()} is called.
     */
    @Nullable
    public Bundle getResultData() throws InterruptedException {
        assertCalled();
        return mResultData;
    }
}
