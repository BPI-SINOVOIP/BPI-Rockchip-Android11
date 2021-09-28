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

package com.android.tradefed.targetprep;

import static org.junit.Assert.assertTrue;

import com.android.ddmlib.IDevice;
import com.android.ddmlib.IShellOutputReceiver;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.TestDeviceState;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.RunUtil;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.concurrent.TimeUnit;


/** Unit Tests for {@link RunCommandTargetPreparer} */
@RunWith(JUnit4.class)
public class RunCommandTargetPreparerTest {

    private static final int LONG_WAIT_TIME_MS = 200;
    private static final TestDeviceState ONLINE_STATE = TestDeviceState.ONLINE;

    private RunCommandTargetPreparer mPreparer = null;
    private ITestDevice mMockDevice = null;
    private IBuildInfo mMockBuildInfo = null;
    private TestInformation mTestInfo;

    @Before
    public void setUp() throws Exception {
        mPreparer = new RunCommandTargetPreparer();
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        mMockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        InvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", mMockDevice);
        context.addDeviceBuildInfo("device", mMockBuildInfo);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
    }

    /**
     * Test that {@link RunCommandTargetPreparer#setUp(TestInformation)} is properly going through
     * without exception when running a command.
     */
    @Test
    public void testSetUp() throws Exception {
        final String command = "mkdir test";
        OptionSetter setter = new OptionSetter(mPreparer);
        setter.setOptionValue("run-command", command);
        CommandResult res = new CommandResult();
        res.setStatus(CommandStatus.SUCCESS);
        res.setStdout("");
        EasyMock.expect(mMockDevice.executeShellV2Command(EasyMock.eq(command))).andReturn(res);
        EasyMock.replay(mMockDevice, mMockBuildInfo);
        mPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockDevice, mMockBuildInfo);
    }

    /**
     * Test that {@link RunCommandTargetPreparer#setUp(TestInformation)} is properly going through
     * without exception when running a command with timeout.
     */
    @Test
    public void testSetUp_withTimeout() throws Exception {
        final String command = "mkdir test";
        OptionSetter setter = new OptionSetter(mPreparer);
        setter.setOptionValue("run-command", command);
        setter.setOptionValue("run-command-timeout", "100");
        CommandResult res = new CommandResult();
        res.setStatus(CommandStatus.SUCCESS);
        res.setStdout("");
        EasyMock.expect(
                        mMockDevice.executeShellV2Command(
                                EasyMock.eq(command),
                                EasyMock.eq(100l),
                                EasyMock.eq(TimeUnit.MILLISECONDS),
                                EasyMock.eq(0)))
                .andReturn(res);
        EasyMock.replay(mMockDevice, mMockBuildInfo);
        mPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockDevice, mMockBuildInfo);
    }

    /**
     * Test that {@link RunCommandTargetPreparer#setUp(TestInformation)} and {@link
     * RunCommandTargetPreparer#tearDown(TestInformation, Throwable)} are properly skipped when
     * disabled and no command is ran.
     */
    @Test
    public void testDisabled() throws Exception {
        final String command = "mkdir test";
        OptionSetter setter = new OptionSetter(mPreparer);
        setter.setOptionValue("run-command", command);
        setter.setOptionValue("disable", "true");
        EasyMock.replay(mMockDevice, mMockBuildInfo);
        assertTrue(mPreparer.isDisabled());
        EasyMock.verify(mMockDevice, mMockBuildInfo);
    }

    /**
     * Test that {@link RunCommandTargetPreparer#tearDown(TestInformation, Throwable)} is properly
     * going through without exception when running a command.
     */
    @Test
    public void testTearDown() throws Exception {
        final String command = "mkdir test";
        OptionSetter setter = new OptionSetter(mPreparer);
        setter.setOptionValue("teardown-command", command);
        CommandResult result = new CommandResult(CommandStatus.SUCCESS);
        EasyMock.expect(mMockDevice.executeShellV2Command(EasyMock.eq(command))).andReturn(result);
        EasyMock.replay(mMockDevice, mMockBuildInfo);
        mPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockDevice, mMockBuildInfo);
    }

    /**
     * Test that {@link RunCommandTargetPreparer#setUp(TestInformation)} and {@link
     * RunCommandTargetPreparer#tearDown(TestInformation, Throwable)} is properly going through
     * without exception when running a background command.
     */
    @Test
    public void testBgCmd() throws Exception {
        final String command = "mkdir test";
        OptionSetter setter = new OptionSetter(mPreparer);
        setter.setOptionValue("run-bg-command", command);
        IDevice mMockIDevice = EasyMock.createMock(IDevice.class);
        EasyMock.expect(mMockIDevice.getSerialNumber()).andReturn("SERIAL").anyTimes();
        IShellOutputReceiver mMockReceiver = EasyMock.createMock(IShellOutputReceiver.class);
        mMockReceiver.addOutput((byte[]) EasyMock.anyObject(), EasyMock.anyInt(),
                EasyMock.anyInt());
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(mMockIDevice).atLeastOnce();
        EasyMock.expect(mMockDevice.getSerialNumber()).andReturn("SERIAL").atLeastOnce();
        mMockIDevice.executeShellCommand(EasyMock.eq(command), EasyMock.same(mMockReceiver),
                EasyMock.anyLong(), EasyMock.eq(TimeUnit.MILLISECONDS));
        EasyMock.expect(mMockDevice.getDeviceState()).andReturn(ONLINE_STATE).atLeastOnce();
        EasyMock.replay(mMockDevice, mMockBuildInfo);
        mPreparer.setUp(mTestInfo);
        RunUtil.getDefault().sleep(LONG_WAIT_TIME_MS);
        mPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockDevice, mMockBuildInfo);
    }
}
