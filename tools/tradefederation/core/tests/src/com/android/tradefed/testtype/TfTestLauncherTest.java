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

import static org.junit.Assert.assertTrue;

import com.android.tradefed.build.IFolderBuildInfo;
import com.android.tradefed.command.CommandOptions;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.IRunUtil.EnvPriority;
import com.android.tradefed.util.SystemUtil.EnvVariable;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.io.File;
import java.io.FileOutputStream;
import java.util.HashMap;

/** Unit tests for {@link TfTestLauncher} */
@RunWith(JUnit4.class)
public class TfTestLauncherTest {

    private static final String CONFIG_NAME = "FAKE_CONFIG";
    private static final String TEST_TAG = "FAKE_TAG";
    private static final String BUILD_BRANCH = "FAKE_BRANCH";
    private static final String BUILD_ID = "FAKE_BUILD_ID";
    private static final String BUILD_FLAVOR = "FAKE_FLAVOR";
    private static final String SUB_GLOBAL_CONFIG = "FAKE_GLOBAL_CONFIG";

    private TfTestLauncher mTfTestLauncher;
    private ITestInvocationListener mMockListener;
    private IRunUtil mMockRunUtil;
    private IFolderBuildInfo mMockBuildInfo;
    private IConfiguration mMockConfig;
    private TestInformation mTestInfo;

    @Before
    public void setUp() throws Exception {
        mMockListener = EasyMock.createMock(ITestInvocationListener.class);
        mMockRunUtil = EasyMock.createMock(IRunUtil.class);
        mMockBuildInfo = EasyMock.createMock(IFolderBuildInfo.class);
        mMockConfig = EasyMock.createMock(IConfiguration.class);

        mTfTestLauncher = new TfTestLauncher();
        mTfTestLauncher.setRunUtil(mMockRunUtil);
        mTfTestLauncher.setBuild(mMockBuildInfo);
        mTfTestLauncher.setEventStreaming(false);
        mTfTestLauncher.setConfiguration(mMockConfig);

        IInvocationContext context = new InvocationContext();
        context.addDeviceBuildInfo("device", mMockBuildInfo);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();

        EasyMock.expect(mMockConfig.getCommandOptions()).andStubReturn(new CommandOptions());

        OptionSetter setter = new OptionSetter(mTfTestLauncher);
        setter.setOptionValue("config-name", CONFIG_NAME);
        setter.setOptionValue("sub-global-config", SUB_GLOBAL_CONFIG);
    }

    /** Test {@link TfTestLauncher#run(TestInformation, ITestInvocationListener)} */
    @Test
    public void testRun() throws DeviceNotAvailableException {
        CommandResult cr = new CommandResult(CommandStatus.SUCCESS);
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(),
                                (FileOutputStream) EasyMock.anyObject(),
                                (FileOutputStream) EasyMock.anyObject(),
                                EasyMock.endsWith("/java"),
                                (String) EasyMock.anyObject(),
                                EasyMock.eq("--add-opens=java.base/java.nio=ALL-UNNAMED"),
                                EasyMock.eq(
                                        "--add-opens=java.base/sun.reflect.annotation=ALL-UNNAMED"),
                                EasyMock.eq("--add-opens=java.base/java.io=ALL-UNNAMED"),
                                EasyMock.eq("-cp"),
                                (String) EasyMock.anyObject(),
                                EasyMock.eq("com.android.tradefed.command.CommandRunner"),
                                EasyMock.eq(CONFIG_NAME),
                                EasyMock.eq("-n"),
                                EasyMock.eq("--test-tag"),
                                EasyMock.eq(TEST_TAG),
                                EasyMock.eq("--build-id"),
                                EasyMock.eq(BUILD_ID),
                                EasyMock.eq("--branch"),
                                EasyMock.eq(BUILD_BRANCH),
                                EasyMock.eq("--build-flavor"),
                                EasyMock.eq(BUILD_FLAVOR),
                                EasyMock.eq("--" + CommandOptions.INVOCATION_DATA),
                                EasyMock.eq(SubprocessTfLauncher.SUBPROCESS_TAG_NAME),
                                EasyMock.eq("true"),
                                EasyMock.eq("--subprocess-report-file"),
                                (String) EasyMock.anyObject()))
                .andReturn(cr);

        mMockRunUtil.unsetEnvVariable(GlobalConfiguration.GLOBAL_CONFIG_VARIABLE);
        mMockRunUtil.unsetEnvVariable(GlobalConfiguration.GLOBAL_CONFIG_SERVER_CONFIG_VARIABLE);
        mMockRunUtil.unsetEnvVariable(SubprocessTfLauncher.ANDROID_SERIAL_VAR);
        mMockRunUtil.unsetEnvVariable(EnvVariable.ANDROID_HOST_OUT_TESTCASES.name());
        mMockRunUtil.unsetEnvVariable(EnvVariable.ANDROID_TARGET_OUT_TESTCASES.name());
        mMockRunUtil.setEnvVariablePriority(EnvPriority.SET);
        mMockRunUtil.setEnvVariable(GlobalConfiguration.GLOBAL_CONFIG_VARIABLE, SUB_GLOBAL_CONFIG);

        EasyMock.expect(mMockBuildInfo.getTestTag()).andReturn(TEST_TAG);
        EasyMock.expect(mMockBuildInfo.getBuildBranch()).andReturn(BUILD_BRANCH).times(3);
        EasyMock.expect(mMockBuildInfo.getBuildFlavor()).andReturn(BUILD_FLAVOR).times(2);

        EasyMock.expect(mMockBuildInfo.getRootDir()).andReturn(new File(""));
        EasyMock.expect(mMockBuildInfo.getBuildId()).andReturn(BUILD_ID).times(3);
        mMockBuildInfo.addBuildAttribute(SubprocessTfLauncher.PARENT_PROC_TAG_NAME, "true");

        mMockListener.testLog((String)EasyMock.anyObject(), (LogDataType)EasyMock.anyObject(),
                (FileInputStreamSource)EasyMock.anyObject());
        EasyMock.expectLastCall().times(3);

        mMockListener.testRunStarted("temporaryFiles", 1);
        mMockListener.testRunStarted("StdErr", 1);
        for (int i = 0; i < 2; i++) {
            mMockListener.testStarted((TestDescription) EasyMock.anyObject());
            mMockListener.testEnded(
                    (TestDescription) EasyMock.anyObject(),
                    EasyMock.eq(new HashMap<String, Metric>()));
            mMockListener.testRunEnded(0, new HashMap<String, Metric>());
        }
        mMockListener.testRunStarted("elapsed-time", 1);
        mMockListener.testStarted(EasyMock.anyObject());
        mMockListener.testEnded(
                EasyMock.anyObject(), EasyMock.<HashMap<String, Metric>>anyObject());
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());

        EasyMock.replay(mMockBuildInfo, mMockRunUtil, mMockListener, mMockConfig);
        mTfTestLauncher.run(mTestInfo, mMockListener);
        EasyMock.verify(mMockBuildInfo, mMockRunUtil, mMockListener, mMockConfig);
    }

    /**
     * Test {@link TfTestLauncher#testTmpDirClean(File, ITestInvocationListener)}
     */
    @Test
    public void testTestTmpDirClean_success() {
        mMockListener.testRunStarted("temporaryFiles", 1);
        mMockListener.testStarted((TestDescription) EasyMock.anyObject());
        mMockListener.testEnded(
                (TestDescription) EasyMock.anyObject(), EasyMock.eq(new HashMap<String, Metric>()));
        mMockListener.testRunEnded(0, new HashMap<String, Metric>());
        File tmpDir = Mockito.mock(File.class);
        Mockito.when(tmpDir.list())
                .thenReturn(new String[] {"inv_123", "tradefed_global_log_123", "lc_cache",
                        "stage-android-build-api"});
        EasyMock.replay(mMockListener);
        mTfTestLauncher.testTmpDirClean(tmpDir, mMockListener);
        EasyMock.verify(mMockListener);
    }

    /**
     * Test {@link TfTestLauncher#testTmpDirClean(File, ITestInvocationListener)}
     *
     * Test should fail if there are extra files do not match expected pattern.
     */
    @Test
    public void testTestTmpDirClean_failExtraFile() {
        mTfTestLauncher.setBuild(mMockBuildInfo);
        EasyMock.expect(mMockBuildInfo.getBuildBranch()).andReturn(BUILD_BRANCH).times(1);
        mMockListener.testRunStarted("temporaryFiles", 1);
        mMockListener.testStarted((TestDescription) EasyMock.anyObject());
        mMockListener.testFailed(
                (TestDescription) EasyMock.anyObject(), (String) EasyMock.anyObject());
        mMockListener.testEnded(
                (TestDescription) EasyMock.anyObject(), EasyMock.eq(new HashMap<String, Metric>()));
        mMockListener.testRunEnded(0, new HashMap<String, Metric>());
        File tmpDir = Mockito.mock(File.class);
        Mockito.when(tmpDir.list()).thenReturn(new String[] {"extra_file"});
        EasyMock.replay(mMockListener, mMockBuildInfo);
        mTfTestLauncher.testTmpDirClean(tmpDir, mMockListener);
        EasyMock.verify(mMockListener, mMockBuildInfo);
    }

    /**
     * Test {@link TfTestLauncher#testTmpDirClean(File, ITestInvocationListener)}
     *
     * Test should fail if there are multiple files matching an expected pattern.
     */
    @Test
    public void testTestTmpDirClean_failMultipleFiles() {
        mTfTestLauncher.setBuild(mMockBuildInfo);
        EasyMock.expect(mMockBuildInfo.getBuildBranch()).andReturn(BUILD_BRANCH).times(1);
        mMockListener.testRunStarted("temporaryFiles", 1);
        mMockListener.testStarted((TestDescription) EasyMock.anyObject());
        mMockListener.testFailed(
                (TestDescription) EasyMock.anyObject(), (String) EasyMock.anyObject());
        mMockListener.testEnded(
                (TestDescription) EasyMock.anyObject(), EasyMock.eq(new HashMap<String, Metric>()));
        mMockListener.testRunEnded(0, new HashMap<String, Metric>());
        File tmpDir = Mockito.mock(File.class);
        Mockito.when(tmpDir.list()).thenReturn(new String[] {"inv_1", "inv_2"});
        EasyMock.replay(mMockListener, mMockBuildInfo);
        mTfTestLauncher.testTmpDirClean(tmpDir, mMockListener);
        EasyMock.verify(mMockListener, mMockBuildInfo);
    }

    /** Test that when code coverage option is on, we add the javaagent to the java arguments. */
    @Test
    public void testRunCoverage() throws Exception {
        OptionSetter setter = new OptionSetter(mTfTestLauncher);
        setter.setOptionValue("jacoco-code-coverage", "true");
        setter.setOptionValue("include-coverage", "com.android.tradefed*");
        setter.setOptionValue("include-coverage", "com.google.android.tradefed*");
        setter.setOptionValue("exclude-coverage", "com.test*");
        EasyMock.expect(mMockBuildInfo.getRootDir()).andReturn(new File(""));
        EasyMock.expect(mMockBuildInfo.getTestTag()).andReturn(TEST_TAG);
        EasyMock.expect(mMockBuildInfo.getBuildBranch()).andReturn(BUILD_BRANCH).times(2);
        EasyMock.expect(mMockBuildInfo.getBuildFlavor()).andReturn(BUILD_FLAVOR).times(2);
        EasyMock.expect(mMockBuildInfo.getBuildId()).andReturn(BUILD_ID).times(2);
        mMockRunUtil.unsetEnvVariable(GlobalConfiguration.GLOBAL_CONFIG_VARIABLE);
        mMockRunUtil.unsetEnvVariable(GlobalConfiguration.GLOBAL_CONFIG_SERVER_CONFIG_VARIABLE);
        mMockRunUtil.unsetEnvVariable(SubprocessTfLauncher.ANDROID_SERIAL_VAR);
        mMockRunUtil.unsetEnvVariable(EnvVariable.ANDROID_HOST_OUT_TESTCASES.name());
        mMockRunUtil.unsetEnvVariable(EnvVariable.ANDROID_TARGET_OUT_TESTCASES.name());
        mMockRunUtil.setEnvVariablePriority(EnvPriority.SET);
        mMockRunUtil.setEnvVariable(GlobalConfiguration.GLOBAL_CONFIG_VARIABLE, SUB_GLOBAL_CONFIG);
        EasyMock.replay(mMockBuildInfo, mMockRunUtil, mMockListener, mMockConfig);
        try {
            mTfTestLauncher.preRun();
            EasyMock.verify(mMockBuildInfo, mMockRunUtil, mMockListener);
            assertTrue(mTfTestLauncher.mCmdArgs.get(2).startsWith("-javaagent:"));
            assertTrue(
                    mTfTestLauncher
                            .mCmdArgs
                            .get(2)
                            .contains(
                                    "includes=com.android.tradefed*:com.google.android.tradefed*,"
                                            + "excludes=com.test*"));
        } finally {
            FileUtil.recursiveDelete(mTfTestLauncher.mTmpDir);
            mTfTestLauncher.cleanTmpFile();
        }
        EasyMock.verify(mMockBuildInfo, mMockRunUtil, mMockListener, mMockConfig);
    }
}
