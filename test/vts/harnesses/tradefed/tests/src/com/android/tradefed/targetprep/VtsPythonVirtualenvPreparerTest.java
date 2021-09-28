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

package com.android.tradefed.targetprep;

import static org.easymock.EasyMock.anyLong;
import static org.easymock.EasyMock.expect;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;

import java.io.File;
import java.io.IOException;

/**
 * Unit tests for {@link VtsPythonVirtualenvPreparer}.</p>
 * TODO: add tests to cover a full end-to-end scenario.
 */
@RunWith(JUnit4.class)
public class VtsPythonVirtualenvPreparerTest {
    private VtsPythonVirtualenvPreparer mPreparer;
    private IRunUtil mMockRunUtil;
    @Mock private IBuildInfo mBuildInfo;

    @Before
    public void setUp() throws Exception {
        mMockRunUtil = EasyMock.createMock(IRunUtil.class);

        mPreparer = new VtsPythonVirtualenvPreparer() {
            @Override
            protected IRunUtil getRunUtil() {
                return mMockRunUtil;
            }
        };
        mPreparer.mVenvDir = new File("");
        mPreparer.mDepModules.add("enum");
    }

    /**
     * Test that the installation of dependencies and requirements file is as expected.
     */
    @Test
    public void testInstallDeps_reqFile_success() throws Exception {
        File requirementFile = FileUtil.createTempFile("reqfile", ".txt");
        try {
            mPreparer.setRequirementsFile(requirementFile);
            CommandResult result = new CommandResult(CommandStatus.SUCCESS);
            result.setStdout("output");
            result.setStderr("std err");
            // First check that the install requirements was attempted.
            expect(mMockRunUtil.runTimedCmd(anyLong(), EasyMock.eq(mPreparer.getPipPath()),
                           EasyMock.eq("install"), EasyMock.eq("-r"),
                           EasyMock.eq(requirementFile.getAbsolutePath())))
                    .andReturn(result);
            // Check that all default modules are installed
            addDefaultModuleExpectations(mMockRunUtil, result);
            EasyMock.replay(mMockRunUtil);
            mPreparer.installDeps();
            EasyMock.verify(mMockRunUtil);
        } finally {
            FileUtil.deleteFile(requirementFile);
        }
    }

    /**
     * Test that if an extra dependency module is required, we install it too.
     */
    @Test
    public void testInstallDeps_depModule_success() throws Exception {
        mPreparer.addDepModule("blahblah");
        CommandResult result = new CommandResult(CommandStatus.SUCCESS);
        result.setStdout("output");
        result.setStderr("std err");
        addDefaultModuleExpectations(mMockRunUtil, result);
        // The non default module provided is also attempted to be installed.
        expect(mMockRunUtil.runTimedCmd(anyLong(), EasyMock.eq(mPreparer.getPipPath()),
                       EasyMock.eq("install"), EasyMock.eq("blahblah")))
                .andReturn(result);
        mMockRunUtil.sleep(VtsPythonVirtualenvPreparer.PIP_INSTALL_DELAY);
        EasyMock.replay(mMockRunUtil);
        mPreparer.installDeps();
        EasyMock.verify(mMockRunUtil);
    }

    /**
     * Tests the value of PIP_INSTALL_DELAY is at least 1 second.
     */
    @Test
    public void test_PIP_INSTALL_DELAY_minimum_value() {
        assertTrue(VtsPythonVirtualenvPreparer.PIP_INSTALL_DELAY >= 1000);
    }

    /**
     * Test that an installation failure of the requirements file throws a {@link TargetSetupError}.
     */
    @Test
    public void testInstallDeps_reqFile_failure() throws Exception {
        File requirementFile = FileUtil.createTempFile("reqfile", ".txt");
        try {
            mPreparer.setRequirementsFile(requirementFile);
            CommandResult result = new CommandResult(CommandStatus.TIMED_OUT);
            result.setStdout("output");
            result.setStderr("std err");
            expect(mMockRunUtil.runTimedCmd(anyLong(), EasyMock.eq(mPreparer.getPipPath()),
                           EasyMock.eq("install"), EasyMock.eq("-r"),
                           EasyMock.eq(requirementFile.getAbsolutePath())))
                    .andReturn(result)
                    .times(VtsPythonVirtualenvPreparer.PIP_RETRY + 1);
            mMockRunUtil.sleep(EasyMock.anyLong());
            EasyMock.expectLastCall().times(VtsPythonVirtualenvPreparer.PIP_RETRY);
            EasyMock.replay(mMockRunUtil);
            IBuildInfo buildInfo = new BuildInfo();
            try {
                mPreparer.installDeps();
                fail("installDeps succeeded despite a failed command");
            } catch (TargetSetupError e) {
                assertTrue(buildInfo.getFile("PYTHONPATH") == null);
            }
            EasyMock.verify(mMockRunUtil);
        } finally {
            FileUtil.deleteFile(requirementFile);
        }
    }

    /**
     * Test that an installation failure of the dep module throws a {@link TargetSetupError}.
     */
    @Test
    public void testInstallDeps_depModule_failure() throws Exception {
        CommandResult result = new CommandResult(CommandStatus.TIMED_OUT);
        result.setStdout("output");
        result.setStderr("std err");
        expect(mMockRunUtil.runTimedCmd(
                       anyLong(), EasyMock.eq(mPreparer.getPipPath()), EasyMock.eq("list")))
                .andReturn(result);
        expect(mMockRunUtil.runTimedCmd(anyLong(), EasyMock.eq(mPreparer.getPipPath()),
                       EasyMock.eq("install"), EasyMock.eq("enum")))
                .andReturn(result)
                .times(VtsPythonVirtualenvPreparer.PIP_RETRY + 1);
        // If installing the dependency failed, an upgrade is attempted:
        expect(mMockRunUtil.runTimedCmd(anyLong(), EasyMock.eq(mPreparer.getPipPath()),
                       EasyMock.eq("install"), EasyMock.eq("--upgrade"), EasyMock.eq("enum")))
                .andReturn(result)
                .times(VtsPythonVirtualenvPreparer.PIP_RETRY + 1);
        mMockRunUtil.sleep(EasyMock.anyLong());
        EasyMock.expectLastCall().times(VtsPythonVirtualenvPreparer.PIP_RETRY);
        EasyMock.replay(mMockRunUtil);
        IBuildInfo buildInfo = new BuildInfo();
        try {
            mPreparer.installDeps();
            fail("installDeps succeeded despite a failed command");
        } catch (TargetSetupError e) {
            assertTrue(buildInfo.getFile("PYTHONPATH") == null);
        }
        EasyMock.verify(mMockRunUtil);
    }

    private void addDefaultModuleExpectations(IRunUtil mockRunUtil, CommandResult result) {
        expect(mockRunUtil.runTimedCmd(
                       anyLong(), EasyMock.eq(mPreparer.getPipPath()), EasyMock.eq("list")))
                .andReturn(result);
        expect(mockRunUtil.runTimedCmd(anyLong(), EasyMock.eq(mPreparer.getPipPath()),
                       EasyMock.eq("install"), EasyMock.eq("enum")))
                .andReturn(result);
        mMockRunUtil.sleep(VtsPythonVirtualenvPreparer.PIP_INSTALL_DELAY);
    }

    /**
     * Tests the functionality of createVirtualenv.
     * @throws IOException
     */
    @Test
    public void test_initVirtualenv_creationSuccess() throws IOException {
        // Create virutalenv dir command success
        CommandResult result = new CommandResult();
        result.setStatus(CommandStatus.SUCCESS);
        expect(mMockRunUtil.runTimedCmd(EasyMock.anyInt(), EasyMock.eq("virtualenv"),
                       EasyMock.anyObject(), EasyMock.anyObject(), EasyMock.anyObject()))
                .andReturn(result);
        EasyMock.replay(mMockRunUtil);

        File testDir = FileUtil.createTempDir("vts-test-dir");
        try {
            assertTrue(mPreparer.createVirtualenv(testDir));
        } finally {
            FileUtil.recursiveDelete(testDir);
        }
        EasyMock.verify(mMockRunUtil);
    }

    /**
     * Tests the functionality of createVirtualenv.
     */
    @Test
    public void test_initVirtualenv_creationFail_Errno26_waitSucceed() throws IOException {
        // Create virutalenv dir command success
        CommandResult result = new CommandResult(CommandStatus.FAILED);
        result.setStderr("...Errno 26...");
        expect(mMockRunUtil.runTimedCmd(EasyMock.anyLong(), EasyMock.eq("virtualenv"),
                       EasyMock.anyObject(), EasyMock.anyObject(), EasyMock.anyObject()))
                .andReturn(result)
                .once();
        CommandResult nextResult = new CommandResult(CommandStatus.SUCCESS);
        expect(mMockRunUtil.runTimedCmd(EasyMock.anyLong(), EasyMock.eq("virtualenv"),
                       EasyMock.anyObject(), EasyMock.anyObject(), EasyMock.anyObject()))
                .andReturn(nextResult)
                .once();
        mMockRunUtil.sleep(EasyMock.anyLong());

        EasyMock.replay(mMockRunUtil);
        File testDir = FileUtil.createTempDir("vts-test-dir");
        try {
            assertTrue(mPreparer.createVirtualenv(testDir));
        } finally {
            FileUtil.recursiveDelete(testDir);
        }
        EasyMock.verify(mMockRunUtil);
    }

    /**
     * Tests the functionality of createVirtualenv.
     * @throws IOException
     */
    @Test
    public void test_initVirtualenv_creationFail_Errno26_waitFailed() throws IOException {
        // Create virutalenv dir command success
        CommandResult result = new CommandResult();
        result.setStatus(CommandStatus.FAILED);
        result.setStderr("...Errno 26...");
        expect(mMockRunUtil.runTimedCmd(EasyMock.anyInt(), EasyMock.eq("virtualenv"),
                       EasyMock.anyObject(), EasyMock.anyObject(), EasyMock.anyObject()))
                .andReturn(result)
                .times(VtsPythonVirtualenvPreparer.PIP_RETRY + 1);
        mMockRunUtil.sleep(EasyMock.anyLong());
        EasyMock.expectLastCall().times(VtsPythonVirtualenvPreparer.PIP_RETRY);
        EasyMock.replay(mMockRunUtil);
        File testDir = FileUtil.createTempDir("vts-test-dir");
        try {
            assertFalse(mPreparer.createVirtualenv(testDir));
        } finally {
            FileUtil.recursiveDelete(testDir);
        }
        EasyMock.verify(mMockRunUtil);
    }

    /**
     * Tests the functionality of createVirtualenv.
     * @throws IOException
     */
    @Test
    public void test_initVirtualenv_creationFail_noErrno26() throws IOException {
        // Create virutalenv dir command success
        CommandResult result = new CommandResult();
        result.setStatus(CommandStatus.FAILED);
        result.setStderr("...");
        expect(mMockRunUtil.runTimedCmd(EasyMock.anyInt(), EasyMock.eq("virtualenv"),
                       EasyMock.anyObject(), EasyMock.anyObject(), EasyMock.anyObject()))
                .andReturn(result);
        EasyMock.replay(mMockRunUtil);

        File testDir = FileUtil.createTempDir("vts-test-dir");
        try {
            assertFalse(mPreparer.createVirtualenv(testDir));
        } finally {
            FileUtil.recursiveDelete(testDir);
        }
        EasyMock.verify(mMockRunUtil);
    }
}