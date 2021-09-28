/*
 * Copyright (C) 2010 The Android Open Source Project
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
import static org.junit.Assert.fail;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import com.android.ddmlib.IDevice;
import com.android.helper.aoa.UsbDevice;
import com.android.helper.aoa.UsbHelper;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.IRunUtil;

import com.google.common.util.concurrent.SettableFuture;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

/** Unit tests for {@link WaitDeviceRecovery}. */
@RunWith(JUnit4.class)
public class WaitDeviceRecoveryTest {

    private IRunUtil mMockRunUtil;
    private WaitDeviceRecovery mRecovery;
    private IDeviceStateMonitor mMockMonitor;
    private IDevice mMockDevice;
    private UsbHelper mMockUsbHelper;

    @Before
    public void setUp() throws Exception {
        mMockRunUtil = EasyMock.createMock(IRunUtil.class);
        mMockUsbHelper = Mockito.mock(UsbHelper.class);
        mRecovery =
                new WaitDeviceRecovery() {
                    @Override
                    protected IRunUtil getRunUtil() {
                        return mMockRunUtil;
                    }

                    @Override
                    UsbHelper getUsbHelper() {
                        return mMockUsbHelper;
                    }
                };
        mMockMonitor = EasyMock.createMock(IDeviceStateMonitor.class);
        EasyMock.expect(mMockMonitor.getSerialNumber()).andStubReturn("serial");
        mMockDevice = EasyMock.createMock(IDevice.class);
    }

    /**
     * Test {@link WaitDeviceRecovery#recoverDevice(IDeviceStateMonitor, boolean)} when devices
     * comes back online on its own accord.
     */
    @Test
    public void testRecoverDevice_success() throws DeviceNotAvailableException {
        // expect initial sleep
        mMockRunUtil.sleep(EasyMock.anyLong());
        mMockMonitor.waitForDeviceBootloaderStateUpdate();
        EasyMock.expect(mMockMonitor.getDeviceState()).andReturn(TestDeviceState.NOT_AVAILABLE);
        EasyMock.expect(mMockMonitor.waitForDeviceOnline(EasyMock.anyLong())).andReturn(mMockDevice);
        EasyMock.expect(mMockMonitor.waitForDeviceShell(EasyMock.anyLong())).andReturn(true);
        EasyMock.expect(mMockMonitor.waitForDeviceAvailable(EasyMock.anyLong())).andReturn(
                mMockDevice);
        EasyMock.expect(mMockMonitor.waitForDeviceOnline(EasyMock.anyLong())).andReturn(mMockDevice);
        replayMocks();
        mRecovery.recoverDevice(mMockMonitor, false);
        verifyMocks();
    }

    /**
     * Test {@link WaitDeviceRecovery#recoverDevice(IDeviceStateMonitor, boolean)} when device is
     * not available.
     */
    @Test
    public void testRecoverDevice_unavailable() {
        // expect initial sleep
        mMockRunUtil.sleep(EasyMock.anyLong());
        mMockMonitor.waitForDeviceBootloaderStateUpdate();
        EasyMock.expect(mMockMonitor.getDeviceState()).andStubReturn(TestDeviceState.NOT_AVAILABLE);
        EasyMock.expect(mMockMonitor.waitForDeviceOnline(EasyMock.anyLong())).andReturn(null);
        replayMocks();
        try {
            mRecovery.recoverDevice(mMockMonitor, false);
            fail("DeviceNotAvailableException not thrown");
        } catch (DeviceNotAvailableException e) {
            // expected
        }
        verifyMocks();
    }

    @Test
    public void testRecoverDevice_unavailable_recovers() throws Exception {
        // expect initial sleep
        mMockRunUtil.sleep(EasyMock.anyLong());
        mMockMonitor.waitForDeviceBootloaderStateUpdate();
        EasyMock.expect(mMockMonitor.getDeviceState()).andStubReturn(TestDeviceState.NOT_AVAILABLE);
        EasyMock.expect(mMockMonitor.waitForDeviceOnline(EasyMock.anyLong())).andReturn(null);

        UsbDevice mockDevice = Mockito.mock(UsbDevice.class);
        doReturn(mockDevice).when(mMockUsbHelper).getDevice("serial");
        EasyMock.expect(mMockMonitor.waitForDeviceAvailable()).andReturn(mMockDevice);
        EasyMock.expect(mMockMonitor.waitForDeviceOnline(EasyMock.anyLong()))
                .andReturn(mMockDevice);

        replayMocks();
        // Device recovers successfully
        mRecovery.recoverDevice(mMockMonitor, false);
        verifyMocks();
        verify(mockDevice).reset();
    }

    @Test
    public void testRecoverDevice_unavailable_recovery() throws Exception {
        // expect initial sleep
        mMockRunUtil.sleep(EasyMock.anyLong());
        mMockMonitor.waitForDeviceBootloaderStateUpdate();
        EasyMock.expect(mMockMonitor.getDeviceState()).andStubReturn(TestDeviceState.RECOVERY);
        EasyMock.expect(mMockMonitor.waitForDeviceOnline(EasyMock.anyLong())).andReturn(null);

        UsbDevice mockDevice = Mockito.mock(UsbDevice.class);
        doReturn(mockDevice).when(mMockUsbHelper).getDevice("serial");
        EasyMock.expect(mMockMonitor.waitForDeviceAvailable()).andReturn(null);

        EasyMock.expect(mMockMonitor.waitForDeviceInRecovery()).andReturn(mMockDevice);
        mMockDevice.reboot(null);
        EasyMock.expect(mMockMonitor.waitForDeviceAvailable()).andReturn(mMockDevice);
        EasyMock.expect(mMockMonitor.waitForDeviceOnline(EasyMock.anyLong()))
                .andReturn(mMockDevice);

        replayMocks();
        // Device recovers successfully
        mRecovery.recoverDevice(mMockMonitor, false);
        verifyMocks();
        verify(mockDevice).reset();
    }

    @Test
    public void testRecoverDevice_unavailable_recovery_fail() throws Exception {
        // expect initial sleep
        mMockRunUtil.sleep(EasyMock.anyLong());
        mMockMonitor.waitForDeviceBootloaderStateUpdate();
        EasyMock.expect(mMockMonitor.getDeviceState()).andReturn(TestDeviceState.RECOVERY).times(3);
        EasyMock.expect(mMockMonitor.waitForDeviceOnline(EasyMock.anyLong())).andReturn(null);

        UsbDevice mockDevice = Mockito.mock(UsbDevice.class);
        doReturn(mockDevice).when(mMockUsbHelper).getDevice("serial");
        EasyMock.expect(mMockMonitor.waitForDeviceAvailable()).andReturn(null);

        EasyMock.expect(mMockMonitor.waitForDeviceInRecovery()).andReturn(null);
        replayMocks();
        try {
            mRecovery.recoverDevice(mMockMonitor, false);
            fail("DeviceNotAvailableException not thrown");
        } catch (DeviceNotAvailableException e) {
            // expected
        }
        verifyMocks();
        verify(mockDevice).reset();
    }

    @Test
    public void testRecoverDevice_unavailable_fastboot() throws Exception {
        // expect initial sleep
        mMockRunUtil.sleep(EasyMock.anyLong());
        mMockMonitor.waitForDeviceBootloaderStateUpdate();
        EasyMock.expect(mMockMonitor.getDeviceState()).andStubReturn(TestDeviceState.FASTBOOT);
        CommandResult result = new CommandResult();
        result.setStatus(CommandStatus.SUCCESS);
        // expect reboot
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(),
                                EasyMock.eq("fastboot"),
                                EasyMock.eq("-s"),
                                EasyMock.eq("serial"),
                                EasyMock.eq("reboot")))
                .andReturn(result);

        EasyMock.expect(mMockMonitor.getFastbootSerialNumber()).andReturn("serial");
        EasyMock.expect(mMockMonitor.waitForDeviceOnline(EasyMock.anyLong())).andReturn(null);
        replayMocks();
        try {
            mRecovery.recoverDevice(mMockMonitor, false);
            fail("DeviceNotAvailableException not thrown");
        } catch (DeviceNotAvailableException e) {
            // expected
        }
        verifyMocks();
    }

    /**
     * Test {@link WaitDeviceRecovery#recoverDevice(IDeviceStateMonitor, boolean)} when device is
     * not responsive.
     */
    @Test
    public void testRecoverDevice_unresponsive() throws Exception {
        // expect initial sleep
        mMockRunUtil.sleep(EasyMock.anyLong());
        mMockMonitor.waitForDeviceBootloaderStateUpdate();
        EasyMock.expect(mMockMonitor.getDeviceState()).andReturn(TestDeviceState.ONLINE);
        EasyMock.expect(mMockMonitor.waitForDeviceOnline(EasyMock.anyLong()))
                .andReturn(mMockDevice).anyTimes();
        EasyMock.expect(mMockMonitor.waitForDeviceShell(EasyMock.anyLong())).andReturn(true);
        EasyMock.expect(mMockMonitor.waitForDeviceAvailable(EasyMock.anyLong()))
                .andReturn(null).anyTimes();
        mMockDevice.reboot((String)EasyMock.isNull());
        replayMocks();
        try {
            mRecovery.recoverDevice(mMockMonitor, false);
            fail("DeviceUnresponsiveException not thrown");
        } catch (DeviceUnresponsiveException e) {
            // expected
        }
        verifyMocks();
    }

    /**
     * Test {@link WaitDeviceRecovery#recoverDevice(IDeviceStateMonitor, boolean)} when device is in
     * fastboot.
     */
    @Test
    public void testRecoverDevice_fastboot() throws DeviceNotAvailableException {
        // expect initial sleep
        mMockRunUtil.sleep(EasyMock.anyLong());
        mMockMonitor.waitForDeviceBootloaderStateUpdate();
        EasyMock.expect(mMockMonitor.getDeviceState()).andReturn(TestDeviceState.FASTBOOT);
        CommandResult result = new CommandResult();
        result.setStatus(CommandStatus.SUCCESS);
        // expect reboot
        EasyMock.expect(mMockMonitor.getFastbootSerialNumber()).andReturn("serial");
        EasyMock.expect(mMockRunUtil.runTimedCmd(EasyMock.anyLong(), EasyMock.eq("fastboot"),
                EasyMock.eq("-s"), EasyMock.eq("serial"), EasyMock.eq("reboot"))).
                andReturn(result);
        EasyMock.expect(mMockMonitor.waitForDeviceOnline(EasyMock.anyLong()))
                .andReturn(mMockDevice);
        EasyMock.expect(mMockMonitor.waitForDeviceShell(EasyMock.anyLong())).andReturn(true);
        EasyMock.expect(mMockMonitor.waitForDeviceAvailable(EasyMock.anyLong())).andReturn(
                mMockDevice);
        EasyMock.expect(mMockMonitor.waitForDeviceOnline(EasyMock.anyLong()))
                .andReturn(mMockDevice);
        replayMocks();
        mRecovery.recoverDevice(mMockMonitor, false);
        verifyMocks();
    }

    /**
     * Test {@link WaitDeviceRecovery#recoverDeviceBootloader(IDeviceStateMonitor)} when device is
     * already in bootloader
     */
    @Test
    public void testRecoverDeviceBootloader_fastboot() throws DeviceNotAvailableException {
        mMockRunUtil.sleep(EasyMock.anyLong());
        // expect reboot
        EasyMock.expect(mMockRunUtil.runTimedCmd(EasyMock.anyLong(), EasyMock.eq("fastboot"),
                EasyMock.eq("-s"), EasyMock.eq("serial"), EasyMock.eq("reboot-bootloader"))).
                andReturn(new CommandResult(CommandStatus.SUCCESS));
        EasyMock.expect(mMockMonitor.waitForDeviceNotAvailable(EasyMock.anyLong())).andReturn(
                Boolean.TRUE);
        EasyMock.expect(mMockMonitor.waitForDeviceBootloader(EasyMock.anyLong())).andReturn(
                Boolean.TRUE).times(2);
        EasyMock.expect(mMockRunUtil.runTimedCmd(EasyMock.anyLong(), EasyMock.eq("fastboot"),
                EasyMock.eq("-s"), EasyMock.eq("serial"), EasyMock.eq("getvar"),
                EasyMock.eq("product"))).
                andReturn(new CommandResult(CommandStatus.SUCCESS));
        replayMocks();
        mRecovery.recoverDeviceBootloader(mMockMonitor);
        verifyMocks();
    }

    /**
     * Test {@link WaitDeviceRecovery#recoverDeviceBootloader(IDeviceStateMonitor)} when device is
     * unavailable but comes back to bootloader on its own
     */
    @Test
    public void testRecoverDeviceBootloader_unavailable() throws DeviceNotAvailableException {
        mMockRunUtil.sleep(EasyMock.anyLong());
        EasyMock.expect(mMockMonitor.waitForDeviceBootloader(EasyMock.anyLong())).andReturn(
                Boolean.FALSE);
        EasyMock.expect(mMockMonitor.getDeviceState()).andReturn(TestDeviceState.NOT_AVAILABLE);
        // expect reboot
        EasyMock.expect(mMockRunUtil.runTimedCmd(EasyMock.anyLong(), EasyMock.eq("fastboot"),
                EasyMock.eq("-s"), EasyMock.eq("serial"), EasyMock.eq("reboot-bootloader"))).
                andReturn(new CommandResult(CommandStatus.SUCCESS));
        EasyMock.expect(mMockMonitor.waitForDeviceNotAvailable(EasyMock.anyLong())).andReturn(
                Boolean.TRUE);
        EasyMock.expect(mMockMonitor.waitForDeviceBootloader(EasyMock.anyLong())).andReturn(
                Boolean.TRUE).times(2);
        EasyMock.expect(mMockRunUtil.runTimedCmd(EasyMock.anyLong(), EasyMock.eq("fastboot"),
                EasyMock.eq("-s"), EasyMock.eq("serial"), EasyMock.eq("getvar"),
                EasyMock.eq("product"))).
                andReturn(new CommandResult(CommandStatus.SUCCESS));
        replayMocks();
        mRecovery.recoverDeviceBootloader(mMockMonitor);
        verifyMocks();
    }

    /**
     * Test {@link WaitDeviceRecovery#recoverDeviceBootloader(IDeviceStateMonitor)} when device is
     * online when bootloader is expected
     */
    @Test
    public void testRecoverDeviceBootloader_online() throws Exception {
        mMockRunUtil.sleep(EasyMock.anyLong());
        EasyMock.expect(mMockMonitor.waitForDeviceBootloader(EasyMock.anyLong())).andReturn(
                Boolean.FALSE);
        EasyMock.expect(mMockMonitor.getDeviceState()).andReturn(TestDeviceState.ONLINE);
        EasyMock.expect(mMockMonitor.waitForDeviceOnline(EasyMock.anyLong()))
                .andReturn(mMockDevice);
        mMockDevice.reboot("bootloader");
        EasyMock.expect(mMockMonitor.waitForDeviceBootloader(EasyMock.anyLong())).andReturn(
                Boolean.TRUE);
        replayMocks();
        mRecovery.recoverDeviceBootloader(mMockMonitor);
        verifyMocks();
    }

    /**
     * Test {@link WaitDeviceRecovery#recoverDeviceBootloader(IDeviceStateMonitor)} when device is
     * initially unavailable, then comes online when bootloader is expected
     */
    @Test
    public void testRecoverDeviceBootloader_unavailable_online() throws Exception {
        mMockRunUtil.sleep(EasyMock.anyLong());
        EasyMock.expect(mMockMonitor.waitForDeviceBootloader(EasyMock.anyLong())).andReturn(
                Boolean.FALSE);
        EasyMock.expect(mMockMonitor.getDeviceState()).andReturn(TestDeviceState.NOT_AVAILABLE);
        EasyMock.expect(mMockMonitor.waitForDeviceBootloader(EasyMock.anyLong())).andReturn(
                Boolean.FALSE);
        EasyMock.expect(mMockMonitor.getDeviceState()).andReturn(TestDeviceState.ONLINE);
        EasyMock.expect(mMockMonitor.waitForDeviceOnline(EasyMock.anyLong()))
                .andReturn(mMockDevice);
        mMockDevice.reboot("bootloader");
        EasyMock.expect(mMockMonitor.waitForDeviceBootloader(EasyMock.anyLong())).andReturn(
                Boolean.TRUE);
        replayMocks();
        mRecovery.recoverDeviceBootloader(mMockMonitor);
        verifyMocks();
    }

    /**
     * Test {@link WaitDeviceRecovery#recoverDeviceBootloader(IDeviceStateMonitor)} when device is
     * unavailable
     */
    @Test
    public void testRecoverDeviceBootloader_unavailable_failure() throws Exception {
        mMockRunUtil.sleep(EasyMock.anyLong());
        EasyMock.expect(mMockMonitor.waitForDeviceBootloader(EasyMock.anyLong())).andStubReturn(
                Boolean.FALSE);
        EasyMock.expect(mMockMonitor.getDeviceState()).andStubReturn(TestDeviceState.NOT_AVAILABLE);
        replayMocks();
        try {
            mRecovery.recoverDeviceBootloader(mMockMonitor);
            fail("DeviceNotAvailableException not thrown");
        } catch (DeviceNotAvailableException e) {
            // expected
        }
        verifyMocks();
    }

    /**
     * Test {@link WaitDeviceRecovery#checkMinBatteryLevel(IDevice)} throws an exception if battery
     * level is not readable.
     */
    @Test
    public void testCheckMinBatteryLevel_unreadable() throws Exception {
        OptionSetter setter = new OptionSetter(mRecovery);
        setter.setOptionValue("min-battery-after-recovery", "50");
        SettableFuture<Integer> future = SettableFuture.create();
        future.set(null);
        EasyMock.expect(mMockDevice.getBattery()).andReturn(future);
        EasyMock.expect(mMockDevice.getSerialNumber()).andReturn("SERIAL");
        replayMocks();
        try {
            mRecovery.checkMinBatteryLevel(mMockDevice);
            fail("DeviceNotAvailableException not thrown");
        } catch (DeviceNotAvailableException expected) {
            assertEquals("Cannot read battery level but a min is required", expected.getMessage());
        }
        verifyMocks();
    }

    /**
     * Test {@link WaitDeviceRecovery#checkMinBatteryLevel(IDevice)} throws an exception if battery
     * level is below the minimal expected.
     */
    @Test
    public void testCheckMinBatteryLevel_belowLevel() throws Exception {
        OptionSetter setter = new OptionSetter(mRecovery);
        setter.setOptionValue("min-battery-after-recovery", "50");
        SettableFuture<Integer> future = SettableFuture.create();
        future.set(49);
        EasyMock.expect(mMockDevice.getBattery()).andReturn(future);
        EasyMock.expect(mMockDevice.getSerialNumber()).andReturn("SERIAL");
        replayMocks();
        try {
            mRecovery.checkMinBatteryLevel(mMockDevice);
            fail("DeviceNotAvailableException not thrown");
        } catch (DeviceNotAvailableException expected) {
            assertEquals("After recovery, device battery level 49 is lower than required minimum "
                    + "50", expected.getMessage());
        }
        verifyMocks();
    }

    /**
     * Test {@link WaitDeviceRecovery#checkMinBatteryLevel(IDevice)} returns without exception when
     * battery level after recovery is above or equals minimum expected.
     */
    @Test
    public void testCheckMinBatteryLevel() throws Exception {
        OptionSetter setter = new OptionSetter(mRecovery);
        setter.setOptionValue("min-battery-after-recovery", "50");
        SettableFuture<Integer> future = SettableFuture.create();
        future.set(50);
        EasyMock.expect(mMockDevice.getBattery()).andReturn(future);
        replayMocks();
        mRecovery.checkMinBatteryLevel(mMockDevice);
        verifyMocks();
    }

    /**
     * Verify all the mock objects
     */
    private void verifyMocks() {
        EasyMock.verify(mMockRunUtil, mMockMonitor, mMockDevice);
    }

    /**
     * Switch all the mock objects to replay mode
     */
    private void replayMocks() {
        EasyMock.replay(mMockRunUtil, mMockMonitor, mMockDevice);
    }

}
