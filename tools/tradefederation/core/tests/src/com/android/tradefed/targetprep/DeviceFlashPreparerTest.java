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

package com.android.tradefed.targetprep;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.tradefed.build.DeviceBuildInfo;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.device.DeviceAllocationState;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.DeviceUnresponsiveException;
import com.android.tradefed.device.IDeviceManager;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.ITestDevice.RecoveryMode;
import com.android.tradefed.device.TestDeviceOptions;
import com.android.tradefed.host.IHostOptions;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.targetprep.IDeviceFlasher.UserDataFlashOption;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;

import org.easymock.EasyMock;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.util.Arrays;

/** Unit tests for {@link DeviceFlashPreparer}. */
@RunWith(JUnit4.class)
public class DeviceFlashPreparerTest {

    private IDeviceManager mMockDeviceManager;
    private IDeviceFlasher mMockFlasher;
    private DeviceFlashPreparer mDeviceFlashPreparer;
    private ITestDevice mMockDevice;
    private IDeviceBuildInfo mMockBuildInfo;
    private IHostOptions mMockHostOptions;
    private File mTmpDir;
    private boolean mFlashingMetricsReported;
    private TestInformation mTestInfo;

    @Before
    public void setUp() throws Exception {
        mMockDeviceManager = EasyMock.createMock(IDeviceManager.class);
        mMockFlasher = EasyMock.createMock(IDeviceFlasher.class);
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mMockDevice.getSerialNumber()).andReturn("foo").anyTimes();
        EasyMock.expect(mMockDevice.getOptions()).andReturn(new TestDeviceOptions()).anyTimes();
        mMockBuildInfo = new DeviceBuildInfo("0", "");
        mMockBuildInfo.setDeviceImageFile(new File("foo"), "0");
        mMockBuildInfo.setBuildFlavor("flavor");
        mMockHostOptions = EasyMock.createMock(IHostOptions.class);
        mFlashingMetricsReported = false;
        mDeviceFlashPreparer = new DeviceFlashPreparer() {
            @Override
            protected IDeviceFlasher createFlasher(ITestDevice device) {
                return mMockFlasher;
            }

            @Override
            int getDeviceBootPollTimeMs() {
                return 100;
            }

            @Override
            IDeviceManager getDeviceManager() {
                return mMockDeviceManager;
            }

            @Override
            protected IHostOptions getHostOptions() {
                return mMockHostOptions;
            }

            @Override
            protected void reportFlashMetrics(String branch, String buildFlavor, String buildId,
                    String serial, long queueTime, long flashingTime,
                    CommandStatus flashingStatus) {
                mFlashingMetricsReported = true;
            }
        };
        // Reset default settings
        mDeviceFlashPreparer.setDeviceBootTime(100);
        // expect this call
        mMockFlasher.setUserDataFlashOption(UserDataFlashOption.FLASH);
        mTmpDir = FileUtil.createTempDir("tmp");
        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", mMockDevice);
        context.addDeviceBuildInfo("device", mMockBuildInfo);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
    }

    @After
    public void tearDown() throws Exception {
        FileUtil.recursiveDelete(mTmpDir);
    }

    /** Simple normal case test for {@link DeviceFlashPreparer#setUp(TestInformation)}. */
    @Test
    public void testSetup() throws Exception {
        doSetupExpectations();
        mMockFlasher.setShouldFlashRamdisk(false);
        // report flashing success in normal case
        EasyMock.expect(mMockFlasher.getSystemFlashingStatus())
            .andReturn(CommandStatus.SUCCESS).anyTimes();
        EasyMock.replay(mMockFlasher, mMockDevice);
        mDeviceFlashPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockFlasher, mMockDevice);
        assertTrue("should report flashing metrics in normal case", mFlashingMetricsReported);
    }

    /**
     * Set EasyMock expectations for a normal setup call
     */
    private void doSetupExpectations() throws TargetSetupError, DeviceNotAvailableException {
        mMockDevice.setRecoveryMode(RecoveryMode.ONLINE);
        mMockFlasher.overrideDeviceOptions(mMockDevice);
        mMockFlasher.setForceSystemFlash(false);
        mMockFlasher.setDataWipeSkipList(Arrays.asList(new String[]{}));
        mMockFlasher.flash(mMockDevice, mMockBuildInfo);
        mMockFlasher.setWipeTimeout(EasyMock.anyLong());
        mMockDevice.waitForDeviceOnline();
        EasyMock.expect(mMockDevice.enableAdbRoot()).andStubReturn(Boolean.TRUE);
        mMockDevice.setDate(null);
        EasyMock.expect(mMockDevice.getBuildId()).andReturn(mMockBuildInfo.getBuildId());
        EasyMock.expect(mMockDevice.getBuildFlavor()).andReturn(mMockBuildInfo.getBuildFlavor());
        EasyMock.expect(mMockDevice.isEncryptionSupported()).andStubReturn(Boolean.TRUE);
        EasyMock.expect(mMockDevice.isDeviceEncrypted()).andStubReturn(Boolean.FALSE);
        mMockDevice.clearLogcat();
        mMockDevice.waitForDeviceAvailable(EasyMock.anyLong());
        mMockDevice.setRecoveryMode(RecoveryMode.AVAILABLE);
        mMockDevice.postBootSetup();
    }

    /**
     * Test {@link DeviceFlashPreparer#setUp(TestInformation)} when a non IDeviceBuildInfo type is
     * provided.
     */
    @Test
    public void testSetUp_nonDevice() throws Exception {
        try {
            mTestInfo
                    .getContext()
                    .addDeviceBuildInfo("device", EasyMock.createMock(IBuildInfo.class));
            mDeviceFlashPreparer.setUp(mTestInfo);
            fail("IllegalArgumentException not thrown");
        } catch (IllegalArgumentException e) {
            // expected
        }
    }

    /**
     * Test {@link DeviceFlashPreparer#setUp(TestInformation)} when ramdisk flashing is required via
     * parameter but not provided in build info
     */
    @Test
    public void testSetUp_noRamdisk() throws Exception {
        try {
            mDeviceFlashPreparer.setShouldFlashRamdisk(true);
            mDeviceFlashPreparer.setUp(mTestInfo);
            fail("IllegalArgumentException not thrown");
        } catch (IllegalArgumentException e) {
            // expected
        }
    }

    /** Test {@link DeviceFlashPreparer#setUp(TestInformation)} when build does not boot. */
    @Test
    public void testSetup_buildError() throws Exception {
        mMockDevice.setRecoveryMode(RecoveryMode.ONLINE);
        mMockFlasher.overrideDeviceOptions(mMockDevice);
        mMockFlasher.setForceSystemFlash(false);
        mMockFlasher.setDataWipeSkipList(Arrays.asList(new String[]{}));
        mMockFlasher.setShouldFlashRamdisk(false);
        mMockFlasher.flash(mMockDevice, mMockBuildInfo);
        mMockFlasher.setWipeTimeout(EasyMock.anyLong());
        mMockDevice.waitForDeviceOnline();
        EasyMock.expect(mMockDevice.enableAdbRoot()).andStubReturn(Boolean.TRUE);
        mMockDevice.setDate(null);
        EasyMock.expect(mMockDevice.getBuildId()).andReturn(mMockBuildInfo.getBuildId());
        EasyMock.expect(mMockDevice.getBuildFlavor()).andReturn(mMockBuildInfo.getBuildFlavor());
        EasyMock.expect(mMockDevice.isEncryptionSupported()).andStubReturn(Boolean.TRUE);
        EasyMock.expect(mMockDevice.isDeviceEncrypted()).andStubReturn(Boolean.FALSE);
        mMockDevice.clearLogcat();
        mMockDevice.waitForDeviceAvailable(EasyMock.anyLong());
        EasyMock.expectLastCall().andThrow(new DeviceUnresponsiveException("foo", "fakeserial"));
        mMockDevice.setRecoveryMode(RecoveryMode.AVAILABLE);
        EasyMock.expect(mMockDevice.getDeviceDescriptor()).andReturn(
                new DeviceDescriptor("SERIAL", false, DeviceAllocationState.Available, "unknown",
                        "unknown", "unknown", "unknown", "unknown"));
        // report SUCCESS since device was flashed successfully (but didn't boot up)
        EasyMock.expect(mMockFlasher.getSystemFlashingStatus())
            .andReturn(CommandStatus.SUCCESS).anyTimes();
        EasyMock.replay(mMockFlasher, mMockDevice);
        try {
            mDeviceFlashPreparer.setUp(mTestInfo);
            fail("DeviceFlashPreparerTest not thrown");
        } catch (BuildError e) {
            // expected; use the general version to make absolutely sure that
            // DeviceFailedToBootError properly masquerades as a BuildError.
            assertTrue(e instanceof DeviceFailedToBootError);
        }
        EasyMock.verify(mMockFlasher, mMockDevice);
        assertTrue("should report flashing metrics with device boot failure",
                mFlashingMetricsReported);
    }

    /**
     * Test {@link DeviceFlashPreparer#setUp(TestInformation)} when flashing step hits device
     * failure.
     */
    @Test
    public void testSetup_flashException() throws Exception {
        mMockDevice.setRecoveryMode(RecoveryMode.ONLINE);
        mMockFlasher.overrideDeviceOptions(mMockDevice);
        mMockFlasher.setForceSystemFlash(false);
        mMockFlasher.setDataWipeSkipList(Arrays.asList(new String[]{}));
        mMockFlasher.setShouldFlashRamdisk(false);
        mMockFlasher.flash(mMockDevice, mMockBuildInfo);
        EasyMock.expectLastCall().andThrow(new DeviceNotAvailableException("test", "serial"));
        mMockFlasher.setWipeTimeout(EasyMock.anyLong());
        // report exception
        EasyMock.expect(mMockFlasher.getSystemFlashingStatus())
            .andReturn(CommandStatus.EXCEPTION).anyTimes();
        EasyMock.replay(mMockFlasher, mMockDevice);
        try {
            mDeviceFlashPreparer.setUp(mTestInfo);
            fail("DeviceNotAvailableException not thrown");
        } catch (DeviceNotAvailableException e) {
            // expected
        }
        EasyMock.verify(mMockFlasher, mMockDevice);
        assertTrue("should report flashing metrics with device flash failure",
                mFlashingMetricsReported);
    }

    /**
     * Test {@link DeviceFlashPreparer#setUp(TestInformation)} when flashing of system partitions
     * are skipped.
     */
    @Test
    public void testSetup_flashSkipped() throws Exception {
        doSetupExpectations();
        mMockFlasher.setShouldFlashRamdisk(false);
        // report flashing status as null (for not flashing system partitions)
        EasyMock.expect(mMockFlasher.getSystemFlashingStatus()).andReturn(null).anyTimes();
        EasyMock.replay(mMockFlasher, mMockDevice);
        mDeviceFlashPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockFlasher, mMockDevice);
        assertFalse("should not report flashing metrics in normal case", mFlashingMetricsReported);
    }

    /**
     * Verifies that the ramdisk flashing parameter is passed down to the device flasher
     *
     * @throws Exception
     */
    @Test
    public void testSetup_flashRamdisk() throws Exception {
        mDeviceFlashPreparer.setShouldFlashRamdisk(true);
        mMockBuildInfo.setRamdiskFile(new File("foo"), "0");
        doSetupExpectations();
        // report flashing success in normal case
        EasyMock.expect(mMockFlasher.getSystemFlashingStatus())
                .andReturn(CommandStatus.SUCCESS)
                .anyTimes();
        mMockFlasher.setShouldFlashRamdisk(true);
        EasyMock.expectLastCall();
        EasyMock.replay(mMockFlasher, mMockDevice);
        mDeviceFlashPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockFlasher, mMockDevice);
    }
}
