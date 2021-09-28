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
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.ddmlib.AndroidDebugBridge.IDeviceChangeListener;
import com.android.ddmlib.IDevice;
import com.android.ddmlib.IDevice.DeviceState;
import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.config.IGlobalConfiguration;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.IManagedTestDevice.DeviceEventResponse;
import com.android.tradefed.host.HostOptions;
import com.android.tradefed.host.IHostOptions;
import com.android.tradefed.log.ILogRegistry.EventType;
import com.android.tradefed.util.ArrayUtil;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.ZipUtil;

import org.easymock.Capture;
import org.easymock.EasyMock;
import org.easymock.IAnswer;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Semaphore;

/** Unit tests for {@link DeviceManager}. */
@RunWith(JUnit4.class)
public class DeviceManagerTest {

    private static final String DEVICE_SERIAL = "serial";
    private static final String MAC_ADDRESS = "FF:FF:FF:FF:FF:FF";
    private static final String SIM_STATE = "READY";
    private static final String SIM_OPERATOR = "operator";

    private IAndroidDebugBridge mMockAdbBridge;
    private IDevice mMockIDevice;
    private IDeviceStateMonitor mMockStateMonitor;
    private IRunUtil mMockRunUtil;
    private IHostOptions mMockHostOptions;
    private IManagedTestDevice mMockTestDevice;
    private IManagedTestDeviceFactory mMockDeviceFactory;
    private IGlobalConfiguration mMockGlobalConfig;
    private DeviceSelectionOptions mDeviceSelections;

    /**
     * a reference to the DeviceManager's IDeviceChangeListener. Used for triggering device
     * connection events
     */
    private IDeviceChangeListener mDeviceListener;

    static class MockProcess extends Process {

        /**
         * {@inheritDoc}
         */
        @Override
        public void destroy() {
            // ignore
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public int exitValue() {
            return 0;
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public InputStream getErrorStream() {
            return null;
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public InputStream getInputStream() {
            return null;
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public OutputStream getOutputStream() {
            return null;
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public int waitFor() throws InterruptedException {
            return 0;
        }
    }

    @Before
    public void setUp() throws Exception {
        mMockAdbBridge = EasyMock.createNiceMock(IAndroidDebugBridge.class);
        mMockAdbBridge.addDeviceChangeListener((IDeviceChangeListener) EasyMock.anyObject());
        EasyMock.expectLastCall()
                .andDelegateTo(
                        new IAndroidDebugBridge() {
                            @Override
                            public void addDeviceChangeListener(
                                    final IDeviceChangeListener listener) {
                                mDeviceListener = listener;
                            }

                            @Override
                            public IDevice[] getDevices() {
                                return null;
                            }

                            @Override
                            public void removeDeviceChangeListener(
                                    IDeviceChangeListener listener) {}

                            @Override
                            public void init(boolean clientSupport, String adbOsLocation) {}

                            @Override
                            public void terminate() {}

                            @Override
                            public void disconnectBridge() {}

                            @Override
                            public String getAdbVersion(String adbOsLocation) {
                                return null;
                            }
                        });
        mMockIDevice = EasyMock.createMock(IDevice.class);
        mMockStateMonitor = EasyMock.createMock(IDeviceStateMonitor.class);
        mMockRunUtil = EasyMock.createMock(IRunUtil.class);
        mMockHostOptions = EasyMock.createMock(IHostOptions.class);

        mMockTestDevice = EasyMock.createMock(IManagedTestDevice.class);
        mMockDeviceFactory = new ManagedTestDeviceFactory(false, null, null) {
            @Override
            public IManagedTestDevice createDevice(IDevice idevice) {
                mMockTestDevice.setIDevice(idevice);
                return mMockTestDevice;
            }
            @Override
            protected CollectingOutputReceiver createOutputReceiver() {
                return new CollectingOutputReceiver() {
                    @Override
                    public String getOutput() {
                        return "/system/bin/pm";
                    }
                };
            }
            @Override
            public void setFastbootEnabled(boolean enable) {
                // ignore
            }
        };
        mMockGlobalConfig = EasyMock.createNiceMock(IGlobalConfiguration.class);
        EasyMock.expect(mMockGlobalConfig.getHostOptions()).andStubReturn(new HostOptions());

        EasyMock.expect(mMockIDevice.getSerialNumber()).andStubReturn(DEVICE_SERIAL);
        EasyMock.expect(mMockStateMonitor.getSerialNumber()).andStubReturn(DEVICE_SERIAL);
        mMockStateMonitor.waitForDeviceBootloaderStateUpdate();
        EasyMock.expectLastCall().anyTimes();
        EasyMock.expect(mMockIDevice.isEmulator()).andStubReturn(Boolean.FALSE);
        EasyMock.expect(mMockTestDevice.getMacAddress()).andStubReturn(MAC_ADDRESS);
        EasyMock.expect(mMockTestDevice.getSimState()).andStubReturn(SIM_STATE);
        EasyMock.expect(mMockTestDevice.getSimOperator()).andStubReturn(SIM_OPERATOR);
        final Capture<IDevice> capturedIDevice = new Capture<>();
        mMockTestDevice.setIDevice(EasyMock.capture(capturedIDevice));
        EasyMock.expectLastCall().anyTimes();
        EasyMock.expect(mMockTestDevice.getIDevice()).andStubAnswer(new IAnswer<IDevice>() {
            @Override
            public IDevice answer() throws Throwable {
                return capturedIDevice.getValue();
            }
        });
        EasyMock.expect(mMockTestDevice.getSerialNumber()).andStubAnswer(new IAnswer<String>() {
            @Override
            public String answer() throws Throwable {
                return capturedIDevice.getValue().getSerialNumber();
            }
        });
        EasyMock.expect(mMockTestDevice.getMonitor()).andStubReturn(mMockStateMonitor);
        EasyMock.expect(
                mMockRunUtil.runTimedCmd(EasyMock.anyLong(), (String) EasyMock.anyObject(),
                        (String) EasyMock.anyObject())).andStubReturn(new CommandResult());
        EasyMock.expect(
                mMockRunUtil.runTimedCmdSilently(EasyMock.anyLong(), (String) EasyMock.anyObject(),
                        (String) EasyMock.anyObject())).andStubReturn(new CommandResult());
        // Avoid any issue related to env. variable.
        mDeviceSelections =
                new DeviceSelectionOptions() {
                    @Override
                    public String fetchEnvironmentVariable(String name) {
                        return null;
                    }
                };
        EasyMock.expect(mMockGlobalConfig.getDeviceRequirements()).andStubReturn(mDeviceSelections);
    }

    private DeviceManager createDeviceManager(List<IDeviceMonitor> deviceMonitors,
            IDevice... devices) {
        DeviceManager mgr = createDeviceManagerNoInit();
        mgr.init(null, deviceMonitors, mMockDeviceFactory);
        for (IDevice device : devices) {
            mDeviceListener.deviceConnected(device);
        }
        return mgr;
    }

    private DeviceManager createDeviceManagerNoInit() {

        DeviceManager mgr =
                new DeviceManager() {
                    @Override
                    IAndroidDebugBridge createAdbBridge() {
                        return mMockAdbBridge;
                    }

                    @Override
                    void startFastbootMonitor() {}

                    @Override
                    void startDeviceRecoverer() {}

                    @Override
                    void logDeviceEvent(EventType event, String serial) {}

                    @Override
                    IDeviceStateMonitor createStateMonitor(IDevice device) {
                        return mMockStateMonitor;
                    }

                    @Override
                    IGlobalConfiguration getGlobalConfig() {
                        return mMockGlobalConfig;
                    }

                    @Override
                    IRunUtil getRunUtil() {
                        return mMockRunUtil;
                    }

                    @Override
                    IHostOptions getHostOptions() {
                        return mMockHostOptions;
                    }
                };
        mgr.setSynchronousMode(true);
        mgr.setMaxEmulators(0);
        mgr.setMaxNullDevices(0);
        mgr.setMaxTcpDevices(0);
        mgr.setMaxGceDevices(0);
        mgr.setMaxRemoteDevices(0);
        return mgr;
    }

    /**
     * Test @link DeviceManager#allocateDevice()} when a IDevice is present on DeviceManager
     * creation.
     */
    @Test
    public void testAllocateDevice() {
        setCheckAvailableDeviceExpectations();
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));

        replayMocks();
        DeviceManager manager = createDeviceManager(null, mMockIDevice);
        assertNotNull(manager.allocateDevice(mDeviceSelections));
        EasyMock.verify(mMockStateMonitor);
    }

    /**
     * Test {@link DeviceManager#allocateDevice(IDeviceSelection, boolean)} when device is returned.
     */
    @Test
    public void testAllocateDevice_match() {
        mDeviceSelections.addSerial(DEVICE_SERIAL);
        setCheckAvailableDeviceExpectations();
        EasyMock.expect(
                        mMockTestDevice.handleAllocationEvent(
                                DeviceEvent.EXPLICIT_ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));
        replayMocks();
        DeviceManager manager = createDeviceManager(null, mMockIDevice);
        assertEquals(mMockTestDevice, manager.allocateDevice(mDeviceSelections, false));
        EasyMock.verify(mMockStateMonitor);
    }

    /**
     * Test that when allocating a fake device we create a placeholder then delete it at the end.
     */
    @Test
    public void testAllocateDevice_match_temporary() {
        mDeviceSelections.setNullDeviceRequested(true);
        mDeviceSelections.addSerial(DEVICE_SERIAL);
        // Force create a device
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.FORCE_AVAILABLE))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Available, true));
        // Device get allocated
        EasyMock.expect(
                        mMockTestDevice.handleAllocationEvent(
                                DeviceEvent.EXPLICIT_ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));

        mMockTestDevice.stopLogcat();

        // De-allocate
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.FREE_UNKNOWN))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Unknown, true));

        replayMocks();
        DeviceManager manager = createDeviceManager(null);
        assertEquals(mMockTestDevice, manager.allocateDevice(mDeviceSelections, true));
        String serial = mMockTestDevice.getSerialNumber();
        assertTrue(serial.startsWith("null-device-temp-"));

        // Release device
        manager.freeDevice(mMockTestDevice, FreeDeviceState.AVAILABLE);
        // Check that temp device was deleted.
        DeviceSelectionOptions validation = new DeviceSelectionOptions();
        validation.setNullDeviceRequested(true);
        validation.addSerial(serial);
        // If we request the particular null-device again it doesn't exists.
        assertNull(manager.allocateDevice(validation, false));

        EasyMock.verify(mMockStateMonitor);
    }

    /**
     * Test {@link DeviceManager#allocateDevice(IDeviceSelection, boolean)} when stub emulator is
     * requested.
     */
    @Test
    public void testAllocateDevice_stubEmulator() {
        mDeviceSelections.setStubEmulatorRequested(true);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.FORCE_AVAILABLE))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Available, true));
        EasyMock.expect(mMockIDevice.isEmulator()).andStubReturn(Boolean.TRUE);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));
        replayMocks();
        DeviceManager mgr = createDeviceManagerNoInit();
        mgr.setMaxEmulators(1);
        mgr.init(null, null, mMockDeviceFactory);
        assertNotNull(mgr.allocateDevice(mDeviceSelections, false));
        verifyMocks();
    }

    /** Test that when a zipped fastboot file is provided we unpack it and use it. */
    @Test
    public void testUnpackZippedFastboot() throws Exception {
        File tmpDir = FileUtil.createTempDir("fake-fastbootdir");
        File fastboot = new File(tmpDir, "fastboot");
        FileUtil.writeToFile("TEST", fastboot);
        File zipDir = ZipUtil.createZip(tmpDir);
        replayMocks();
        DeviceManager mgr = createDeviceManagerNoInit();
        try {
            OptionSetter setter = new OptionSetter(mgr);
            setter.setOptionValue("fastboot-path", zipDir.getAbsolutePath());
            mgr.init(null, null, mMockDeviceFactory);
            assertTrue(mgr.getFastbootPath().contains("fastboot"));
            assertEquals("TEST", FileUtil.readStringFromFile(new File(mgr.getFastbootPath())));
            verifyMocks();
        } finally {
            FileUtil.recursiveDelete(tmpDir);
            FileUtil.deleteFile(zipDir);
            mgr.terminate();
        }
    }

    /** Test freeing an emulator */
    @Test
    public void testFreeDevice_emulator() {
        mDeviceSelections.setStubEmulatorRequested(true);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.FORCE_AVAILABLE))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Available, true));
        EasyMock.expect(mMockIDevice.isEmulator()).andStubReturn(Boolean.TRUE);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));
        mMockTestDevice.stopLogcat();
        EasyMock.expect(mMockTestDevice.getEmulatorProcess()).andStubReturn(new MockProcess());
        EasyMock.expect(mMockTestDevice.waitForDeviceNotAvailable(EasyMock.anyLong())).andReturn(
                Boolean.TRUE);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.FREE_AVAILABLE))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Available, true));
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));
        mMockTestDevice.stopEmulatorOutput();
        replayMocks();
        DeviceManager manager = createDeviceManagerNoInit();
        manager.setMaxEmulators(1);
        manager.init(null, null, mMockDeviceFactory);
        IManagedTestDevice emulator =
                (IManagedTestDevice) manager.allocateDevice(mDeviceSelections, false);
        assertNotNull(emulator);
        // a freed 'unavailable' emulator should be returned to the available
        // queue.
        manager.freeDevice(emulator, FreeDeviceState.UNAVAILABLE);
        // ensure device can be allocated again
        assertNotNull(manager.allocateDevice(mDeviceSelections, false));
        verifyMocks();
    }

    /**
     * Test {@link DeviceManager#allocateDevice(IDeviceSelection, boolean)} when a null device is
     * requested.
     */
    @Test
    public void testAllocateDevice_nullDevice() {
        mDeviceSelections.setNullDeviceRequested(true);
        EasyMock.expect(mMockIDevice.isEmulator()).andStubReturn(Boolean.FALSE);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.FORCE_AVAILABLE))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Available, true));
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));
        replayMocks();
        DeviceManager mgr = createDeviceManagerNoInit();
        mgr.setMaxNullDevices(1);
        mgr.init(null, null, mMockDeviceFactory);
        ITestDevice device = mgr.allocateDevice(mDeviceSelections, false);
        assertNotNull(device);
        assertTrue(device.getIDevice() instanceof NullDevice);
        verifyMocks();
    }

    /**
     * Test that DeviceManager will add devices on fastboot to available queue on startup, and that
     * they can be allocated.
     */
    @Test
    public void testAllocateDevice_fastboot() {
        EasyMock.reset(mMockRunUtil);
        // mock 'fastboot help' call
        EasyMock.expect(
                mMockRunUtil.runTimedCmdSilently(EasyMock.anyLong(), EasyMock.eq("fastboot"),
                        EasyMock.eq("help"))).andReturn(new CommandResult(CommandStatus.SUCCESS));

        // mock 'fastboot devices' call to return one device
        CommandResult fastbootResult = new CommandResult(CommandStatus.SUCCESS);
        fastbootResult.setStdout("serial        fastboot\n");
        EasyMock.expect(
                        mMockRunUtil.runTimedCmdSilently(
                                EasyMock.anyLong(),
                                EasyMock.eq("fastboot"),
                                EasyMock.eq("devices")))
                .andReturn(fastbootResult);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.FORCE_AVAILABLE))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Available, true));
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));
        replayMocks();
        DeviceManager manager = createDeviceManager(null);
        assertNotNull(manager.allocateDevice(mDeviceSelections));
        verifyMocks();
    }

    /** Test {@link DeviceManager#forceAllocateDevice(String)} when device is unknown */
    @Test
    public void testForceAllocateDevice() {
        replayMocks();
        DeviceManager manager = createDeviceManager(null);
        assertNull(manager.forceAllocateDevice("unknownserial"));
        verifyMocks();
    }

    /** Test {@link DeviceManager#forceAllocateDevice(String)} when device is available */
    @Test
    public void testForceAllocateDevice_available() {
        setCheckAvailableDeviceExpectations();
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.FORCE_ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));
        replayMocks();
        DeviceManager manager = createDeviceManager(null, mMockIDevice);
        assertNotNull(manager.forceAllocateDevice(DEVICE_SERIAL));
        verifyMocks();
    }

    /** Test {@link DeviceManager#forceAllocateDevice(String)} when device is already allocated */
    @Test
    public void testForceAllocateDevice_alreadyAllocated() {
        setCheckAvailableDeviceExpectations();
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.FORCE_ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, false));
        replayMocks();
        DeviceManager manager = createDeviceManager(null, mMockIDevice);
        assertNotNull(manager.allocateDevice(mDeviceSelections));
        assertNull(manager.forceAllocateDevice(DEVICE_SERIAL));
        verifyMocks();
    }

    /** Test method for {@link DeviceManager#freeDevice(ITestDevice, FreeDeviceState)}. */
    @Test
    public void testFreeDevice() {
        setCheckAvailableDeviceExpectations();
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.FREE_AVAILABLE))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Available, true));
        mMockTestDevice.stopLogcat();
        replayMocks();
        DeviceManager manager = createDeviceManager(null);
        mDeviceListener.deviceConnected(mMockIDevice);
        assertNotNull(manager.allocateDevice(mDeviceSelections));
        manager.freeDevice(mMockTestDevice, FreeDeviceState.AVAILABLE);
        verifyMocks();
    }

    /**
     * Verified that {@link DeviceManager#freeDevice(ITestDevice, FreeDeviceState)} ignores a call
     * with a device that has not been allocated.
     */
    @Test
    public void testFreeDevice_noop() {
        setCheckAvailableDeviceExpectations();
        IManagedTestDevice testDevice = EasyMock.createNiceMock(IManagedTestDevice.class);
        IDevice mockIDevice = EasyMock.createNiceMock(IDevice.class);
        EasyMock.expect(testDevice.getIDevice()).andReturn(mockIDevice);
        EasyMock.expect(mockIDevice.isEmulator()).andReturn(Boolean.FALSE);

        replayMocks(testDevice, mockIDevice);
        DeviceManager manager = createDeviceManager(null, mMockIDevice);
        manager.freeDevice(testDevice, FreeDeviceState.AVAILABLE);
        verifyMocks(testDevice, mockIDevice);
    }

    /**
     * Verified that {@link DeviceManager} calls {@link IManagedTestDevice#setIDevice(IDevice)} when
     * DDMS allocates a new IDevice on connection.
     */
    @Test
    public void testSetIDevice() {
        setCheckAvailableDeviceExpectations();
        EasyMock.expect(mMockTestDevice.getAllocationState())
                .andReturn(DeviceAllocationState.Available);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.DISCONNECTED)).andReturn(
                new DeviceEventResponse(DeviceAllocationState.Allocated, false));
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.CONNECTED_ONLINE))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, false));
        IDevice newMockDevice = EasyMock.createMock(IDevice.class);
        EasyMock.expect(newMockDevice.getSerialNumber()).andReturn(DEVICE_SERIAL).anyTimes();
        EasyMock.expect(newMockDevice.getState()).andReturn(DeviceState.ONLINE);
        mMockTestDevice.setDeviceState(TestDeviceState.NOT_AVAILABLE);
        mMockTestDevice.setDeviceState(TestDeviceState.ONLINE);
        replayMocks(newMockDevice);
        DeviceManager manager = createDeviceManager(null, mMockIDevice);
        ITestDevice device = manager.allocateDevice(mDeviceSelections);
        assertNotNull(device);
        // now trigger a device disconnect + reconnection
        mDeviceListener.deviceDisconnected(mMockIDevice);
        mDeviceListener.deviceConnected(newMockDevice);
        assertEquals(newMockDevice, device.getIDevice());
        verifyMocks(newMockDevice);
    }

    /**
     * Test {@link DeviceManager#allocateDevice()} when {@link DeviceManager#init()} has not been
     * called.
     */
    @Test
    public void testAllocateDevice_noInit() {
        try {
            createDeviceManagerNoInit().allocateDevice(mDeviceSelections);
            fail("IllegalStateException not thrown when manager has not been initialized");
        } catch (IllegalStateException e) {
            // expected
        }
    }

    /** Test {@link DeviceManager#init(IDeviceSelection, List)} with a global exclusion filter */
    @Test
    public void testInit_excludeDevice() throws Exception {
        EasyMock.expect(mMockIDevice.getState()).andReturn(DeviceState.ONLINE);
        mMockTestDevice.setDeviceState(TestDeviceState.ONLINE);
        EasyMock.expectLastCall();
        DeviceEventResponse der =
                new DeviceEventResponse(DeviceAllocationState.Checking_Availability, true);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.CONNECTED_ONLINE))
                .andReturn(der);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.AVAILABLE_CHECK_IGNORED))
                .andReturn(null);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Ignored, false));
        replayMocks();
        DeviceManager manager = createDeviceManagerNoInit();
        DeviceSelectionOptions excludeFilter = new DeviceSelectionOptions();
        excludeFilter.addExcludeSerial(mMockIDevice.getSerialNumber());
        manager.init(excludeFilter, null, mMockDeviceFactory);
        mDeviceListener.deviceConnected(mMockIDevice);
        assertEquals(1, manager.getDeviceList().size());
        assertNull(manager.allocateDevice(mDeviceSelections));
        verifyMocks();
    }

    /** Test {@link DeviceManager#init(IDeviceSelection, List)} with a global inclusion filter */
    @Test
    public void testInit_includeDevice() throws Exception {
        IDevice excludedDevice = EasyMock.createMock(IDevice.class);
        EasyMock.expect(excludedDevice.getSerialNumber()).andStubReturn("excluded");
        EasyMock.expect(excludedDevice.getState()).andStubReturn(DeviceState.ONLINE);
        EasyMock.expect(mMockIDevice.getState()).andStubReturn(DeviceState.ONLINE);
        EasyMock.expect(excludedDevice.isEmulator()).andStubReturn(Boolean.FALSE);
        mMockTestDevice.setDeviceState(TestDeviceState.ONLINE);
        DeviceEventResponse der =
                new DeviceEventResponse(DeviceAllocationState.Checking_Availability, true);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.CONNECTED_ONLINE))
                .andReturn(der);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.AVAILABLE_CHECK_IGNORED))
                .andReturn(null);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Ignored, false));
        replayMocks(excludedDevice);
        DeviceManager manager = createDeviceManagerNoInit();
        mDeviceSelections.addSerial(mMockIDevice.getSerialNumber());
        manager.init(mDeviceSelections, null, mMockDeviceFactory);
        mDeviceListener.deviceConnected(excludedDevice);
        assertEquals(1, manager.getDeviceList().size());
        // ensure excludedDevice cannot be allocated
        assertNull(manager.allocateDevice());
        verifyMocks(excludedDevice);
    }

    /** Verified that a disconnected device state gets updated */
    @Test
    public void testSetState_disconnected() {
        setCheckAvailableDeviceExpectations();
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.DISCONNECTED))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, false));
        mMockTestDevice.setDeviceState(TestDeviceState.NOT_AVAILABLE);
        replayMocks();
        DeviceManager manager = createDeviceManager(null, mMockIDevice);
        assertEquals(mMockTestDevice, manager.allocateDevice(mDeviceSelections));
        mDeviceListener.deviceDisconnected(mMockIDevice);
        verifyMocks();
    }

    /** Verified that a offline device state gets updated */
    @Test
    public void testSetState_offline() {
        setCheckAvailableDeviceExpectations();
        EasyMock.expect(mMockTestDevice.getAllocationState())
                .andReturn(DeviceAllocationState.Allocated);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));
        mMockTestDevice.setDeviceState(TestDeviceState.NOT_AVAILABLE);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.STATE_CHANGE_OFFLINE))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Unavailable, true));
        replayMocks();
        DeviceManager manager = createDeviceManager(null, mMockIDevice);
        assertEquals(mMockTestDevice, manager.allocateDevice(mDeviceSelections));
        IDevice newDevice = EasyMock.createMock(IDevice.class);
        EasyMock.expect(newDevice.getSerialNumber()).andReturn(DEVICE_SERIAL).anyTimes();
        EasyMock.expect(newDevice.getState()).andReturn(DeviceState.OFFLINE).times(2);
        EasyMock.replay(newDevice);
        mDeviceListener.deviceChanged(newDevice, IDevice.CHANGE_STATE);
        verifyMocks();
    }

    // TODO: add test for fastboot state changes

    /** Test normal success case for {@link DeviceManager#connectToTcpDevice(String)} */
    @Test
    public void testConnectToTcpDevice() throws Exception {
        final String ipAndPort = "ip:5555";
        setConnectToTcpDeviceExpectations(ipAndPort);
        mMockTestDevice.waitForDeviceOnline();
        replayMocks();
        DeviceManager manager = createDeviceManager(null);
        IManagedTestDevice device = (IManagedTestDevice) manager.connectToTcpDevice(ipAndPort);
        assertNotNull(device);
        verifyMocks();
    }

    /**
     * Test a {@link DeviceManager#connectToTcpDevice(String)} call where device is already
     * allocated
     */
    @Test
    public void testConnectToTcpDevice_alreadyAllocated() throws Exception {
        final String ipAndPort = "ip:5555";
        setConnectToTcpDeviceExpectations(ipAndPort);
        mMockTestDevice.waitForDeviceOnline();
        EasyMock.expect(mMockTestDevice.getAllocationState())
                .andReturn(DeviceAllocationState.Allocated);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.FORCE_ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, false));
        replayMocks();
        DeviceManager manager = createDeviceManager(null);
        IManagedTestDevice device = (IManagedTestDevice) manager.connectToTcpDevice(ipAndPort);
        assertNotNull(device);
        // now attempt to re-allocate
        assertNull(manager.connectToTcpDevice(ipAndPort));
        verifyMocks();
    }

    /** Test {@link DeviceManager#connectToTcpDevice(String)} where device does not appear on adb */
    @Test
    public void testConnectToTcpDevice_notOnline() throws Exception {
        final String ipAndPort = "ip:5555";
        setConnectToTcpDeviceExpectations(ipAndPort);
        mMockTestDevice.waitForDeviceOnline();
        EasyMock.expectLastCall().andThrow(new DeviceNotAvailableException("test", "serial"));
        mMockTestDevice.stopLogcat();
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.FREE_UNKNOWN)).andReturn(
                new DeviceEventResponse(DeviceAllocationState.Unknown, false));
        replayMocks();
        DeviceManager manager = createDeviceManager(null);
        assertNull(manager.connectToTcpDevice(ipAndPort));
        // verify device is not in list
        assertEquals(0, manager.getDeviceList().size());
        verifyMocks();
    }

    /** Test {@link DeviceManager#connectToTcpDevice(String)} where the 'adb connect' call fails. */
    @Test
    public void testConnectToTcpDevice_connectFailed() throws Exception {
        final String ipAndPort = "ip:5555";

        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.FORCE_ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.FREE_UNKNOWN))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Unknown, true));
        CommandResult connectResult = new CommandResult(CommandStatus.SUCCESS);
        connectResult.setStdout(String.format("failed to connect to %s", ipAndPort));
        EasyMock.expect(mMockRunUtil.runTimedCmd(EasyMock.anyLong(), EasyMock.eq("adb"),
                EasyMock.eq("connect"), EasyMock.eq(ipAndPort)))
                .andReturn(connectResult)
                .times(3);
        mMockRunUtil.sleep(EasyMock.anyLong());
        EasyMock.expectLastCall().times(3);

        mMockTestDevice.stopLogcat();

        replayMocks();
        DeviceManager manager = createDeviceManager(null);
        assertNull(manager.connectToTcpDevice(ipAndPort));
        // verify device is not in list
        assertEquals(0, manager.getDeviceList().size());
        verifyMocks();
    }

    /** Test normal success case for {@link DeviceManager#disconnectFromTcpDevice(ITestDevice)} */
    @Test
    public void testDisconnectFromTcpDevice() throws Exception {
        final String ipAndPort = "ip:5555";
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.FREE_UNKNOWN)).andReturn(
                new DeviceEventResponse(DeviceAllocationState.Unknown, true));
        setConnectToTcpDeviceExpectations(ipAndPort);
        EasyMock.expect(mMockTestDevice.switchToAdbUsb()).andReturn(Boolean.TRUE);
        mMockTestDevice.waitForDeviceOnline();
        mMockTestDevice.stopLogcat();
        replayMocks();
        DeviceManager manager = createDeviceManager(null);
        assertNotNull(manager.connectToTcpDevice(ipAndPort));
        manager.disconnectFromTcpDevice(mMockTestDevice);
        // verify device is not in allocated or available list
        assertEquals(0, manager.getDeviceList().size());
        verifyMocks();
    }

    /** Test normal success case for {@link DeviceManager#reconnectDeviceToTcp(ITestDevice)}. */
    @Test
    public void testReconnectDeviceToTcp() throws Exception {
        final String ipAndPort = "ip:5555";
        // use the mMockTestDevice as the initially connected to usb device
        setCheckAvailableDeviceExpectations();
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));
        EasyMock.expect(mMockTestDevice.switchToAdbTcp()).andReturn(ipAndPort);
        setConnectToTcpDeviceExpectations(ipAndPort);
        mMockTestDevice.waitForDeviceOnline();
        replayMocks();
        DeviceManager manager = createDeviceManager(null, mMockIDevice);
        assertEquals(mMockTestDevice, manager.allocateDevice(mDeviceSelections));
        assertNotNull(manager.reconnectDeviceToTcp(mMockTestDevice));
        verifyMocks();
    }

    /**
     * Test {@link DeviceManager#reconnectDeviceToTcp(ITestDevice)} when tcp connected device does
     * not come online.
     */
    @Test
    public void testReconnectDeviceToTcp_notOnline() throws Exception {
        final String ipAndPort = "ip:5555";
        // use the mMockTestDevice as the initially connected to usb device
        setCheckAvailableDeviceExpectations();
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));
        EasyMock.expect(mMockTestDevice.switchToAdbTcp()).andReturn(ipAndPort);
        setConnectToTcpDeviceExpectations(ipAndPort);
        mMockTestDevice.waitForDeviceOnline();
        EasyMock.expectLastCall().andThrow(new DeviceNotAvailableException("test", "serial"));
        // expect recover to be attempted on usb device
        mMockTestDevice.recoverDevice();
        mMockTestDevice.stopLogcat();
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.FREE_UNKNOWN)).andReturn(
                new DeviceEventResponse(DeviceAllocationState.Unknown, true));
        replayMocks();
        DeviceManager manager = createDeviceManager(null, mMockIDevice);
        assertEquals(mMockTestDevice, manager.allocateDevice(mDeviceSelections));
        assertNull(manager.reconnectDeviceToTcp(mMockTestDevice));
        // verify only usb device is in list
        assertEquals(1, manager.getDeviceList().size());
        verifyMocks();
    }

    /** Basic test for {@link DeviceManager#sortDeviceList(List)} */
    @Test
    public void testSortDeviceList() {
        DeviceDescriptor availDevice1 = createDeviceDesc("aaa", DeviceAllocationState.Available);
        DeviceDescriptor availDevice2 = createDeviceDesc("bbb", DeviceAllocationState.Available);
        DeviceDescriptor allocatedDevice = createDeviceDesc("ccc", DeviceAllocationState.Allocated);
        List<DeviceDescriptor> deviceList = ArrayUtil.list(availDevice1, availDevice2,
                allocatedDevice);
        List<DeviceDescriptor> sortedList = DeviceManager.sortDeviceList(deviceList);
        assertEquals(allocatedDevice, sortedList.get(0));
        assertEquals(availDevice1, sortedList.get(1));
        assertEquals(availDevice2, sortedList.get(2));
    }

    /**
     * Helper method to create a {@link DeviceDescriptor} using only serial and state.
     */
    private DeviceDescriptor createDeviceDesc(String serial, DeviceAllocationState state) {
        return new DeviceDescriptor(serial, false, state, null, null, null, null, null);
    }

    /**
     * Set EasyMock expectations for a successful {@link DeviceManager#connectToTcpDevice(String)}
     * call.
     *
     * @param ipAndPort the ip and port of the device
     * @throws DeviceNotAvailableException
     */
    private void setConnectToTcpDeviceExpectations(final String ipAndPort)
            throws DeviceNotAvailableException {
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.FORCE_ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));
        mMockTestDevice.setRecovery((IDeviceRecovery) EasyMock.anyObject());
        CommandResult connectResult = new CommandResult(CommandStatus.SUCCESS);
        connectResult.setStdout(String.format("connected to %s", ipAndPort));
        EasyMock.expect(mMockRunUtil.runTimedCmd(EasyMock.anyLong(), EasyMock.eq("adb"),
                EasyMock.eq("connect"), EasyMock.eq(ipAndPort)))
                .andReturn(connectResult);
    }

    /**
     * Sets all member mock objects into replay mode.
     *
     * @param additionalMocks extra local mock objects to set to replay mode
     */
    private void replayMocks(Object... additionalMocks) {
        EasyMock.replay(mMockStateMonitor, mMockTestDevice, mMockIDevice, mMockAdbBridge,
                mMockRunUtil, mMockGlobalConfig);
        for (Object mock : additionalMocks) {
            EasyMock.replay(mock);
        }
    }

    /**
     * Verify all member mock objects.
     *
     * @param additionalMocks extra local mock objects to set to verify
     */
    private void verifyMocks(Object... additionalMocks) {
        EasyMock.verify(mMockStateMonitor, mMockTestDevice, mMockIDevice, mMockAdbBridge,
                mMockRunUtil);
        for (Object mock : additionalMocks) {
            EasyMock.verify(mock);
        }
    }

    /**
     * Configure EasyMock expectations for a
     * {@link DeviceManager#checkAndAddAvailableDevice(IManagedTestDevice)} call
     * for an online device
     */
    @SuppressWarnings("javadoc")
    private void setCheckAvailableDeviceExpectations() {
        setCheckAvailableDeviceExpectations(mMockIDevice);
    }

    private void setCheckAvailableDeviceExpectations(IDevice iDevice) {
        EasyMock.expect(mMockIDevice.isEmulator()).andStubReturn(Boolean.FALSE);
        EasyMock.expect(iDevice.getState()).andReturn(DeviceState.ONLINE);
        EasyMock.expect(mMockStateMonitor.waitForDeviceShell(EasyMock.anyLong())).andReturn(
                Boolean.TRUE);
        mMockTestDevice.setDeviceState(TestDeviceState.ONLINE);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.CONNECTED_ONLINE))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Checking_Availability,
                        true));
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.AVAILABLE_CHECK_PASSED))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Available, true));
    }

    /** Test freeing a tcp device, it must return to an unavailable status */
    @Test
    public void testFreeDevice_tcpDevice() {
        mDeviceSelections.setTcpDeviceRequested(true);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.FORCE_AVAILABLE))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Available, true));
        EasyMock.expect(mMockIDevice.isEmulator()).andStubReturn(Boolean.FALSE);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));
        mMockTestDevice.stopLogcat();
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.ALLOCATE_REQUEST))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Allocated, true));
        EasyMock.expect(mMockTestDevice.getDeviceState())
                .andReturn(TestDeviceState.NOT_AVAILABLE).times(2);
        EasyMock.expect(mMockTestDevice.handleAllocationEvent(DeviceEvent.FREE_UNKNOWN))
                .andReturn(new DeviceEventResponse(DeviceAllocationState.Available, true));
        mMockTestDevice.setDeviceState(TestDeviceState.NOT_AVAILABLE);
        replayMocks();
        DeviceManager manager = createDeviceManagerNoInit();
        manager.setMaxTcpDevices(1);
        manager.init(null, null, mMockDeviceFactory);
        IManagedTestDevice tcpDevice =
                (IManagedTestDevice) manager.allocateDevice(mDeviceSelections, false);
        assertNotNull(tcpDevice);
        // a freed 'unavailable' emulator should be returned to the available
        // queue.
        manager.freeDevice(tcpDevice, FreeDeviceState.UNAVAILABLE);
        // ensure device can be allocated again
        ITestDevice tcp = manager.allocateDevice(mDeviceSelections, false);
        assertNotNull(tcp);
        assertTrue(tcp.getDeviceState() == TestDeviceState.NOT_AVAILABLE);
        verifyMocks();
    }

    /**
     * Test freeing a device that was unable but showing in adb devices. Device will become
     * Unavailable but still seen by the DeviceManager.
     */
    @Test
    public void testFreeDevice_unavailable() {
        EasyMock.expect(mMockIDevice.isEmulator()).andStubReturn(Boolean.FALSE);
        EasyMock.expect(mMockIDevice.getState()).andReturn(DeviceState.ONLINE);
        EasyMock.expect(mMockStateMonitor.waitForDeviceShell(EasyMock.anyLong()))
                .andReturn(Boolean.TRUE);
        mMockStateMonitor.setState(TestDeviceState.NOT_AVAILABLE);

        CommandResult stubAdbDevices = new CommandResult(CommandStatus.SUCCESS);
        stubAdbDevices.setStdout("List of devices attached\nserial\toffline\n");
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(), EasyMock.eq("adb"), EasyMock.eq("devices")))
                .andReturn(stubAdbDevices);

        replayMocks();
        IManagedTestDevice testDevice = new TestDevice(mMockIDevice, mMockStateMonitor, null);
        DeviceManager manager = createDeviceManagerNoInit();
        manager.init(
                null,
                null,
                new ManagedTestDeviceFactory(false, null, null) {
                    @Override
                    public IManagedTestDevice createDevice(IDevice idevice) {
                        mMockTestDevice.setIDevice(idevice);
                        return testDevice;
                    }

                    @Override
                    protected CollectingOutputReceiver createOutputReceiver() {
                        return new CollectingOutputReceiver() {
                            @Override
                            public String getOutput() {
                                return "/system/bin/pm";
                            }
                        };
                    }

                    @Override
                    public void setFastbootEnabled(boolean enable) {
                        // ignore
                    }
                });

        mDeviceListener.deviceConnected(mMockIDevice);

        IManagedTestDevice device = (IManagedTestDevice) manager.allocateDevice(mDeviceSelections);
        assertNotNull(device);
        // device becomes unavailable
        device.setDeviceState(TestDeviceState.NOT_AVAILABLE);
        // a freed 'unavailable' device becomes UNAVAILABLE state
        manager.freeDevice(device, FreeDeviceState.UNAVAILABLE);
        // ensure device cannot be allocated again
        ITestDevice device2 = manager.allocateDevice(mDeviceSelections);
        assertNull(device2);
        verifyMocks();
        // We still have the device in the list
        assertEquals(1, manager.getDeviceList().size());
    }

    /** Ensure that an unavailable device in recovery mode is released properly. */
    @Test
    public void testFreeDevice_recovery() {
        EasyMock.expect(mMockIDevice.isEmulator()).andStubReturn(Boolean.FALSE);
        EasyMock.expect(mMockIDevice.getState()).andReturn(DeviceState.ONLINE);
        EasyMock.expect(mMockStateMonitor.waitForDeviceShell(EasyMock.anyLong()))
                .andReturn(Boolean.TRUE);
        mMockStateMonitor.setState(TestDeviceState.NOT_AVAILABLE);

        CommandResult stubAdbDevices = new CommandResult(CommandStatus.SUCCESS);
        stubAdbDevices.setStdout("List of devices attached\nserial\trecovery\n");
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(), EasyMock.eq("adb"), EasyMock.eq("devices")))
                .andReturn(stubAdbDevices);

        replayMocks();
        IManagedTestDevice testDevice = new TestDevice(mMockIDevice, mMockStateMonitor, null);
        DeviceManager manager = createDeviceManagerNoInit();
        manager.init(
                null,
                null,
                new ManagedTestDeviceFactory(false, null, null) {
                    @Override
                    public IManagedTestDevice createDevice(IDevice idevice) {
                        mMockTestDevice.setIDevice(idevice);
                        return testDevice;
                    }

                    @Override
                    protected CollectingOutputReceiver createOutputReceiver() {
                        return new CollectingOutputReceiver() {
                            @Override
                            public String getOutput() {
                                return "/system/bin/pm";
                            }
                        };
                    }

                    @Override
                    public void setFastbootEnabled(boolean enable) {
                        // ignore
                    }
                });

        mDeviceListener.deviceConnected(mMockIDevice);

        IManagedTestDevice device = (IManagedTestDevice) manager.allocateDevice(mDeviceSelections);
        assertNotNull(device);
        // Device becomes unavailable
        device.setDeviceState(TestDeviceState.NOT_AVAILABLE);
        // A freed 'unavailable' device becomes UNAVAILABLE state
        manager.freeDevice(device, FreeDeviceState.UNAVAILABLE);
        // Ensure device cannot be allocated again
        ITestDevice device2 = manager.allocateDevice(mDeviceSelections);
        assertNull(device2);
        verifyMocks();
        // We still have the device in the list because device is not lost.
        assertEquals(1, manager.getDeviceList().size());
    }

    /**
     * Test that when freeing an Unavailable device that is not in 'adb devices' we correctly remove
     * it from our tracking list.
     */
    @Test
    public void testFreeDevice_unknown() {
        EasyMock.expect(mMockIDevice.isEmulator()).andStubReturn(Boolean.FALSE);
        EasyMock.expect(mMockIDevice.getState()).andReturn(DeviceState.ONLINE);
        EasyMock.expect(mMockStateMonitor.waitForDeviceShell(EasyMock.anyLong()))
                .andReturn(Boolean.TRUE);
        mMockStateMonitor.setState(TestDeviceState.NOT_AVAILABLE);

        CommandResult stubAdbDevices = new CommandResult(CommandStatus.SUCCESS);
        // device serial is not in the list
        stubAdbDevices.setStdout("List of devices attached\n");
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(), EasyMock.eq("adb"), EasyMock.eq("devices")))
                .andReturn(stubAdbDevices);

        replayMocks();
        IManagedTestDevice testDevice = new TestDevice(mMockIDevice, mMockStateMonitor, null);
        DeviceManager manager = createDeviceManagerNoInit();
        manager.init(
                null,
                null,
                new ManagedTestDeviceFactory(false, null, null) {
                    @Override
                    public IManagedTestDevice createDevice(IDevice idevice) {
                        mMockTestDevice.setIDevice(idevice);
                        return testDevice;
                    }

                    @Override
                    protected CollectingOutputReceiver createOutputReceiver() {
                        return new CollectingOutputReceiver() {
                            @Override
                            public String getOutput() {
                                return "/system/bin/pm";
                            }
                        };
                    }

                    @Override
                    public void setFastbootEnabled(boolean enable) {
                        // ignore
                    }
                });

        mDeviceListener.deviceConnected(mMockIDevice);

        IManagedTestDevice device = (IManagedTestDevice) manager.allocateDevice(mDeviceSelections);
        assertNotNull(device);
        // device becomes unavailable
        device.setDeviceState(TestDeviceState.NOT_AVAILABLE);
        // a freed 'unavailable' device becomes UNAVAILABLE state
        manager.freeDevice(device, FreeDeviceState.UNAVAILABLE);
        // ensure device cannot be allocated again
        ITestDevice device2 = manager.allocateDevice(mDeviceSelections);
        assertNull(device2);
        verifyMocks();
        // We have 0 device in the list since it was removed
        assertEquals(0, manager.getDeviceList().size());
    }

    /**
     * Test that when freeing an Unavailable device that is not in 'adb devices' we correctly remove
     * it from our tracking list even if its serial is a substring of another serial.
     */
    @Test
    public void testFreeDevice_unknown_subName() {
        EasyMock.expect(mMockIDevice.isEmulator()).andStubReturn(Boolean.FALSE);
        EasyMock.expect(mMockIDevice.getState()).andReturn(DeviceState.ONLINE);
        EasyMock.expect(mMockStateMonitor.waitForDeviceShell(EasyMock.anyLong()))
                .andReturn(Boolean.TRUE);
        mMockStateMonitor.setState(TestDeviceState.NOT_AVAILABLE);

        CommandResult stubAdbDevices = new CommandResult(CommandStatus.SUCCESS);
        // device serial is not in the list
        stubAdbDevices.setStdout("List of devices attached\n2serial\tdevice\n");
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(), EasyMock.eq("adb"), EasyMock.eq("devices")))
                .andReturn(stubAdbDevices);

        replayMocks();
        IManagedTestDevice testDevice = new TestDevice(mMockIDevice, mMockStateMonitor, null);
        DeviceManager manager = createDeviceManagerNoInit();
        manager.init(
                null,
                null,
                new ManagedTestDeviceFactory(false, null, null) {
                    @Override
                    public IManagedTestDevice createDevice(IDevice idevice) {
                        mMockTestDevice.setIDevice(idevice);
                        return testDevice;
                    }

                    @Override
                    protected CollectingOutputReceiver createOutputReceiver() {
                        return new CollectingOutputReceiver() {
                            @Override
                            public String getOutput() {
                                return "/system/bin/pm";
                            }
                        };
                    }

                    @Override
                    public void setFastbootEnabled(boolean enable) {
                        // ignore
                    }
                });

        mDeviceListener.deviceConnected(mMockIDevice);

        IManagedTestDevice device = (IManagedTestDevice) manager.allocateDevice(mDeviceSelections);
        assertNotNull(device);
        // device becomes unavailable
        device.setDeviceState(TestDeviceState.NOT_AVAILABLE);
        // a freed 'unavailable' device becomes UNAVAILABLE state
        manager.freeDevice(device, FreeDeviceState.UNAVAILABLE);
        // ensure device cannot be allocated again
        ITestDevice device2 = manager.allocateDevice(mDeviceSelections);
        assertNull(device2);
        verifyMocks();
        // We have 0 device in the list since it was removed
        assertEquals(0, manager.getDeviceList().size());
    }

    /** Helper to set the expectation when a {@link DeviceDescriptor} is expected. */
    private void setDeviceDescriptorExpectation(boolean cached) {
        DeviceDescriptor descriptor =
                new DeviceDescriptor(
                        "serial",
                        null,
                        false,
                        DeviceState.ONLINE,
                        DeviceAllocationState.Available,
                        TestDeviceState.ONLINE,
                        "hardware_test",
                        "product_test",
                        "sdk",
                        "bid_test",
                        "50",
                        "class",
                        MAC_ADDRESS,
                        SIM_STATE,
                        SIM_OPERATOR,
                        false,
                        null);
        if (cached) {
            EasyMock.expect(mMockTestDevice.getCachedDeviceDescriptor()).andReturn(descriptor);
        } else {
            EasyMock.expect(mMockTestDevice.getDeviceDescriptor()).andReturn(descriptor);
        }
    }

    /** Test that {@link DeviceManager#listAllDevices()} returns a list with all devices. */
    @Test
    public void testListAllDevices() throws Exception {
        setCheckAvailableDeviceExpectations();
        setDeviceDescriptorExpectation(true);
        replayMocks();
        DeviceManager manager = createDeviceManager(null, mMockIDevice);
        List<DeviceDescriptor> res = manager.listAllDevices();
        assertEquals(1, res.size());
        assertEquals("[serial hardware_test:product_test bid_test]", res.get(0).toString());
        assertEquals(MAC_ADDRESS, res.get(0).getMacAddress());
        assertEquals(SIM_STATE, res.get(0).getSimState());
        assertEquals(SIM_OPERATOR, res.get(0).getSimOperator());
        verifyMocks();
    }

    /**
     * Test {@link DeviceManager#getDeviceDescriptor(String)} returns the device with the given
     * serial.
     */
    @Test
    public void testGetDeviceDescriptor() throws Exception {
        setCheckAvailableDeviceExpectations();
        setDeviceDescriptorExpectation(false);
        replayMocks();
        DeviceManager manager = createDeviceManager(null, mMockIDevice);
        DeviceDescriptor res = manager.getDeviceDescriptor(mMockIDevice.getSerialNumber());
        assertEquals("[serial hardware_test:product_test bid_test]", res.toString());
        assertEquals(MAC_ADDRESS, res.getMacAddress());
        assertEquals(SIM_STATE, res.getSimState());
        assertEquals(SIM_OPERATOR, res.getSimOperator());
        verifyMocks();
    }

    /**
     * Test that {@link DeviceManager#getDeviceDescriptor(String)} returns null if there are no
     * devices with the given serial.
     */
    @Test
    public void testGetDeviceDescriptor_noMatch() throws Exception {
        setCheckAvailableDeviceExpectations();
        replayMocks();
        DeviceManager manager = createDeviceManager(null, mMockIDevice);
        DeviceDescriptor res = manager.getDeviceDescriptor("nomatch");
        assertNull(res);
        verifyMocks();
    }

    /**
     * Test that {@link DeviceManager#displayDevicesInfo(PrintWriter, boolean)} properly print out
     * the device info.
     */
    @Test
    public void testDisplayDevicesInfo() throws Exception {
        setCheckAvailableDeviceExpectations();
        setDeviceDescriptorExpectation(true);
        replayMocks();
        DeviceManager manager = createDeviceManager(null, mMockIDevice);
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        PrintWriter pw = new PrintWriter(out);
        manager.displayDevicesInfo(pw, false);
        pw.flush();
        verifyMocks();
        assertEquals("Serial  State   Allocation  Product        Variant       Build     Battery  "
                + "\nserial  ONLINE  Available   hardware_test  product_test  bid_test  50       "
                + "\n", out.toString());
    }

    /**
     * Test that {@link DeviceManager#shouldAdbBridgeBeRestarted()} properly reports the flag state
     * based on if it was requested or not.
     */
    @Test
    public void testAdbBridgeFlag() throws Exception {
        setCheckAvailableDeviceExpectations();
        replayMocks();
        DeviceManager manager = createDeviceManager(null, mMockIDevice);

        assertFalse(manager.shouldAdbBridgeBeRestarted());
        manager.stopAdbBridge();
        assertTrue(manager.shouldAdbBridgeBeRestarted());
        manager.restartAdbBridge();
        assertFalse(manager.shouldAdbBridgeBeRestarted());

        verifyMocks();
    }

    /**
     * Test that when a {@link IDeviceMonitor} is available in {@link DeviceManager} it properly
     * goes through its life cycle.
     */
    @Test
    public void testDeviceMonitorLifeCycle() throws Exception {
        IDeviceMonitor mockMonitor = EasyMock.createMock(IDeviceMonitor.class);
        List<IDeviceMonitor> monitors = new ArrayList<>();
        monitors.add(mockMonitor);
        setCheckAvailableDeviceExpectations();

        mockMonitor.setDeviceLister(EasyMock.anyObject());
        mockMonitor.run();
        mockMonitor.stop();

        replayMocks(mockMonitor);
        DeviceManager manager = createDeviceManager(monitors, mMockIDevice);
        manager.terminateDeviceMonitor();
        verifyMocks(mockMonitor);
    }

    /** Ensure that restarting adb bridge doesn't restart {@link IDeviceMonitor}. */
    @Test
    public void testDeviceMonitorLifeCycleWhenAdbRestarts() throws Exception {
        IDeviceMonitor mockMonitor = EasyMock.createMock(IDeviceMonitor.class);
        List<IDeviceMonitor> monitors = new ArrayList<>();
        monitors.add(mockMonitor);
        setCheckAvailableDeviceExpectations();

        mockMonitor.setDeviceLister(EasyMock.anyObject());
        mockMonitor.run();
        mockMonitor.stop();

        replayMocks(mockMonitor);
        DeviceManager manager = createDeviceManager(monitors, mMockIDevice);
        manager.stopAdbBridge();
        manager.restartAdbBridge();
        manager.terminateDeviceMonitor();
        verifyMocks(mockMonitor);
    }

    /** Ensure that the flasher instance limiting machinery is working as expected. */
    @Test
    public void testFlashLimit() throws Exception {
        setCheckAvailableDeviceExpectations();
        replayMocks();
        final DeviceManager manager = createDeviceManager(null, mMockIDevice);
        try {
            Thread waiter = new Thread() {
                @Override
                public void run() {
                    manager.takeFlashingPermit();
                    manager.returnFlashingPermit();
                }
            };
            EasyMock.expect(mMockHostOptions.getConcurrentFlasherLimit())
                .andReturn(null).anyTimes();
            EasyMock.replay(mMockHostOptions);
            manager.setConcurrentFlashSettings(new Semaphore(1), true);
            // take the permit; the next attempt to take the permit should block
            manager.takeFlashingPermit();
            assertFalse(manager.getConcurrentFlashLock().hasQueuedThreads());

            waiter.start();
            RunUtil.getDefault().sleep(200); // Thread start should take <200ms
            assertTrue("Invalid state: waiter thread is not alive", waiter.isAlive());
            assertTrue("No queued threads", manager.getConcurrentFlashLock().hasQueuedThreads());

            manager.returnFlashingPermit();
            RunUtil.getDefault().sleep(200); // Thread start should take <200ms
            assertFalse("Unexpected queued threads",
                    manager.getConcurrentFlashLock().hasQueuedThreads());

            waiter.join(1000);
            assertFalse("waiter thread has not returned", waiter.isAlive());
        } finally {
            // Attempt to reset concurrent flash settings to defaults
            manager.setConcurrentFlashSettings(null, true);
        }
    }

    /** Ensure that the flasher limiting respects {@link IHostOptions}. */
    @Test
    public void testFlashLimit_withHostOptions() throws Exception {
        setCheckAvailableDeviceExpectations();
        replayMocks();
        final DeviceManager manager = createDeviceManager(null, mMockIDevice);
        try {
            Thread waiter = new Thread() {
                @Override
                public void run() {
                    manager.takeFlashingPermit();
                    manager.returnFlashingPermit();
                }
            };
            EasyMock.expect(mMockHostOptions.getConcurrentFlasherLimit()).andReturn(1).anyTimes();
            EasyMock.replay(mMockHostOptions);
            // take the permit; the next attempt to take the permit should block
            manager.takeFlashingPermit();
            assertFalse(manager.getConcurrentFlashLock().hasQueuedThreads());

            waiter.start();
            RunUtil.getDefault().sleep(100);  // Thread start should take <100ms
            assertTrue("Invalid state: waiter thread is not alive", waiter.isAlive());
            assertTrue("No queued threads", manager.getConcurrentFlashLock().hasQueuedThreads());

            manager.returnFlashingPermit();
            RunUtil.getDefault().sleep(100);  // Thread start should take <100ms
            assertFalse("Unexpected queued threads",
                    manager.getConcurrentFlashLock().hasQueuedThreads());

            waiter.join(1000);
            assertFalse("waiter thread has not returned", waiter.isAlive());
            EasyMock.verify(mMockHostOptions);
        } finally {
            // Attempt to reset concurrent flash settings to defaults
            manager.setConcurrentFlashSettings(null, true);
        }
    }

    /** Ensure that the flasher instance limiting machinery is working as expected. */
    @Test
    public void testUnlimitedFlashLimit() throws Exception {
        setCheckAvailableDeviceExpectations();
        replayMocks();
        final DeviceManager manager = createDeviceManager(null, mMockIDevice);
        try {
            Thread waiter = new Thread() {
                @Override
                public void run() {
                    manager.takeFlashingPermit();
                    manager.returnFlashingPermit();
                }
            };
            manager.setConcurrentFlashSettings(null, true);
            // take a permit; the next attempt to take the permit should proceed without blocking
            manager.takeFlashingPermit();
            assertNull("Flash lock is non-null", manager.getConcurrentFlashLock());

            waiter.start();
            RunUtil.getDefault().sleep(100);  // Thread start should take <100ms
            Thread.State waiterState = waiter.getState();
            assertTrue("Invalid state: waiter thread hasn't started",
                    waiter.isAlive() || Thread.State.TERMINATED.equals(waiterState));
            assertNull("Flash lock is non-null", manager.getConcurrentFlashLock());

            manager.returnFlashingPermit();
            RunUtil.getDefault().sleep(100);  // Thread start should take <100ms
            assertNull("Flash lock is non-null", manager.getConcurrentFlashLock());

            waiter.join(1000);
            assertFalse("waiter thread has not returned", waiter.isAlive());
        } finally {
            // Attempt to reset concurrent flash settings to defaults
            manager.setConcurrentFlashSettings(null, true);
        }
    }
}
