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

/** Unit tests for {@link GkiDevicePreparer}. */
@RunWith(JUnit4.class)
public class GkiDeviceFlashPreparerTest {

    private IDeviceManager mMockDeviceManager;
    private IDeviceFlasher mMockFlasher;
    private GkiDeviceFlashPreparer mPreparer;
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
                new GkiDeviceFlashPreparer() {
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

    /* Verifies that validateGkiBootImg will throw TargetSetupError if there is no BuildInfo files */
    @Test
    public void testValidateGkiBootImg_NoBuildInfoFiles() throws Exception {
        EasyMock.replay(mMockDevice, mMockRunUtil);
        try {
            mPreparer.validateGkiBootImg(mMockDevice, mBuildInfo, mTmpDir);
            fail("TargetSetupError is expected");
        } catch (TargetSetupError e) {
            // expected
        }
        EasyMock.verify(mMockDevice, mMockRunUtil);
    }

    /* Verifies that validateGkiBootImg will throw exception when there is no ramdisk-recovery.img*/
    @Test
    public void testValidateGkiBootImg_NoRamdiskRecoveryImg() throws Exception {
        File kernelImage = new File(mTmpDir, "Image.gz");
        mBuildInfo.setFile("Image.gz", kernelImage, "0");
        EasyMock.replay(mMockDevice, mMockRunUtil);
        try {
            mPreparer.validateGkiBootImg(mMockDevice, mBuildInfo, mTmpDir);
            fail("TargetSetupError is expected");
        } catch (TargetSetupError e) {
            // expected
        }
        EasyMock.verify(mMockDevice, mMockRunUtil);
    }

    /* Verifies that validateGkiBootImg will throw exception when there is no otatools.zip*/
    @Test
    public void testValidateGkiBootImg_NoOtatoolsZip() throws Exception {
        File kernelImage = new File(mTmpDir, "Image.gz");
        mBuildInfo.setFile("Image.gz", kernelImage, "0");
        File ramdiskRecoveryImage = new File(mTmpDir, "ramdisk-recovery.img");
        mBuildInfo.setFile("ramdisk-recovery.img", ramdiskRecoveryImage, "0");
        EasyMock.replay(mMockDevice, mMockRunUtil);
        try {
            mPreparer.validateGkiBootImg(mMockDevice, mBuildInfo, mTmpDir);
            fail("TargetSetupError is expected");
        } catch (TargetSetupError e) {
            // expected
        }
        EasyMock.verify(mMockDevice, mMockRunUtil);
    }

    /* Verifies that validateGkiBootImg will throw exception when there is no valid mkbootimg in otatools.zip*/
    @Test
    public void testValidateGkiBootImg_NoMkbootimgInOtatoolsZip() throws Exception {
        File kernelImage = new File(mTmpDir, "Image.gz");
        mBuildInfo.setFile("Image.gz", kernelImage, "0");
        File ramdiskRecoveryImage = new File(mTmpDir, "ramdisk-recovery.img");
        mBuildInfo.setFile("ramdisk-recovery.img", ramdiskRecoveryImage, "0");
        File otaDir = FileUtil.createTempDir("otatool_folder", mTmpDir);
        File tmpFile = new File(otaDir, "test");
        File otatoolsZip = FileUtil.createTempFile("otatools", ".zip", mTmpDir);
        ZipUtil.createZip(List.of(tmpFile), otatoolsZip);
        mBuildInfo.setFile("otatools.zip", otatoolsZip, "0");
        EasyMock.replay(mMockDevice, mMockRunUtil);
        try {
            mPreparer.validateGkiBootImg(mMockDevice, mBuildInfo, mTmpDir);
            fail("TargetSetupError is expected");
        } catch (TargetSetupError e) {
            // expected
        }
        EasyMock.verify(mMockDevice, mMockRunUtil);
    }

    /* Verifies that validateGkiBootImg will throw exception when fail to generate GKI boot.img*/
    @Test
    public void testValidateGkiBootImg_FailToGenerateBootImg() throws Exception {
        File kernelImage = new File(mTmpDir, "Image.gz");
        mBuildInfo.setFile("Image.gz", kernelImage, "0");
        File ramdiskRecoveryImage = new File(mTmpDir, "ramdisk-recovery.img");
        mBuildInfo.setFile("ramdisk-recovery.img", ramdiskRecoveryImage, "0");
        File otaDir = FileUtil.createTempDir("otatool_folder", mTmpDir);
        File mkbootimgFile = new File(otaDir, "mkbootimg");
        File otatoolsZip = FileUtil.createTempFile("otatools", ".zip", mTmpDir);
        ZipUtil.createZip(List.of(mkbootimgFile), otatoolsZip);
        mBuildInfo.setFile("otatools.zip", otatoolsZip, "0");
        CommandResult cmdResult = new CommandResult();
        cmdResult.setStatus(CommandStatus.FAILED);
        cmdResult.setStdout("output");
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(),
                                EasyMock.matches(".*mkbootimg.*"),
                                EasyMock.eq("--kernel"),
                                EasyMock.eq(kernelImage.getAbsolutePath()),
                                EasyMock.eq("--header_version"),
                                EasyMock.eq("3"),
                                EasyMock.eq("--base"),
                                EasyMock.eq("0x00000000"),
                                EasyMock.eq("--pagesize"),
                                EasyMock.eq("4096"),
                                EasyMock.eq("--ramdisk"),
                                EasyMock.eq(ramdiskRecoveryImage.getAbsolutePath()),
                                EasyMock.eq("-o"),
                                EasyMock.matches(".*boot.*img.*")))
                .andReturn(cmdResult);
        EasyMock.replay(mMockDevice, mMockRunUtil);
        try {
            mPreparer.validateGkiBootImg(mMockDevice, mBuildInfo, mTmpDir);
            fail("TargetSetupError is expected");
        } catch (TargetSetupError e) {
            // expected
        }
        EasyMock.verify(mMockDevice, mMockRunUtil);
    }

    /* Verifies that setUp will throw TargetSetupError if there is no gki boot.img */
    @Test
    public void testSetUp_NoGkiBootImg() throws Exception {
        EasyMock.replay(mMockDevice, mMockRunUtil);
        try {
            mPreparer.setUp(mTestInfo);
            fail("TargetSetupError is expected");
        } catch (TargetSetupError e) {
            // expected
        }
        EasyMock.verify(mMockDevice, mMockRunUtil);
    }

    /* Verifies that setUp will throw exception when there is no valid mkbootimg in otatools.zip*/
    @Test
    public void testSetUp_NoMkbootimgInOtatoolsZip() throws Exception {
        File kernelImage = new File(mTmpDir, "Image.gz");
        mBuildInfo.setFile("Image.gz", kernelImage, "0");
        File ramdiskRecoveryImage = new File(mTmpDir, "ramdisk-recovery.img");
        mBuildInfo.setFile("ramdisk-recovery.img", ramdiskRecoveryImage, "0");
        File otaDir = FileUtil.createTempDir("otatool_folder", mTmpDir);
        File tmpFile = new File(otaDir, "test");
        File otatoolsZip = FileUtil.createTempFile("otatools", ".zip", mTmpDir);
        ZipUtil.createZip(List.of(tmpFile), otatoolsZip);
        mBuildInfo.setFile("otatools.zip", otatoolsZip, "0");
        EasyMock.replay(mMockDevice, mMockRunUtil);
        try {
            mPreparer.setUp(mTestInfo);
            fail("TargetSetupError is expected");
        } catch (TargetSetupError e) {
            // expected
        }
        EasyMock.verify(mMockDevice, mMockRunUtil);
    }

    /* Verifies that preparer can flash GKI boot image */
    @Test
    public void testSetup_Success() throws Exception {
        File bootImg = FileUtil.createTempFile("boot", ".img", mTmpDir);
        bootImg.renameTo(new File(mTmpDir, "boot.img"));
        FileUtil.writeToFile("ddd", bootImg);
        mBuildInfo.setFile("gki_boot.img", bootImg, "0");
        mMockDevice.waitForDeviceOnline();
        mMockDevice.rebootIntoBootloader();
        mMockRunUtil.allowInterrupt(false);
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
        mPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockDevice, mMockRunUtil);
    }

    /* Verifies that preparer can flash GKI boot image from a Zip file*/
    @Test
    public void testSetup_Success_FromZip() throws Exception {
        File gkiDir = FileUtil.createTempDir("gki_folder", mTmpDir);
        File bootImg = new File(gkiDir, "boot-5.4.img");
        FileUtil.writeToFile("ddd", bootImg);
        File gkiZip = FileUtil.createTempFile("gki_image", ".zip", mTmpDir);
        ZipUtil.createZip(List.of(bootImg), gkiZip);
        mBuildInfo.setFile("gki_boot.img", gkiZip, "0");
        mMockDevice.waitForDeviceOnline();
        mMockDevice.rebootIntoBootloader();
        mMockRunUtil.allowInterrupt(false);
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                EasyMock.eq("flash"),
                                EasyMock.eq("boot"),
                                EasyMock.matches(".*boot-5.4.img")))
                .andReturn(mSuccessResult);
        mMockRunUtil.allowInterrupt(true);
        doSetupExpectations();
        EasyMock.replay(mMockDevice, mMockRunUtil);
        mPreparer.setUp(mTestInfo);
        mPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockDevice, mMockRunUtil);
    }

    /* Verifies that preparer will throw TargetSetupError with GKI flash failure*/
    @Test
    public void testSetUp_GkiFlashFailure() throws Exception {
        File bootImg = FileUtil.createTempFile("boot", ".img", mTmpDir);
        bootImg.renameTo(new File(mTmpDir, "boot.img"));
        FileUtil.writeToFile("ddd", bootImg);
        mBuildInfo.setFile("gki_boot.img", bootImg, "0");
        File deviceImg = FileUtil.createTempFile("device_image", ".zip", mTmpDir);
        FileUtil.writeToFile("not an empty file", deviceImg);
        mBuildInfo.setDeviceImageFile(deviceImg, "0");
        mMockDevice.waitForDeviceOnline();
        mMockDevice.rebootIntoBootloader();
        mMockRunUtil.allowInterrupt(false);
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                "flash",
                                "boot",
                                mBuildInfo.getFile("gki_boot.img").getAbsolutePath()))
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
        File bootImg = FileUtil.createTempFile("boot", ".img", mTmpDir);
        bootImg.renameTo(new File(mTmpDir, "boot.img"));
        FileUtil.writeToFile("ddd", bootImg);
        mBuildInfo.setFile("gki_boot.img", bootImg, "0");
        File deviceImg = FileUtil.createTempFile("device_image", ".zip", mTmpDir);
        FileUtil.writeToFile("not an empty file", deviceImg);
        mBuildInfo.setDeviceImageFile(deviceImg, "0");
        mMockDevice.waitForDeviceOnline();
        mMockDevice.rebootIntoBootloader();
        mMockRunUtil.allowInterrupt(false);
        EasyMock.expect(
                        mMockDevice.executeLongFastbootCommand(
                                "flash",
                                "boot",
                                mBuildInfo.getFile("gki_boot.img").getAbsolutePath()))
                .andReturn(mSuccessResult);
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
