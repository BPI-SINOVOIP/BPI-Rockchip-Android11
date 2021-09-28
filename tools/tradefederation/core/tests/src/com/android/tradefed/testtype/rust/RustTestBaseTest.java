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

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import com.android.tradefed.testtype.ITestFilterReceiver;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.text.ParseException;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/** Unit tests for {@link RustTestBase}. */
@RunWith(JUnit4.class)
public class RustTestBaseTest extends RustParserTestBase {

    /** Test a child class of {@link RustTestBase}. */
    static class RustTestBaseImpl extends RustTestBase {}

    /** Test include filters. */
    @Test
    public void testIncludeFilters() throws Exception {
        ITestFilterReceiver runner = new RustTestBaseImpl();
        String[] s = {"test*filter1", "s1", "*s2", "s3*"};
        Set<String> set0 = new HashSet<>();
        Set<String> set1 = new HashSet<>(Arrays.asList(s[0]));
        Set<String> set2 = new HashSet<>(Arrays.asList(s[1], s[2], s[3]));
        assertEquals(set0, runner.getIncludeFilters());
        runner.addIncludeFilter(s[0]);
        assertEquals(set1, runner.getIncludeFilters());
        runner.clearIncludeFilters();
        assertEquals(set0, runner.getIncludeFilters());
        runner.addIncludeFilter(s[2]);
        runner.addAllIncludeFilters(set2);
        assertEquals(set2, runner.getIncludeFilters());
        assertEquals(set0, runner.getExcludeFilters());
        runner.clearIncludeFilters();
        assertEquals(set0, runner.getIncludeFilters());
    }

    /** Test exclude filters. */
    @Test
    public void testExcludeFilters() throws Exception {
        ITestFilterReceiver runner = new RustTestBaseImpl();
        String[] s = {"test*filter1", "s1", "*s2", "s3*"};
        Set<String> set0 = new HashSet<>();
        Set<String> set1 = new HashSet<>(Arrays.asList(s[0]));
        Set<String> set2 = new HashSet<>(Arrays.asList(s[1], s[2], s[3]));
        assertEquals(set0, runner.getExcludeFilters());
        runner.addExcludeFilter(s[0]);
        assertEquals(set1, runner.getExcludeFilters());
        runner.clearExcludeFilters();
        assertEquals(set0, runner.getExcludeFilters());
        runner.addExcludeFilter(s[2]);
        runner.addAllExcludeFilters(set2);
        assertEquals(set2, runner.getExcludeFilters());
        assertEquals(set0, runner.getIncludeFilters());
        runner.clearExcludeFilters();
        assertEquals(set0, runner.getExcludeFilters());
    }

    /** Test parsing of various forms of Rust test list summary output. */
    @Test
    public void testParseTestListSummaries() throws Exception {
        assertEquals(0, RustTestBase.parseTestListCount(new String[] {"0 tests, 0 benchmarks"}));
        assertEquals(1, RustTestBase.parseTestListCount(new String[] {"1 test, 0 benchmarks"}));
        assertEquals(0, RustTestBase.parseTestListCount(new String[] {"0 tests, 1 benchmark"}));
        assertEquals(2, RustTestBase.parseTestListCount(new String[] {"2 tests, 0 benchmarks"}));
        assertEquals(2, RustTestBase.parseTestListCount(new String[] {"2 tests, 1 benchmark"}));
        assertEquals(2, RustTestBase.parseTestListCount(new String[] {"2 tests, 2 benchmarks"}));
    }

    /** Test parsing of a real Rust test list. */
    @Test
    public void testParseTestListRealOutput() throws Exception {
        String[] contents = readInFile(RUST_LIST_FILE_1);
        assertEquals(42, RustTestBase.parseTestListCount(contents));
    }

    /** Test parsing of an empty Rust test list. */
    @Test
    public void testParseTestListEmpty() throws Exception {
        try {
            RustTestBase.parseTestListCount(new String[] {});
            fail("Should have thrown an exception.");
        } catch (ParseException e) {
            assertThat(e).hasMessageThat().contains("Test did not return any output");
        }
    }

    /** Test parsing of a malformed or non-standard Rust test list. */
    @Test
    public void testParseTestListMalformed() throws Exception {
        String[] contents = {
            "some other output", "that does not contain a summary line",
        };
        try {
            RustTestBase.parseTestListCount(contents);
            fail("Should have thrown an exception.");
        } catch (ParseException e) {
            assertThat(e).hasMessageThat().contains("Could not match total");
        }
    }
}
