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
package com.android.tradefed.suite.checker;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.suite.checker.StatusCheckerResult.CheckStatus;
import com.android.tradefed.util.ProcessInfo;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;


/** Unit tests for {@link SystemServerStatusChecker} */
@RunWith(JUnit4.class)
public class SystemServerStatusCheckerTest {

    private SystemServerStatusChecker mChecker;
    private ITestDevice mMockDevice;

    @Before
    public void setUp() throws DeviceNotAvailableException {
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mMockDevice.getSerialNumber()).andStubReturn("SERIAL");
        EasyMock.expect(mMockDevice.getApiLevel()).andStubReturn(29);
        mChecker =
                new SystemServerStatusChecker() {
                    @Override
                    protected long getCurrentTime() {
                        return 500L;
                    }
                };
    }

    /** Test that system checker pass if system_server didn't restart. */
    @Test
    public void testSystemServerProcessNotRestarted() throws Exception {
        EasyMock.expect(mMockDevice.getProcessByName(EasyMock.eq("system_server")))
                .andReturn(new ProcessInfo("system", 914, "system_server", 1559091922L));
        EasyMock.expect(mMockDevice.deviceSoftRestarted(EasyMock.anyObject())).andReturn(false);
        EasyMock.replay(mMockDevice);
        assertEquals(CheckStatus.SUCCESS, mChecker.preExecutionCheck(mMockDevice).getStatus());
        assertEquals(CheckStatus.SUCCESS, mChecker.postExecutionCheck(mMockDevice).getStatus());
        EasyMock.verify(mMockDevice);
    }

    /** Test that system checker fail if system_server restarted without device reboot. */
    @Test
    public void testSystemServerProcessRestartedWithoutDeviceReboot() throws Exception {
        EasyMock.expect(mMockDevice.getProcessByName(EasyMock.eq("system_server")))
                .andReturn(new ProcessInfo("system", 914, "system_server", 1559091922L));
        EasyMock.expect(mMockDevice.deviceSoftRestarted(EasyMock.anyObject())).andReturn(true);
        EasyMock.replay(mMockDevice);
        assertEquals(CheckStatus.SUCCESS, mChecker.preExecutionCheck(mMockDevice).getStatus());
        StatusCheckerResult result = mChecker.postExecutionCheck(mMockDevice);
        assertEquals(CheckStatus.FAILED, result.getStatus());
        assertTrue(result.isBugreportNeeded());
        EasyMock.verify(mMockDevice);
    }

    /** Test that system checker fail if system_server restarted with device reboot. */
    @Test
    public void testSystemServerProcessRestartedWithAbnormalDeviceReboot() throws Exception {
        EasyMock.expect(mMockDevice.getProcessByName(EasyMock.eq("system_server")))
                .andReturn(new ProcessInfo("system", 914, "system_server", 1559091922L));
        EasyMock.expect(mMockDevice.deviceSoftRestarted(EasyMock.anyObject()))
                .andThrow(new RuntimeException("abnormal reboot"));
        EasyMock.replay(mMockDevice);
        assertEquals(CheckStatus.SUCCESS, mChecker.preExecutionCheck(mMockDevice).getStatus());
        StatusCheckerResult result = mChecker.postExecutionCheck(mMockDevice);
        assertEquals(CheckStatus.FAILED, result.getStatus());
        assertTrue(result.isBugreportNeeded());
        EasyMock.verify(mMockDevice);
    }

    /**
     * Test that if fail to get system_server process at preExecutionCheck, we skip the
     * system_server check in postExecution.
     */
    @Test
    public void testFailToGetSystemServerProcess() throws Exception {
        EasyMock.expect(mMockDevice.getProcessByName(EasyMock.eq("system_server"))).andReturn(null);
        mMockDevice.reboot();
        EasyMock.replay(mMockDevice);
        assertEquals(CheckStatus.FAILED, mChecker.preExecutionCheck(mMockDevice).getStatus());
        assertEquals(CheckStatus.SUCCESS, mChecker.postExecutionCheck(mMockDevice).getStatus());
        EasyMock.verify(mMockDevice);
    }

}
