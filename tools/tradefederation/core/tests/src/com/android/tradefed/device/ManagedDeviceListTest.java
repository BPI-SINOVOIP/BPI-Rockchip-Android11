/*
 * Copyright (C) 2014 The Android Open Source Project
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
package com.android.tradefed.device;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import com.android.ddmlib.IDevice;
import com.android.tradefed.device.IManagedTestDevice.DeviceEventResponse;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.HashSet;
import java.util.Set;

/** Unit tests for {@link ManagedDeviceList}. */
@RunWith(JUnit4.class)
public class ManagedDeviceListTest {

    private ManagedDeviceList mManagedDeviceList;

    @Before
    public void setUp() {
        mManagedDeviceList = new ManagedDeviceList(new IManagedTestDeviceFactory() {

            @Override
            public IManagedTestDevice createDevice(IDevice stubDevice) {
                // use real TestDevice to get allocation state machine
                return new TestDevice(stubDevice, EasyMock.createNiceMock(
                        IDeviceStateMonitor.class), null);
            }

            @Override
            public void setFastbootEnabled(boolean enable) {
                // ignore
            }
        });
    }

    /**
     * Basic test for {@link ManagedDeviceList#find(String)} and {@link
     * ManagedDeviceList#findOrCreate(IDevice)}
     */
    @Test
    public void testFindOrCreate() {
        // verify find returns null when list is empty
        assertNull(mManagedDeviceList.find("foo"));
        // verify device is created
        ITestDevice d = mManagedDeviceList.findOrCreate(new StubDevice("foo"));
        assertNotNull(d);
        // verify device can be found
        assertEquals(d, mManagedDeviceList.find("foo"));
        // verify same device is found, and new one is not created
        assertEquals(d, mManagedDeviceList.findOrCreate(new StubDevice("foo")));
        assertEquals(1, mManagedDeviceList.size());
    }

    /**
     * Test that {@link ManagedDeviceList#findOrCreate(IDevice)} ignores devices with invalid
     * serials
     */
    @Test
    public void testFindOrCreate_invalidSerial() {
        assertNull(mManagedDeviceList.findOrCreate(new StubDevice("????")));
    }

    /** Basic test for {@link ManagedDeviceList#allocate(IDeviceSelection)} */
    @Test
    public void testAllocate() {
        // verify allocate fails when no devices are in list
        assertNull(mManagedDeviceList.allocate(DeviceManager.ANY_DEVICE_OPTIONS));
        IManagedTestDevice d = mManagedDeviceList.findOrCreate(new StubDevice("foo"));
        assertNotNull(d);
        // verify allocate fails because device is not available
        assertNull(mManagedDeviceList.allocate(DeviceManager.ANY_DEVICE_OPTIONS));
        d.handleAllocationEvent(DeviceEvent.FORCE_AVAILABLE);
        // verify allocate succeeds because device is available
        assertNotNull(mManagedDeviceList.allocate(DeviceManager.ANY_DEVICE_OPTIONS));
        // verify allocate fails because only device is already allocated
        assertNull(mManagedDeviceList.allocate(DeviceManager.ANY_DEVICE_OPTIONS));
    }

    /**
     * Basic test for {@link ManagedDeviceList#handleDeviceEvent(IManagedTestDevice, DeviceEvent)}
     */
    @Test
    public void testHandleDeviceEvent() {
        // verify new device can be created
        IManagedTestDevice d = mManagedDeviceList.findOrCreate(new StubDevice("foo"));
        assertNotNull(d);
        d.handleAllocationEvent(DeviceEvent.FORCE_ALLOCATE_REQUEST);
        // verify allocated device remains in list on disconnect
        mManagedDeviceList.handleDeviceEvent(d, DeviceEvent.DISCONNECTED);
        assertEquals(1, mManagedDeviceList.size());
        d.handleAllocationEvent(DeviceEvent.FREE_AVAILABLE);
        // verify available device is removed from list on disconnect
        mManagedDeviceList.handleDeviceEvent(d, DeviceEvent.DISCONNECTED);
        assertEquals(0, mManagedDeviceList.size());
    }

    /**
     * Test for {@link ManagedDeviceList#updateFastbootStates(Set, boolean)} when device switch to
     * fastboot state.
     */
    @Test
    public void testUpdateFastbootState() {
        IManagedTestDevice mockDevice = EasyMock.createMock(IManagedTestDevice.class);
        EasyMock.expect(mockDevice.getSerialNumber()).andReturn("serial1");
        mockDevice.setDeviceState(TestDeviceState.FASTBOOT);
        EasyMock.expectLastCall();
        mManagedDeviceList.add(mockDevice);
        assertEquals(1, mManagedDeviceList.size());
        EasyMock.replay(mockDevice);
        Set<String> serialFastbootSet = new HashSet<>();
        serialFastbootSet.add("serial1");
        mManagedDeviceList.updateFastbootStates(serialFastbootSet, false);
        EasyMock.verify(mockDevice);
        // Device is still showing in the list of device
        assertEquals(1, mManagedDeviceList.size());
    }

    /**
     * Test for {@link ManagedDeviceList#updateFastbootStates(Set, boolean)} when device was in
     * fastboot and appears to be gone now.
     */
    @Test
    public void testUpdateFastbootState_gone() {
        IManagedTestDevice mockDevice = EasyMock.createMock(IManagedTestDevice.class);
        EasyMock.expect(mockDevice.getSerialNumber()).andStubReturn("serial1");
        EasyMock.expect(mockDevice.getDeviceState()).andReturn(TestDeviceState.FASTBOOT);
        mockDevice.setDeviceState(TestDeviceState.NOT_AVAILABLE);
        EasyMock.expectLastCall();
        DeviceEventResponse der = new DeviceEventResponse(DeviceAllocationState.Unknown, true);
        EasyMock.expect(mockDevice.handleAllocationEvent(DeviceEvent.DISCONNECTED)).andReturn(der);
        mManagedDeviceList.add(mockDevice);
        assertEquals(1, mManagedDeviceList.size());
        EasyMock.replay(mockDevice);
        Set<String> serialFastbootSet = new HashSet<>();
        mManagedDeviceList.updateFastbootStates(serialFastbootSet, false);
        EasyMock.verify(mockDevice);
        // Device has been removed from list
        assertEquals(0, mManagedDeviceList.size());
    }
}
