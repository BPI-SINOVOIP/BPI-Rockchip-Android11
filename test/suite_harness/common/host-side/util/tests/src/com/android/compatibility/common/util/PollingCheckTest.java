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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Tests for {@link PollingCheck}. */
@RunWith(JUnit4.class)
public class PollingCheckTest {
    private PollingCheck.PollingCheckClock testClock;

    @Before
    public void setUp() {
        testClock = new TestClock();
    }

    @Test
    public void testCheckSuccess() throws Exception {
        assertTrue("check failed", PollingCheck.check(testClock, 10, 100, () -> true));
    }

    @Test
    public void testCheckEventualSuccess() throws Exception {
        final int[] i = {0};
        assertTrue("check failed", PollingCheck.check(testClock, 10, 1000, () -> ++i[0] == 3));
        assertEquals("Condition expected to be checked three times", 3, i[0]);
        assertEquals("Time advanced unexpectedly", 20, testClock.currentTimeMillis());
    }

    @Test
    public void testCheckTimeout() throws Exception {
        final int[] i = {0};
        assertFalse(
                "Expected failure due to timeout",
                PollingCheck.check(testClock, 10, 50, () -> i[0]++ == 9));
    }

    @Test
    public void testCheckFailure() throws Exception {
        assertFalse(
                "Expected failure due to condition not being true",
                PollingCheck.check(testClock, 10, 100, () -> false));
    }

    @Test
    public void testCheckChecksConditionAtLeastOnce() throws Exception {
        final boolean[] conditionChecked = {false};
        PollingCheck.check(
                testClock,
                10,
                0,
                () -> {
                    conditionChecked[0] = true;
                    return true;
                });

        assertTrue("Expected condition to be checked", conditionChecked[0]);
    }

    @Test
    public void testLongConditionCheck() throws Exception {
        PollingCheck.check(
                testClock,
                10,
                500,
                () -> {
                    testClock.sleep(5); // condition takes some time to evaluate
                    return testClock.currentTimeMillis() >= 50;
                });

        assertEquals(50, testClock.currentTimeMillis());
    }

    @Test
    public void testCheckMessage() throws Exception {
        try {
            PollingCheck.check(testClock, "Expected message", 10, 0, () -> false);
        } catch (AssertionError e) {
            assertEquals("Expected message", e.getMessage());
        }
    }

    @Test
    public void testWaitForTimeout() throws Exception {
        try {
            PollingCheck.waitFor(testClock, 10, 500, () -> false);
        } catch (AssertionError e) {
            assertEquals("Unexpected timeout waiting for condition", e.getMessage());
        }

        assertEquals(500, testClock.currentTimeMillis());
    }

    @Test
    public void testWaitForSuccess() throws Exception {
        PollingCheck.waitFor(testClock, 10, 500, () -> testClock.currentTimeMillis() >= 200);
        assertEquals(200, testClock.currentTimeMillis());
    }

    private final class TestClock implements PollingCheck.PollingCheckClock {
        private long currentTime = 0;

        @Override
        public long currentTimeMillis() {
            return currentTime;
        }

        @Override
        public void sleep(long millis) {
            currentTime += millis;
        }
    }
}
