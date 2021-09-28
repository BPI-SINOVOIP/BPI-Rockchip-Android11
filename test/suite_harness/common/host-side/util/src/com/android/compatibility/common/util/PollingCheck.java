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
 * limitations under the License.
 */

package com.android.compatibility.common.util;

import static org.junit.Assert.fail;

import java.util.concurrent.Callable;

/** Test utility to check for or wait for a condition by polling. */
public abstract class PollingCheck {
    public static final PollingCheckClock DEFAULT_CLOCK = new SystemClock();
    private static final long TIME_SLICE = 50;

    /** Clock for polling. */
    public interface PollingCheckClock {
        long currentTimeMillis();

        default void sleep(long millis) throws InterruptedException {
            Thread.sleep(millis);
        }
    }

    private static class SystemClock implements PollingCheckClock {
        @Override
        public long currentTimeMillis() {
            return System.currentTimeMillis();
        }
    }

    /**
     * Repeatedly check a condition.
     *
     * @param clock Clock used for checking time and sleeping between checks
     * @param pollInterval Time interval to wait between checks
     * @param timeout Timeout after which to stop checking
     * @param condition Condition to check
     * @return {@code true} if the condition became true within the timeout, {@code false} otherwise
     * @throws Exception
     */
    public static boolean check(
            PollingCheckClock clock, long pollInterval, long timeout, Callable<Boolean> condition)
            throws Exception {
        long start = clock.currentTimeMillis();

        if (condition.call()) {
            return true;
        }

        while ((clock.currentTimeMillis() - start) < timeout) {
            clock.sleep(pollInterval);

            if (condition.call()) {
                return true;
            }
        }

        return false;
    }

    /**
     * Repeatedly check a condition. Uses the default clock and polling interval.
     *
     * @param timeout Timeout after which to stop checking
     * @param condition Condition to check
     * @return {@code true} if the condition became true within the timeout, {@code false} otherwise
     * @throws Exception
     */
    public static boolean check(long timeout, Callable<Boolean> condition) throws Exception {
        return check(DEFAULT_CLOCK, TIME_SLICE, timeout, condition);
    }

    /**
     * Repeatedly check a condition. Throws an {@link AssertionError} if the condition does not
     * become {@code true} within the timeout.
     *
     * @param clock Clock used for checking time and sleeping between checks
     * @param pollInterval Time interval to wait between checks
     * @param timeout Timeout after which to stop checking
     * @param condition Condition to check
     * @throws Exception
     */
    public static void check(
            PollingCheckClock clock,
            String message,
            long pollInterval,
            long timeout,
            Callable<Boolean> condition)
            throws Exception {
        if (!check(clock, pollInterval, timeout, condition)) {
            fail(message);
        }
    }

    /**
     * Repeatedly check a condition. Uses the default clock and polling interval. Throws an {@link
     * AssertionError} if the condition does not become {@code true} within the timeout.
     *
     * @param timeout Timeout after which to stop checking
     * @param condition Condition to check
     * @throws Exception
     */
    public static void check(String message, long timeout, Callable<Boolean> condition)
            throws Exception {
        check(DEFAULT_CLOCK, message, TIME_SLICE, timeout, condition);
    }

    /**
     * Waits for a condition to become {@code true}. Throws an {@link AssertionError} if the
     * condition does not become {@code true} within the timeout.
     *
     * @param clock Clock used for checking time and sleeping between checks
     * @param pollInterval Time interval to wait between checks
     * @param timeout Timeout after which to stop checking
     * @param condition Condition to check
     * @throws Exception
     */
    public static void waitFor(
            PollingCheckClock clock,
            long pollInterval,
            long timeout,
            final Callable<Boolean> condition)
            throws Exception {
        check(clock, "Unexpected timeout waiting for condition", pollInterval, timeout, condition);
    }

    /**
     * Waits for a condition to become {@code true}. Uses the default clock and polling interval.
     * Throws an {@link AssertionError} if the condition does not become {@code true} within the
     * timeout.
     *
     * @param timeout Timeout after which to stop checking
     * @param condition Condition to check
     * @throws Exception
     */
    public static void waitFor(long timeout, final Callable<Boolean> condition) throws Exception {
        waitFor(DEFAULT_CLOCK, TIME_SLICE, timeout, condition);
    }
}
