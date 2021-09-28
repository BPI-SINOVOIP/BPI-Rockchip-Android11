/*
 * Copyright (C) 2016 The Android Open Source Project
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
 * limitations under the License
 */

package com.android;

import static org.mockito.Mockito.spy;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import com.android.internal.telephony.PhoneConfigurationManager;

import org.mockito.MockitoAnnotations;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Helper class to load Mockito Resources into a test.
 */
public class TelephonyTestBase {

    protected TestContext mContext;

    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mContext = spy(new TestContext());
        // Set up the looper if it does not exist on the test thread.
        if (Looper.myLooper() == null) {
            Looper.prepare();
            // Wait until the looper is not null anymore
            for(int i = 0; i < 5; i++) {
                if (Looper.myLooper() != null) {
                    break;
                }
                Looper.prepare();
                Thread.sleep(100);
            }
        }
    }

    public void tearDown() throws Exception {
        // Ensure there are no static references to handlers after test completes.
        PhoneConfigurationManager.unregisterAllMultiSimConfigChangeRegistrants();
    }

    protected final void waitForHandlerAction(Handler h, long timeoutMillis) {
        final CountDownLatch lock = new CountDownLatch(1);
        h.post(lock::countDown);
        while (lock.getCount() > 0) {
            try {
                lock.await(timeoutMillis, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                // do nothing
            }
        }
    }

    protected final void waitForHandlerActionDelayed(Handler h, long timeoutMillis, long delayMs) {
        final CountDownLatch lock = new CountDownLatch(1);
        h.postDelayed(lock::countDown, delayMs);
        while (lock.getCount() > 0) {
            try {
                lock.await(timeoutMillis, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                // do nothing
            }
        }
    }

    protected void waitForMs(long ms) {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException e) {
            Log.e("TelephonyTestBase", "InterruptedException while waiting: " + e);
        }
    }

    protected TestContext getTestContext() {
        return mContext;
    }
}
