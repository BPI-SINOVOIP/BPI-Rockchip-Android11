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
package android.autofillservice.cts;

import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.util.Log;

import com.android.compatibility.common.util.RetryableException;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class MyDrawable extends Drawable {

    private static final String TAG = "MyDrawable";

    private static CountDownLatch sLatch;
    private static MyDrawable sInstance;

    private static Rect sAutofilledBounds;

    public MyDrawable() {
        if (sInstance != null) {
            throw new IllegalStateException("There can be only one!");
        }
        sInstance = this;
    }

    @Override
    public void draw(Canvas canvas) {
        if (sInstance != null && sAutofilledBounds == null) {
            sAutofilledBounds = new Rect(getBounds());
            Log.d(TAG, "Autofilled at " + sAutofilledBounds);
            sLatch.countDown();
        }
    }

    public static Rect getAutofilledBounds() throws InterruptedException {
        if (sLatch == null) {
            throw new AssertionError("sLatch should be not null");
        }

        if (!sLatch.await(Timeouts.FILL_TIMEOUT.ms(), TimeUnit.MILLISECONDS)) {
            throw new RetryableException(Timeouts.FILL_TIMEOUT, "custom drawable not drawn");
        }
        return sAutofilledBounds;
    }

    /**
     * Asserts the custom drawable is not drawn.
     */
    public static void assertDrawableNotDrawn() throws Exception {
        if (sLatch == null) {
            throw new AssertionError("sLatch should be not null");
        }

        if (sLatch.await(Timeouts.DRAWABLE_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            throw new AssertionError("custom drawable is drawn");
        }
    }

    public static void initStatus() {
        sLatch = new CountDownLatch(1);
        sInstance = null;
        sAutofilledBounds = null;
    }

    public static void clearStatus() {
        sLatch = null;
        sInstance = null;
        sAutofilledBounds = null;
    }

    @Override
    public void setAlpha(int alpha) {
    }

    @Override
    public void setColorFilter(ColorFilter colorFilter) {
    }

    @Override
    public int getOpacity() {
        return 0;
    }
}
