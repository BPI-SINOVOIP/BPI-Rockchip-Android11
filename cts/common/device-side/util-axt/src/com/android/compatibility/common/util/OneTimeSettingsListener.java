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

package com.android.compatibility.common.util;

import static com.android.compatibility.common.util.SettingsUtils.NAMESPACE_GLOBAL;
import static com.android.compatibility.common.util.SettingsUtils.NAMESPACE_SECURE;

import android.content.ContentResolver;
import android.content.Context;
import android.database.ContentObserver;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.provider.Settings;
import android.util.Log;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Helper used to block tests until a secure settings value has been updated.
 */
public final class OneTimeSettingsListener extends ContentObserver {

    private static final String TAG = "OneTimeSettingsListener";
    public static final long DEFAULT_TIMEOUT_MS = 30_000;

    private final CountDownLatch mLatch = new CountDownLatch(1);
    private final ContentResolver mResolver;
    private final String mKey;
    private final long mStarted;

    public OneTimeSettingsListener(Context context, String namespace, String key) {
        super(new Handler(Looper.getMainLooper()));
        mStarted = SystemClock.elapsedRealtime();
        mKey = key;
        mResolver = context.getContentResolver();
        final Uri uri;
        switch (namespace) {
            case NAMESPACE_SECURE:
                uri = Settings.Secure.getUriFor(key);
                break;
            case NAMESPACE_GLOBAL:
                uri = Settings.Global.getUriFor(key);
                break;
            default:
                throw new IllegalArgumentException("invalid namespace: " + namespace);
        }
        mResolver.registerContentObserver(uri, false, this);
    }

    @Override
    public void onChange(boolean selfChange, Uri uri) {
        mResolver.unregisterContentObserver(this);
        mLatch.countDown();
    }

    /**
     * Blocks for a few seconds until it's called.
     *
     * @throws IllegalStateException if it's not called.
     */
    public void assertCalled() {
        try {
            final boolean updated = mLatch.await(DEFAULT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            if (!updated) {
                throw new RetryableException(
                        "Settings " + mKey + " not called in " + DEFAULT_TIMEOUT_MS + "ms");
            }
            final long delta = SystemClock.elapsedRealtime() - mStarted;
            // TODO: usually it's notified in ~50-150ms, but for some reason it takes ~10s
            // on some ViewAttributesTest methods, hence the 30s limit
            Log.v(TAG, TestNameUtils.getCurrentTestName() + "/" + mKey + ": " + delta + "ms");
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new IllegalStateException("Interrupted", e);
        }
    }
}
