/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.game.qualification.tests;

import static com.google.common.truth.Truth.assertWithMessage;

import android.app.Activity;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class TestActivity extends Activity {

    private CountDownLatch latch = new CountDownLatch(1);

    @Override
    protected void onDestroy() {
        super.onDestroy();
        latch.countDown();
    }

    /**
     * Wait until onDestroy is called or 10 seconds has elapsed.
     *
     * AssertionError is thrown if wait time exceeded timeout.
     */
    public void waitUntilDestroyed() throws InterruptedException {
        waitUntilDestroyed(10000);
    }

    /**
     * Wait until onDestroy is called or the specified timeoutMs has elapsed.
     *
     * AssertionError is thrown if wait time exceeded timeout.
     */
    public void waitUntilDestroyed(int timeoutMs) throws InterruptedException {
        boolean timeout = latch.await(timeoutMs, TimeUnit.MILLISECONDS);
        assertWithMessage("Timeout waiting for the activity to be destroyed").that(timeout).isTrue();
    }

    static {
        System.loadLibrary("gamecore_java_tests_jni");
    }

}
