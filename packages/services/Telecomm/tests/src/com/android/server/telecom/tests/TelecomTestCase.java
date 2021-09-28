/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.server.telecom.tests;

import android.content.Context;
import android.os.Handler;
import android.telecom.Log;

import androidx.test.InstrumentationRegistry;

import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.function.Predicate;

public abstract class TelecomTestCase {
    protected static final String TESTING_TAG = "Telecom-TEST";
    protected Context mContext;

    MockitoHelper mMockitoHelper = new MockitoHelper();
    ComponentContextFixture mComponentContextFixture;

    public void setUp() throws Exception {
        Log.setTag(TESTING_TAG);
        Log.setIsExtendedLoggingEnabled(true);
        mMockitoHelper.setUp(InstrumentationRegistry.getContext(), getClass());
        mComponentContextFixture = new ComponentContextFixture();
        mContext = mComponentContextFixture.getTestDouble().getApplicationContext();
        Log.setSessionContext(mComponentContextFixture.getTestDouble().getApplicationContext());
        Log.getSessionManager().mCleanStaleSessions = null;
        MockitoAnnotations.initMocks(this);
    }

    public void tearDown() throws Exception {
        mComponentContextFixture = null;
        mMockitoHelper.tearDown();
        Mockito.framework().clearInlineMocks();
    }

    protected static void waitForHandlerAction(Handler h, long timeoutMillis) {
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

    protected static <T> int findFirstIndexMatching(List<T> items, Predicate<T> matcher) {
        for (int i = 0; i < items.size(); i++) {
            if (matcher.test(items.get(i))) {
                return i;
            }
        }
        return -1;
    }
}
