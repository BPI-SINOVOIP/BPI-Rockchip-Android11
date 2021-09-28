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
package com.android.tests.firmware;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.util.AbiFormatter;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.TargetFileUtils;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.zip.GZIPInputStream;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

/* VTS test to verify boot/recovery image header. */
@RunWith(DeviceJUnit4ClassRunner.class)
public class FirmwareBootHeaderVerification extends BaseHostJUnit4Test {
    private static final int PLATFORM_API_LEVEL_P = 28;
    // Path to platform block devices.
    private static final String BLOCK_DEV_PATH = "/dev/block/platform";
    // Indicates current slot suffix for A/B devices.
    private static final String PROPERTY_SLOT_SUFFIX = "ro.boot.slot_suffix";

    private ITestDevice mDevice;
    private String mBlockDevPath = BLOCK_DEV_PATH;
    private int mLaunchApiLevel;
    private String mSlotSuffix = null;
    private static File mTemptFolder = null;
    private ArrayList<String> mSupportedAbis = null;

    @BeforeClass
    public static void oneTimeSetup() throws Exception {
        mTemptFolder = FileUtil.createTempDir("firmware-boot-header-verify");
    }

    @Before
    public void setUp() throws Exception {
        mDevice = getDevice();
        mLaunchApiLevel = mDevice.getLaunchApiLevel();
        mSlotSuffix = mDevice.getProperty(PROPERTY_SLOT_SUFFIX);
        if (mSlotSuffix == null) {
            mSlotSuffix = "";
        }
        CLog.i("Current slot suffix: %s", mSlotSuffix);
        mSupportedAbis = new ArrayList<>(Arrays.asList(AbiFormatter.getSupportedAbis(mDevice, "")));
        Assume.assumeTrue("Skipping test for x86 NON-ACPI ABI", isFullfeelPrecondition());
    }

    private boolean isFullfeelPrecondition() throws DeviceNotAvailableException {
        if (mSupportedAbis.contains("x86")) {
            mBlockDevPath = "/dev/block";
            CommandResult cmdResult = mDevice.executeShellV2Command("cat /proc/cmdline |"
                    + "grep -o \"'androidboot.acpio_idx=[^ ]*'\" |"
                    + "cut -d \"=\" -f 2 ");
            Assert.assertEquals(String.format("Checking if x86 device is NON-ACPI ABI: %s",
                                        cmdResult.getStderr()),
                    cmdResult.getExitCode().intValue(), 0);
            String acpio_idx_string = cmdResult.getStdout().replace("\n", "");
            if (acpio_idx_string.equals("")) {
                return false;
            }
        }
        return true;
    }

    private void CheckImageHeader(String imagePath, boolean isRecovery) throws IOException {
        BootImageInfo bootImgInfo = new BootImageInfo(imagePath);
        // Check kernel size.
        Assert.assertNotEquals(
                "boot.img/recovery.img must contain kernel", bootImgInfo.getKernelSize(), 0);
        if (mLaunchApiLevel > PLATFORM_API_LEVEL_P) {
            // Check image version.
            Assert.assertTrue("Device must at least have a boot image of version 2",
                    (bootImgInfo.getImgHeaderVer() >= 2));
            // Check ramdisk size.
            Assert.assertNotEquals(
                    "boot.img must contain ramdisk", bootImgInfo.getRamdiskSize(), 0);
        } else {
            Assert.assertTrue("Device must at least have a boot image of version 1",
                    (bootImgInfo.getImgHeaderVer() >= 1));
        }
        if (isRecovery) {
            Assert.assertNotEquals(
                    "recovery partition for non-A/B devices must contain the recovery DTBO",
                    bootImgInfo.getRecoveryDtboSize(), 0);
        }
        if (bootImgInfo.getImgHeaderVer() == 2) {
            Assert.assertNotEquals(
                    "Boot/recovery image must contain DTB", bootImgInfo.getDtbSize(), 0);
        }
        Assert.assertEquals(
                String.format(
                        "Test failure due to boot header size mismatch. Expected %s Actual %s",
                        bootImgInfo.getExpectHeaderSize(), bootImgInfo.getBootHeaderSize()),
                bootImgInfo.getExpectHeaderSize(), bootImgInfo.getBootHeaderSize());
    }

    @AfterClass
    public static void postTestTearDown() {
        mTemptFolder.delete();
    }

    /* Validates boot image header. */
    @Test
    public void testBootImageHeader() throws Exception {
        String currentBootPartition = "boot" + mSlotSuffix;
        String[] options = {"-type", "l"};
        ArrayList<String> bootPaths = TargetFileUtils.findFile(
                mBlockDevPath, currentBootPartition, Arrays.asList(options), mDevice);
        CLog.d("Boot path %s", bootPaths);
        Assert.assertFalse("Unable to find path to boot image on device.", bootPaths.isEmpty());
        File localBootImg = new File(mTemptFolder, "boot.img");
        Assert.assertTrue("Pull " + bootPaths.get(0) + " failed!",
                mDevice.pullFile(bootPaths.get(0), localBootImg));
        CLog.d("Remote boot path %s", bootPaths);
        CLog.d("Local boot path %s", localBootImg.getAbsolutePath());
        CheckImageHeader(localBootImg.getAbsolutePath(), false);
    }

    /* Validates recovery image header. */
    @Test
    public void testRecoveryImageHeader() throws Exception {
        Assume.assumeTrue("Skipping test: A/B devices do not have a separate recovery partition",
                mSlotSuffix.equals(""));
        String[] options = {"-type", "l"};
        ArrayList<String> recoveryPaths = TargetFileUtils.findFile(
                mBlockDevPath, "recovery", Arrays.asList(options), mDevice);
        CLog.d("Recovery path %s", recoveryPaths);
        Assert.assertFalse(
                "Unable to find path to recovery image on device.", recoveryPaths.isEmpty());
        File localRecoveryImg = new File(mTemptFolder, "recovery.img");
        Assert.assertTrue("Pull " + recoveryPaths.get(0) + " failed!",
                mDevice.pullFile(recoveryPaths.get(0), localRecoveryImg));
        CLog.d("Remote boot path %s", recoveryPaths);
        CLog.d("Local boot path %s", localRecoveryImg.getAbsolutePath());
        CheckImageHeader(localRecoveryImg.getAbsolutePath(), true);
    }
}
