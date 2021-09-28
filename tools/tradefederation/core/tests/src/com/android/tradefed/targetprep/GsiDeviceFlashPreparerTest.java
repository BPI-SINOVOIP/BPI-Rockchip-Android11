/*
 * Copyright (C) 2020 The Android Open Source Project
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

import static org.junit.Assert.fail;

import com.android.tradefed.build.DeviceBuildInfo;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.device.DeviceAllocationState;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.IDeviceManager;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.ITestDevice.RecoveryMode;
import com.android.tradefed.device.TestDeviceOptions;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.ZipUtil;

import org.easymock.EasyMock;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.util.List;

/** Unit tests for {@link GsiDevicePreparer}. */
@RunWith(JUnit4.class)
public class GsiDeviceFlashPreparerTest {

    private IDeviceManager mMockDeviceManager;
    private IDeviceFlasher mMockFlasher;
    private GsiDeviceFlashPreparer mPreparer;
    private ITestDevice mMockDevice;
    private IDeviceBuildInfo mBuildInfo;
    private File mTmpDir;
    private TestInformation mTestInfo;
    private OptionSetter mOptionSetter;
    private CommandResult mSuccessResult;
    private CommandResult mFailureResult;
    private IRunUtil mMockRunUtil;
    private DeviceDescriptor mDeviceDescriptor;

    @Before
    public void setUp() throws Exception {
        mDeviceDescriptor =
                new DeviceDescriptor(
                        "serial_1",
                        false,
                        DeviceAllocationState.Available,
                        "unknown",
                        "unknown",
                        "unknown",
                        "unknown",
                        "unknown");
        mMockDeviceManager = EasyMock.createMock(IDeviceManager.class);
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mMockDevice.getSerialNumber()).andStubReturn("serial_1");
        EasyMock.expect(mMockDevice.getDeviceDescriptor()).andStubReturn(mDeviceDescriptor);
        EasyMock.expect(mMockDevice.getOptions()).andReturn(new TestDeviceOptions()).anyTimes();
        mMockRunUtil = EasyMock.createMock(IRunUtil.class);
        mPreparer =
                new GsiDeviceFlashPreparer() {
                    @Override
                    protected IRunUtil getRunUtil() {
                        return mMockRunUtil;
                    }

                    @Override
                    IDeviceManager getDeviceManager() {
                        return mMockDeviceManager;
                    }
                };
        // Reset default settings
        mOptionSetter = new OptionSetter(mPreparer);
        mTmpDir = FileUtil.createTempDir("tmp");
        mBuildInfo = new DeviceBuildInfo("0", "");
        mBuildInfo.setBuildFlavor("flavor");
        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", mMockDevice);
        context.addDeviceBuildInfo("device", mBuildInfo);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
        mSuccessResult = new CommandResult(CommandStatus.SUCCESS);
        mSuccessResult.setStderr("OKAY [  0.043s]");
        mSuccessResult.setStdout("");
        mFailureResult = new CommandResult(CommandStatus.FAILED);
        mFailureResult.setStderr("FAILED (remote: 'Partition error')");
        mFailureResult.setStdout("");
    }

    @After
    public void tearDown() throws Exception {
        FileUtil.recursiveDelete(mTmpDir);
    }

    /** Set EasyMock expectations for a normal setup call */
    private void doSetupExpectations() throws Exception {
        mMockRunUtil.sleep(EasyMock.anyLong());
        mMockDevice.rebootUntilOnline();
        EasyMock.expect(mMockDevice.enableAdbRoot()).andStubReturn(Boolean.TRUE);
        mMockDevice.setDate(null);
        mMockDevice.waitForDeviceAvailable(EasyMock.anyLong());
        mMockDevice.setRecoveryMode(RecoveryMode.AVAILABLE);
        mMockDevice.postBootSetup();
    }

    /** Set EasyMock expectations for getting current slot */
    private void doGetSlotExpectation() throws Exception {
        CommandResult fastbootResult = new CommandResult();
        fastbootResult.setStatus(CommandStatus.SUCCESS);
        fastbootResult.setStderr("current-slot: _a\nfinished. total time 0.001s");
        fastbootResult.setStdout("");
        EasyMock.expect(mMockDevice.executeLongFastbootCommand("getvar", "current-slot"))
                .andReturn(fastbootResult);
    }

    /** Set EasyMock expectations for no current slot */
    private void doGetEmptySlotExpectation() throws Exception {
        CommandResult fastbootResult = new CommandResult();
        fastbootResult.setStatus(CommandStatus.SUCCESS);
        fastbootResult.setStderr("current-slot: \nfinished. total time 0.001s");
        fastbootResult.setStdout("");
        EasyMock.expect(mMockDevice.executeLongFastbootCommand("getvar", "current-slot"))
                .andReturn(fastbootResult);
    }

    /* Verifies that setUp will throw TargetSetupError if there is no gki boot.img */
    @Test
    public void testSetUp_NoGsiImg() throws Exception {
        EasyMock.replay(mMockDevice, mMockRunUtil);
        try {
            mPreparer.setUp(mTestInfo);
            fail("TargetSetupError is expected");
        } catch (TargetSetupError e) {
            // expected
        }
        EasyMock.verify(mMockDevice, mMockRunUtil);
    }

    /* Verifies that setUp will throw exception when there is no system.img in the zip file */
    @Test
    public void testSetUp_NoSystemImageInGsiZip() throws Exception {
        File gsiDir = FileUtil.createTempDir("gsi_folder", mTmpDir);
        File tmpFile = new File(gsiDir, "test");
        File gsiZip = FileUtil.createTempFile("gsi_image", ".zip", mTmpDir);
        ZipUtil.createZip(List.of(tmpFile), gsiZip);
        mBuildInfo.setFile("gsi_system.img", gsiZip, "0");
        EasyMock.replay(mMockDevice, mMockRunUtil);
        try {
            mPreparer.setUp(mTestInfo);
            fail("TargetSetupError is expected");
        } catch (TargetSetupError e) {
            // expected
        }
        EasyMock.verify(mMockDevice, mMockRunUtil);
    }

    /* Verifies that setUp will throw exception when there is no vbmeta.img in the zip file*/
    @Test
    public void testSetUp_NoVbmetaImageInGsiZip() throws Exception {
        File gsiDir = FileUtil.createTempDir("gsi_folder", mTmpDir);
        File systemImg = new File(gsiDir, "system.img");
        File gsiZip = FileUtil.createTempFile("gsi_image", ".zip", mTmpDir);
        ZipUtil.createZip(List.of(systemImg), gsiZip);
        mBuildInfo.setFile("gsi_system.img", gsiZip, "0");
        EasyMock.replay(mMockDevice, mMockRunUtil);
        try {
            mPreparer.setUp(mTestInfo);
            fail("TargetSetupError is expected");
        } catch (TargetSetupError e) {
            // expected
        }
        EasyMock.verify(mMockDevice, mMockRunUtil);
    }

    /* Verifies that preparer can flash GSI image */
    @Test
    public void testSetup_Success() throws Exception {
        File gsiDir = FileUtil.createTempDir("gsi_folder", mTmpDir);
        File systemImg = new File(gsiDir, "system.img");
        File vbmetaImg = new File(gsiDir, "vbmeta.img");
        FileUtil.writeToFile("ddd", systemImg);
        FileUtil.writeToFile("ddd", vbmetaImg);
        mBuildInfo.setFile("gsi_system.img", systemImg, "0");
        mBuildInfo.setFile("gsi_vbmeta.img", vbmetaImg, "0");
        mMockDevice.waitForDeviceOnline();
        EasyMock.expect(mMockDevice.getApiLevel()).andReturn(29);
        mMockDevice.rebootIntoBootloader();
        mMockRunUtil.allowInterrupt(false);
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                "--disable-verification",
                                "flash",
                                "vbmeta",
                                mBuildInfo.getFile("gsi_vbmeta.img").getAbsolutePath()))
                .andReturn(mSuccessResult);
        mMockDevice.rebootIntoFastbootd();
        doGetSlotExpectation();
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                "delete-logical-partition", "product_a"))
                .andReturn(mSuccessResult);
        EasyMock.expect(mMockDevice.executeLongFastbootCommand("erase", "system_a"))
                .andReturn(mSuccessResult);
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                "flash",
                                "system",
                                mBuildInfo.getFile("gsi_system.img").getAbsolutePath()))
                .andReturn(mSuccessResult);
        EasyMock.expect(mMockDevice.executeLongFastbootCommand("-w")).andReturn(mSuccessResult);
        mMockRunUtil.allowInterrupt(true);
        doSetupExpectations();
        EasyMock.replay(mMockDevice, mMockRunUtil);
        mPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockDevice, mMockRunUtil);
    }

    /* Verifies that preparer can flash GSI image from Zip file*/
    @Test
    public void testSetup_Success_FromZipFile() throws Exception {
        File gsiDir = FileUtil.createTempDir("gsi_folder", mTmpDir);
        File systemImg = new File(gsiDir, "system.img");
        File vbmetaImg = new File(gsiDir, "vbmeta.img");
        FileUtil.writeToFile("ddd", systemImg);
        FileUtil.writeToFile("ddd", vbmetaImg);
        File gsiZip = FileUtil.createTempFile("gsi_image", ".zip", mTmpDir);
        ZipUtil.createZip(List.of(systemImg, vbmetaImg), gsiZip);
        mBuildInfo.setFile("gsi_system.img", gsiZip, "0");
        mBuildInfo.setFile("gsi_vbmeta.img", gsiZip, "0");
        mMockDevice.waitForDeviceOnline();
        EasyMock.expect(mMockDevice.getApiLevel()).andReturn(29);
        mMockDevice.rebootIntoBootloader();
        mMockRunUtil.allowInterrupt(false);
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                EasyMock.eq("--disable-verification"),
                                EasyMock.eq("flash"),
                                EasyMock.eq("vbmeta"),
                                EasyMock.matches(".*vbmeta.img")))
                .andReturn(mSuccessResult);
        mMockDevice.rebootIntoFastbootd();
        doGetSlotExpectation();
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                "delete-logical-partition", "product_a"))
                .andReturn(mSuccessResult);
        EasyMock.expect(mMockDevice.executeLongFastbootCommand("erase", "system_a"))
                .andReturn(mSuccessResult);
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                EasyMock.eq("flash"),
                                EasyMock.eq("system"),
                                EasyMock.matches(".*system.img")))
                .andReturn(mSuccessResult);
        EasyMock.expect(mMockDevice.executeLongFastbootCommand("-w")).andReturn(mSuccessResult);
        mMockRunUtil.allowInterrupt(true);
        doSetupExpectations();
        EasyMock.replay(mMockDevice, mMockRunUtil);
        mPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockDevice, mMockRunUtil);
    }

    /* Verifies that preparer can flash GSI and GKI image */
    @Test
    public void testSetup_Success_WithGkiBootImg() throws Exception {
        File gsiDir = FileUtil.createTempDir("gsi_folder", mTmpDir);
        File systemImg = new File(gsiDir, "system.img");
        File vbmetaImg = new File(gsiDir, "vbmeta.img");
        File bootImg = new File(gsiDir, "boot-5.4.img");
        FileUtil.writeToFile("ddd", systemImg);
        FileUtil.writeToFile("ddd", vbmetaImg);
        FileUtil.writeToFile("ddd", bootImg);
        mBuildInfo.setFile("gsi_system.img", systemImg, "0");
        mBuildInfo.setFile("gsi_vbmeta.img", vbmetaImg, "0");
        mBuildInfo.setFile("gki_boot.img", bootImg, "0");
        mMockDevice.waitForDeviceOnline();
        EasyMock.expect(mMockDevice.getApiLevel()).andReturn(29);
        mMockDevice.rebootIntoBootloader();
        mMockRunUtil.allowInterrupt(false);
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                "--disable-verification",
                                "flash",
                                "vbmeta",
                                mBuildInfo.getFile("gsi_vbmeta.img").getAbsolutePath()))
                .andReturn(mSuccessResult);
        mMockDevice.rebootIntoFastbootd();
        doGetSlotExpectation();
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                "delete-logical-partition", "product_a"))
                .andReturn(mSuccessResult);
        EasyMock.expect(mMockDevice.executeLongFastbootCommand("erase", "system_a"))
                .andReturn(mSuccessResult);
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                "flash",
                                "system",
                                mBuildInfo.getFile("gsi_system.img").getAbsolutePath()))
                .andReturn(mSuccessResult);
        EasyMock.expect(mMockDevice.executeLongFastbootCommand("-w")).andReturn(mSuccessResult);
        mMockDevice.rebootIntoBootloader();
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                "flash",
                                "boot",
                                mBuildInfo.getFile("gki_boot.img").getAbsolutePath()))
                .andReturn(mSuccessResult);
        mMockRunUtil.allowInterrupt(true);
        doSetupExpectations();
        EasyMock.replay(mMockDevice, mMockRunUtil);
        mPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockDevice, mMockRunUtil);
    }

    /* Verifies that preparer can flash GSI and GKI image from a Zip file*/
    @Test
    public void testSetup_Success_WithGkiBootImgInZip() throws Exception {
        File gsiDir = FileUtil.createTempDir("gsi_folder", mTmpDir);
        File systemImg = new File(gsiDir, "system.img");
        File vbmetaImg = new File(gsiDir, "vbmeta.img");
        File bootImg = new File(gsiDir, "boot-5.4.img");
        FileUtil.writeToFile("ddd", systemImg);
        FileUtil.writeToFile("ddd", vbmetaImg);
        FileUtil.writeToFile("ddd", bootImg);
        File gsiZip = FileUtil.createTempFile("gsi_image", ".zip", mTmpDir);
        ZipUtil.createZip(List.of(systemImg, vbmetaImg, bootImg), gsiZip);
        mBuildInfo.setFile("gsi_system.img", gsiZip, "0");
        mBuildInfo.setFile("gsi_vbmeta.img", gsiZip, "0");
        mBuildInfo.setFile("gki_boot.img", gsiZip, "0");
        mMockDevice.waitForDeviceOnline();
        EasyMock.expect(mMockDevice.getApiLevel()).andReturn(29);
        mMockDevice.rebootIntoBootloader();
        mMockRunUtil.allowInterrupt(false);
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                EasyMock.eq("--disable-verification"),
                                EasyMock.eq("flash"),
                                EasyMock.eq("vbmeta"),
                                EasyMock.matches(".*/vbmeta.img")))
                .andReturn(mSuccessResult);
        mMockDevice.rebootIntoFastbootd();
        doGetSlotExpectation();
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                "delete-logical-partition", "product_a"))
                .andReturn(mSuccessResult);
        EasyMock.expect(mMockDevice.executeLongFastbootCommand("erase", "system_a"))
                .andReturn(mSuccessResult);
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                EasyMock.eq("flash"),
                                EasyMock.eq("system"),
                                EasyMock.matches(".*/system.img")))
                .andReturn(mSuccessResult);
        EasyMock.expect(mMockDevice.executeLongFastbootCommand("-w")).andReturn(mSuccessResult);
        mMockDevice.rebootIntoBootloader();
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                EasyMock.eq("flash"),
                                EasyMock.eq("boot"),
                                EasyMock.matches(".*/boot-5.4.img")))
                .andReturn(mSuccessResult);
        mMockRunUtil.allowInterrupt(true);
        doSetupExpectations();
        EasyMock.replay(mMockDevice, mMockRunUtil);
        mPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockDevice, mMockRunUtil);
    }

    /* Verifies that preparer can flash GSI image on Android 9 device*/
    @Test
    public void testSetup_Success_Api28() throws Exception {
        File gsiDir = FileUtil.createTempDir("gsi_folder", mTmpDir);
        File systemImg = new File(gsiDir, "system.img");
        File vbmetaImg = new File(gsiDir, "vbmeta.img");
        FileUtil.writeToFile("ddd", systemImg);
        FileUtil.writeToFile("ddd", vbmetaImg);
        mBuildInfo.setFile("gsi_system.img", systemImg, "0");
        mBuildInfo.setFile("gsi_vbmeta.img", vbmetaImg, "0");
        mMockDevice.waitForDeviceOnline();
        EasyMock.expect(mMockDevice.getApiLevel()).andReturn(28);
        mMockDevice.rebootIntoBootloader();
        mMockRunUtil.allowInterrupt(false);
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                "--disable-verification",
                                "flash",
                                "vbmeta",
                                mBuildInfo.getFile("gsi_vbmeta.img").getAbsolutePath()))
                .andReturn(mSuccessResult);
        doGetEmptySlotExpectation();
        EasyMock.expect(mMockDevice.executeLongFastbootCommand("erase", "system"))
                .andReturn(mSuccessResult);
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                "flash",
                                "system",
                                mBuildInfo.getFile("gsi_system.img").getAbsolutePath()))
                .andReturn(mSuccessResult);
        EasyMock.expect(mMockDevice.executeLongFastbootCommand("-w")).andReturn(mSuccessResult);
        mMockRunUtil.allowInterrupt(true);
        doSetupExpectations();
        EasyMock.replay(mMockDevice, mMockRunUtil);
        mPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockDevice, mMockRunUtil);
    }

    /* Verifies that preparer will throw TargetSetupError with GSI flash failure*/
    @Test
    public void testSetup_GsiFlashFailure() throws Exception {
        File gsiDir = FileUtil.createTempDir("gsi_folder", mTmpDir);
        File systemImg = new File(gsiDir, "system.img");
        File vbmetaImg = new File(gsiDir, "vbmeta.img");
        FileUtil.writeToFile("ddd", systemImg);
        FileUtil.writeToFile("ddd", vbmetaImg);
        mBuildInfo.setFile("gsi_system.img", systemImg, "0");
        mBuildInfo.setFile("gsi_vbmeta.img", vbmetaImg, "0");
        mMockDevice.waitForDeviceOnline();
        EasyMock.expect(mMockDevice.getApiLevel()).andReturn(29);
        mMockDevice.rebootIntoBootloader();
        mMockRunUtil.allowInterrupt(false);
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                "--disable-verification",
                                "flash",
                                "vbmeta",
                                mBuildInfo.getFile("gsi_vbmeta.img").getAbsolutePath()))
                .andReturn(mSuccessResult);
        mMockDevice.rebootIntoFastbootd();
        doGetSlotExpectation();
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                "delete-logical-partition", "product_a"))
                .andReturn(mSuccessResult);
        EasyMock.expect(mMockDevice.executeLongFastbootCommand("erase", "system_a"))
                .andReturn(mSuccessResult);
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                "flash",
                                "system",
                                mBuildInfo.getFile("gsi_system.img").getAbsolutePath()))
                .andReturn(mFailureResult);
        mMockRunUtil.allowInterrupt(true);
        EasyMock.replay(mMockDevice, mMockRunUtil);
        try {
            mPreparer.setUp(mTestInfo);
            fail("Expect to get TargetSetupError from setUp");
        } catch (TargetSetupError e) {
            // expected
        }
        EasyMock.verify(mMockDevice, mMockRunUtil);
    }

    /* Verifies that preparer will throw DeviceNotAvailableException if device fails to boot up */
    @Test
    public void testSetUp_BootFailure() throws Exception {
        File gsiDir = FileUtil.createTempDir("gsi_folder", mTmpDir);
        File systemImg = new File(gsiDir, "system.img");
        File vbmetaImg = new File(gsiDir, "vbmeta.img");
        FileUtil.writeToFile("ddd", systemImg);
        FileUtil.writeToFile("ddd", vbmetaImg);
        mBuildInfo.setFile("gsi_system.img", systemImg, "0");
        mBuildInfo.setFile("gsi_vbmeta.img", vbmetaImg, "0");
        mMockDevice.waitForDeviceOnline();
        EasyMock.expect(mMockDevice.getApiLevel()).andReturn(29);
        mMockDevice.rebootIntoBootloader();
        mMockRunUtil.allowInterrupt(false);
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                "--disable-verification",
                                "flash",
                                "vbmeta",
                                mBuildInfo.getFile("gsi_vbmeta.img").getAbsolutePath()))
                .andReturn(mSuccessResult);
        doGetSlotExpectation();
        mMockDevice.rebootIntoFastbootd();
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                "delete-logical-partition", "product_a"))
                .andReturn(mSuccessResult);
        EasyMock.expect(mMockDevice.executeLongFastbootCommand("erase", "system_a"))
                .andReturn(mSuccessResult);
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                "flash",
                                "system",
                                mBuildInfo.getFile("gsi_system.img").getAbsolutePath()))
                .andReturn(mSuccessResult);
        EasyMock.expect(mMockDevice.executeLongFastbootCommand("-w")).andReturn(mSuccessResult);
        mMockRunUtil.allowInterrupt(true);
        mMockRunUtil.sleep(EasyMock.anyLong());
        mMockDevice.rebootUntilOnline();
        EasyMock.expectLastCall().andThrow(new DeviceNotAvailableException("test", "serial"));
        EasyMock.replay(mMockDevice, mMockRunUtil);
        try {
            mPreparer.setUp(mTestInfo);
            fail("Expect to get DeviceNotAvailableException from setUp");
        } catch (DeviceNotAvailableException e) {
            // expected
        }
        EasyMock.verify(mMockDevice, mMockRunUtil);
    }
}
