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
package com.android.tradefed.testtype.rust;

import com.android.ddmlib.MultiLineReceiver;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Interprets the output of tests run with Rust's unittest framework and translates it into calls on
 * a series of {@link ITestInvocationListener}s.
 *
 * <p>Looks for the following output from Rust tests:
 *
 * <p><code>
 * running 10 tests
 * test LexError ... ok
 * test idents ... FAILED
 * test make_sure_no_proc_macro ... ignored
 * ...
 *
 * test result: ok. 10 passed; 1 failed; 1 ignored; 0 measured; 0 filtered out
 * </code> @See <a href="Rust test output
 * examples">https://doc.rust-lang.org/book/ch11-00-testing.html</a>
 */
public class RustTestResultParser extends MultiLineReceiver {

    private String mCurrentTestFile;
    private String mCurrentTestName;
    private String mCurrentTestStatus;
    private Matcher mCurrentMatcher;

    // General state
    private Collection<ITestInvocationListener> mListeners = new ArrayList<>();
    private Map<TestDescription, String> mTestResultCache;
    // Use a special entry to mark skipped test in mTestResultCache
    static final String SKIPPED_ENTRY = "Skipped";
    // Failed but without stacktrace tests in mTestResultCache
    static final String FAILED_ENTRY = "FailedNoStack";

    static final Pattern COMPLETE_PATTERN =
            Pattern.compile("test result: (.*) (\\d+) passed; (\\d+) failed; (\\d+) ignored;.*");

    static final Pattern RUST_ONE_LINE_RESULT = Pattern.compile("test (\\S*) \\.\\.\\. (\\S*)");

    /**
     * Create a new {@link RustTestResultParser} that reports to the given {@link
     * ITestInvocationListener}.
     *
     * @param listener the test invocation listener
     * @param runName the test name
     */
    public RustTestResultParser(ITestInvocationListener listener, String runName) {
        this(Arrays.asList(listener), runName);
    }

    /**
     * Create a new {@link RustTestResultParser} that reports to the given {@link
     * ITestInvocationListener}s.
     *
     * @param listeners the test invocation listeners
     * @param runName the test name
     */
    public RustTestResultParser(Collection<ITestInvocationListener> listeners, String runName) {
        mListeners.addAll(listeners);
        mCurrentTestFile = runName;
        mTestResultCache = new HashMap<>();
    }

    /**
     * Process Rust unittest output and report parsed results.
     *
     * <p>This method should be called only once with the full output, unlike the base method in
     * {@link MultiLineReceiver}.
     */
    @Override
    public void processNewLines(String[] lines) {
        try {
            boolean hasContent = false;
            for (String line : lines) {
                hasContent |= line.length() > 0;
                if (lineMatchesPattern(line, COMPLETE_PATTERN)) {
                    reportToListeners(line);
                    return;
                }
                if (lineMatchesPattern(line, RUST_ONE_LINE_RESULT)) {
                    mCurrentTestName = mCurrentMatcher.group(1);
                    mCurrentTestStatus = mCurrentMatcher.group(2);
                    reportTestResult();
                }
            }
            if (hasContent) {
                throw new Exception(
                        String.format("Missing summary line:\n%s\n", String.join("\n", lines)));
            }
        } catch (Exception e) {
            throw new RuntimeException("Failed to parse Rust test result\n" + e);
        }
    }

    /** Process the run summary line and return total test count. */
    int processRunSummary(String line) {
        try {
            if (!lineMatchesPattern(line, COMPLETE_PATTERN)) {
                throw new RuntimeException("Failed to parse summary line: " + line);
            }
            int passed = Integer.parseInt(mCurrentMatcher.group(2));
            int failed = Integer.parseInt(mCurrentMatcher.group(3));
            int ignored = Integer.parseInt(mCurrentMatcher.group(4));
            return passed + failed + ignored;
        } catch (NumberFormatException e) {
            // this should never happen, since regular expression matches on digits
            throw new RuntimeException("Failed to parse number in " + line);
        }
    }

    /** Check if the given line matches the given pattern and caches the matcher object */
    private boolean lineMatchesPattern(String line, Pattern p) {
        mCurrentMatcher = p.matcher(line);
        return mCurrentMatcher.matches();
    }

    /** Send recorded test results to all listeners. */
    private void reportToListeners(String line) {
        for (ITestInvocationListener listener : mListeners) {
            for (Entry<TestDescription, String> test : mTestResultCache.entrySet()) {
                listener.testStarted(test.getKey());
                if (SKIPPED_ENTRY.equals(test.getValue())) {
                    listener.testIgnored(test.getKey());
                } else if (FAILED_ENTRY.equals(test.getValue())) {
                    listener.testFailed(test.getKey(), ""); // no stacktrace
                } else if (test.getValue() != null) {
                    // Report all unexpected test result as failed tests,
                    // so they are not missed.
                    listener.testFailed(test.getKey(), test.getValue());
                }
                listener.testEnded(test.getKey(), new HashMap<String, Metric>());
            }
        }
    }

    /** Record a test case. */
    private void reportTestResult() {
        TestDescription testId = new TestDescription(mCurrentTestFile, mCurrentTestName);
        if (mCurrentTestStatus.equals("ok")) {
            mTestResultCache.put(testId, null);
        } else if (mCurrentTestStatus.equals("ignored")) {
            mTestResultCache.put(testId, SKIPPED_ENTRY);
        } else if (mCurrentTestStatus.equals("FAILED")) {
            // Rust tests report "FAILED" without stack trace.
            mTestResultCache.put(testId, FAILED_ENTRY);
        } else {
            mTestResultCache.put(testId, mCurrentTestStatus);
        }
    }

    @Override
    public boolean isCancelled() {
        return false;
    }
}
