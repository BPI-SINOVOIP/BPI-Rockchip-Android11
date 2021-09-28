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

package android.security.cts;

import java.util.concurrent.TimeoutException;
import java.util.regex.Pattern;
import java.util.regex.Matcher;
import com.android.ddmlib.Log.LogLevel;
import com.android.tradefed.log.LogUtil.CLog;

import static org.junit.Assert.*;

public class RegexUtils {
    private static final int TIMEOUT_DURATION = 20 * 60_000; // 20 minutes
    private static final int WARNING_THRESHOLD = 1000; // 1 second
    private static final int CONTEXT_RANGE = 100; // chars before/after matched input string

    public static void assertContains(String pattern, String input) throws Exception {
        assertFind(pattern, input, false, false);
    }

    public static void assertContainsMultiline(String pattern, String input) throws Exception {
        assertFind(pattern, input, false, true);
    }

    public static void assertNotContains(String pattern, String input) throws Exception {
        assertFind(pattern, input, true, false);
    }

    public static void assertNotContainsMultiline(String pattern, String input) throws Exception {
        assertFind(pattern, input, true, true);
    }

    private static void assertFind(
            String pattern, String input, boolean shouldFind, boolean multiline) {
        // The input string throws an error when used after the timeout
        TimeoutCharSequence timedInput = new TimeoutCharSequence(input, TIMEOUT_DURATION);
        Matcher matcher = null;
        if (multiline) {
            // DOTALL lets .* match line separators
            // MULTILINE lets ^ and $ match line separators instead of input start and end
            matcher = Pattern.compile(
                    pattern, Pattern.DOTALL|Pattern.MULTILINE).matcher(timedInput);
        } else {
            matcher = Pattern.compile(pattern).matcher(timedInput);
        }

        try {
            long start = System.currentTimeMillis();
            boolean found = matcher.find();
            long duration = System.currentTimeMillis() - start;

            if (duration > WARNING_THRESHOLD) {
                // Provide a warning to the test developer that their regex should be optimized.
                CLog.logAndDisplay(LogLevel.WARN, "regex match took " + duration + "ms.");
            }

            if (found && shouldFind) { // failed notContains
                String substring = input.substring(matcher.start(), matcher.end());
                String context = getInputContext(input, matcher.start(), matcher.end(),
                        CONTEXT_RANGE, CONTEXT_RANGE);
                fail("Pattern found: '" + pattern + "' -> '" + substring + "' for input:\n..." +
                        context + "...");
            } else if (!found && !shouldFind) { // failed contains
                fail("Pattern not found: '" + pattern + "' for input:\n..." + input + "...");
            }
        } catch (TimeoutCharSequence.CharSequenceTimeoutException e) {
            // regex match has taken longer than the timeout
            // this usually means the input is extremely long or the regex is catastrophic
            fail("Regex timeout with pattern: '" + pattern + "' for input:\n..." + input + "...");
        }
    }

    /*
     * Helper method to grab the nearby chars for a subsequence. Similar to the -A and -B flags for
     * grep.
     */
    private static String getInputContext(String input, int start, int end, int before, int after) {
        start = Math.max(0, start - before);
        end = Math.min(input.length(), end + after);
        return input.substring(start, end);
    }

    /*
     * Wrapper for a given CharSequence. When charAt() is called, the current time is compared
     * against the timeout. If the current time is greater than the expiration time, an exception is
     * thrown. The expiration time is (time of object construction) + (timeout in milliseconds).
     */
    private static class TimeoutCharSequence implements CharSequence {
        long expireTime = 0;
        CharSequence chars = null;

        TimeoutCharSequence(CharSequence chars, long timeout) {
            this.chars = chars;
            expireTime = System.currentTimeMillis() + timeout;
        }

        @Override
        public char charAt(int index) {
            if (System.currentTimeMillis() > expireTime) {
                throw new CharSequenceTimeoutException(
                        "TimeoutCharSequence was used after the expiration time.");
            }
            return chars.charAt(index);
        }

        @Override
        public int length() {
            return chars.length();
        }

        @Override
        public CharSequence subSequence(int start, int end) {
            return new TimeoutCharSequence(chars.subSequence(start, end),
                    expireTime - System.currentTimeMillis());
        }

        @Override
        public String toString() {
            return chars.toString();
        }

        private static class CharSequenceTimeoutException extends RuntimeException {
            public CharSequenceTimeoutException(String message) {
                super(message);
            }
        }
    }
}
