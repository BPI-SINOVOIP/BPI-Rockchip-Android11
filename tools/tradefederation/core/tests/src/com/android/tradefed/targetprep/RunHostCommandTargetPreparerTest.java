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

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.IRunUtil;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnitRunner;

import java.io.OutputStream;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Unit test for {@link RunHostCommandTargetPreparer}. */
@RunWith(MockitoJUnitRunner.class)
public final class RunHostCommandTargetPreparerTest {

    private static final String DEVICE_SERIAL = "123456";
    private static final String FULL_COMMAND = "command    \t\t\t  \t  argument $SERIAL";

    @Mock private ITestDevice mDevice;
    @Mock private RunHostCommandTargetPreparer.BgCommandLog mBgCommandLog;
    @Mock private IRunUtil mRunUtil;
    private RunHostCommandTargetPreparer mPreparer;
    private TestInformation mTestInfo;

    @Before
    public void setUp() {
        when(mDevice.getSerialNumber()).thenReturn(DEVICE_SERIAL);
        mPreparer =
                new RunHostCommandTargetPreparer() {
                    @Override
                    protected IRunUtil getRunUtil() {
                        return mRunUtil;
                    }

                    @Override
                    protected List<BgCommandLog> createBgCommandLogs() {
                        return Collections.singletonList(mBgCommandLog);
                    }
                };
        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", mDevice);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
    }

    @Test
    public void testSetUp() throws Exception {
        OptionSetter optionSetter = new OptionSetter(mPreparer);
        optionSetter.setOptionValue("host-setup-command", FULL_COMMAND);
        optionSetter.setOptionValue("host-cmd-timeout", "10");

        CommandResult result = new CommandResult(CommandStatus.SUCCESS);
        when(mRunUtil.runTimedCmd(anyLong(), any())).thenReturn(result);

        // Verify timeout and command (split, removed whitespace, and device serial)
        mPreparer.setUp(mTestInfo);
        verify(mRunUtil).runTimedCmd(eq(10L), eq("command"), eq("argument"), eq(DEVICE_SERIAL));
    }

    @Test
    public void testSetUp_withWorkDir() throws Exception {
        final OptionSetter optionSetter = new OptionSetter(mPreparer);
        optionSetter.setOptionValue("work-dir", "/working/directory");
        optionSetter.setOptionValue("host-setup-command", "command");
        optionSetter.setOptionValue("host-cmd-timeout", "10");

        CommandResult result = new CommandResult(CommandStatus.SUCCESS);
        when(mRunUtil.runTimedCmd(anyLong(), any())).thenReturn(result);

        // Verify working directory and command execution
        mPreparer.setUp(mTestInfo);
        verify(mRunUtil).setWorkingDir(any());
        verify(mRunUtil).runTimedCmd(eq(10L), eq("command"));
    }

    @Test(expected = TargetSetupError.class)
    public void testSetUp_withErrors() throws Exception {
        OptionSetter optionSetter = new OptionSetter(mPreparer);
        optionSetter.setOptionValue("host-setup-command", "command");
        optionSetter.setOptionValue("host-cmd-timeout", "10");

        // Verify that failed commands will throw exception during setup
        CommandResult result = new CommandResult(CommandStatus.FAILED);
        when(mRunUtil.runTimedCmd(anyLong(), any())).thenReturn(result);
        mPreparer.setUp(mTestInfo);
    }

    @Test
    public void testTearDown() throws Exception {
        OptionSetter optionSetter = new OptionSetter(mPreparer);
        optionSetter.setOptionValue("host-teardown-command", FULL_COMMAND);
        optionSetter.setOptionValue("host-cmd-timeout", "10");

        CommandResult result = new CommandResult(CommandStatus.SUCCESS);
        when(mRunUtil.runTimedCmd(anyLong(), any())).thenReturn(result);

        // Verify timeout and command (split, removed whitespace, and device serial)
        mPreparer.tearDown(mTestInfo, null);
        verify(mRunUtil).runTimedCmd(eq(10L), eq("command"), eq("argument"), eq(DEVICE_SERIAL));
    }

    @Test
    public void testTearDown_withError() throws Exception {
        OptionSetter optionSetter = new OptionSetter(mPreparer);
        optionSetter.setOptionValue("host-teardown-command", "command");
        optionSetter.setOptionValue("host-cmd-timeout", "10");

        // Verify that failed commands will NOT throw exception during teardown
        CommandResult result = new CommandResult(CommandStatus.FAILED);
        when(mRunUtil.runTimedCmd(anyLong(), any())).thenReturn(result);
        mPreparer.tearDown(mTestInfo, null);
    }

    @Test
    public void testBgCommand() throws Exception {
        OptionSetter optionSetter = new OptionSetter(mPreparer);
        optionSetter.setOptionValue("host-background-command", FULL_COMMAND);

        when(mRunUtil.runCmdInBackground(anyList(), any())).thenReturn(mock(Process.class));
        OutputStream os = mock(OutputStream.class);
        when(mBgCommandLog.getOutputStream()).thenReturn(os);

        // Verify command (split, removed whitespace, and device serial) and output stream
        mPreparer.setUp(mTestInfo);
        verify(mRunUtil)
                .runCmdInBackground(
                        eq(Arrays.asList("command", "argument", DEVICE_SERIAL)), eq(os));
    }
}
