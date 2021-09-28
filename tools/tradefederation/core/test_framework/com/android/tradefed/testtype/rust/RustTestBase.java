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

import com.android.ddmlib.IShellOutputReceiver;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.ITestFilterReceiver;

import com.google.common.annotations.VisibleForTesting;

import java.text.ParseException;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Base class of RustBinaryHostTest and RustBinaryTest */
@OptionClass(alias = "rust-test")
public abstract class RustTestBase implements IRemoteTest, ITestFilterReceiver {

    private static final Pattern TEST_LIST_PATTERN =
            Pattern.compile("(\\d+) tests?, (\\d+) benchmarks?");

    @Option(
            name = "test-options",
            description = "Option string to be passed to the binary when running")
    protected List<String> mTestOptions = new ArrayList<>();

    @Option(
            name = "test-timeout",
            description = "Timeout for a single test file to terminate.",
            isTimeVal = true)
    protected long mTestTimeout = 20 * 1000L; // milliseconds

    @Option(
            name = "include-filter",
            description = "A substr filter of the test names to run; only the first one is used.")
    private Set<String> mIncludeFilters = new LinkedHashSet<>();

    @Option(name = "exclude-filter", description = "A substr filter of the test names to skip.")
    private Set<String> mExcludeFilters = new LinkedHashSet<>();

    // A wrapper that can be redefined in unit tests to create a (mocked) result parser.
    @VisibleForTesting
    IShellOutputReceiver createParser(ITestInvocationListener listener, String runName) {
        return new RustTestResultParser(listener, runName);
    }

    /**
     * Parse and return the number of tests from the list of tests from a Rust test harness.
     *
     * @param testList the output of the Rust test list (output of `test_binary --list`).
     */
    protected static int parseTestListCount(String[] testList) throws ParseException {
        // Get the number of tests we are running, assuming this is a standard
        // rust test harness. If this isn't, this command will fail and we will
        // report 0 tests, which is fine.
        int testCount = 0;
        if (testList.length > 0) {
            Matcher matcher = TEST_LIST_PATTERN.matcher(testList[testList.length - 1]);
            if (matcher.matches()) {
                testCount = Integer.parseInt(matcher.group(1));
            } else {
                throw new ParseException(
                        "Could not match total test/benchmark count output. "
                                + "Does this test use the standard Rust test harness?",
                        0);
            }
        } else {
            throw new ParseException(
                    "Test did not return any output with --list argument. "
                            + "Does this test use the standard Rust test harness?",
                    0);
        }
        return testCount;
    }

    /** {@inheritDoc} */
    @Override
    public void addIncludeFilter(String filter) {
        mIncludeFilters.add(filter);
    }

    /** {@inheritDoc} */
    @Override
    public void addExcludeFilter(String filter) {
        mExcludeFilters.add(filter);
    }

    /** {@inheritDoc} */
    @Override
    public void addAllIncludeFilters(Set<String> filters) {
        mIncludeFilters.addAll(filters);
    }

    /** {@inheritDoc} */
    @Override
    public void addAllExcludeFilters(Set<String> filters) {
        mExcludeFilters.addAll(filters);
    }

    /** {@inheritDoc} */
    @Override
    public void clearIncludeFilters() {
        mIncludeFilters.clear();
    }

    /** {@inheritDoc} */
    @Override
    public void clearExcludeFilters() {
        mExcludeFilters.clear();
    }

    /** {@inheritDoc} */
    @Override
    public Set<String> getIncludeFilters() {
        return mIncludeFilters;
    }

    /** {@inheritDoc} */
    @Override
    public Set<String> getExcludeFilters() {
        return mExcludeFilters;
    }

    private void checkMultipleIncludeFilters() {
        if (mIncludeFilters.size() > 1) {
            CLog.e("Found multiple include filters; all except the 1st are ignored.");
        }
    }

    protected void addFiltersToArgs(List<String> args) {
        checkMultipleIncludeFilters();
        for (String s : mIncludeFilters) {
            args.add(s);
        }
        for (String s : mExcludeFilters) {
            args.add("--skip");
            args.add(s);
        }
    }

    protected String addFiltersToCommand(String cmd) {
        if (!mIncludeFilters.isEmpty()) {
            checkMultipleIncludeFilters();
            cmd += " " + String.join(" ", mIncludeFilters);
        }
        if (!mExcludeFilters.isEmpty()) {
            cmd += " --skip " + String.join(" --skip ", mExcludeFilters);
        }
        return cmd;
    }
}
