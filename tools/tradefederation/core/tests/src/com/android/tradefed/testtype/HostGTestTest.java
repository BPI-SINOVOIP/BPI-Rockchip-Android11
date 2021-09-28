/*
 * Copyright (C) 2018 The Android Open Source Project
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
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import com.android.tradefed.build.BuildInfoKey;
import com.android.tradefed.build.DeviceBuildInfo;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FakeShellOutputReceiver;
import com.android.tradefed.util.FileUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.nio.file.Paths;

/** Unit tests for {@link HostGTest}. */
@RunWith(JUnit4.class)
public class HostGTestTest {
    private File mTestsDir;
    private HostGTest mHostGTest;
    private TestInformation mTestInfo;
    private ITestInvocationListener mMockInvocationListener;
    private FakeShellOutputReceiver mFakeReceiver;
    private OptionSetter mSetter;

    /** Helper to initialize the object or folder for unittest need. */
    @Before
    public void setUp() throws Exception {
        mTestsDir = FileUtil.createTempDir("test_folder_for_unittest");
        mMockInvocationListener = Mockito.mock(ITestInvocationListener.class);
        mFakeReceiver = new FakeShellOutputReceiver();

        mHostGTest = Mockito.spy(new HostGTest());
        GTestXmlResultParser mockXmlParser = Mockito.mock(GTestXmlResultParser.class);
        when(mHostGTest.createXmlParser(any(), any())).thenReturn(mockXmlParser);
        when(mHostGTest.createResultParser(any(), any())).thenReturn(mFakeReceiver);

        mSetter = new OptionSetter(mHostGTest);

        mTestInfo = TestInformation.newBuilder().build();
    }

    @After
    public void afterMethod() {
        FileUtil.recursiveDelete(mTestsDir);
    }

    /**
     * Helper to create a executable script for use in these unit tests.
     *
     * <p>This method will create a executable file for unittest. This executable file is shell
     * script file. It will output all arguments to a file like "file.called" when it has been
     * called. It will also output to both stdout and stderr. Unit tests can check the .called file
     * or the test output (or both) to determine test success or not.
     *
     * @param folderName The path where to create.
     * @param fileName The file name you want to create.
     * @return The file path of "file.called", it is used to check if the file has been called
     *     correctly or not.
     */
    private File createTestScript(String folderName, String fileName) throws IOException {
        final Path outputPath = Paths.get(folderName, fileName + ".called");
        final String script =
                String.format(
                        "echo \"$@\" > %s; echo \"stdout: %s\"; echo \"stderr: %s\" >&2",
                        outputPath, fileName, fileName);

        final Path scriptPath = Paths.get(folderName, fileName);
        createExecutableFile(scriptPath, script);

        return outputPath.toFile();
    }

    /**
     * Helper to create an executable file with the given contents.
     *
     * @param outPath The path where to create.
     * @param contents Contents to write to the file
     */
    private void createExecutableFile(Path outPath, String contents) throws IOException {
        final File outFile = outPath.toFile();
        FileUtil.writeToFile(contents, outFile);
        outFile.setExecutable(true);
    }

    /**
     * Helper to create a sub folder in mTestsDir.
     *
     * @param folderName The path where to create.
     * @return Sub folder File.
     */
    private File createSubFolder(String folderName) throws IOException {
        return FileUtil.createTempDir(folderName, mTestsDir);
    }

    /** Test the executeHostCommand method. */
    @Test
    public void testExecuteHostCommand_success() {
        CommandResult lsResult = mHostGTest.executeHostCommand("ls");
        assertNotEquals("", lsResult.getStdout());
        assertEquals(CommandStatus.SUCCESS, lsResult.getStatus());
    }

    /** Test the executeHostCommand method. */
    @Test
    public void testExecuteHostCommand_fail() {
        CommandResult cmdResult = mHostGTest.executeHostCommand("");
        assertNotEquals(CommandStatus.SUCCESS, cmdResult.getStatus());
    }

    /** Test the loadFilter method. */
    @Test
    public void testLoadFilter() throws ConfigurationException, IOException {
        String moduleName = "hello_world_test";
        String testFilterKey = "presubmit";
        OptionSetter setter = new OptionSetter(mHostGTest);
        setter.setOptionValue("test-filter-key", testFilterKey);

        String filter = "LayerTransactionTest.*:LayerUpdateTest.*";
        String json_content =
                "{\n"
                        + "        \""
                        + testFilterKey
                        + "\": {\n"
                        + "            \"filter\": \""
                        + filter
                        + "\"\n"
                        + "        }\n"
                        + "}\n";
        Path path = Paths.get(mTestsDir.getAbsolutePath(), moduleName + GTestBase.FILTER_EXTENSION);
        File filterFile = path.toFile();
        filterFile.createNewFile();
        filterFile.setReadable(true);
        FileUtil.writeToFile(json_content, filterFile);
        assertEquals(
                mHostGTest.loadFilter(filterFile.getParent() + File.separator + moduleName),
                filter);
    }

    /** Test runTest method. */
    @Test
    public void testRunTest()
            throws ConfigurationException, IOException, DeviceNotAvailableException {
        String moduleName = "hello_world_test";
        String dirPath = mTestsDir.getAbsolutePath();
        File cmd1 = createTestScript(dirPath, "cmd1");
        File cmd2 = createTestScript(dirPath, "cmd2");
        File cmd3 = createTestScript(dirPath, "cmd3");
        File cmd4 = createTestScript(dirPath, "cmd4");

        mSetter.setOptionValue("before-test-cmd", dirPath + File.separator + "cmd1");
        mSetter.setOptionValue("before-test-cmd", dirPath + File.separator + "cmd2");
        mSetter.setOptionValue("after-test-cmd", dirPath + File.separator + "cmd3");
        mSetter.setOptionValue("after-test-cmd", dirPath + File.separator + "cmd4");
        mSetter.setOptionValue("module-name", moduleName);

        File hostLinkedFolder = createSubFolder("hosttestcases");
        createTestScript(hostLinkedFolder.getAbsolutePath(), moduleName);

        DeviceBuildInfo buildInfo = new DeviceBuildInfo();
        buildInfo.setFile(BuildInfoKey.BuildInfoFileKey.HOST_LINKED_DIR, hostLinkedFolder, "0.0");
        mHostGTest.setBuild(buildInfo);

        mHostGTest.run(mTestInfo, mMockInvocationListener);

        assertTrue(cmd1.exists());
        assertTrue(cmd2.exists());
        assertTrue(cmd3.exists());
        assertTrue(cmd4.exists());
        assertNotEquals(0, mFakeReceiver.getReceivedOutput().length);
    }

    /** Test the run method for host linked folder is set. */
    @Test
    public void testRun_priority_get_testcase_from_hostlinked_folder()
            throws IOException, ConfigurationException, DeviceNotAvailableException {
        String moduleName = "hello_world_test";
        String hostLinkedFolderName = "hosttestcases";
        File hostLinkedFolder = createSubFolder(hostLinkedFolderName);
        File hostTestcaseExecutedCheckFile =
                createTestScript(hostLinkedFolder.getAbsolutePath(), moduleName);

        String testFolderName = "testcases";
        File testcasesFolder = createSubFolder(testFolderName);
        File testfolderTestcaseCheckExecuted =
                createTestScript(testcasesFolder.getAbsolutePath(), moduleName);

        mSetter.setOptionValue("module-name", moduleName);
        DeviceBuildInfo buildInfo = new DeviceBuildInfo();
        buildInfo.setFile(BuildInfoKey.BuildInfoFileKey.HOST_LINKED_DIR, hostLinkedFolder, "0.0");
        buildInfo.setTestsDir(testcasesFolder, "0.0");
        mHostGTest.setBuild(buildInfo);

        mHostGTest.run(mTestInfo, mMockInvocationListener);
        assertTrue(hostTestcaseExecutedCheckFile.exists());
        assertFalse(testfolderTestcaseCheckExecuted.exists());
        assertNotEquals(0, mFakeReceiver.getReceivedOutput().length);
    }

    /** Test the run method for host linked folder is not set. */
    @Test
    public void testRun_get_testcase_from_testcases_folder_if_no_hostlinked_dir_set()
            throws IOException, ConfigurationException, DeviceNotAvailableException {
        String moduleName = "hello_world_test";
        String hostLinkedFolderName = "hosttestcases";
        File hostLinkedFolder = createSubFolder(hostLinkedFolderName);
        File hostTestcaseExecutedCheckFile =
                createTestScript(hostLinkedFolder.getAbsolutePath(), moduleName);

        String testFolderName = "testcases";
        File testcasesFolder = createSubFolder(testFolderName);
        File testfolderTestcaseCheckExecuted =
                createTestScript(testcasesFolder.getAbsolutePath(), moduleName);

        mSetter.setOptionValue("module-name", moduleName);
        DeviceBuildInfo buildInfo = new DeviceBuildInfo();
        buildInfo.setTestsDir(testcasesFolder, "0.0");
        mHostGTest.setBuild(buildInfo);

        mHostGTest.run(mTestInfo, mMockInvocationListener);
        assertFalse(hostTestcaseExecutedCheckFile.exists());
        assertTrue(testfolderTestcaseCheckExecuted.exists());
        assertNotEquals(0, mFakeReceiver.getReceivedOutput().length);
    }

    /** Test can't find testcase. */
    @Test(expected = RuntimeException.class)
    public void testRun_can_not_find_testcase()
            throws ConfigurationException, DeviceNotAvailableException {
        String moduleName = "hello_world_test";
        mSetter.setOptionValue("module-name", moduleName);
        DeviceBuildInfo buildInfo = new DeviceBuildInfo();
        mHostGTest.setBuild(buildInfo);

        mHostGTest.run(mTestInfo, mMockInvocationListener);
        assertNotEquals(0, mFakeReceiver.getReceivedOutput().length);
    }

    /** Test the run method for a binary with a suffix. */
    @Test
    public void testRun_withSuffix() throws Exception {
        String moduleName = "hello_world_test";
        String hostLinkedFolderName = "hosttestcases";
        File hostLinkedFolder = createSubFolder(hostLinkedFolderName);
        // The actual execution file has a suffix
        File hostTestcaseExecutedCheckFile =
                createTestScript(hostLinkedFolder.getAbsolutePath(), moduleName + "32");

        String testFolderName = "testcases";
        File testcasesFolder = createSubFolder(testFolderName);
        File testfolderTestcaseCheckExecuted =
                createTestScript(testcasesFolder.getAbsolutePath(), moduleName + "32");

        mSetter.setOptionValue("module-name", moduleName);
        DeviceBuildInfo buildInfo = new DeviceBuildInfo();
        buildInfo.setFile(BuildInfoKey.BuildInfoFileKey.HOST_LINKED_DIR, hostLinkedFolder, "0.0");
        buildInfo.setTestsDir(testcasesFolder, "0.0");
        mHostGTest.setBuild(buildInfo);

        mHostGTest.run(mTestInfo, mMockInvocationListener);
        assertTrue(hostTestcaseExecutedCheckFile.exists());
        assertFalse(testfolderTestcaseCheckExecuted.exists());
        assertNotEquals(0, mFakeReceiver.getReceivedOutput().length);
    }

    /* Test that some command in the test run fails, an exception is thrown and the run stops. */
    @Test
    public void testBeforeCmdError() throws Exception {
        String moduleName = "hello_world_test";
        String hostLinkedFolderName = "hosttestcases";
        File hostLinkedFolder = createSubFolder(hostLinkedFolderName);
        File hostTestcaseExecutedCheckFile =
                createTestScript(hostLinkedFolder.getAbsolutePath(), moduleName);

        String testDir = mTestsDir.getAbsolutePath();
        Path errorScriptPath = Paths.get(testDir, "bad_cmd");
        createExecutableFile(errorScriptPath, "exit 1");

        DeviceBuildInfo buildInfo = new DeviceBuildInfo();
        buildInfo.setFile(BuildInfoKey.BuildInfoFileKey.HOST_LINKED_DIR, hostLinkedFolder, "0.0");
        mHostGTest.setBuild(buildInfo);

        mSetter.setOptionValue("module-name", moduleName);
        mSetter.setOptionValue("before-test-cmd", errorScriptPath.toString());

        try {
            mHostGTest.run(mTestInfo, mMockInvocationListener);
            fail("Didn't throw RuntimeException for before cmd with non-zero exit code");
        } catch (RuntimeException e) {
            // Expected exception
        }
        assertFalse(hostTestcaseExecutedCheckFile.exists());
    }

    /* Test that if the test module exits with code 1, no exception is thrown and the run completes
     * normally. */
    @Test
    public void testTestFailureHandledCorrectly() throws Exception {
        String moduleName = "hello_world_test";
        String hostLinkedFolderName = "hosttestcases";
        File hostLinkedFolder = createSubFolder(hostLinkedFolderName);
        Path errorScriptPath = Paths.get(hostLinkedFolder.getAbsolutePath(), moduleName);
        createExecutableFile(errorScriptPath, "echo 'TEST FAILED'; exit 1");

        DeviceBuildInfo buildInfo = new DeviceBuildInfo();
        buildInfo.setFile(BuildInfoKey.BuildInfoFileKey.HOST_LINKED_DIR, hostLinkedFolder, "0.0");
        mHostGTest.setBuild(buildInfo);

        mSetter.setOptionValue("module-name", moduleName);

        mHostGTest.run(mTestInfo, mMockInvocationListener);
        String testOutput = new String(mFakeReceiver.getReceivedOutput(), StandardCharsets.UTF_8);
        assertEquals("TEST FAILED\n", testOutput);
    }

    /* Test that if the test module exits with a non-zero code other than 1, an exception is thrown
     * and the run stops. */
    @Test
    public void testAbnormalTestCmdExitHandled() throws Exception {
        String moduleName = "hello_world_test";
        String hostLinkedFolderName = "hosttestcases";
        File hostLinkedFolder = createSubFolder(hostLinkedFolderName);
        Path errorScriptPath = Paths.get(hostLinkedFolder.getAbsolutePath(), moduleName);
        createExecutableFile(errorScriptPath, "echo 'TEST BLOWING UP'; exit 2");

        DeviceBuildInfo buildInfo = new DeviceBuildInfo();
        buildInfo.setFile(BuildInfoKey.BuildInfoFileKey.HOST_LINKED_DIR, hostLinkedFolder, "0.0");
        mHostGTest.setBuild(buildInfo);

        mSetter.setOptionValue("module-name", moduleName);

        try {
            mHostGTest.run(mTestInfo, mMockInvocationListener);
            fail("Didn't throw RuntimeException for test cmd with bad exit code");
        } catch (RuntimeException e) {
            // Expected exception
        }
        assertNotEquals(0, mFakeReceiver.getReceivedOutput().length);
    }

    @Test
    public void testBothStdoutAndStderrCollected() throws Exception {
        String moduleName = "hello_world_test";
        String hostLinkedFolderName = "hosttestcases";
        File hostLinkedFolder = createSubFolder(hostLinkedFolderName);
        File hostTestcaseExecutedCheckFile =
                createTestScript(hostLinkedFolder.getAbsolutePath(), moduleName);

        DeviceBuildInfo buildInfo = new DeviceBuildInfo();
        buildInfo.setFile(BuildInfoKey.BuildInfoFileKey.HOST_LINKED_DIR, hostLinkedFolder, "0.0");
        mHostGTest.setBuild(buildInfo);

        mSetter.setOptionValue("module-name", moduleName);
        mHostGTest.run(mTestInfo, mMockInvocationListener);
        assertTrue(hostTestcaseExecutedCheckFile.exists());

        String expected = String.format("stdout: %s\nstderr: %s\n", moduleName, moduleName);
        String testOutput = new String(mFakeReceiver.getReceivedOutput(), StandardCharsets.UTF_8);
        assertEquals(expected, testOutput);
    }
}
