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

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.android.ddmlib.IDevice;
import com.android.tradefed.device.IManagedTestDevice;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link BatteryUnavailableDeviceRecovery} */
@RunWith(JUnit4.class)
public class BatteryUnavailableDeviceRecoveryTest {
    private BatteryUnavailableDeviceRecovery mRecoverer;
    private IManagedTestDevice mMockDevice;
    private IDevice mMockIDevice;

    @Before
    public void setUp() {
        mRecoverer = new BatteryUnavailableDeviceRecovery();
        mMockDevice = EasyMock.createMock(IManagedTestDevice.class);
        mMockIDevice = EasyMock.createMock(IDevice.class);
        EasyMock.expect(mMockDevice.getIDevice()).andStubReturn(mMockIDevice);
    }

    @Test
    public void testShouldSkip() {
        EasyMock.expect(mMockDevice.isStateBootloaderOrFastbootd()).andReturn(false);
        EasyMock.expect(mMockDevice.getBattery()).andReturn(50);
        EasyMock.replay(mMockDevice);
        assertTrue(mRecoverer.shouldSkip(mMockDevice));
        EasyMock.verify(mMockDevice);
    }

    @Test
    public void testShouldSkip_fastboot() {
        EasyMock.expect(mMockDevice.isStateBootloaderOrFastbootd()).andReturn(true);
        EasyMock.replay(mMockDevice);
        assertTrue(mRecoverer.shouldSkip(mMockDevice));
        EasyMock.verify(mMockDevice);
    }

    @Test
    public void testShouldSkip_recovery() {
        EasyMock.expect(mMockDevice.isStateBootloaderOrFastbootd()).andReturn(false);
        EasyMock.expect(mMockDevice.getBattery()).andReturn(null);
        EasyMock.replay(mMockDevice);
        // Recovery is needed, we don't skip
        assertFalse(mRecoverer.shouldSkip(mMockDevice));
        EasyMock.verify(mMockDevice);
    }
}
