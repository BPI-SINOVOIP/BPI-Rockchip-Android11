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
package com.android.tests.firmwaredtbo;

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.TargetFileUtils;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.zip.DataFormatException;
import java.util.zip.GZIPInputStream;
import java.util.zip.Inflater;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

/* Validates DTBO partition and DT overlay application. */
@RunWith(DeviceJUnit4ClassRunner.class)
public class FirmwareDtboVerification extends BaseHostJUnit4Test {
    // Path to platform block devices
    private static final String BLOCK_DEV_PATH = "/dev/block/platform";
    // Temporary dir in device.
    private static final String DEVICE_TEMP_DIR = "/data/local/tmp/";
    // Path to device tree.
    private static final String FDT_PATH = "/sys/firmware/fdt";
    // Indicates current slot suffix for A/B devices.
    private static final String PROPERTY_SLOT_SUFFIX = "ro.boot.slot_suffix";
    // Offset in DT header to read compression info.
    private static final int DT_HEADER_FLAGS_OFFSET = 16;
    // Bit mask to get compression format from flags field of DT header for version 1 DTBO header.
    private static final int COMPRESSION_FLAGS_BIT_MASK = 0x0f;
    // FDT Magic.
    private static final int FDT_MAGIC = 0xd00dfeed;
    // mkdtboimg.py tool name
    private static final String MKDTBOIMG_TOOL = "mkdtboimg.py";

    private static File mTemptFolder = null;
    private ITestDevice mDevice;
    private String mDeviceTestRoot = null;

    private int NO_COMPRESSION = 0x00;
    private int ZLIB_COMPRESSION = 0x01;
    private int GZIP_COMPRESSION = 0x02;

    @BeforeClass
    public static void oneTimeSetup() throws Exception {
        mTemptFolder = FileUtil.createTempDir("firmware-dtbo-verify");
        CLog.d("mTemptFolder: %s", mTemptFolder);
    }

    @Before
    public void setUp() throws Exception {
        Assume.assumeFalse("Skipping test for x86 ABI", getAbi().getName().contains("x86"));
        mDevice = getDevice();
        mDeviceTestRoot = new File(DEVICE_TEMP_DIR, getAbi().getBitness()).getAbsolutePath();
    }

    @AfterClass
    public static void postTestTearDown() {
        mTemptFolder.delete();
    }

    /* Validates DTBO partition using mkdtboimg.py */
    @Test
    public void testCheckDTBOPartition() throws Exception {
        // Dump dtbo image from device.
        String slot_suffix = mDevice.getProperty(PROPERTY_SLOT_SUFFIX);
        if (slot_suffix == null) {
            slot_suffix = "";
        }
        String currentDtboPartition = "dtbo" + slot_suffix;
        String[] options = {"-type", "l"};
        ArrayList<String> dtboPaths = TargetFileUtils.findFile(
                BLOCK_DEV_PATH, currentDtboPartition, Arrays.asList(options), mDevice);
        CLog.d("DTBO path %s", dtboPaths);
        Assert.assertFalse("Unable to find path to dtbo image on device.", dtboPaths.isEmpty());
        File hostDtboImage = new File(mTemptFolder, "dtbo");
        Assert.assertTrue("Pull " + dtboPaths.get(0) + " failed!",
                mDevice.pullFile(dtboPaths.get(0), hostDtboImage));
        CLog.d("hostDtboImage is %s", hostDtboImage);
        // Using mkdtboimg.py to extract dtbo image.
        File mkdtboimgBin = getTestInformation().getDependencyFile(MKDTBOIMG_TOOL, false);
        File unpackedDtbo = new File(mTemptFolder, "dumped_dtbo");
        RunUtil runUtil = new RunUtil();
        String[] cmds = {mkdtboimgBin.getAbsolutePath(), "dump", hostDtboImage.getAbsolutePath(),
                "-b", unpackedDtbo.getAbsolutePath()};
        long timeoutMs = 5 * 60 * 1000;
        CommandResult result = runUtil.runTimedCmd(timeoutMs, cmds);
        Assert.assertEquals("Invalid DTBO Image: " + result.getStderr(), CommandStatus.SUCCESS,
                result.getStatus());
        decompressDTEntries(hostDtboImage, unpackedDtbo);
    }

    /**
     * Decompresses DT entries based on the flag field in the DT Entry header.
     *
     * @param hostDtboImage DTBO image on host.
     * @param unpackedDtbo DTBO was unpacked.
     */
    private void decompressDTEntries(File hostDtboImage, File unpackedDtbo)
            throws IOException, DataFormatException {
        try (RandomAccessFile raf = new RandomAccessFile(hostDtboImage, "r")) {
            byte[] bytes = new byte[4];
            int[] dtboItems = new int[8];
            final int DTBO_MAGIC_IDX = 0;
            final int DTBO_TOTAL_SIZE_IDX = 1;
            final int DTBO_HEADER_SIZE_IDX = 2;
            final int DTBO_DT_ENTRY_SIZE_IDX = 3;
            final int DTBO_DT_ENTRY_COUNT_IDX = 4;
            final int DTBO_DT_ENTRY_OFFSET_IDX = 5;
            final int DTBO_PAGE_SIZE_IDX = 6;
            final int DTBO_VERSION_IDX = 7;
            for (int i = 0; i < 8; i++) {
                raf.read(bytes);
                dtboItems[i] = ByteBuffer.wrap(bytes).order(ByteOrder.BIG_ENDIAN).getInt();
                CLog.d("dtboItems[%s] is %s", i, dtboItems[i]);
            }
            if (dtboItems[DTBO_VERSION_IDX] > 0) {
                for (int dt_entry_idx = 0; dt_entry_idx < dtboItems[DTBO_DT_ENTRY_COUNT_IDX];
                        dt_entry_idx++) {
                    File dt_entry_file = new File(
                            String.format("%s.%s", unpackedDtbo.getAbsolutePath(), dt_entry_idx));
                    CLog.d("Checking %s", dt_entry_file.getAbsolutePath());
                    raf.seek(dtboItems[DTBO_DT_ENTRY_OFFSET_IDX] + DT_HEADER_FLAGS_OFFSET);
                    dtboItems[DTBO_DT_ENTRY_OFFSET_IDX] += dtboItems[DTBO_DT_ENTRY_SIZE_IDX];
                    raf.read(bytes);
                    int flags = ByteBuffer.wrap(bytes).order(ByteOrder.BIG_ENDIAN).getInt();
                    int compression_format = flags & COMPRESSION_FLAGS_BIT_MASK;
                    CLog.d("compression_format= %s", compression_format);
                    if ((compression_format != ZLIB_COMPRESSION)
                            && (compression_format != GZIP_COMPRESSION)) {
                        Assert.assertEquals(
                                String.format("Unknown compression format %d", compression_format),
                                compression_format, NO_COMPRESSION);
                    }
                    try (InputStream fileInputStream = new FileInputStream(dt_entry_file)) {
                        byte[] cpio_header = new byte[6];
                        if (compression_format == ZLIB_COMPRESSION) {
                            CLog.d("Decompressing %s with ZLIB_COMPRESSION",
                                    dt_entry_file.getAbsolutePath());
                            byte[] compressedData = new byte[(int) dt_entry_file.length()];
                            fileInputStream.read(compressedData);
                            Inflater inflater = new Inflater();
                            inflater.setInput(compressedData);
                            inflater.inflate(cpio_header);
                        } else if (compression_format == GZIP_COMPRESSION) {
                            CLog.d("Decompressing %s with GZIP_COMPRESSION",
                                    dt_entry_file.getAbsolutePath());
                            try (GZIPInputStream gzipStream = new GZIPInputStream(
                                         new BufferedInputStream(fileInputStream))) {
                                gzipStream.read(cpio_header);
                            }
                        } else {
                            fileInputStream.read(cpio_header);
                        }
                        int fdt_magic =
                                ByteBuffer.wrap(cpio_header).order(ByteOrder.BIG_ENDIAN).getInt();
                        CLog.d("fdt_magic: 0x%s", Integer.toHexString(fdt_magic));
                        Assert.assertEquals(
                                "Bad FDT(Flattened Device Tree) Format", fdt_magic, FDT_MAGIC);
                    }
                }
            }
        }
    }

    /* Verifies application of DT overlays. */
    @Test
    public void testVerifyOverlay() throws Exception {
        // testVerifyOverlay depend on testCheckDTBOPartition, check if previous test artifacts
        // exist, if not force run testCheckDTBOPartition().
        File hostDtboImage = new File(mTemptFolder, "dtbo");
        if (!hostDtboImage.exists()) {
            testCheckDTBOPartition();
        }
        String cmd = "cat /proc/cmdline |"
                + "grep -o \"'androidboot.dtbo_idx=[^ ]*'\" |"
                + "cut -d \"=\" -f 2 ";
        CommandResult cmdResult = mDevice.executeShellV2Command(cmd);
        Assert.assertEquals(String.format("Checking kernel dtbo index: %s", cmdResult.getStderr()),
                cmdResult.getExitCode().intValue(), 0);
        String overlay_idx_string = cmdResult.getStdout().replace("\n", "");
        CLog.d("overlay_idx_string=%s", overlay_idx_string);
        Assert.assertNotEquals(
                "Kernel command line missing androidboot.dtbo_idx", overlay_idx_string.length(), 0);
        String verificationTestPath = null;
        String ufdtVerifier = "ufdt_verify_overlay";
        ArrayList<String> matchedResults =
                TargetFileUtils.findFile(DEVICE_TEMP_DIR, ufdtVerifier, null, mDevice);
        for (String matchedResult : matchedResults) {
            if (!mDevice.isDirectory(matchedResult)) {
                verificationTestPath = matchedResult;
                break;
            }
        }
        String chmodCommand = String.format("chmod 755 %s", verificationTestPath);
        CLog.d(chmodCommand);
        cmdResult = mDevice.executeShellV2Command(chmodCommand);
        Assert.assertEquals("Unable to chmod:" + cmdResult.getStderr(), cmdResult.getStatus(),
                CommandStatus.SUCCESS);
        String ufdtVerifierParent = new File(verificationTestPath).getParent();
        String remoteFinalDTPath = new File(ufdtVerifierParent, "final_dt").getAbsolutePath();
        String copyCommand = String.format("cp %s %s", FDT_PATH, remoteFinalDTPath);
        CLog.d(copyCommand);
        cmdResult = mDevice.executeShellV2Command(String.format(copyCommand));
        Assert.assertEquals("Unable to copy to " + remoteFinalDTPath, cmdResult.getStatus(),
                CommandStatus.SUCCESS);
        ArrayList<String> overlayArg = new ArrayList<>();
        for (String overlay_idx : overlay_idx_string.split(",")) {
            String overlayFileName = "dumped_dtbo." + overlay_idx.replaceAll("\\s+$", "");
            File overlayFile = new File(mTemptFolder, overlayFileName);
            // Push the dumped overlay dtbo files to the same direcly of ufdt_verify_overlay
            File remoteOverLayFile = new File(ufdtVerifierParent, overlayFileName);
            CLog.d("Push remoteOverLayFile %s", remoteOverLayFile);
            if (!mDevice.pushFile(overlayFile, remoteOverLayFile.getAbsolutePath())) {
                Assert.fail("Push " + overlayFile + "fail!");
            }
            overlayArg.add(overlayFileName);
        }
        String verifyCmd = String.format("cd %s && ./ufdt_verify_overlay final_dt %s",
                ufdtVerifierParent, String.join(" ", overlayArg));
        CLog.d(verifyCmd);
        cmdResult = mDevice.executeShellV2Command(verifyCmd);
        Assert.assertEquals("Incorrect Overlay Application:" + cmdResult.getStderr(),
                cmdResult.getStatus(), CommandStatus.SUCCESS);
    }
}
