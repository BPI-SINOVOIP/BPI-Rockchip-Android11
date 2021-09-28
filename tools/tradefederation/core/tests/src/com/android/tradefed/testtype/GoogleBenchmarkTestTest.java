/*
 * Copyright (C) 2016 The Android Open Source Project
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
package com.android.tradefed.testtype;

import com.android.tradefed.device.CollectingOutputReceiver;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.MockFileUtil;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;

import junit.framework.TestCase;

import org.easymock.EasyMock;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Unit tests for {@link GoogleBenchmarkTest}.
 */
public class GoogleBenchmarkTestTest extends TestCase {

    private ITestInvocationListener mMockInvocationListener = null;
    private CollectingOutputReceiver mMockReceiver = null;
    private ITestDevice mMockITestDevice = null;
    private GoogleBenchmarkTest mGoogleBenchmarkTest;
    private TestInformation mTestInfo;
    private TestDescription mDummyTest;

    /**
     * Helper to initialize the various EasyMocks we'll need.
     */
    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mMockInvocationListener = EasyMock.createMock(ITestInvocationListener.class);
        mMockReceiver = new CollectingOutputReceiver();
        mMockITestDevice = EasyMock.createMock(ITestDevice.class);
        mDummyTest = new TestDescription("Class", "method");
        EasyMock.expect(mMockITestDevice.getSerialNumber()).andStubReturn("serial");
        mGoogleBenchmarkTest =
                new GoogleBenchmarkTest() {
                    @Override
                    CollectingOutputReceiver createOutputCollector() {
                        return mMockReceiver;
                    }

                    @Override
                    GoogleBenchmarkResultParser createResultParser(
                            String runName, ITestInvocationListener listener) {
                        return new GoogleBenchmarkResultParser(runName, listener) {
                            @Override
                            public Map<String, String> parse(CollectingOutputReceiver output) {
                                listener.testStarted(mDummyTest);
                                listener.testEnded(mDummyTest, Collections.emptyMap());
                                return Collections.emptyMap();
                            }
                        };
                    }
                };
        mGoogleBenchmarkTest.setDevice(mMockITestDevice);
        mTestInfo = TestInformation.newBuilder().build();
    }

    /**
     * Helper that replays all mocks.
     */
    private void replayMocks() {
      EasyMock.replay(mMockInvocationListener, mMockITestDevice);
    }

    /**
     * Helper that verifies all mocks.
     */
    private void verifyMocks() {
      EasyMock.verify(mMockInvocationListener, mMockITestDevice);
    }

    /**
     * Test the run method for a couple tests
     */
    public void testRun() throws DeviceNotAvailableException {
        final String nativeTestPath = GoogleBenchmarkTest.DEFAULT_TEST_PATH;
        final String test1 = "test1";
        final String test2 = "test2";
        MockFileUtil.setMockDirContents(mMockITestDevice, nativeTestPath, test1, test2);
        EasyMock.expect(mMockITestDevice.doesFileExist(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath + "/test1")).andReturn(false);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath + "/test2")).andReturn(false);
        String[] files = new String[] {"test1", "test2"};
        EasyMock.expect(mMockITestDevice.getChildren(nativeTestPath)).andReturn(files);
        EasyMock.expect(mMockITestDevice.executeShellCommand(EasyMock.contains("chmod")))
                .andReturn("").times(2);
        mMockITestDevice.executeShellCommand(EasyMock.contains(test1), EasyMock.same(mMockReceiver),
                EasyMock.anyLong(), (TimeUnit)EasyMock.anyObject(), EasyMock.anyInt());
        mMockITestDevice.executeShellCommand(EasyMock.contains(test2), EasyMock.same(mMockReceiver),
                EasyMock.anyLong(), (TimeUnit)EasyMock.anyObject(), EasyMock.anyInt());

        EasyMock.expect(
                        mMockITestDevice.executeShellCommand(
                                String.format(
                                        "%s/test1 --benchmark_list_tests=true", nativeTestPath)))
                .andReturn("method1\nmethod2\nmethod3");

        EasyMock.expect(
                        mMockITestDevice.executeShellCommand(
                                String.format(
                                        "%s/test2 --benchmark_list_tests=true", nativeTestPath)))
                .andReturn("method1\nmethod2\n");
        mMockInvocationListener.testRunStarted(test1, 3);
        mMockInvocationListener.testStarted(mDummyTest);
        mMockInvocationListener.testEnded(
                EasyMock.eq(mDummyTest), EasyMock.<HashMap<String, String>>anyObject());
        mMockInvocationListener.testRunStarted(test2, 2);
        mMockInvocationListener.testStarted(mDummyTest);
        mMockInvocationListener.testEnded(
                EasyMock.eq(mDummyTest), EasyMock.<HashMap<String, String>>anyObject());
        mMockInvocationListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        EasyMock.expectLastCall().times(2);
        replayMocks();

        mGoogleBenchmarkTest.run(mTestInfo, mMockInvocationListener);
        verifyMocks();
    }

    /**
     * Test the run method when no device is set.
     */
    public void testRun_noDevice() throws DeviceNotAvailableException {
        mGoogleBenchmarkTest.setDevice(null);
        try {
            mGoogleBenchmarkTest.run(mTestInfo, mMockInvocationListener);
        } catch (IllegalArgumentException e) {
            assertEquals("Device has not been set", e.getMessage());
            return;
        }
        fail();
    }

    /**
     * Test the run method for a couple tests
     */
    public void testRun_noBenchmarkDir() throws DeviceNotAvailableException {
        EasyMock.expect(mMockITestDevice.doesFileExist(GoogleBenchmarkTest.DEFAULT_TEST_PATH))
                .andReturn(false);
        replayMocks();
        try {
            mGoogleBenchmarkTest.run(mTestInfo, mMockInvocationListener);
            fail("Should have thrown an exception.");
        } catch (RuntimeException e) {
            // expected
        }
        verifyMocks();
    }

    /**
     * Test the run method for a couple tests with a module name
     */
    public void testRun_withModuleName() throws DeviceNotAvailableException {
        final String moduleName = "module";
        final String nativeTestPath =
                String.format("%s/%s", GoogleBenchmarkTest.DEFAULT_TEST_PATH, moduleName);
        mGoogleBenchmarkTest.setModuleName(moduleName);
        final String test1 = "test1";
        final String test2 = "test2";
        MockFileUtil.setMockDirContents(mMockITestDevice, nativeTestPath, test1, test2);
        EasyMock.expect(mMockITestDevice.doesFileExist(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath + "/test1")).andReturn(false);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath + "/test2")).andReturn(false);
        String[] files = new String[] {"test1", "test2"};
        EasyMock.expect(mMockITestDevice.getChildren(nativeTestPath)).andReturn(files);
        EasyMock.expect(mMockITestDevice.executeShellCommand(EasyMock.contains("chmod")))
                .andReturn("").times(2);
        mMockITestDevice.executeShellCommand(EasyMock.contains(test1), EasyMock.same(mMockReceiver),
                EasyMock.anyLong(), (TimeUnit)EasyMock.anyObject(), EasyMock.anyInt());
        mMockITestDevice.executeShellCommand(EasyMock.contains(test2), EasyMock.same(mMockReceiver),
                EasyMock.anyLong(), (TimeUnit)EasyMock.anyObject(), EasyMock.anyInt());
        EasyMock.expect(
                        mMockITestDevice.executeShellCommand(
                                String.format(
                                        "%s/test1 --benchmark_list_tests=true", nativeTestPath)))
                .andReturn("\nmethod1\nmethod2\nmethod3\n\n");
        EasyMock.expect(
                        mMockITestDevice.executeShellCommand(
                                String.format(
                                        "%s/test2 --benchmark_list_tests=true", nativeTestPath)))
                .andReturn("method1\nmethod2\n");
        mMockInvocationListener.testRunStarted(test1, 3);
        mMockInvocationListener.testStarted(mDummyTest);
        mMockInvocationListener.testEnded(
                EasyMock.eq(mDummyTest), EasyMock.<HashMap<String, String>>anyObject());
        mMockInvocationListener.testRunStarted(test2, 2);
        mMockInvocationListener.testStarted(mDummyTest);
        mMockInvocationListener.testEnded(
                EasyMock.eq(mDummyTest), EasyMock.<HashMap<String, String>>anyObject());
        mMockInvocationListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        EasyMock.expectLastCall().times(2);
        replayMocks();

        mGoogleBenchmarkTest.run(mTestInfo, mMockInvocationListener);
        verifyMocks();
    }

    /**
     * Test the run method for a couple tests with a module name
     */
    public void testRun_withRunReportName() throws DeviceNotAvailableException {
        final String nativeTestPath = GoogleBenchmarkTest.DEFAULT_TEST_PATH;
        final String test1 = "test1";
        final String reportName = "reportName";
        mGoogleBenchmarkTest.setReportRunName(reportName);
        MockFileUtil.setMockDirContents(mMockITestDevice, nativeTestPath, test1);
        EasyMock.expect(mMockITestDevice.doesFileExist(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath + "/test1")).andReturn(false);
        String[] files = new String[] {"test1"};
        EasyMock.expect(mMockITestDevice.getChildren(nativeTestPath)).andReturn(files);
        EasyMock.expect(mMockITestDevice.executeShellCommand(EasyMock.contains("chmod")))
                .andReturn("");
        mMockITestDevice.executeShellCommand(EasyMock.contains(test1), EasyMock.same(mMockReceiver),
                EasyMock.anyLong(), (TimeUnit)EasyMock.anyObject(), EasyMock.anyInt());
        EasyMock.expect(
                        mMockITestDevice.executeShellCommand(
                                String.format(
                                        "%s/test1 --benchmark_list_tests=true", nativeTestPath)))
                .andReturn("method1\nmethod2\nmethod3");
        // Expect reportName instead of test name
        mMockInvocationListener.testRunStarted(reportName, 3);
        mMockInvocationListener.testStarted(mDummyTest);
        mMockInvocationListener.testEnded(
                EasyMock.eq(mDummyTest), EasyMock.<HashMap<String, String>>anyObject());
        mMockInvocationListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        EasyMock.expectLastCall();
        replayMocks();

        mGoogleBenchmarkTest.run(mTestInfo, mMockInvocationListener);
        verifyMocks();
    }

    /**
     * Test the run method when exec shell throw exeception.
     */
    public void testRun_exceptionDuringExecShell() throws DeviceNotAvailableException {
        final String nativeTestPath = GoogleBenchmarkTest.DEFAULT_TEST_PATH;
        final String test1 = "test1";
        MockFileUtil.setMockDirContents(mMockITestDevice, nativeTestPath, test1);
        EasyMock.expect(mMockITestDevice.doesFileExist(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath + "/test1")).andReturn(false);
        String[] files = new String[] {"test1"};
        EasyMock.expect(mMockITestDevice.getChildren(nativeTestPath)).andReturn(files);
        EasyMock.expect(mMockITestDevice.executeShellCommand(EasyMock.contains("chmod")))
                .andReturn("");
        mMockITestDevice.executeShellCommand(EasyMock.contains(test1), EasyMock.same(mMockReceiver),
                EasyMock.anyLong(), (TimeUnit)EasyMock.anyObject(), EasyMock.anyInt());
        EasyMock.expectLastCall().andThrow(new DeviceNotAvailableException("dnae", "serial"));
        EasyMock.expect(
                        mMockITestDevice.executeShellCommand(
                                String.format(
                                        "%s/test1 --benchmark_list_tests=true", nativeTestPath)))
                .andReturn("method1\nmethod2\nmethod3");
        mMockInvocationListener.testRunStarted(test1, 3);
        mMockInvocationListener.testRunFailed((String) EasyMock.anyObject());
        // Even with exception testrunEnded is expected.
        mMockInvocationListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        EasyMock.expectLastCall();
        replayMocks();

        try {
            mGoogleBenchmarkTest.run(mTestInfo, mMockInvocationListener);
            fail();
        } catch (DeviceNotAvailableException e) {
            // expected
        }
        verifyMocks();
    }

    /**
     * File exclusion regex filter should skip matched filepaths.
     */
    public void testFileExclusionRegexFilter_skipMatched() {
        // Skip files ending in .txt
        mGoogleBenchmarkTest.addFileExclusionFilterRegex(".*\\.txt");
        assertFalse(mGoogleBenchmarkTest.shouldSkipFile("/some/path/file/binary"));
        assertFalse(mGoogleBenchmarkTest.shouldSkipFile("/some/path/file/random.dat"));
        assertTrue(mGoogleBenchmarkTest.shouldSkipFile("/some/path/file/test.txt"));
        // Always skip files ending in .config
        assertTrue(mGoogleBenchmarkTest.shouldSkipFile("/some/path/file/random.config"));
    }

    /**
     * File exclusion regex filter for multi filters.
     */
    public void testFileExclusionRegexFilter_skipMultiMatched() {
        // Skip files ending in .txt
        mGoogleBenchmarkTest.addFileExclusionFilterRegex(".*\\.txt");
        // Also skip files ending in .dat
        mGoogleBenchmarkTest.addFileExclusionFilterRegex(".*\\.dat");
        assertFalse(mGoogleBenchmarkTest.shouldSkipFile("/some/path/file/binary"));
        assertTrue(mGoogleBenchmarkTest.shouldSkipFile("/some/path/file/random.dat"));
        assertTrue(mGoogleBenchmarkTest.shouldSkipFile("/some/path/file/test.txt"));
        // Always skip files ending in .config
        assertTrue(mGoogleBenchmarkTest.shouldSkipFile("/some/path/file/random.config"));
    }

    /** File exclusion regex filter should always skip .config file. */
    public void testFileExclusionRegexFilter_skipDefaultMatched() {
        // Always skip files ending in .config
        assertTrue(mGoogleBenchmarkTest.shouldSkipFile("/some/path/file/random.config"));
        // Other file should not be skipped
        assertFalse(mGoogleBenchmarkTest.shouldSkipFile("/some/path/file/random.configs"));
        assertFalse(mGoogleBenchmarkTest.shouldSkipFile("/some/path/file/binary"));
        assertFalse(mGoogleBenchmarkTest.shouldSkipFile("/some/path/file/random.dat"));
        assertFalse(mGoogleBenchmarkTest.shouldSkipFile("/some/path/file/test.txt"));

    }

    /** Test getFilterFlagForFilters. */
    public void testGetFilterFlagForFilters() {
        Set<String> filters = new LinkedHashSet<>(Arrays.asList("filter1", "filter2"));
        String filterFlag = mGoogleBenchmarkTest.getFilterFlagForFilters(filters);
        assertEquals(
                String.format(
                        " %s=%s",
                        GoogleBenchmarkTest.GBENCHMARK_FILTER_OPTION, "filter1\\|filter2"),
                filterFlag);
    }

    /** Test getFilterFlagForFilters - no filters. */
    public void testGetFilterFlagForFilters_noFilters() {
        Set<String> filters = new LinkedHashSet<>();
        String filterFlag = mGoogleBenchmarkTest.getFilterFlagForFilters(filters);
        assertEquals("", filterFlag);
    }

    /** Test getFilterFlagForTests. */
    public void testGetFilterFlagForTests() {
        Set<String> tests = new LinkedHashSet<>(Arrays.asList("test1", "test2"));
        String filterFlag = mGoogleBenchmarkTest.getFilterFlagForTests(tests);
        assertEquals(
                String.format(
                        " %s=%s",
                        GoogleBenchmarkTest.GBENCHMARK_FILTER_OPTION, "^test1$\\|^test2$"),
                filterFlag);
    }

    /** Test getFilterFlagForTests - no tests. */
    public void testGetFilterFlagForTests_noFilters() {
        Set<String> tests = new LinkedHashSet<>();
        String filterFlag = mGoogleBenchmarkTest.getFilterFlagForTests(tests);
        assertEquals("", filterFlag);
    }

    /**
     * Helper function to do the actual filtering test.
     *
     * @param incFilter filter flag for querying tests to include
     * @param incTests tests to include
     * @param excFilter filter flag for querying tests to exclude
     * @param excTests tests to exclude
     * @param testFilter filter flag for running tests
     * @throws DeviceNotAvailableException
     */
    private void doTestFilter(String incTests, String excTests, Set<String> filteredTests)
            throws DeviceNotAvailableException {
        String nativeTestPath = GoogleBenchmarkTest.DEFAULT_TEST_PATH;
        String testPath = nativeTestPath + "/test1";
        // configure the mock file system to have a single test
        MockFileUtil.setMockDirContents(mMockITestDevice, nativeTestPath, "test1");
        EasyMock.expect(mMockITestDevice.doesFileExist(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(testPath)).andReturn(false);
        EasyMock.expect(mMockITestDevice.executeShellCommand(EasyMock.contains("chmod")))
                .andReturn("");
        String[] files = new String[] {"test1"};
        EasyMock.expect(mMockITestDevice.getChildren(nativeTestPath)).andReturn(files);

        // List tests to include
        if (mGoogleBenchmarkTest.getIncludeFilters().size() > 0) {
            String incFilterFlag =
                    mGoogleBenchmarkTest.getFilterFlagForFilters(
                            mGoogleBenchmarkTest.getIncludeFilters());
            EasyMock.expect(mMockITestDevice.executeShellCommand(EasyMock.contains(incFilterFlag)))
                    .andReturn(incTests);
        } else {
            EasyMock.expect(
                            mMockITestDevice.executeShellCommand(
                                    EasyMock.not(
                                            EasyMock.contains(
                                                    GoogleBenchmarkTest.GBENCHMARK_FILTER_OPTION))))
                    .andReturn(incTests);
        }
        if (mGoogleBenchmarkTest.getExcludeFilters().size() > 0) {
            // List tests to exclude
            String excFilterFlag =
                    mGoogleBenchmarkTest.getFilterFlagForFilters(
                            mGoogleBenchmarkTest.getExcludeFilters());
            EasyMock.expect(mMockITestDevice.executeShellCommand(EasyMock.contains(excFilterFlag)))
                    .andReturn(excTests);
        }
        if (filteredTests != null && filteredTests.size() > 0) {
            // Runningt filtered tests
            String testFilterFlag = mGoogleBenchmarkTest.getFilterFlagForTests(filteredTests);
            mMockITestDevice.executeShellCommand(
                    EasyMock.contains(testFilterFlag),
                    EasyMock.same(mMockReceiver),
                    EasyMock.anyLong(),
                    (TimeUnit) EasyMock.anyObject(),
                    EasyMock.anyInt());
            mMockInvocationListener.testRunStarted("test1", filteredTests.size());
            mMockInvocationListener.testStarted(mDummyTest);
            mMockInvocationListener.testEnded(
                    EasyMock.eq(mDummyTest), EasyMock.<HashMap<String, String>>anyObject());
            mMockInvocationListener.testRunEnded(
                    EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
            EasyMock.expectLastCall();
        }
        replayMocks();

        mGoogleBenchmarkTest.run(mTestInfo, mMockInvocationListener);
        verifyMocks();
    }

    /** Test no matching tests for the filters. */
    public void testNoMatchingTests() throws DeviceNotAvailableException {
        Set<String> incFilters = new LinkedHashSet<>(Arrays.asList("X", "Y"));
        String incTests = "Failed to match any benchmarks against regex: X|Y";

        mGoogleBenchmarkTest.addAllIncludeFilters(incFilters);
        doTestFilter(incTests, null /* excTests */, null /* testFilter */);
    }

    /** Test the include filtering of test methods. */
    public void testIncludeFilter() throws DeviceNotAvailableException {
        Set<String> incFilters = new LinkedHashSet<>(Arrays.asList("A", "B"));
        String incTests = "A\nAa\nB\nBb";
        Set<String> filteredTests = new LinkedHashSet<>(Arrays.asList("A", "Aa", "B", "Bb"));

        mGoogleBenchmarkTest.addAllIncludeFilters(incFilters);
        doTestFilter(incTests, null /* excTests */, filteredTests);
    }

    /** Test the exclude filtering of test methods. */
    public void testExcludeFilter() throws DeviceNotAvailableException {
        String incTests = "A\nAa\nB\nBb\nC\nCc";
        Set<String> excFilters = new LinkedHashSet<>(Arrays.asList("Bb", "C"));
        String excTests = "Bb\nC\nCc";
        Set<String> filteredTests = new LinkedHashSet<>(Arrays.asList("A", "Aa", "B"));

        mGoogleBenchmarkTest.addAllExcludeFilters(excFilters);
        doTestFilter(incTests, excTests, filteredTests);
    }

    /** Test the include & exclude filtering of test methods. */
    public void testIncludeAndExcludeFilter() throws DeviceNotAvailableException {
        Set<String> incFilters = new LinkedHashSet<>(Arrays.asList("A", "B"));
        String incTests = "A\nAa\nB\nBb";
        Set<String> excFilters = new LinkedHashSet<>(Arrays.asList("Bb", "C"));
        String excTests = "Bb\nC\nCc";
        Set<String> filteredTests = new LinkedHashSet<>(Arrays.asList("A", "Aa", "B"));

        mGoogleBenchmarkTest.addAllIncludeFilters(incFilters);
        mGoogleBenchmarkTest.addAllExcludeFilters(excFilters);
        doTestFilter(incTests, excTests, filteredTests);
    }

    /** Test the ITestDescription filter format "class#method". */
    public void testClearFilter() throws DeviceNotAvailableException {
        Set<String> incFilters = new LinkedHashSet<>(Arrays.asList("X#A", "X#B"));
        Set<String> expectedIncFilters = new LinkedHashSet<>(Arrays.asList("A", "B"));
        mGoogleBenchmarkTest.addAllIncludeFilters(incFilters);
        assertEquals(expectedIncFilters, mGoogleBenchmarkTest.getIncludeFilters());
    }

    /** Test behavior for command lines too long to be run by ADB */
    public void testCommandTooLong() throws DeviceNotAvailableException {
        String deviceScriptPath = "/data/local/tmp/gbenchmarktest_script.sh";
        StringBuilder testNameBuilder = new StringBuilder();
        for (int i = 0; i < GoogleBenchmarkTest.ADB_CMD_CHAR_LIMIT; i++) {
            testNameBuilder.append("a");
        }
        String testName = testNameBuilder.toString();
        // filter string will be longer than GTest.ADB_CMD_CHAR_LIMIT

        String nativeTestPath = GoogleBenchmarkTest.DEFAULT_TEST_PATH;
        String testPath = nativeTestPath + "/" + testName;
        // configure the mock file system to have a single test
        MockFileUtil.setMockDirContents(mMockITestDevice, nativeTestPath, "test1");
        EasyMock.expect(mMockITestDevice.doesFileExist(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(testPath)).andReturn(false);
        EasyMock.expect(mMockITestDevice.executeShellCommand(EasyMock.contains("chmod")))
                .andReturn("");
        String[] files = new String[] {testName};
        EasyMock.expect(mMockITestDevice.getChildren(nativeTestPath)).andReturn(files);
        // List tests
        EasyMock.expect(
                        mMockITestDevice.pushString(
                                EasyMock.<String>anyObject(), EasyMock.eq(deviceScriptPath)))
                .andReturn(Boolean.TRUE);
        EasyMock.expect(
                        mMockITestDevice.executeShellCommand(
                                EasyMock.eq(String.format("sh %s", deviceScriptPath))))
                .andReturn("test");
        mMockITestDevice.deleteFile(deviceScriptPath);
        // Run tests
        EasyMock.expect(
                        mMockITestDevice.pushString(
                                EasyMock.<String>anyObject(), EasyMock.eq(deviceScriptPath)))
                .andReturn(Boolean.TRUE);
        mMockITestDevice.executeShellCommand(
                EasyMock.eq(String.format("sh %s", deviceScriptPath)),
                EasyMock.same(mMockReceiver),
                EasyMock.anyLong(),
                (TimeUnit) EasyMock.anyObject(),
                EasyMock.anyInt());
        mMockITestDevice.deleteFile(deviceScriptPath);
        mMockInvocationListener.testRunStarted(testName, 1);
        mMockInvocationListener.testStarted(mDummyTest);
        mMockInvocationListener.testEnded(
                EasyMock.eq(mDummyTest), EasyMock.<HashMap<String, String>>anyObject());
        mMockInvocationListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        EasyMock.expectLastCall();
        replayMocks();

        mGoogleBenchmarkTest.run(mTestInfo, mMockInvocationListener);
        verifyMocks();
    }
}
