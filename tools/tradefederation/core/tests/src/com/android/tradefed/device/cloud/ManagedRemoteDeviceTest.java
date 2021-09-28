/*
 * Copyright (C) 2019 The Android Open Source Project
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
package com.android.tradefed.device.cloud;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;

import com.android.ddmlib.IDevice;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.device.IDeviceMonitor;
import com.android.tradefed.device.IDeviceStateMonitor;
import com.android.tradefed.device.TestDeviceOptions;
import com.android.tradefed.log.ITestLogger;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

/** Unit tests for {@link ManagedRemoteDevice}. */
@RunWith(JUnit4.class)
public class ManagedRemoteDeviceTest {
    private ManagedRemoteDevice mDevice;
    private IDevice mIDevice;
    private IDeviceStateMonitor mStateMonitor;
    private IDeviceMonitor mDeviceMonitor;
    private ITestLogger mMockLogger;

    @BeforeClass
    public static void setUpClass() throws Exception {
        try {
            GlobalConfiguration.createGlobalConfiguration(new String[] {"empty"});
        } catch (IllegalStateException e) {
            // Ignore
        }
    }

    @Before
    public void setUp() {
        mIDevice = Mockito.mock(IDevice.class);
        mStateMonitor = Mockito.mock(IDeviceStateMonitor.class);
        mDeviceMonitor = Mockito.mock(IDeviceMonitor.class);
        mMockLogger = Mockito.mock(ITestLogger.class);
        mDevice = new ManagedRemoteDevice(mIDevice, mStateMonitor, mDeviceMonitor);
        mDevice.setTestLogger(mMockLogger);
    }

    @Test
    public void testGetOptions() {
        TestDeviceOptions originalOptions = new TestDeviceOptions();
        mDevice.setOptions(originalOptions);
        TestDeviceOptions get = mDevice.getOptions();
        assertFalse(get.equals(originalOptions));
        TestDeviceOptions get2 = mDevice.getOptions();
        // Same during the same session
        assertEquals(get2, get);

        mDevice.postInvocationTearDown(null);
        TestDeviceOptions get3 = mDevice.getOptions();
        assertNotEquals(get2, get3);
    }
}
