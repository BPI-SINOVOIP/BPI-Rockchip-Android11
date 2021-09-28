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

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit Tests for {@link RebootTargetPreparer}. */
@RunWith(JUnit4.class)
public class RebootTargetPreparerTest {

    private RebootTargetPreparer mRebootTargetPreparer;
    private ITestDevice mMockDevice;
    private IBuildInfo mMockBuildInfo;
    private TestInformation mTestInfo;

    @Before
    public void setUp() {
        mRebootTargetPreparer = new RebootTargetPreparer();
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        mMockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", mMockDevice);
        context.addDeviceBuildInfo("device", mMockBuildInfo);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
    }

    @Test
    public void testSetUp() throws Exception {
        mMockDevice.reboot();
        EasyMock.expectLastCall().once();
        EasyMock.replay(mMockDevice, mMockBuildInfo);

        mRebootTargetPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockDevice, mMockBuildInfo);
    }
}
