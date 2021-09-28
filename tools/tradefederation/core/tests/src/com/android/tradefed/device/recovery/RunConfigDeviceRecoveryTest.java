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
package com.android.tradefed.device.recovery;

import static org.junit.Assert.assertEquals;

import com.android.ddmlib.IDevice;
import com.android.tradefed.command.ICommandScheduler;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceAllocationState;
import com.android.tradefed.device.DeviceManager.FastbootDevice;
import com.android.tradefed.device.IDeviceManager;
import com.android.tradefed.device.IManagedTestDevice;
import com.android.tradefed.device.ITestDevice;

import org.easymock.Capture;
import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link RunConfigDeviceRecovery}. */
@RunWith(JUnit4.class)
public class RunConfigDeviceRecoveryTest {

    private RunConfigDeviceRecovery mRecoverer;
    private IManagedTestDevice mMockTestDevice;
    private IDevice mMockIDevice;
    private IDeviceManager mMockDeviceManager;
    private ICommandScheduler mMockScheduler;

    @Before
    public void setup() throws Exception {
        mMockTestDevice = EasyMock.createMock(IManagedTestDevice.class);
        mMockIDevice = EasyMock.createMock(IDevice.class);
        mMockDeviceManager = EasyMock.createMock(IDeviceManager.class);
        mMockScheduler = EasyMock.createMock(ICommandScheduler.class);
        EasyMock.expect(mMockTestDevice.getSerialNumber()).andStubReturn("serial");
        EasyMock.expect(mMockTestDevice.getIDevice()).andStubReturn(mMockIDevice);
        mRecoverer =
                new RunConfigDeviceRecovery() {
                    @Override
                    protected IDeviceManager getDeviceManager() {
                        return mMockDeviceManager;
                    }

                    @Override
                    protected ICommandScheduler getCommandScheduler() {
                        return mMockScheduler;
                    }
                };
        OptionSetter setter = new OptionSetter(mRecoverer);
        setter.setOptionValue("recovery-config-name", "empty");
    }

    @Test
    public void testRecoverDevice_allocated() {
        List<IManagedTestDevice> devices = new ArrayList<>();
        devices.add(mMockTestDevice);
        EasyMock.expect(mMockTestDevice.getAllocationState())
                .andReturn(DeviceAllocationState.Allocated);
        replay();
        mRecoverer.recoverDevices(devices);
        verify();
    }

    @Test
    public void testRecoverDevice_offline() throws Exception {
        List<IManagedTestDevice> devices = new ArrayList<>();
        devices.add(mMockTestDevice);
        EasyMock.expect(mMockTestDevice.getAllocationState())
                .andReturn(DeviceAllocationState.Available);
        ITestDevice device = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mMockDeviceManager.forceAllocateDevice("serial")).andReturn(device);

        mMockScheduler.execCommand(EasyMock.anyObject(), EasyMock.eq(device), EasyMock.anyObject());

        replay();
        mRecoverer.recoverDevices(devices);
        verify();
    }

    /** Test that FastbootDevice are considered for recovery. */
    @Test
    public void testRecoverDevice_fastboot() throws Exception {
        EasyMock.reset(mMockTestDevice);
        EasyMock.expect(mMockTestDevice.getIDevice()).andStubReturn(new FastbootDevice("serial"));
        EasyMock.expect(mMockTestDevice.getSerialNumber()).andStubReturn("serial");
        List<IManagedTestDevice> devices = new ArrayList<>();
        devices.add(mMockTestDevice);
        EasyMock.expect(mMockTestDevice.getAllocationState())
                .andReturn(DeviceAllocationState.Available);
        ITestDevice device = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mMockDeviceManager.forceAllocateDevice("serial")).andReturn(device);

        mMockScheduler.execCommand(EasyMock.anyObject(), EasyMock.eq(device), EasyMock.anyObject());

        replay();
        mRecoverer.recoverDevices(devices);
        verify();
    }

    @Test
    public void testRecoverDevice_run() throws Exception {
        List<IManagedTestDevice> devices = new ArrayList<>();
        devices.add(mMockTestDevice);
        EasyMock.expect(mMockTestDevice.getAllocationState())
                .andReturn(DeviceAllocationState.Available);

        ITestDevice device = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mMockDeviceManager.forceAllocateDevice("serial")).andReturn(device);

        Capture<String[]> captured = new Capture<>();
        mMockScheduler.execCommand(
                EasyMock.anyObject(), EasyMock.eq(device), EasyMock.capture(captured));

        replay(device);
        mRecoverer.recoverDevices(devices);
        verify(device);

        String[] args = captured.getValue();
        assertEquals("empty", args[0]);
    }

    private void replay(Object... mocks) {
        EasyMock.replay(mMockTestDevice, mMockIDevice, mMockDeviceManager, mMockScheduler);
        EasyMock.replay(mocks);
    }

    private void verify(Object... mocks) {
        EasyMock.verify(mMockTestDevice, mMockIDevice, mMockDeviceManager, mMockScheduler);
        EasyMock.verify(mocks);
    }
}
