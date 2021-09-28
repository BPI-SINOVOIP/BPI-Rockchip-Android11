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
package com.android.tradefed.targetprep;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit Tests for {@link DisableSELinuxTargetPreparerTest}. */
@RunWith(JUnit4.class)
public class DisableSELinuxTargetPreparerTest {

    private DisableSELinuxTargetPreparer mDisableSELinuxTargetPreparer;
    private ITestDevice mMockDevice;
    private IBuildInfo mMockBuildInfo;
    private TestInformation mTestInfo;
    private static final String PERMISSIVE = "Permissive";
    private static final String ENFORCED = "Enforced";
    private static final String GETENFORCE = "getenforce";
    private static final String SETENFORCE = "setenforce ";

    @Before
    public void setUp() {
        mDisableSELinuxTargetPreparer = new DisableSELinuxTargetPreparer();
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        mMockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", mMockDevice);
        context.addDeviceBuildInfo("device", mMockBuildInfo);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
    }

    @Test
    public void testSetUpSuccess_permissive() throws Exception {
        CommandResult result = new CommandResult();
        result.setStdout(PERMISSIVE);
        result.setStatus(CommandStatus.SUCCESS);
        EasyMock.expect(mMockDevice.executeShellV2Command(GETENFORCE)).andReturn(result).once();
        EasyMock.replay(mMockDevice, mMockBuildInfo);

        mDisableSELinuxTargetPreparer.setUp(mTestInfo);
        mDisableSELinuxTargetPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockDevice, mMockBuildInfo);
    }

    @Test
    public void testSetUpSuccess_enforced_rootBefore() throws Exception {
        CommandResult result = new CommandResult();
        result.setStdout(ENFORCED);
        result.setStatus(CommandStatus.SUCCESS);
        EasyMock.expect(mMockDevice.executeShellV2Command(GETENFORCE)).andReturn(result).once();
        EasyMock.expect(mMockDevice.isAdbRoot()).andReturn(true).once();
        EasyMock.expect(mMockDevice.executeShellV2Command(SETENFORCE + "0"))
                .andReturn(result)
                .once();
        EasyMock.expect(mMockDevice.executeShellV2Command(SETENFORCE + "1"))
                .andReturn(result)
                .once();
        EasyMock.replay(mMockDevice, mMockBuildInfo);

        mDisableSELinuxTargetPreparer.setUp(mTestInfo);
        mDisableSELinuxTargetPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockDevice, mMockBuildInfo);
    }

    @Test
    public void testSetUpSuccess_enforced_notRootBefore() throws Exception {
        CommandResult result = new CommandResult();
        result.setStdout(ENFORCED);
        result.setStatus(CommandStatus.SUCCESS);
        EasyMock.expect(mMockDevice.executeShellV2Command(GETENFORCE)).andReturn(result).once();
        EasyMock.expect(mMockDevice.isAdbRoot()).andReturn(false).once();
        EasyMock.expect(mMockDevice.enableAdbRoot()).andReturn(true).times(2);
        EasyMock.expect(mMockDevice.disableAdbRoot()).andReturn(true).times(2);

        EasyMock.expect(mMockDevice.executeShellV2Command(SETENFORCE + "0"))
                .andReturn(result)
                .once();
        EasyMock.expect(mMockDevice.executeShellV2Command(SETENFORCE + "1"))
                .andReturn(result)
                .once();
        EasyMock.replay(mMockDevice, mMockBuildInfo);

        mDisableSELinuxTargetPreparer.setUp(mTestInfo);
        mDisableSELinuxTargetPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockDevice, mMockBuildInfo);
    }

    @Test(expected = TargetSetupError.class)
    public void testSetUp_rootFail() throws Exception {
        CommandResult result = new CommandResult();
        result.setStdout(ENFORCED);
        EasyMock.expect(mMockDevice.executeShellV2Command(GETENFORCE)).andReturn(result).once();
        EasyMock.expect(mMockDevice.isAdbRoot()).andReturn(false).once();
        EasyMock.expect(mMockDevice.enableAdbRoot()).andReturn(false).once();
        EasyMock.expect(mMockDevice.getDeviceDescriptor()).andReturn(null).once();
        try {
            EasyMock.replay(mMockDevice, mMockBuildInfo);
            mDisableSELinuxTargetPreparer.setUp(mTestInfo);
        } finally {
            EasyMock.verify(mMockDevice, mMockBuildInfo);
        }
    }

    @Test(expected = TargetSetupError.class)
    public void testSetUp_disableSELinuxFail() throws Exception {
        CommandResult result = new CommandResult();
        result.setStdout(ENFORCED);
        result.setStatus(CommandStatus.FAILED);
        EasyMock.expect(mMockDevice.getDeviceDescriptor()).andReturn(null);
        EasyMock.expect(mMockDevice.executeShellV2Command(GETENFORCE)).andReturn(result).once();
        EasyMock.expect(mMockDevice.isAdbRoot()).andReturn(false).once();
        EasyMock.expect(mMockDevice.enableAdbRoot()).andReturn(true).once();
        EasyMock.expect(mMockDevice.executeShellV2Command(SETENFORCE + "0"))
                .andReturn(result)
                .once();
        try {
            EasyMock.replay(mMockDevice, mMockBuildInfo);
            mDisableSELinuxTargetPreparer.setUp(mTestInfo);
        } finally {
            EasyMock.verify(mMockDevice, mMockBuildInfo);
        }
    }
}
