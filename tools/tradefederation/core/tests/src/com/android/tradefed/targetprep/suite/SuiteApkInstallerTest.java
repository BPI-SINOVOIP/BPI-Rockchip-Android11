/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.tradefed.targetprep.suite;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.ExecutionFiles.FilesKey;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.util.FileUtil;

import org.easymock.EasyMock;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.io.File;
import java.io.IOException;

/** Unit test for {@link SuiteApkInstaller} */
@RunWith(JUnit4.class)
public class SuiteApkInstallerTest {

    private SuiteApkInstaller mPreparer;
    private IBuildInfo mMockBuildInfo;
    private ITestDevice mMockDevice;
    private TestInformation mTestInfo;
    private File mTmpDepDir;

    @Before
    public void setUp() throws IOException {
        mPreparer = new SuiteApkInstaller();
        mMockBuildInfo = Mockito.mock(IBuildInfo.class);
        mMockDevice = Mockito.mock(ITestDevice.class);
        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", mMockDevice);
        context.addDeviceBuildInfo("device", mMockBuildInfo);
        mTmpDepDir = FileUtil.createTempDir("suite-apk-installer-dep");
        mTestInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(context)
                        .setDependenciesFolder(mTmpDepDir)
                        .build();
    }

    @After
    public void tearDown() {
        FileUtil.recursiveDelete(mTmpDepDir);
    }

    /**
     * Tests that when $ANDROID_TARGET_OUT_TESTCASES is defined it is returned, we do not check
     * ROOT_DIR.
     */
    @Test
    public void testGetLocalPathForFilename_withVariable() throws Exception {
        File apk = FileUtil.createTempFile("testapk", ".apk", mTmpDepDir);
        File res = mPreparer.getLocalPathForFilename(mTestInfo, apk.getName());
        verify(mMockBuildInfo, times(0)).getBuildAttributes();
        assertNotNull(res);
        assertEquals(apk.getAbsolutePath(), res.getAbsolutePath());
    }

    /**
     * Tests that when $ANDROID_TARGET_OUT_TESTCASES is defined but is not a directory, we check and
     * return ROOT_DIR instead.
     */
    @Test
    public void testGetTestsDir_notDir() throws Exception {
        File varDir = FileUtil.createTempFile("suite-apk-installer-var", ".txt");
        mTestInfo.executionFiles().put(FilesKey.TARGET_TESTS_DIRECTORY, varDir);
        File tmpDir = FileUtil.createTempDir("suite-apk-installer");
        mTestInfo.executionFiles().put(FilesKey.TESTS_DIRECTORY, tmpDir);
        File apkFile = FileUtil.createTempFile("apk-test", ".apk", tmpDir);
        try {
            File res = mPreparer.getLocalPathForFilename(mTestInfo, apkFile.getName());
            assertNotNull(res);
            assertEquals(apkFile.getAbsolutePath(), res.getAbsolutePath());
        } finally {
            FileUtil.recursiveDelete(varDir);
            FileUtil.recursiveDelete(tmpDir);
        }
    }

    /**
     * Test that {@link SuiteApkInstaller#getLocalPathForFilename(TestInformation, String)} returns
     * the apk file when found.
     */
    @Test
    public void testGetLocalPathForFileName() throws Exception {
        File tmpApk = FileUtil.createTempFile("suite-apk-installer", ".apk");
        mTestInfo.executionFiles().put(tmpApk.getName(), tmpApk);
        try {
            File apk = mPreparer.getLocalPathForFilename(mTestInfo, tmpApk.getName());
            assertEquals(tmpApk.getAbsolutePath(), apk.getAbsolutePath());
        } finally {
            FileUtil.deleteFile(tmpApk);
        }
    }

    /**
     * Test that {@link SuiteApkInstaller#getLocalPathForFilename(TestInformation, String)} throws
     * an exception when the apk file is not found.
     */
    @Test
    public void testGetLocalPathForFileName_noFound() throws Exception {
        File tmpApk = FileUtil.createTempFile("suite-apk-installer", ".apk");
        try {
            mPreparer.getLocalPathForFilename(mTestInfo, "no_exist");
            fail("Should have thrown an exception.");
        } catch (TargetSetupError expected) {
            // expected
        } finally {
            FileUtil.deleteFile(tmpApk);
        }
    }

    /**
     * Test that {@link SuiteApkInstaller#getLocalPathForFilename(TestInformation, String)} returns
     * the apk file located in IDeviceBuildInfo.getTestsDir().
     */
    @Test
    public void testGetLocalPathForFileName_testsDir() throws Exception {
        IDeviceBuildInfo deviceBuildInfo = EasyMock.createMock(IDeviceBuildInfo.class);
        File tmpDir = null;
        try {
            tmpDir = FileUtil.createTempDir("test");
            mTestInfo.executionFiles().put(FilesKey.TESTS_DIRECTORY, tmpDir);
            File tmpApk = FileUtil.createTempFile("suite-apk-installer", ".apk", tmpDir);
            EasyMock.replay(deviceBuildInfo);
            mTestInfo.getContext().addDeviceBuildInfo("device", deviceBuildInfo);
            File apk = mPreparer.getLocalPathForFilename(mTestInfo, tmpApk.getName());
            assertEquals(tmpApk.getAbsolutePath(), apk.getAbsolutePath());
            EasyMock.verify(deviceBuildInfo);
        } finally {
            FileUtil.recursiveDelete(tmpDir);
        }
    }

    /** If the file is found directly in the build info keys, use it. */
    @Test
    public void testGetLocalPathForFileName_inBuildKey() throws Exception {
        File tmpApk = FileUtil.createTempFile("suite-apk-installer", ".apk");
        mTestInfo.executionFiles().put("foo.apk", tmpApk);
        try {
            File apk = mPreparer.getLocalPathForFilename(mTestInfo, "foo.apk");
            assertEquals(tmpApk.getAbsolutePath(), apk.getAbsolutePath());
        } finally {
            FileUtil.deleteFile(tmpApk);
        }
    }

    /**
     * Test that {@link SuiteApkInstaller#getLocalPathForFilename(TestInformation, String)} returns
     * the apk file retrieved from remote artifacts.
     */
    @Test
    public void testGetLocalPathForFileName_remoteZip() throws Exception {
        IDeviceBuildInfo deviceBuildInfo = EasyMock.createMock(IDeviceBuildInfo.class);
        File tmpDir = null;
        try {
            tmpDir = FileUtil.createTempDir("test");
            mTestInfo.executionFiles().put(FilesKey.TESTS_DIRECTORY, tmpDir);
            // Change the name so direct file search will return null.
            File tmpApk = FileUtil.createTempFile("suite-apk-installer-2", ".apk", tmpDir);
            EasyMock.expect(
                            deviceBuildInfo.stageRemoteFile(
                                    EasyMock.eq("suite-apk-installer.apk"), EasyMock.eq(tmpDir)))
                    .andReturn(tmpApk);
            mTestInfo.getContext().addDeviceBuildInfo("device", deviceBuildInfo);
            EasyMock.replay(deviceBuildInfo);
            File apk = mPreparer.getLocalPathForFilename(mTestInfo, "suite-apk-installer.apk");
            assertEquals(tmpApk.getAbsolutePath(), apk.getAbsolutePath());
            EasyMock.verify(deviceBuildInfo);
        } finally {
            FileUtil.recursiveDelete(tmpDir);
        }
    }

    /** If the file is found in the build shared resources directory, use it. */
    @Test
    public void testGetLocalPathForFileName_inDependenciesDir() throws Exception {
        File tmpApk = FileUtil.createTempFile("suite-apk-installer", ".apk", mTmpDepDir);
        try {
            File apk = mPreparer.getLocalPathForFilename(mTestInfo, tmpApk.getName());
            assertEquals(tmpApk.getAbsolutePath(), apk.getAbsolutePath());
        } finally {
            FileUtil.deleteFile(tmpApk);
        }
    }
}
