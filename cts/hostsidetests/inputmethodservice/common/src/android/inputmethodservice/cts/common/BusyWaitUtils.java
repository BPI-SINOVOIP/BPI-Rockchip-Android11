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

package android.inputmethodservice.cts.common;

import static org.junit.Assert.fail;

import java.util.concurrent.TimeUnit;

/**
 * Utility class for busy waiting.
 */
public final class BusyWaitUtils {

    private static final long POLLING_INTERVAL = TimeUnit.MILLISECONDS.toMillis(50);

    /**
     * Callback interface for {@link #pollingCheck(PollingCondition, long, String)} and
     * {@link #waitFor(PollingCondition, long)}.
     */
    @FunctionalInterface
    public interface PollingCondition {
        /**
         * Called back for polling check.
         *
         * @return {@code true} when the polling condition is met.
         * @throws Exception when whatever unexpected problem happened.
         */
        boolean check() throws Exception;
    }

    // This is utility class, can't instantiate.
    private BusyWaitUtils() {}

    /**
     * Busy waiting until {@link PollingCondition#check()} returns {@code true}.
     * @param condition {@link PollingCondition} to be checked.
     * @param timeout milliseconds before time out happens.
     * @param message when time out happens, {@link org.junit.Assert#fail(String)} is called with
     *                this message.
     * @throws Exception
     */
    public static void pollingCheck(PollingCondition condition, long timeout, String message)
            throws Exception {
        if (waitFor(condition, timeout)) {
            return;
        }
        fail(message);
    }

    /**
     * Busy waiting until {@link PollingCondition#check()} returns {@code true}.
     * @param condition {@link PollingCondition} to be checked.
     * @param timeout milliseconds before time out happens.
     * @return true when {@code condition} returns {@code true}, false when timed out.
     * @throws Exception
     */
    static boolean waitFor(PollingCondition condition, long timeout) throws Exception {
        for (long remaining = timeout; remaining > 0; remaining -= POLLING_INTERVAL) {
            if (condition.check()) {
                return true;
            }
            Thread.sleep(POLLING_INTERVAL);
        }
        return false;
    }
}
