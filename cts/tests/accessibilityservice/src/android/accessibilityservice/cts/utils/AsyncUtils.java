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

package android.accessibilityservice.cts.utils;

import static android.accessibilityservice.cts.utils.CtsTestUtils.assertThrows;

import static java.util.concurrent.TimeUnit.MILLISECONDS;

import com.android.compatibility.common.util.TestUtils;

import java.util.concurrent.CancellationException;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.function.BooleanSupplier;

public class AsyncUtils {
    public static final long DEFAULT_TIMEOUT_MS = 5000;

    private AsyncUtils() {}

    public static <T> T await(Future<T> f) {
        return await(f, DEFAULT_TIMEOUT_MS, MILLISECONDS);
    }

    public static <T> T await(Future<T> f, long time, TimeUnit timeUnit) {
        try {
            return f.get(time, timeUnit);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    public static Throwable awaitFailure(Future<?> f) {
        return awaitFailure(f, DEFAULT_TIMEOUT_MS, MILLISECONDS);
    }

    public static Throwable awaitFailure(Future<?> f, long time, TimeUnit timeUnit) {
        return assertThrows(() -> await(f, time, timeUnit));
    }

    public static <T> CancellationException awaitCancellation(Future<T> f) {
       return awaitCancellation(f, DEFAULT_TIMEOUT_MS, MILLISECONDS);
    }

    public static <T> CancellationException awaitCancellation(
            Future<T> f, long time, TimeUnit timeUnit) {
        Throwable ex = awaitFailure(f, time, timeUnit);
        Throwable current = ex;
        while (current != null) {
            if (current instanceof CancellationException) {
                return (CancellationException) current;
            }
            current = current.getCause();
        }
        throw new AssertionError("Expected cancellation", ex);
    }

    public static void waitOn(Object notifyLock, BooleanSupplier condition) {
        TestUtils.waitOn(notifyLock, condition, DEFAULT_TIMEOUT_MS, null);
    }
}
