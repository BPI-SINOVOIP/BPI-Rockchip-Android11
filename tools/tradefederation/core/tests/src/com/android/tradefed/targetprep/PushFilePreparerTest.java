/*
 * Copyright (C) 2011 The Android Open Source Project
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

package com.android.tradefed.targetprep;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;

import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.build.BuildInfoKey.BuildInfoFileKey;
import com.android.tradefed.build.DeviceBuildInfo;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.testtype.Abi;
import com.android.tradefed.testtype.suite.ModuleDefinition;
import com.android.tradefed.util.FileUtil;

import org.easymock.Capture;
import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.util.Set;

/** Unit tests for {@link PushFilePreparer} */
@RunWith(JUnit4.class)
public class PushFilePreparerTest {

    private static final String HOST_TESTCASES = "host/testcases";

    private PushFilePreparer mPreparer = null;
    private ITestDevice mMockDevice = null;
    private OptionSetter mOptionSetter = null;
    private TestInformation mTestInfo;

    @Before
    public void setUp() throws Exception {
        mMockDevice = EasyMock.createStrictMock(ITestDevice.class);
        EasyMock.expect(mMockDevice.getDeviceDescriptor()).andStubReturn(null);
        EasyMock.expect(mMockDevice.getSerialNumber()).andStubReturn("SERIAL");
        mPreparer = new PushFilePreparer();
        mOptionSetter = new OptionSetter(mPreparer);
        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", mMockDevice);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
    }

    /** When there's nothing to be done, expect no exception to be thrown */
    @Test
    public void testNoop() throws Exception {
        EasyMock.replay(mMockDevice);
        mPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockDevice);
    }

    @Test
    public void testLocalNoExist() throws Exception {
        mOptionSetter.setOptionValue("push-file", "/noexist", "/data/");
        mOptionSetter.setOptionValue("post-push", "ls /");
        EasyMock.replay(mMockDevice);
        try {
            mTestInfo.getContext().addDeviceBuildInfo("device", new BuildInfo());
            // Should throw TargetSetupError and _not_ run any post-push command
            mPreparer.setUp(mTestInfo);
            fail("TargetSetupError not thrown");
        } catch (TargetSetupError e) {
            // expected
        }
        EasyMock.verify(mMockDevice);
    }

    @Test
    public void testRemoteNoExist() throws Exception {
        mOptionSetter.setOptionValue("push-file", "/bin/sh", "/noexist/");
        mOptionSetter.setOptionValue("post-push", "ls /");
        // expect a pushFile() call and return false (failed)
        EasyMock.expect(
                mMockDevice.pushFile((File)EasyMock.anyObject(), EasyMock.eq("/noexist/")))
                .andReturn(Boolean.FALSE);
        EasyMock.replay(mMockDevice);
        try {
            mTestInfo.getContext().addDeviceBuildInfo("device", new BuildInfo());
            // Should throw TargetSetupError and _not_ run any post-push command
            mPreparer.setUp(mTestInfo);
            fail("TargetSetupError not thrown");
        } catch (TargetSetupError e) {
            // expected
        }
        EasyMock.verify(mMockDevice);
    }

    /**
     * Test pushing a file to remote dir. The 'push' contract allows to push the file to a named
     * directory.
     */
    @Test
    public void testPushFile_RemoteIsDir() throws Exception {
        BuildInfo info = new BuildInfo();
        File testsDir = FileUtil.createTempDir("tests_dir");
        try {
            File testFile = new File(testsDir, "perf_test");
            testFile.createNewFile();
            info.setFile("perf_test", testFile, "v1");
            mOptionSetter.setOptionValue("push-file", "perf_test", "/data/local/tmp/");
            // expect a pushFile() to be done with the appended file name.
            EasyMock.expect(
                            mMockDevice.pushFile(
                                    EasyMock.eq(testFile), EasyMock.eq("/data/local/tmp/")))
                    .andReturn(Boolean.TRUE);
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            EasyMock.replay(mMockDevice);
            mPreparer.setUp(mTestInfo);
            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    /** Pushing the same file to two different locations is working. */
    @Test
    public void testPushFile_duplicateKey() throws Exception {
        BuildInfo info = new BuildInfo();
        File testsDir = FileUtil.createTempDir("tests_dir");
        try {
            File testFile = new File(testsDir, "perf_test");
            testFile.createNewFile();
            info.setFile("perf_test", testFile, "v1");
            mOptionSetter.setOptionValue("push-file", "perf_test", "/data/local/tmp/perf_test1");
            mOptionSetter.setOptionValue("push-file", "perf_test", "/data/local/tmp/perf_test2");
            // expect a pushFile() to be done with the appended file name.
            EasyMock.expect(
                            mMockDevice.pushFile(
                                    EasyMock.eq(testFile),
                                    EasyMock.eq("/data/local/tmp/perf_test1")))
                    .andReturn(Boolean.TRUE);
            EasyMock.expect(
                            mMockDevice.pushFile(
                                    EasyMock.eq(testFile),
                                    EasyMock.eq("/data/local/tmp/perf_test2")))
                    .andReturn(Boolean.TRUE);
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            EasyMock.replay(mMockDevice);
            mPreparer.setUp(mTestInfo);
            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    /** Test pushing a directory to an existing remote directory. */
    @Test
    public void testPushDir_RemoteIsDir() throws Exception {
        BuildInfo info = new BuildInfo();
        File testsDir = FileUtil.createTempDir("tests_dir");
        try {
            File testFile = new File(testsDir, "perf_test");
            testFile.mkdir();
            info.setFile("perf_test", testFile, "v1");
            mOptionSetter.setOptionValue("push-file", "perf_test", "/data/local/tmp/");
            EasyMock.expect(mMockDevice.doesFileExist("/data/local/tmp/")).andReturn(true);
            EasyMock.expect(mMockDevice.isDirectory("/data/local/tmp/")).andReturn(true);
            // expect a pushFile() to be done with the appended file name.
            EasyMock.expect(
                            mMockDevice.pushDir(
                                    EasyMock.eq(testFile),
                                    EasyMock.eq("/data/local/tmp/"),
                                    EasyMock.anyObject()))
                    .andReturn(Boolean.TRUE);
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            EasyMock.replay(mMockDevice);
            mPreparer.setUp(mTestInfo);
            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    /** Test when we attempt to push a directory but the receiving location is an existing file. */
    @Test
    public void testPushDir_RemoteIsFile() throws Exception {
        BuildInfo info = new BuildInfo();
        File testsDir = FileUtil.createTempDir("tests_dir");
        try {
            File testFile = new File(testsDir, "perf_test");
            testFile.mkdir();
            info.setFile("perf_test", testFile, "v1");
            mOptionSetter.setOptionValue("push-file", "perf_test", "/data/local/tmp/file");
            EasyMock.expect(mMockDevice.doesFileExist("/data/local/tmp/file")).andReturn(true);
            EasyMock.expect(mMockDevice.isDirectory("/data/local/tmp/file")).andReturn(false);
            EasyMock.replay(mMockDevice);
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            try {
                mPreparer.setUp(mTestInfo);
                fail("Should have thrown an exception.");
            } catch (TargetSetupError expected) {
                // Expected
            }
            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    /**
     * Test pushing a file to remote dir. If there are multiple files push to the same place, the
     * latest win.
     */
    @Test
    public void testRemotePush_override() throws Exception {
        BuildInfo info = new BuildInfo();
        File testsDir = FileUtil.createTempDir("tests_dir");
        try {
            File testFile = new File(testsDir, "perf_test");
            testFile.createNewFile();
            File testFile2 = new File(testsDir, "perf_test2");
            testFile2.createNewFile();
            info.setFile("perf_test", testFile, "v1");
            info.setFile("perf_test2", testFile2, "v1");
            mOptionSetter.setOptionValue("push-file", "perf_test", "/data/local/tmp/perf_test");
            mOptionSetter.setOptionValue("push-file", "perf_test2", "/data/local/tmp/perf_test");
            EasyMock.expect(mMockDevice.isDirectory(EasyMock.anyObject())).andStubReturn(false);
            // the latest config win.
            EasyMock.expect(
                            mMockDevice.pushFile(
                                    EasyMock.eq(testFile2),
                                    EasyMock.eq("/data/local/tmp/perf_test")))
                    .andReturn(Boolean.TRUE);
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            EasyMock.replay(mMockDevice);
            mPreparer.setUp(mTestInfo);
            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    /**
     * Test pushing a file to remote dir. If both push and push-file push to the same remote file,
     * the push-file win.
     */
    @Test
    public void testPushFileAndPush_override() throws Exception {
        BuildInfo info = new BuildInfo();
        File testsDir = FileUtil.createTempDir("tests_dir");
        try {
            File testFile = new File(testsDir, "perf_test");
            testFile.createNewFile();
            File testFile2 = new File(testsDir, "perf_test2");
            testFile2.createNewFile();
            info.setFile("perf_test", testFile, "v1");
            info.setFile("perf_test2", testFile2, "v1");

            mOptionSetter.setOptionValue("push-file", "perf_test2", "/data/local/tmp/perf_test");
            mOptionSetter.setOptionValue("push", "perf_test->/data/local/tmp/perf_test");
            EasyMock.expect(mMockDevice.isDirectory(EasyMock.anyObject())).andStubReturn(false);
            // the latest config win.
            EasyMock.expect(
                            mMockDevice.pushFile(
                                    EasyMock.eq(testFile2),
                                    EasyMock.eq("/data/local/tmp/perf_test")))
                    .andReturn(Boolean.TRUE);
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            EasyMock.replay(mMockDevice);
            mPreparer.setUp(mTestInfo);
            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    @Test
    public void testWarnOnFailure() throws Exception {
        mOptionSetter.setOptionValue("push", "/bin/sh->/noexist/");
        mOptionSetter.setOptionValue("post-push", "ls /");
        mOptionSetter.setOptionValue("abort-on-push-failure", "false");

        // expect a pushFile() call and return false (failed)
        EasyMock.expect(
                mMockDevice.pushFile((File)EasyMock.anyObject(), EasyMock.eq("/noexist/")))
                .andReturn(Boolean.FALSE);
        // Because we're only warning, the post-push command should be run despite the push failures
        EasyMock.expect(mMockDevice.executeShellCommand(EasyMock.eq("ls /"))).andReturn("");
        EasyMock.replay(mMockDevice);
        mTestInfo.getContext().addDeviceBuildInfo("device", new BuildInfo());
        // Don't expect any exceptions to be thrown
        mPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockDevice);
    }

    /**
     * Test {@link PushFilePreparer#resolveRelativeFilePath(IBuildInfo, String)} do not search
     * additional tests directory if the given build if is not of IBuildInfo type.
     */
    @Test
    public void testResolveRelativeFilePath_noDeviceBuildInfo() {
        IBuildInfo buildInfo = EasyMock.createStrictMock(IBuildInfo.class);
        String fileName = "source_file";
        EasyMock.expect(buildInfo.getFile(fileName)).andReturn(null);
        EasyMock.replay(buildInfo);

        assertNull(mPreparer.resolveRelativeFilePath(buildInfo, fileName));
        EasyMock.verify(buildInfo);
    }

    /**
     * Test {@link PushFilePreparer#resolveRelativeFilePath(IBuildInfo, String)} can locate a source
     * file existed in tests directory of a device build.
     */
    @Test
    public void testResolveRelativeFilePath_withDeviceBuildInfo() throws Exception {
        IDeviceBuildInfo buildInfo = EasyMock.createStrictMock(IDeviceBuildInfo.class);
        String fileName = "source_file";

        File testsDir = null;
        try {
            testsDir = FileUtil.createTempDir("tests_dir");
            File hostTestCasesDir = FileUtil.getFileForPath(testsDir, HOST_TESTCASES);
            FileUtil.mkdirsRWX(hostTestCasesDir);
            File sourceFile = FileUtil.createTempFile(fileName, null, hostTestCasesDir);

            fileName = sourceFile.getName();
            EasyMock.expect(buildInfo.getFile(fileName)).andReturn(null);
            EasyMock.expect(buildInfo.getTestsDir()).andReturn(testsDir);
            EasyMock.expect(buildInfo.getFile(BuildInfoFileKey.TARGET_LINKED_DIR)).andReturn(null);
            EasyMock.replay(buildInfo);

            assertEquals(
                    sourceFile.getAbsolutePath(),
                    mPreparer.resolveRelativeFilePath(buildInfo, fileName).getAbsolutePath());
            EasyMock.verify(buildInfo);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    /**
     * Test {@link PushFilePreparer#resolveRelativeFilePath(IBuildInfo, String)} can locate a source
     * file existed in a remote zip of a device build.
     */
    @Test
    public void testResolveRelativeFilePath_withDeviceBuildInfo_remoteZip() throws Exception {
        IDeviceBuildInfo buildInfo = EasyMock.createStrictMock(IDeviceBuildInfo.class);
        String fileName = "source_file";

        File testsDir = null;
        try {
            testsDir = FileUtil.createTempDir("tests_dir");
            File hostTestCasesDir = FileUtil.getFileForPath(testsDir, HOST_TESTCASES);
            FileUtil.mkdirsRWX(hostTestCasesDir);
            File sourceFile = FileUtil.createTempFile(fileName, null, hostTestCasesDir);

            // Change the file name so direct file search will return null.
            fileName = sourceFile.getName() + "-2";
            EasyMock.expect(buildInfo.getFile(fileName)).andReturn(null);
            EasyMock.expect(buildInfo.getTestsDir()).andReturn(testsDir);
            EasyMock.expect(buildInfo.getFile(BuildInfoFileKey.TARGET_LINKED_DIR)).andReturn(null);
            EasyMock.expect(buildInfo.stageRemoteFile(EasyMock.eq(fileName), EasyMock.eq(testsDir)))
                    .andReturn(sourceFile);
            EasyMock.replay(buildInfo);

            assertEquals(
                    sourceFile.getAbsolutePath(),
                    mPreparer.resolveRelativeFilePath(buildInfo, fileName).getAbsolutePath());
            EasyMock.verify(buildInfo);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    /**
     * If a folder is found match it first and push it while filtering the abi that are not
     * considered.
     */
    @Test
    public void testPush_abiDirectory_noBitness() throws Exception {
        mOptionSetter.setOptionValue("push", "debugger->/data/local/tmp/debugger");
        mPreparer.setAbi(new Abi("x86", "32"));
        IDeviceBuildInfo info = new DeviceBuildInfo();
        File tmpFolder = FileUtil.createTempDir("push-file-tests-dir");
        try {
            File debuggerFile = new File(tmpFolder, "target/testcases/debugger/x86/debugger");
            FileUtil.mkdirsRWX(debuggerFile);

            info.setFile(BuildInfoFileKey.TESTDIR_IMAGE, tmpFolder, "v1");
            EasyMock.expect(mMockDevice.doesFileExist("/data/local/tmp/debugger")).andReturn(false);
            EasyMock.expect(
                            mMockDevice.executeShellCommand(
                                    "mkdir -p \"/data/local/tmp/debugger\""))
                    .andReturn("");
            Capture<Set<String>> capture = new Capture<>();
            EasyMock.expect(
                            mMockDevice.pushDir(
                                    EasyMock.eq(new File(tmpFolder, "target/testcases/debugger")),
                                    EasyMock.eq("/data/local/tmp/debugger"),
                                    EasyMock.capture(capture)))
                    .andReturn(true);
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            EasyMock.replay(mMockDevice);
            mPreparer.setUp(mTestInfo);
            EasyMock.verify(mMockDevice);
            // The x86 folder was not filtered
            Set<String> capValue = capture.getValue();
            assertFalse(capValue.contains("x86"));
        } finally {
            FileUtil.recursiveDelete(tmpFolder);
        }
    }

    @Test
    public void testPush_abiDirectory_noBitness_withModule() throws Exception {
        mOptionSetter.setOptionValue("push", "folder->/data/local/tmp/folder");
        mPreparer.setAbi(new Abi("x86", "32"));
        mPreparer.setInvocationContext(createModuleWithName("debugger"));
        IDeviceBuildInfo info = new DeviceBuildInfo();
        File tmpFolder = FileUtil.createTempDir("push-file-tests-dir");
        try {
            File debuggerFile = new File(tmpFolder, "target/testcases/debugger/x86/folder");
            FileUtil.mkdirsRWX(debuggerFile);

            info.setFile(BuildInfoFileKey.TESTDIR_IMAGE, tmpFolder, "v1");
            EasyMock.expect(mMockDevice.doesFileExist("/data/local/tmp/folder")).andReturn(false);
            EasyMock.expect(mMockDevice.executeShellCommand("mkdir -p \"/data/local/tmp/folder\""))
                    .andReturn("");
            Capture<Set<String>> capture = new Capture<>();
            EasyMock.expect(
                            mMockDevice.pushDir(
                                    EasyMock.eq(
                                            new File(
                                                    tmpFolder,
                                                    "target/testcases/debugger/x86/folder")),
                                    EasyMock.eq("/data/local/tmp/folder"),
                                    EasyMock.capture(capture)))
                    .andReturn(true);
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            EasyMock.replay(mMockDevice);
            mPreparer.setUp(mTestInfo);
            EasyMock.verify(mMockDevice);
            // The x86 folder was not filtered
            Set<String> capValue = capture.getValue();
            assertFalse(capValue.contains("x86"));
        } finally {
            FileUtil.recursiveDelete(tmpFolder);
        }
    }

    /** Test when pushing a directory and the subfolders are abi marked. */
    @Test
    public void testPush_abiDirectory() throws Exception {
        mOptionSetter.setOptionValue("push", "debugger->/data/local/tmp/debugger");
        mPreparer.setAbi(new Abi("x86", "32"));
        IDeviceBuildInfo info = new DeviceBuildInfo();
        File tmpFolder = FileUtil.createTempDir("push-file-tests-dir");
        try {
            File debuggerFile = new File(tmpFolder, "target/testcases/debugger/x86/debugger32");
            FileUtil.mkdirsRWX(debuggerFile);
            info.setFile(BuildInfoFileKey.TESTDIR_IMAGE, tmpFolder, "v1");
            EasyMock.expect(mMockDevice.doesFileExist("/data/local/tmp/debugger")).andReturn(false);
            EasyMock.expect(
                            mMockDevice.executeShellCommand(
                                    "mkdir -p \"/data/local/tmp/debugger\""))
                    .andReturn("");
            Capture<Set<String>> capture = new Capture<>();
            EasyMock.expect(
                            mMockDevice.pushDir(
                                    EasyMock.eq(new File(tmpFolder, "target/testcases/debugger")),
                                    EasyMock.eq("/data/local/tmp/debugger"),
                                    EasyMock.capture(capture)))
                    .andReturn(true);
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            EasyMock.replay(mMockDevice);
            mPreparer.setUp(mTestInfo);
            EasyMock.verify(mMockDevice);
            // The x86 folder was not filtered
            Set<String> capValue = capture.getValue();
            assertFalse(capValue.contains("x86"));
        } finally {
            FileUtil.recursiveDelete(tmpFolder);
        }
    }

    /** Test that if a module name exists we attempt to search it first. */
    @Test
    public void testPush_moduleName_dirs() throws Exception {
        mOptionSetter.setOptionValue("push", "lib64->/data/local/tmp/lib");
        mPreparer.setAbi(new Abi("x86_64", "64"));

        mPreparer.setInvocationContext(createModuleWithName("debugger"));
        IDeviceBuildInfo info = new DeviceBuildInfo();
        File tmpFolder = FileUtil.createTempDir("push-file-tests-dir");
        try {
            File beforeName = new File(tmpFolder, "target/testcases/aaaaa/x86_64/lib64");
            FileUtil.mkdirsRWX(beforeName);
            File libX86File = new File(tmpFolder, "target/testcases/debugger/x86_64/lib64");
            FileUtil.mkdirsRWX(libX86File);
            File otherLib = new File(tmpFolder, "target/testcases/random/x86_64/lib64");
            FileUtil.mkdirsRWX(otherLib);
            info.setFile(BuildInfoFileKey.TESTDIR_IMAGE, tmpFolder, "v1");
            EasyMock.expect(mMockDevice.doesFileExist("/data/local/tmp/lib")).andReturn(false);
            EasyMock.expect(mMockDevice.executeShellCommand("mkdir -p \"/data/local/tmp/lib\""))
                    .andReturn("");
            Capture<Set<String>> capture = new Capture<>();
            EasyMock.expect(
                            mMockDevice.pushDir(
                                    EasyMock.eq(
                                            new File(
                                                    tmpFolder,
                                                    "target/testcases/debugger/x86_64/lib64")),
                                    EasyMock.eq("/data/local/tmp/lib"),
                                    EasyMock.capture(capture)))
                    .andReturn(true);
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            EasyMock.replay(mMockDevice);
            mPreparer.setUp(mTestInfo);
            EasyMock.verify(mMockDevice);
            // The x86 folder was not filtered
            Set<String> capValue = capture.getValue();
            assertFalse(capValue.contains("x86_64"));
        } finally {
            FileUtil.recursiveDelete(tmpFolder);
        }
    }

    @Test
    public void testPush_moduleName_files() throws Exception {
        mOptionSetter.setOptionValue("push", "file->/data/local/tmp/file");
        mPreparer.setAbi(new Abi("x86_64", "64"));

        mPreparer.setInvocationContext(createModuleWithName("aaaaa"));
        IDeviceBuildInfo info = new DeviceBuildInfo();
        File tmpFolder = FileUtil.createTempDir("push-file-tests-dir");
        try {
            File beforeName = new File(tmpFolder, "target/testcases/aaaaa/x86_64/file");
            FileUtil.mkdirsRWX(beforeName.getParentFile());
            beforeName.createNewFile();
            File libX86File = new File(tmpFolder, "target/testcases/debugger/x86_64/file");
            FileUtil.mkdirsRWX(libX86File.getParentFile());
            libX86File.createNewFile();
            File otherLib = new File(tmpFolder, "target/testcases/random/x86_64/file");
            FileUtil.mkdirsRWX(otherLib.getParentFile());
            otherLib.createNewFile();
            info.setFile(BuildInfoFileKey.TESTDIR_IMAGE, tmpFolder, "v1");
            EasyMock.expect(
                            mMockDevice.pushFile(
                                    EasyMock.eq(
                                            new File(
                                                    tmpFolder,
                                                    "target/testcases/aaaaa/x86_64/file")),
                                    EasyMock.eq("/data/local/tmp/file")))
                    .andReturn(true);
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            EasyMock.replay(mMockDevice);
            mPreparer.setUp(mTestInfo);
            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(tmpFolder);
        }
    }

    /** Test that if multiple files exists, push the one with matching ABI. */
    @Test
    public void testPush_moduleName_files_abi_32bit() throws Exception {
        mOptionSetter.setOptionValue("push", "file->/data/local/tmp/file");
        mPreparer.setAbi(new Abi("x86", "32"));

        mPreparer.setInvocationContext(createModuleWithName("aaaaa"));
        IDeviceBuildInfo info = new DeviceBuildInfo();
        File tmpFolder = FileUtil.createTempDir("push-file-tests-dir");
        try {
            File beforeName = new File(tmpFolder, "target/testcases/aaaaa/x86_64/file");
            FileUtil.mkdirsRWX(beforeName.getParentFile());
            beforeName.createNewFile();
            File x86File = new File(tmpFolder, "target/testcases/aaaaa/x86/file");
            FileUtil.mkdirsRWX(x86File.getParentFile());
            x86File.createNewFile();
            info.setFile(BuildInfoFileKey.TESTDIR_IMAGE, tmpFolder, "v1");
            EasyMock.expect(
                            mMockDevice.pushFile(
                                    EasyMock.eq(
                                            new File(tmpFolder, "target/testcases/aaaaa/x86/file")),
                                    EasyMock.eq("/data/local/tmp/file")))
                    .andReturn(true);
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            EasyMock.replay(mMockDevice);
            mPreparer.setUp(mTestInfo);
            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(tmpFolder);
        }
    }

    /** Test that if multiple files exists, push the one with matching ABI. */
    @Test
    public void testPush_moduleName_files_abi_64bit() throws Exception {
        mOptionSetter.setOptionValue("push", "file->/data/local/tmp/file");
        mPreparer.setAbi(new Abi("x86_64", "64"));

        mPreparer.setInvocationContext(createModuleWithName("aaaaa"));
        IDeviceBuildInfo info = new DeviceBuildInfo();
        File tmpFolder = FileUtil.createTempDir("push-file-tests-dir");
        try {
            File beforeName = new File(tmpFolder, "target/testcases/aaaaa/x86_64/file");
            FileUtil.mkdirsRWX(beforeName.getParentFile());
            beforeName.createNewFile();
            File x86File = new File(tmpFolder, "target/testcases/aaaaa/x86/file");
            FileUtil.mkdirsRWX(x86File.getParentFile());
            x86File.createNewFile();
            info.setFile(BuildInfoFileKey.TESTDIR_IMAGE, tmpFolder, "v1");
            EasyMock.expect(
                            mMockDevice.pushFile(
                                    EasyMock.eq(
                                            new File(
                                                    tmpFolder,
                                                    "target/testcases/aaaaa/x86_64/file")),
                                    EasyMock.eq("/data/local/tmp/file")))
                    .andReturn(true);
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            EasyMock.replay(mMockDevice);
            mPreparer.setUp(mTestInfo);
            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(tmpFolder);
        }
    }

    @Test
    public void testPush_moduleName_ignored() throws Exception {
        mOptionSetter.setOptionValue("push", "lib64->/data/local/tmp/lib");
        mPreparer.setAbi(new Abi("x86_64", "64"));

        mPreparer.setInvocationContext(createModuleWithName("debugger"));
        IDeviceBuildInfo info = new DeviceBuildInfo();
        File tmpFolder = FileUtil.createTempDir("push-file-tests-dir");
        try {
            File beforeName = new File(tmpFolder, "target/testcases/aaaaa/x86_64/lib64");
            FileUtil.mkdirsRWX(beforeName);
            File libX86File = new File(tmpFolder, "target/testcases/debugger/x86_64/minilib64");
            FileUtil.mkdirsRWX(libX86File);
            info.setFile(BuildInfoFileKey.TESTDIR_IMAGE, tmpFolder, "v1");
            EasyMock.expect(mMockDevice.doesFileExist("/data/local/tmp/lib")).andReturn(false);
            EasyMock.expect(mMockDevice.executeShellCommand("mkdir -p \"/data/local/tmp/lib\""))
                    .andReturn("");
            Capture<Set<String>> capture = new Capture<>();
            // Use the only dir matching the file, regardless of the module if the module doesn't
            // contain the file.
            EasyMock.expect(
                            mMockDevice.pushDir(
                                    EasyMock.eq(
                                            new File(
                                                    tmpFolder,
                                                    "target/testcases/aaaaa/x86_64/lib64")),
                                    EasyMock.eq("/data/local/tmp/lib"),
                                    EasyMock.capture(capture)))
                    .andReturn(true);
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            EasyMock.replay(mMockDevice);
            mPreparer.setUp(mTestInfo);
            EasyMock.verify(mMockDevice);
            // The x86 folder was not filtered
            Set<String> capValue = capture.getValue();
            assertFalse(capValue.contains("x86_64"));
        } finally {
            FileUtil.recursiveDelete(tmpFolder);
        }
    }

    @Test
    public void testPush_moduleName_multiAbi() throws Exception {
        mOptionSetter.setOptionValue("push", "lib64->/data/local/tmp/lib");
        mPreparer.setAbi(new Abi("x86", "32"));

        mPreparer.setInvocationContext(createModuleWithName("debugger"));
        IDeviceBuildInfo info = new DeviceBuildInfo();
        File tmpFolder = FileUtil.createTempDir("push-file-tests-dir");
        try {
            File beforeName = new File(tmpFolder, "target/testcases/aaaaa/x86_64/lib64");
            FileUtil.mkdirsRWX(beforeName);
            File libX86File = new File(tmpFolder, "target/testcases/debugger/lib64");
            FileUtil.mkdirsRWX(libX86File);
            File otherLib = new File(tmpFolder, "target/testcases/debugger/lib32");
            FileUtil.mkdirsRWX(otherLib);
            info.setFile(BuildInfoFileKey.TESTDIR_IMAGE, tmpFolder, "v1");
            EasyMock.expect(mMockDevice.doesFileExist("/data/local/tmp/lib")).andReturn(false);
            EasyMock.expect(mMockDevice.executeShellCommand("mkdir -p \"/data/local/tmp/lib\""))
                    .andReturn("");
            Capture<Set<String>> capture = new Capture<>();
            EasyMock.expect(
                            mMockDevice.pushDir(
                                    EasyMock.eq(
                                            new File(tmpFolder, "target/testcases/debugger/lib64")),
                                    EasyMock.eq("/data/local/tmp/lib"),
                                    EasyMock.capture(capture)))
                    .andReturn(true);
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            EasyMock.replay(mMockDevice);
            mPreparer.setUp(mTestInfo);
            EasyMock.verify(mMockDevice);
            // The x86 folder was not filtered
            Set<String> capValue = capture.getValue();
            assertFalse(capValue.contains("x86"));
        } finally {
            FileUtil.recursiveDelete(tmpFolder);
        }
    }

    @Test
    public void testPush_moduleName_multiabi_files() throws Exception {
        mOptionSetter.setOptionValue("push", "debugger->/data/local/tmp/debugger");
        mPreparer.setAbi(new Abi("x86", "32"));

        mPreparer.setInvocationContext(createModuleWithName("debugger"));
        IDeviceBuildInfo info = new DeviceBuildInfo();
        File tmpFolder = FileUtil.createTempDir("push-file-tests-dir");
        try {
            File beforeName = new File(tmpFolder, "target/testcases/debugger/x86/debugger");
            FileUtil.mkdirsRWX(beforeName.getParentFile());
            beforeName.createNewFile();
            File libX86File = new File(tmpFolder, "target/testcases/debugger/x86_64/debugger");
            FileUtil.mkdirsRWX(libX86File.getParentFile());
            libX86File.createNewFile();
            File otherLib = new File(tmpFolder, "target/testcases/debugger/arm/debugger");
            FileUtil.mkdirsRWX(otherLib.getParentFile());
            otherLib.createNewFile();
            info.setFile(BuildInfoFileKey.TESTDIR_IMAGE, tmpFolder, "v1");

            EasyMock.expect(mMockDevice.doesFileExist("/data/local/tmp/debugger")).andReturn(false);
            EasyMock.expect(
                            mMockDevice.executeShellCommand(
                                    "mkdir -p \"/data/local/tmp/debugger\""))
                    .andReturn("");
            Capture<Set<String>> capture = new Capture<>();
            EasyMock.expect(
                            mMockDevice.pushDir(
                                    EasyMock.eq(new File(tmpFolder, "target/testcases/debugger")),
                                    EasyMock.eq("/data/local/tmp/debugger"),
                                    EasyMock.capture(capture)))
                    .andReturn(true);
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            EasyMock.replay(mMockDevice);
            mPreparer.setUp(mTestInfo);
            EasyMock.verify(mMockDevice);
            // The x86 folder was not filtered
            Set<String> capValue = capture.getValue();
            assertFalse(capValue.contains("x86"));
        } finally {
            FileUtil.recursiveDelete(tmpFolder);
        }
    }

    /**
     * Ensure that in case we don't find the module directory. We fallback to top level match first
     * and not first found.
     */
    @Test
    public void testPush_moduleName_noMatch() throws Exception {
        mOptionSetter.setOptionValue("push", "lib64->/data/local/tmp/lib");
        mPreparer.setAbi(new Abi("x86_64", "64"));

        mPreparer.setInvocationContext(createModuleWithName("CtsBionicTestCases"));
        IDeviceBuildInfo info = new DeviceBuildInfo();
        File tmpFolder = FileUtil.createTempDir("push-file-tests-dir");
        try {
            File beforeName = new File(tmpFolder, "lib64");
            FileUtil.mkdirsRWX(beforeName);
            File libX86File = new File(tmpFolder, "DATA/lib64");
            FileUtil.mkdirsRWX(libX86File);
            info.setFile(BuildInfoFileKey.TESTDIR_IMAGE, tmpFolder, "v1");
            EasyMock.expect(mMockDevice.doesFileExist("/data/local/tmp/lib")).andReturn(false);
            EasyMock.expect(mMockDevice.executeShellCommand("mkdir -p \"/data/local/tmp/lib\""))
                    .andReturn("");
            Capture<Set<String>> capture = new Capture<>();
            EasyMock.expect(
                            mMockDevice.pushDir(
                                    EasyMock.eq(new File(tmpFolder, "lib64")),
                                    EasyMock.eq("/data/local/tmp/lib"),
                                    EasyMock.capture(capture)))
                    .andReturn(true);
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            EasyMock.replay(mMockDevice);
            mPreparer.setUp(mTestInfo);
            EasyMock.verify(mMockDevice);
            // The x86 folder was not filtered
            Set<String> capValue = capture.getValue();
            assertFalse(capValue.contains("x86_64"));
        } finally {
            FileUtil.recursiveDelete(tmpFolder);
        }
    }

    /**
     * Test that if a binary name is repeating in another directory that is searched first we don't
     * use it and do prioritize the module name directory.
     */
    @Test
    public void testPush_moduleName_repeating_name() throws Exception {
        mOptionSetter.setOptionValue(
                "push",
                "propertyinfoserializer_tests->/data/local/tmp/propertyinfoserializer_tests");
        mPreparer.setAbi(new Abi("x86", "32"));

        mPreparer.setInvocationContext(createModuleWithName("propertyinfoserializer_tests"));
        IDeviceBuildInfo info = new DeviceBuildInfo();
        File tmpFolder = FileUtil.createTempDir("push-file-tests-dir");
        try {
            File beforeName =
                    new File(
                            tmpFolder,
                            "target/testcases/propertyinfoserializer_tests/x86/"
                                    + "propertyinfoserializer_tests");
            FileUtil.mkdirsRWX(beforeName.getParentFile());
            beforeName.createNewFile();
            File libArmFile =
                    new File(
                            tmpFolder,
                            "target/testcases/propertyinfoserializer_tests/arm/"
                                    + "propertyinfoserializer_tests");
            FileUtil.mkdirsRWX(libArmFile.getParentFile());
            libArmFile.createNewFile();
            File otherModule =
                    new File(
                            tmpFolder,
                            "target/testcases/propertyinfoserializer_tests.vendor/arm/"
                                    + "propertyinfoserializer_tests");
            FileUtil.mkdirsRWX(otherModule.getParentFile());
            otherModule.createNewFile();
            File otherModule2 =
                    new File(
                            tmpFolder,
                            "target/testcases/propertyinfoserializer_tests.vendor/x86/"
                                    + "propertyinfoserializer_tests");
            FileUtil.mkdirsRWX(otherModule2.getParentFile());
            otherModule2.createNewFile();
            info.setFile(BuildInfoFileKey.TESTDIR_IMAGE, tmpFolder, "v1");

            EasyMock.expect(
                            mMockDevice.doesFileExist(
                                    "/data/local/tmp/propertyinfoserializer_tests"))
                    .andReturn(false);
            EasyMock.expect(
                            mMockDevice.executeShellCommand(
                                    "mkdir -p \"/data/local/tmp/propertyinfoserializer_tests\""))
                    .andReturn("");
            Capture<Set<String>> capture = new Capture<>();
            EasyMock.expect(
                            mMockDevice.pushDir(
                                    EasyMock.eq(
                                            new File(
                                                    tmpFolder,
                                                    "target/testcases/propertyinfoserializer_tests")),
                                    EasyMock.eq("/data/local/tmp/propertyinfoserializer_tests"),
                                    EasyMock.capture(capture)))
                    .andReturn(true);
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            EasyMock.replay(mMockDevice);
            mPreparer.setUp(mTestInfo);
            EasyMock.verify(mMockDevice);
            // The x86 folder was not filtered
            Set<String> capValue = capture.getValue();
            assertFalse(capValue.contains("x86"));
        } finally {
            FileUtil.recursiveDelete(tmpFolder);
        }
    }

    @Test
    public void testPush_moduleName_multiAbi_repeatedName() throws Exception {
        mOptionSetter.setOptionValue("push", "lib64->/data/local/tmp/lib");
        mPreparer.setAbi(new Abi("arm64-v8a", "64"));

        mPreparer.setInvocationContext(createModuleWithName("bionic"));
        IDeviceBuildInfo info = new DeviceBuildInfo();
        File tmpFolder = FileUtil.createTempDir("push-file-tests-dir");
        try {
            File libX86File =
                    new File(
                            tmpFolder,
                            "target/testcases/bionic/lib64/bionic-loader-test-libs/dt_runpath_y/lib64/arm64/");
            FileUtil.mkdirsRWX(libX86File);
            info.setFile(BuildInfoFileKey.TESTDIR_IMAGE, tmpFolder, "v1");
            EasyMock.expect(mMockDevice.doesFileExist("/data/local/tmp/lib")).andReturn(false);
            EasyMock.expect(mMockDevice.executeShellCommand("mkdir -p \"/data/local/tmp/lib\""))
                    .andReturn("");
            Capture<Set<String>> capture = new Capture<>();
            EasyMock.expect(
                            mMockDevice.pushDir(
                                    EasyMock.eq(
                                            new File(tmpFolder, "target/testcases/bionic/lib64")),
                                    EasyMock.eq("/data/local/tmp/lib"),
                                    EasyMock.capture(capture)))
                    .andReturn(true);
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            EasyMock.replay(mMockDevice);
            mPreparer.setUp(mTestInfo);
            EasyMock.verify(mMockDevice);
            // The x86 folder was not filtered
            Set<String> capValue = capture.getValue();
            assertFalse(capValue.contains("arm64"));
        } finally {
            FileUtil.recursiveDelete(tmpFolder);
        }
    }

    private IInvocationContext createModuleWithName(String name) {
        IInvocationContext moduleContext = new InvocationContext();
        moduleContext.addInvocationAttribute(ModuleDefinition.MODULE_NAME, name);
        return moduleContext;
    }
}

