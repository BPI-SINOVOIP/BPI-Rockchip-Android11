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
package com.android.compatibility.common.util;

import static org.junit.Assert.fail;

// TODO(b/131736394): Remove duplication with HostSideTestUtils.
/** Utility class for tests. */
public class CommonTestUtils {
    @FunctionalInterface
    public interface BooleanSupplierWithThrow<E extends Throwable> {
        boolean getAsBoolean() throws E;
    }

    /** Wait until {@code predicate} is satisfied, or fail, with a given timeout. */
    public static <E extends Throwable> void waitUntil(
            String message, long timeoutSeconds, BooleanSupplierWithThrow<E> predicate)
            throws E, InterruptedException {
        int sleep = 125;
        final long timeout = System.currentTimeMillis() + timeoutSeconds * 1000;
        while (System.currentTimeMillis() < timeout) {
            if (predicate.getAsBoolean()) {
                return;
            }
            Thread.sleep(sleep);
        }
        fail(message);
    }
}
