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

import com.android.ddmlib.IDevice;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit Tests for {@link RootTargetPreparer}. */
@RunWith(JUnit4.class)
public class RootTargetPreparerTest {

    private RootTargetPreparer mRootTargetPreparer;
    private ITestDevice mMockDevice;
    private IDevice mMockIDevice;
    private IBuildInfo mMockBuildInfo;
    private TestInformation mTestInfo;

    @Before
    public void setUp() {
        mRootTargetPreparer = new RootTargetPreparer();
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        mMockIDevice = EasyMock.createMock(IDevice.class);
        EasyMock.expect(mMockDevice.getIDevice()).andStubReturn(mMockIDevice);
        mMockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", mMockDevice);
        context.addDeviceBuildInfo("device", mMockBuildInfo);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
    }

    @Test
    public void testSetUpSuccess_rootBefore() throws Exception {
        EasyMock.expect(mMockDevice.isAdbRoot()).andReturn(true).once();
        EasyMock.replay(mMockDevice, mMockBuildInfo);

        mRootTargetPreparer.setUp(mTestInfo);
        mRootTargetPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockDevice, mMockBuildInfo);
    }

    @Test
    public void testSetUpSuccess_notRootBefore() throws Exception {
        EasyMock.expect(mMockDevice.isAdbRoot()).andReturn(false).once();
        EasyMock.expect(mMockDevice.enableAdbRoot()).andReturn(true).once();
        EasyMock.expect(mMockDevice.disableAdbRoot()).andReturn(true).once();
        EasyMock.replay(mMockDevice, mMockBuildInfo);

        mRootTargetPreparer.setUp(mTestInfo);
        mRootTargetPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockDevice, mMockBuildInfo);
    }

    @Test(expected = TargetSetupError.class)
    public void testSetUpFail() throws Exception {
        EasyMock.expect(mMockDevice.isAdbRoot()).andReturn(false).once();
        EasyMock.expect(mMockDevice.enableAdbRoot()).andReturn(false).once();
        EasyMock.expect(mMockDevice.getDeviceDescriptor()).andReturn(null).once();
        EasyMock.replay(mMockDevice, mMockBuildInfo);

        mRootTargetPreparer.setUp(mTestInfo);
    }

    @Test
    public void testSetUpSuccess_rootBefore_forceUnroot() throws Exception {
        OptionSetter setter = new OptionSetter(mRootTargetPreparer);
        setter.setOptionValue("force-root", "false");

        EasyMock.expect(mMockDevice.isAdbRoot()).andReturn(true).once();
        EasyMock.expect(mMockDevice.disableAdbRoot()).andReturn(true).once();
        EasyMock.expect(mMockDevice.enableAdbRoot()).andReturn(true).once();
        EasyMock.replay(mMockDevice, mMockBuildInfo);

        mRootTargetPreparer.setUp(mTestInfo);
        mRootTargetPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockDevice, mMockBuildInfo);
    }

    @Test
    public void testSetUpSuccess_notRootBefore_forceUnroot() throws Exception {
        OptionSetter setter = new OptionSetter(mRootTargetPreparer);
        setter.setOptionValue("force-root", "false");

        EasyMock.expect(mMockDevice.isAdbRoot()).andReturn(false).once();
        EasyMock.replay(mMockDevice, mMockBuildInfo);

        mRootTargetPreparer.setUp(mTestInfo);
        mRootTargetPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockDevice, mMockBuildInfo);
    }

    @Test(expected = TargetSetupError.class)
    public void testSetUpFail_forceUnroot() throws Exception {
        OptionSetter setter = new OptionSetter(mRootTargetPreparer);
        setter.setOptionValue("force-root", "false");

        EasyMock.expect(mMockDevice.isAdbRoot()).andReturn(true).once();
        EasyMock.expect(mMockDevice.disableAdbRoot()).andReturn(false).once();
        EasyMock.expect(mMockDevice.getDeviceDescriptor()).andReturn(null).once();
        EasyMock.replay(mMockDevice, mMockBuildInfo);

        mRootTargetPreparer.setUp(mTestInfo);
    }

    @Test
    public void testSetUpFail_forceRoot_ignoresFailure() throws Exception {
        OptionSetter setter = new OptionSetter(mRootTargetPreparer);
        setter.setOptionValue("throw-on-error", "false");

        EasyMock.expect(mMockDevice.isAdbRoot()).andReturn(false).once();
        EasyMock.expect(mMockDevice.enableAdbRoot()).andReturn(false).once();
        EasyMock.expect(mMockDevice.getDeviceDescriptor()).andReturn(null).once();
        EasyMock.expect(mMockDevice.disableAdbRoot()).andReturn(true).once();
        EasyMock.replay(mMockDevice, mMockBuildInfo);

        mRootTargetPreparer.setUp(mTestInfo);
        mRootTargetPreparer.tearDown(mTestInfo, null);
    }

    @Test
    public void testSetUpFail_forceUnroot_ignoresFailure() throws Exception {
        OptionSetter setter = new OptionSetter(mRootTargetPreparer);
        setter.setOptionValue("force-root", "false");
        setter.setOptionValue("throw-on-error", "false");

        EasyMock.expect(mMockDevice.isAdbRoot()).andReturn(true).once();
        EasyMock.expect(mMockDevice.disableAdbRoot()).andReturn(false).once();
        EasyMock.expect(mMockDevice.getDeviceDescriptor()).andReturn(null).once();
        EasyMock.expect(mMockDevice.enableAdbRoot()).andReturn(true).once();
        EasyMock.replay(mMockDevice, mMockBuildInfo);

        mRootTargetPreparer.setUp(mTestInfo);
        mRootTargetPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockDevice, mMockBuildInfo);
    }
}
