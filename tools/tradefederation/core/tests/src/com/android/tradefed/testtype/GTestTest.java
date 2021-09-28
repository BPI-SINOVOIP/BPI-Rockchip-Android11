/*
 * Copyright (C) 2010 The Android Open Source Project
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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.ddmlib.FileListingService;
import com.android.ddmlib.IShellOutputReceiver;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.CollectingOutputReceiver;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.MockFileUtil;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.testtype.coverage.CoverageOptions;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;


/** Unit tests for {@link GTest}. */
@RunWith(JUnit4.class)
public class GTestTest {
    private static final String GTEST_FLAG_FILTER = "--gtest_filter";
    private ITestInvocationListener mMockInvocationListener = null;
    private IShellOutputReceiver mMockReceiver = null;
    private ITestDevice mMockITestDevice = null;
    private GTest mGTest;
    private OptionSetter mSetter;

    private TestInformation mTestInfo;
    private Configuration mConfiguration;
    private CoverageOptions mCoverageOptions;
    private OptionSetter mCoverageOptionsSetter;

    /** Helper to initialize the various EasyMocks we'll need. */
    @Before
    public void setUp() throws Exception {
        mMockInvocationListener = EasyMock.createMock(ITestInvocationListener.class);
        mMockReceiver = EasyMock.createMock(IShellOutputReceiver.class);
        mMockITestDevice = EasyMock.createMock(ITestDevice.class);
        mMockReceiver.flush();
        EasyMock.expectLastCall().anyTimes();
        EasyMock.expect(mMockITestDevice.getSerialNumber()).andStubReturn("serial");
        mGTest =
                new GTest() {
                    @Override
                    IShellOutputReceiver createResultParser(
                            String runName, ITestInvocationListener listener) {
                        return mMockReceiver;
                    }

                    @Override
                    GTestXmlResultParser createXmlParser(
                            String testRunName, ITestInvocationListener listener) {
                        return new GTestXmlResultParser(testRunName, listener) {
                            @Override
                            public void parseResult(File f, CollectingOutputReceiver output) {
                                return;
                            }
                        };
                    }
                };
        mGTest.setDevice(mMockITestDevice);
        mSetter = new OptionSetter(mGTest);

        // Set up the coverage options
        mConfiguration = new Configuration("", "");
        mCoverageOptions = new CoverageOptions();
        mCoverageOptionsSetter = new OptionSetter(mCoverageOptions);

        mConfiguration.setCoverageOptions(mCoverageOptions);
        mGTest.setConfiguration(mConfiguration);

        mTestInfo = TestInformation.newBuilder().build();
    }

    /**
     * Helper that replays all mocks.
     */
    private void replayMocks() {
      EasyMock.replay(mMockInvocationListener, mMockITestDevice, mMockReceiver);
    }

    /**
     * Helper that verifies all mocks.
     */
    private void verifyMocks() {
      EasyMock.verify(mMockInvocationListener, mMockITestDevice, mMockReceiver);
    }

    /** Test run when the test dir is not found on the device. */
    @Test
    public void testRun_noTestDir() throws DeviceNotAvailableException {
        EasyMock.expect(mMockITestDevice.doesFileExist(GTest.DEFAULT_NATIVETEST_PATH))
                .andReturn(false);
        replayMocks();
        mGTest.run(mTestInfo, mMockInvocationListener);
        verifyMocks();
    }

    /** Test run when no device is set should throw an exception. */
    @Test
    public void testRun_noDevice() throws DeviceNotAvailableException {
        mGTest.setDevice(null);
        replayMocks();
        try {
            mGTest.run(mTestInfo, mMockInvocationListener);
            fail("an exception should have been thrown");
        } catch (IllegalArgumentException e) {
            // expected
        }
        verifyMocks();
    }

    /** Test the run method for a couple tests */
    @Test
    public void testRun() throws DeviceNotAvailableException {
        final String nativeTestPath = GTest.DEFAULT_NATIVETEST_PATH;
        final String test1 = "test1";
        final String test2 = "test2";
        final String testPath1 = String.format("%s/%s", nativeTestPath, test1);
        final String testPath2 = String.format("%s/%s", nativeTestPath, test2);


        MockFileUtil.setMockDirContents(mMockITestDevice, nativeTestPath, test1, test2);
        EasyMock.expect(mMockITestDevice.doesFileExist(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(testPath1)).andReturn(false);
        // report the file as executable
        EasyMock.expect(mMockITestDevice.isExecutable(testPath1)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(testPath2)).andReturn(false);
        // report the file as executable
        EasyMock.expect(mMockITestDevice.isExecutable(testPath2)).andReturn(true);

        String[] files = new String[] {"test1", "test2"};
        EasyMock.expect(mMockITestDevice.getChildren(nativeTestPath)).andReturn(files);
        mMockITestDevice.executeShellCommand(
                EasyMock.contains(test1),
                EasyMock.same(mMockReceiver),
                EasyMock.anyLong(),
                (TimeUnit) EasyMock.anyObject(),
                EasyMock.anyInt());
        mMockITestDevice.executeShellCommand(
                EasyMock.contains(test2),
                EasyMock.same(mMockReceiver),
                EasyMock.anyLong(),
                (TimeUnit) EasyMock.anyObject(),
                EasyMock.anyInt());

        replayMocks();

        mGTest.run(mTestInfo, mMockInvocationListener);
        verifyMocks();
    }

    @Test
    public void testRunFilterAbiPath() throws DeviceNotAvailableException {
        final String nativeTestPath = GTest.DEFAULT_NATIVETEST_PATH;
        final String test1 = "arm/test1";
        final String test2 = "arm64/test2";
        final String testPath2 = String.format("%s/%s", nativeTestPath, test2);
        MockFileUtil.setMockDirContents(mMockITestDevice, nativeTestPath, test1, test2);
        mGTest.setAbi(new Abi("arm64-v8a", "64"));

        EasyMock.expect(mMockITestDevice.doesFileExist(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath + "/arm")).andReturn(true);

        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath + "/arm64")).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(testPath2)).andReturn(false);
        // report the file as executable
        EasyMock.expect(mMockITestDevice.isExecutable(testPath2)).andReturn(true);

        String[] dirs = new String[] {"arm", "arm64"};
        EasyMock.expect(mMockITestDevice.getChildren(nativeTestPath)).andReturn(dirs);
        String[] testFiles = new String[] {"test2"};
        EasyMock.expect(mMockITestDevice.getChildren(nativeTestPath + "/arm64"))
                .andReturn(testFiles);
        mMockITestDevice.executeShellCommand(
                EasyMock.contains(test2),
                EasyMock.same(mMockReceiver),
                EasyMock.anyLong(),
                (TimeUnit) EasyMock.anyObject(),
                EasyMock.anyInt());

        replayMocks();

        mGTest.run(mTestInfo, mMockInvocationListener);
        verifyMocks();
    }

    /** Test the run method when module name is specified */
    @Test
    public void testRun_moduleName() throws DeviceNotAvailableException {
        final String module = "test1";
        final String modulePath = String.format("%s%s%s",
                GTest.DEFAULT_NATIVETEST_PATH, FileListingService.FILE_SEPARATOR, module);
        MockFileUtil.setMockDirContents(mMockITestDevice, modulePath, new String[] {});

        mGTest.setModuleName(module);

        EasyMock.expect(mMockITestDevice.doesFileExist(modulePath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(modulePath)).andReturn(false);
        mMockITestDevice.executeShellCommand(EasyMock.contains(modulePath),
                EasyMock.same(mMockReceiver),
                EasyMock.anyLong(), (TimeUnit)EasyMock.anyObject(), EasyMock.anyInt());
        // report the file as executable
        EasyMock.expect(mMockITestDevice.isExecutable(modulePath)).andReturn(true);

        replayMocks();

        mGTest.run(mTestInfo, mMockInvocationListener);
        verifyMocks();
    }

    /** Test the run method for a test in a subdirectory */
    @Test
    public void testRun_nested() throws DeviceNotAvailableException {
        final String nativeTestPath = GTest.DEFAULT_NATIVETEST_PATH;
        final String subFolderName = "subFolder";
        final String test1 = "test1";
        final String test1Path = String.format("%s%s%s%s%s", nativeTestPath,
                FileListingService.FILE_SEPARATOR,
                subFolderName,
                FileListingService.FILE_SEPARATOR, test1);
        MockFileUtil.setMockDirPath(mMockITestDevice, nativeTestPath, subFolderName, test1);
        EasyMock.expect(mMockITestDevice.doesFileExist(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath + "/" + subFolderName))
                .andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(test1Path)).andReturn(false);
        // report the file as executable
        EasyMock.expect(mMockITestDevice.isExecutable(test1Path)).andReturn(true);
        String[] files = new String[] {subFolderName};
        EasyMock.expect(mMockITestDevice.getChildren(nativeTestPath)).andReturn(files);
        String[] files2 = new String[] {"test1"};
        EasyMock.expect(mMockITestDevice.getChildren(nativeTestPath + "/" + subFolderName))
                .andReturn(files2);
        mMockITestDevice.executeShellCommand(EasyMock.contains(test1Path),
                EasyMock.same(mMockReceiver),
                EasyMock.anyLong(), (TimeUnit)EasyMock.anyObject(), EasyMock.anyInt());

        replayMocks();

        mGTest.run(mTestInfo, mMockInvocationListener);
        verifyMocks();
    }

    /**
     * Helper function to do the actual filtering test.
     *
     * @param filterString The string to search for in the Mock, to verify filtering was called
     * @throws DeviceNotAvailableException
     */
    private void doTestFilter(String filterString) throws DeviceNotAvailableException {
        String nativeTestPath = GTest.DEFAULT_NATIVETEST_PATH;
        String testPath = nativeTestPath + "/test1";
        // configure the mock file system to have a single test
        MockFileUtil.setMockDirContents(mMockITestDevice, nativeTestPath, "test1");
        EasyMock.expect(mMockITestDevice.doesFileExist(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(testPath)).andReturn(false);
        // report the file as executable
        EasyMock.expect(mMockITestDevice.isExecutable(testPath)).andReturn(true);
        String[] files = new String[] {"test1"};
        EasyMock.expect(mMockITestDevice.getChildren(nativeTestPath)).andReturn(files);
        mMockITestDevice.executeShellCommand(EasyMock.contains(filterString),
                EasyMock.same(mMockReceiver), EasyMock.anyLong(), (TimeUnit)EasyMock.anyObject(),
                EasyMock.anyInt());
        replayMocks();
        mGTest.run(mTestInfo, mMockInvocationListener);

        verifyMocks();
    }

    /** Test the include filtering of test methods. */
    @Test
    public void testIncludeFilter() throws DeviceNotAvailableException {
        String includeFilter1 = "abc";
        String includeFilter2 = "def";
        mGTest.addIncludeFilter(includeFilter1);
        mGTest.addIncludeFilter(includeFilter2);
        doTestFilter(String.format("%s=%s:%s", GTEST_FLAG_FILTER, includeFilter1, includeFilter2));
    }

    /** Test that large filters are converted to flagfile. */
    @Test
    public void testLargeFilters() throws DeviceNotAvailableException {
        StringBuilder includeFilter1 = new StringBuilder("abc");
        for (int i = 0; i < 550; i++) {
            includeFilter1.append("a");
        }
        String includeFilter2 = "def";
        mGTest.addIncludeFilter(includeFilter1.toString());
        mGTest.addIncludeFilter(includeFilter2);

        EasyMock.expect(mMockITestDevice.pushFile(EasyMock.anyObject(), EasyMock.anyObject()))
                .andReturn(true);

        doTestFilter(String.format("%s=/data/local/tmp/flagfile", GTestBase.GTEST_FLAG_FILE));
    }

    /** Test the exclude filtering of test methods. */
    @Test
    public void testExcludeFilter() throws DeviceNotAvailableException {
        String excludeFilter1 = "*don?tRunMe*";
        mGTest.addExcludeFilter(excludeFilter1);

        doTestFilter(String.format(
                "%s=-%s", GTEST_FLAG_FILTER, excludeFilter1));
    }

    /** Test simultaneous include and exclude filtering of test methods. */
    @Test
    public void testIncludeAndExcludeFilters() throws DeviceNotAvailableException {
        String includeFilter1 = "pleaseRunMe";
        String includeFilter2 = "andMe";
        String excludeFilter1 = "dontRunMe";
        String excludeFilter2 = "orMe";
        mGTest.addIncludeFilter(includeFilter1);
        mGTest.addExcludeFilter(excludeFilter1);
        mGTest.addIncludeFilter(includeFilter2);
        mGTest.addExcludeFilter(excludeFilter2);

        doTestFilter(String.format("%s=%s:%s-%s:%s", GTEST_FLAG_FILTER,
              includeFilter1, includeFilter2, excludeFilter1, excludeFilter2));
    }

    /** Test behavior for command lines too long to be run by ADB */
    @Test
    public void testCommandTooLong() throws DeviceNotAvailableException {
        String deviceScriptPath = "/data/local/tmp/gtest_script.sh";
        StringBuilder testNameBuilder = new StringBuilder();
        for (int i = 0; i < 1005; i++) {
            testNameBuilder.append("a");
        }
        String testName = testNameBuilder.toString();
        // filter string will be longer than GTest.GTEST_CMD_CHAR_LIMIT

        String nativeTestPath = GTest.DEFAULT_NATIVETEST_PATH;
        String testPath = nativeTestPath + "/" + testName;
        // configure the mock file system to have a single test
        MockFileUtil.setMockDirContents(mMockITestDevice, nativeTestPath, "test1");
        EasyMock.expect(mMockITestDevice.doesFileExist(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(testPath)).andReturn(false);
        // report the file as executable
        EasyMock.expect(mMockITestDevice.isExecutable(testPath)).andReturn(true);
        String[] files = new String[] {testName};
        EasyMock.expect(mMockITestDevice.getChildren(nativeTestPath)).andReturn(files);
        // Expect push of script file
        EasyMock.expect(mMockITestDevice.pushString(EasyMock.<String>anyObject(),
                EasyMock.eq(deviceScriptPath))).andReturn(Boolean.TRUE);
        // chmod 755 for the shell script
        EasyMock.expect(mMockITestDevice.executeShellCommand(EasyMock.contains("chmod")))
                .andReturn("")
                .times(1);
        // Expect command to run shell script, rather than direct adb command
        mMockITestDevice.executeShellCommand(EasyMock.eq(String.format("sh %s", deviceScriptPath)),
                EasyMock.same(mMockReceiver), EasyMock.anyLong(), (TimeUnit)EasyMock.anyObject(),
                EasyMock.anyInt());
        // Expect deletion of file on device
        mMockITestDevice.deleteFile(deviceScriptPath);
        replayMocks();
        mGTest.run(mTestInfo, mMockInvocationListener);

        verifyMocks();
    }

    /** Empty file exclusion regex filter should not skip any files */
    @Test
    public void testFileExclusionRegexFilter_emptyfilters() throws Exception {
        // report /test_file as executable
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mockDevice.isExecutable("/test_file")).andReturn(true);
        EasyMock.replay(mockDevice);
        mGTest.setDevice(mockDevice);
        assertFalse(mGTest.shouldSkipFile("/test_file"));
        EasyMock.verify(mockDevice);
    }

    /** File exclusion regex filter should skip invalid filepath. */
    @Test
    public void testFileExclusionRegexFilter_invalidInputString() throws Exception {
        assertTrue(mGTest.shouldSkipFile(null));
        assertTrue(mGTest.shouldSkipFile(""));
    }

    /** File exclusion regex filter should skip matched filepaths. */
    @Test
    public void testFileExclusionRegexFilter_skipMatched() throws Exception {
        // report all files as executable
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mockDevice.isExecutable("/some/path/file/run_me")).andReturn(true);
        EasyMock.expect(mockDevice.isExecutable("/some/path/file/run_me2")).andReturn(true);
        EasyMock.expect(mockDevice.isExecutable("/some/path/file/run_me.not")).andReturn(true);
        EasyMock.expect(mockDevice.isExecutable("/some/path/file/run_me.so")).andReturn(true);
        EasyMock.replay(mockDevice);
        mGTest.setDevice(mockDevice);
        // Skip files ending in .not
        mGTest.addFileExclusionFilterRegex(".*\\.not");
        assertFalse(mGTest.shouldSkipFile("/some/path/file/run_me"));
        assertFalse(mGTest.shouldSkipFile("/some/path/file/run_me2"));
        assertTrue(mGTest.shouldSkipFile("/some/path/file/run_me.not"));
        // Ensure that the default .so filter is present.
        assertTrue(mGTest.shouldSkipFile("/some/path/file/run_me.so"));
        EasyMock.verify(mockDevice);
    }

    /** File exclusion regex filter for multi filters. */
    @Test
    public void testFileExclusionRegexFilter_skipMultiMatched() throws Exception {
        // report all files as executable
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mockDevice.isExecutable("/some/path/file/run_me")).andReturn(true);
        EasyMock.expect(mockDevice.isExecutable("/some/path/file/run_me.not")).andReturn(true);
        EasyMock.expect(mockDevice.isExecutable("/some/path/file/run_me.not2")).andReturn(true);
        EasyMock.replay(mockDevice);
        mGTest.setDevice(mockDevice);
        // Skip files ending in .not
        mGTest.addFileExclusionFilterRegex(".*\\.not");
        // Also skip files ending in .not2
        mGTest.addFileExclusionFilterRegex(".*\\.not2");
        assertFalse(mGTest.shouldSkipFile("/some/path/file/run_me"));
        assertTrue(mGTest.shouldSkipFile("/some/path/file/run_me.not"));
        assertTrue(mGTest.shouldSkipFile("/some/path/file/run_me.not2"));
    }

    /** Test the run method for a couple tests */
    @Test
    public void testRunXml() throws Exception {
        mSetter.setOptionValue("xml-output", "true");

        final String nativeTestPath = GTest.DEFAULT_NATIVETEST_PATH;
        final String test1 = "test1";
        final String test2 = "test2";
        final String testPath1 = String.format("%s/%s", nativeTestPath, test1);
        final String testPath2 = String.format("%s/%s", nativeTestPath, test2);

        MockFileUtil.setMockDirContents(mMockITestDevice, nativeTestPath, test1, test2);
        EasyMock.expect(mMockITestDevice.doesFileExist(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(testPath1)).andReturn(false);
        EasyMock.expect(mMockITestDevice.isExecutable(testPath1)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(testPath2)).andReturn(false);
        EasyMock.expect(mMockITestDevice.isExecutable(testPath2)).andReturn(true);
        String[] files = new String[] {"test1", "test2"};
        EasyMock.expect(mMockITestDevice.getChildren(nativeTestPath)).andReturn(files);
        mMockITestDevice.deleteFile(testPath1 + "_res.xml");
        mMockITestDevice.deleteFile(testPath2 + "_res.xml");
        EasyMock.expect(mMockITestDevice.pullFile((String)EasyMock.anyObject(),
                (File)EasyMock.anyObject())).andStubReturn(true);
        mMockITestDevice.executeShellCommand(EasyMock.contains(test1),
                (CollectingOutputReceiver) EasyMock.anyObject(),
                EasyMock.anyLong(), (TimeUnit)EasyMock.anyObject(), EasyMock.anyInt());
        mMockITestDevice.executeShellCommand(EasyMock.contains(test2),
                (CollectingOutputReceiver) EasyMock.anyObject(),
                EasyMock.anyLong(), (TimeUnit)EasyMock.anyObject(), EasyMock.anyInt());
        replayMocks();

        mGTest.run(mTestInfo, mMockInvocationListener);
        verifyMocks();
    }

    /** Test cross-process coverage dump for all native processes */
    @Test
    public void testNativeCoverageAllProcesses() throws Exception {
        mCoverageOptionsSetter.setOptionValue("coverage", "true");
        mCoverageOptionsSetter.setOptionValue("coverage-toolchain", "GCOV");
        mCoverageOptionsSetter.setOptionValue("coverage-flush", "true");

        final String nativeTestPath = GTest.DEFAULT_NATIVETEST_PATH;
        final String test1 = "test1";
        final String test2 = "test2";
        final String testPath1 = String.format("%s/%s", nativeTestPath, test1);
        final String testPath2 = String.format("%s/%s", nativeTestPath, test2);

        MockFileUtil.setMockDirContents(mMockITestDevice, nativeTestPath, test1, test2);
        EasyMock.expect(mMockITestDevice.enableAdbRoot()).andReturn(true);
        EasyMock.expect(mMockITestDevice.executeShellCommand("mkdir /data/misc/trace/testcoverage"))
                .andReturn("");
        EasyMock.expect(mMockITestDevice.isAdbRoot()).andReturn(true);
        EasyMock.expect(mMockITestDevice.executeShellCommand("kill -37 -1")).andReturn("");
        // Wait up to 5 minutes for the device to be available after flushing coverage data.
        mMockITestDevice.waitForDeviceAvailable(5 * 60 * 1000);
        EasyMock.expect(mMockITestDevice.executeShellCommand("rm -rf /data/misc/trace/*"))
                .andReturn("");
        EasyMock.expect(mMockITestDevice.doesFileExist(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(testPath1)).andReturn(false);
        // report the file as executable
        EasyMock.expect(mMockITestDevice.isExecutable(testPath1)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(testPath2)).andReturn(false);
        // report the file as executable
        EasyMock.expect(mMockITestDevice.isExecutable(testPath2)).andReturn(true);

        String[] files = new String[] {"test1", "test2"};
        EasyMock.expect(mMockITestDevice.getChildren(nativeTestPath)).andReturn(files);
        mMockITestDevice.executeShellCommand(
                EasyMock.contains(test1),
                EasyMock.same(mMockReceiver),
                EasyMock.anyLong(),
                (TimeUnit) EasyMock.anyObject(),
                EasyMock.anyInt());
        mMockITestDevice.executeShellCommand(
                EasyMock.contains(test2),
                EasyMock.same(mMockReceiver),
                EasyMock.anyLong(),
                (TimeUnit) EasyMock.anyObject(),
                EasyMock.anyInt());

        replayMocks();

        mGTest.run(mTestInfo, mMockInvocationListener);
        verifyMocks();
    }

    /** Test cross-process coverage dump for specific processes */
    @Test
    public void testNativeCoverageSpecificProcesses() throws Exception {
        final List<String> processNames = new ArrayList<>();
        processNames.add("init");
        processNames.add("surfaceflinger");

        mCoverageOptionsSetter.setOptionValue("coverage", "true");
        mCoverageOptionsSetter.setOptionValue("coverage-toolchain", "GCOV");
        mCoverageOptionsSetter.setOptionValue("coverage-flush", "true");
        for (String processName : processNames) {
            mCoverageOptionsSetter.setOptionValue("coverage-processes", processName);
        }

        final String nativeTestPath = GTest.DEFAULT_NATIVETEST_PATH;
        final String test1 = "test1";
        final String test2 = "test2";
        final String testPath1 = String.format("%s/%s", nativeTestPath, test1);
        final String testPath2 = String.format("%s/%s", nativeTestPath, test2);

        MockFileUtil.setMockDirContents(mMockITestDevice, nativeTestPath, test1, test2);
        EasyMock.expect(mMockITestDevice.enableAdbRoot()).andReturn(true);
        EasyMock.expect(mMockITestDevice.executeShellCommand("mkdir /data/misc/trace/testcoverage"))
                .andReturn("");
        // Get the pids to flush coverage data.
        EasyMock.expect(mMockITestDevice.isAdbRoot()).andReturn(true);
        EasyMock.expect(mMockITestDevice.getProcessPid(processNames.get(0))).andReturn("1");
        EasyMock.expect(mMockITestDevice.getProcessPid(processNames.get(1))).andReturn("1000");
        EasyMock.expect(mMockITestDevice.executeShellCommand("kill -37 1 1000")).andReturn("");
        // Wait up to 5 minutes for the device to be available after flushing coverage data.
        mMockITestDevice.waitForDeviceAvailable(5 * 60 * 1000);

        // Clear the coverage data.
        EasyMock.expect(mMockITestDevice.executeShellCommand("rm -rf /data/misc/trace/*"))
                .andReturn("");
        EasyMock.expect(mMockITestDevice.doesFileExist(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(nativeTestPath)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(testPath1)).andReturn(false);
        // report the file as executable
        EasyMock.expect(mMockITestDevice.isExecutable(testPath1)).andReturn(true);
        EasyMock.expect(mMockITestDevice.isDirectory(testPath2)).andReturn(false);
        // report the file as executable
        EasyMock.expect(mMockITestDevice.isExecutable(testPath2)).andReturn(true);

        String[] files = new String[] {"test1", "test2"};
        EasyMock.expect(mMockITestDevice.getChildren(nativeTestPath)).andReturn(files);
        mMockITestDevice.executeShellCommand(
                EasyMock.contains(test1),
                EasyMock.same(mMockReceiver),
                EasyMock.anyLong(),
                (TimeUnit) EasyMock.anyObject(),
                EasyMock.anyInt());
        mMockITestDevice.executeShellCommand(
                EasyMock.contains(test2),
                EasyMock.same(mMockReceiver),
                EasyMock.anyLong(),
                (TimeUnit) EasyMock.anyObject(),
                EasyMock.anyInt());

        replayMocks();

        mGTest.run(mTestInfo, mMockInvocationListener);
        verifyMocks();
    }

    @Test
    public void testGetFileName() {
        String expected = "bar";
        String s1 = "/foo/" + expected;
        String s2 = expected;
        String s3 = "/foo/";
        assertEquals(expected, mGTest.getFileName(s1));
        assertEquals(expected, mGTest.getFileName(s2));
        try {
            mGTest.getFileName(s3);
            fail("Expected IllegalArgumentException not thrown");
        } catch (IllegalArgumentException iae) {
            // expected
        }
    }

    /** Test the include filtering by file of test methods. */
    @Test
    public void testFileFilter() throws Exception {
        String fileFilter = "presubmit";
        mSetter.setOptionValue("test-filter-key", fileFilter);
        String expectedFilterFile = String.format("%s/test1%s",
                GTest.DEFAULT_NATIVETEST_PATH, GTest.FILTER_EXTENSION);
        String fakeContent = "{\n" +
                             "    \"presubmit\": {\n" +
                             "        \"filter\": \"Foo1.*:Foo2.*\"\n" +
                             "    },\n" +
                             "    \"continuous\": {\n" +
                             "        \"filter\": \"Foo1.*:Foo2.*:Bar.*\"\n" +
                             "    }\n" +
                             "}\n";
        EasyMock.expect(mMockITestDevice.doesFileExist(expectedFilterFile)).andReturn(true);
        EasyMock.expect(mMockITestDevice.executeShellCommand("cat \"" + expectedFilterFile + "\""))
                .andReturn(fakeContent);
        doTestFilter(String.format("%s=%s", GTEST_FLAG_FILTER, "Foo1.*:Foo2.*"));
    }

    @Test
    public void testFileFilter_negative() throws Exception {
        String fileFilter = "presubmit";
        mSetter.setOptionValue("test-filter-key", fileFilter);
        String expectedFilterFile =
                String.format("%s/test1%s", GTest.DEFAULT_NATIVETEST_PATH, GTest.FILTER_EXTENSION);
        String fakeContent =
                "{\n"
                        + "    \"presubmit\": {\n"
                        + "        \"filter\": \"Foo1.*-Foo2.*\"\n"
                        + "    },\n"
                        + "    \"continuous\": {\n"
                        + "        \"filter\": \"Foo1.*:Foo2.*:Bar.*\"\n"
                        + "    }\n"
                        + "}\n";
        EasyMock.expect(mMockITestDevice.doesFileExist(expectedFilterFile)).andReturn(true);
        EasyMock.expect(mMockITestDevice.executeShellCommand("cat \"" + expectedFilterFile + "\""))
                .andReturn(fakeContent);
        doTestFilter(String.format("%s=%s", GTEST_FLAG_FILTER, "Foo1.*-Foo2.*"));
    }

    @Test
    public void testFileFilter_negativeOnly() throws Exception {
        String fileFilter = "presubmit";
        mSetter.setOptionValue("test-filter-key", fileFilter);
        String expectedFilterFile =
                String.format("%s/test1%s", GTest.DEFAULT_NATIVETEST_PATH, GTest.FILTER_EXTENSION);
        String fakeContent =
                "{\n"
                        + "    \"presubmit\": {\n"
                        + "        \"filter\": \"-Foo1.*:Foo2.*\"\n"
                        + "    },\n"
                        + "    \"continuous\": {\n"
                        + "        \"filter\": \"Foo1.*:Foo2.*:Bar.*\"\n"
                        + "    }\n"
                        + "}\n";
        EasyMock.expect(mMockITestDevice.doesFileExist(expectedFilterFile)).andReturn(true);
        EasyMock.expect(mMockITestDevice.executeShellCommand("cat \"" + expectedFilterFile + "\""))
                .andReturn(fakeContent);
        doTestFilter(String.format("%s=%s", GTEST_FLAG_FILTER, "-Foo1.*:Foo2.*"));
    }

    /**
     * Test the include filtering by providing a non existing filter. No filter will be applied in
     * this case.
     */
    @Test
    public void testFileFilter_notfound() throws Exception {
        String fileFilter = "garbage";
        mSetter.setOptionValue("test-filter-key", fileFilter);
        String expectedFilterFile = String.format("%s/test1%s",
                GTest.DEFAULT_NATIVETEST_PATH, GTest.FILTER_EXTENSION);
        String fakeContent = "{\n" +
                             "    \"presubmit\": {\n" +
                             "        \"filter\": \"Foo1.*:Foo2.*\"\n" +
                             "    },\n" +
                             "    \"continuous\": {\n" +
                             "        \"filter\": \"Foo1.*:Foo2.*:Bar.*\"\n" +
                             "    }\n" +
                             "}\n";
        EasyMock.expect(mMockITestDevice.doesFileExist(expectedFilterFile)).andReturn(true);
        EasyMock.expect(mMockITestDevice.executeShellCommand("cat \"" + expectedFilterFile + "\""))
                .andReturn(fakeContent);
        doTestFilter("");
    }

    /** Test {@link GTest#getGTestCmdLine(String, String)} with default options. */
    @Test
    public void testGetGTestCmdLine_defaults() {
        String cmd_line = mGTest.getGTestCmdLine("test_path", "flags");
        assertEquals("test_path flags", cmd_line);
    }

    /**
     * Test {@link GTest#getGTestFilters(String)} When the push to file fails, in this case we use
     * the original filter arguments instead of hte flagfile.
     */
    @Test
    public void testGetGTestFilters_largeFilters_pushFail() throws Exception {
        StringBuilder includeFilter1 = new StringBuilder("abc");
        for (int i = 0; i < 550; i++) {
            includeFilter1.append("a");
        }
        mGTest.addIncludeFilter(includeFilter1.toString());
        // Fail to push
        EasyMock.expect(mMockITestDevice.pushFile(EasyMock.anyObject(), EasyMock.anyObject()))
                .andReturn(false);

        EasyMock.replay(mMockITestDevice);
        String flag = mGTest.getGTestFilters("/path/");
        // We fallback to the original command line filter
        assertEquals("--gtest_filter=" + includeFilter1.toString(), flag);
        EasyMock.verify(mMockITestDevice);
    }

    /** Test {@link GTest#getGTestCmdLine(String, String)} with non-default user. */
    @Test
    public void testGetGTestCmdLine_runAs() throws Exception {
        mSetter.setOptionValue("run-test-as", "shell");

        String cmd_line = mGTest.getGTestCmdLine("test_path", "flags");
        assertEquals("su shell test_path flags", cmd_line);
    }

    /** Test GTest command line string for sharded tests. */
    @Test
    public void testGetGTestCmdLine_testShard() {
        mGTest.setShardIndex(1);
        mGTest.setShardCount(3);

        String cmd_line = mGTest.getGTestCmdLine("test_path", "flags");
        assertEquals("GTEST_SHARD_INDEX=1 GTEST_TOTAL_SHARDS=3 test_path flags", cmd_line);
    }
}
