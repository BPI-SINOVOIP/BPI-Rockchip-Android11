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
 * limitations under the License
 */

package android.assist.common;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * A [CountDownLatch] that resets itself to [mCount] after every await call.
 */
public class AutoResetLatch {

    private final int mCount;
    private CountDownLatch mLatch;

    public AutoResetLatch() {
        this(1);
    }

    public AutoResetLatch(int count) {
        mCount = count;
        mLatch = new CountDownLatch(count);
    }

    public void await() throws InterruptedException {
        try {
            mLatch.await();
        } finally {
            mLatch = new CountDownLatch(mCount);
        }
    }

    public boolean await(long timeout, TimeUnit unit) throws InterruptedException {
        try {
            return mLatch.await(timeout, unit);
        } finally {
            mLatch = new CountDownLatch(mCount);
        }
    }

    public void countDown() {
        mLatch.countDown();
    }
}
