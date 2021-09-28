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

import java.io.File;

/** Unit Tests for {@link DeviceStringPusher}. */
@RunWith(JUnit4.class)
public class DeviceStringPusherTest {
    private DeviceStringPusher mDeviceStringPusher;
    private ITestDevice mMockDevice;
    private IBuildInfo mMockBuildInfo;
    private TestInformation mTestInfo;

    @Before
    public void setUp() {
        mDeviceStringPusher = new DeviceStringPusher();
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        mMockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", mMockDevice);
        context.addDeviceBuildInfo("device", mMockBuildInfo);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
    }

    @Test(expected = TargetSetupError.class)
    public void testFail() throws Exception {
        OptionSetter optionSetter = new OptionSetter(mDeviceStringPusher);
        optionSetter.setOptionValue("file-path", "file");
        optionSetter.setOptionValue("file-content", "hi");
        EasyMock.expect(mMockDevice.doesFileExist("file")).andReturn(false).once();
        EasyMock.expect(mMockDevice.pushString("hi", "file")).andReturn(false).once();
        EasyMock.expect(mMockDevice.getDeviceDescriptor()).andReturn(null).once();
        EasyMock.replay(mMockDevice, mMockBuildInfo);

        mDeviceStringPusher.setUp(mTestInfo);
    }

    @Test
    public void testDoesntExist() throws Exception {
        OptionSetter optionSetter = new OptionSetter(mDeviceStringPusher);
        optionSetter.setOptionValue("file-path", "file");
        optionSetter.setOptionValue("file-content", "hi");
        EasyMock.expect(mMockDevice.doesFileExist("file")).andReturn(false).once();
        EasyMock.expect(mMockDevice.pushString("hi", "file")).andReturn(true).once();
        mMockDevice.deleteFile("file");
        EasyMock.replay(mMockDevice, mMockBuildInfo);

        mDeviceStringPusher.setUp(mTestInfo);
        mDeviceStringPusher.tearDown(mTestInfo, null);
        EasyMock.verify(mMockBuildInfo, mMockDevice);
    }

    @Test
    public void testAlreadyExists() throws Exception {
        OptionSetter optionSetter = new OptionSetter(mDeviceStringPusher);
        File file = new File("a");
        optionSetter.setOptionValue("file-path", "file");
        optionSetter.setOptionValue("file-content", "hi");
        EasyMock.expect(mMockDevice.doesFileExist("file")).andReturn(true).once();
        EasyMock.expect(mMockDevice.pullFile("file")).andReturn(file).once();
        EasyMock.expect(mMockDevice.pushString("hi", "file")).andReturn(true).once();
        EasyMock.expect(mMockDevice.pushFile(file, "file")).andReturn(true).once();
        EasyMock.replay(mMockDevice, mMockBuildInfo);

        mDeviceStringPusher.setUp(mTestInfo);
        mDeviceStringPusher.tearDown(mTestInfo, null);
        EasyMock.verify(mMockBuildInfo, mMockDevice);
    }
}
