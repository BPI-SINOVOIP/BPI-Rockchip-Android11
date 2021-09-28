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

package com.android.build.tests;

import static org.junit.Assert.assertEquals;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.RunUtil;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.stream.Stream;

/** A device-less test that test kernel image */
@OptionClass(alias = "kernel-image-check")
@RunWith(DeviceJUnit4ClassRunner.class)
public class KernelImageCheck extends BaseHostJUnit4Test {

    private static final String KERNEL_IMAGE_NAME = "vmlinux";
    private static final String KERNEL_IMAGE_REPO = "common";
    private static final int CMD_TIMEOUT = 1000000;

    @Option(
        name = "kernel-image-check-tool",
        description = "The file path of kernel image check tool (mandatory)",
        mandatory = true
    )
    private File mKernelImageCheckTool = null;

    @Option(
        name = "kernel-image-name",
        description = "The file name of the kernel image. Default: vmlinux"
    )
    private String mKernelImageName = KERNEL_IMAGE_NAME;

    @Option(
        name = "kernel-image-alt-path",
        description = "The kernel image alternative path string"
    )
    private String mKernelImageAltPath = null;

    @Option(
        name = "kernel-abi-file",
        description = "The file path of kernel ABI file"
    )
    private File mKernelAbiFile = null;

    private IBuildInfo mBuildInfo;
    private File mKernelImageFile = null;
    private File mKernelAbiWhitelist = null;

    @Before
    public void setUp() throws Exception {
        if (mKernelImageCheckTool == null || !mKernelImageCheckTool.exists()) {
            throw new IOException("Cannot find kernel image tool at: " + mKernelImageCheckTool);
        }

        // If --kernel-abi-file has not been specified, try to find 'abi.xml'
        // within the downloaded files from the build.
        if (mKernelAbiFile == null) {
            mKernelAbiFile = getBuild().getFile("abi.xml");
        }

        // If there was not any 'abi.xml' and --kernel-abi-file was not set to
        // point at an external one, throw that.
        if (mKernelAbiFile == null) {
            throw new IOException("Cannot find abi.xml within the build results.");
        }

        if (!mKernelAbiFile.exists()) {
            throw new IOException("Cannot find kernel ABI representation at: " + mKernelAbiFile);
        }
        // First try to get kernel image from BuildInfo
        mKernelImageFile = getBuild().getFile(mKernelImageName);
        if ((mKernelImageFile == null || !mKernelImageFile.exists())
                && mKernelImageAltPath != null) {
            // Then check within alternative path.
            File imageDir = new File(mKernelImageAltPath);
            if (imageDir.isDirectory()) {
                mKernelImageFile = new File(imageDir, mKernelImageName);
            }
        }

        if (mKernelImageFile == null || !mKernelImageFile.exists()) {
            throw new RuntimeException("Cannot find kernel image file: " + mKernelImageName);
        }

        // This is an optional parameter. No need to check for an error here.
        mKernelAbiWhitelist = getBuild().getFile("abi_whitelist");
        if ((mKernelAbiWhitelist == null || !mKernelAbiWhitelist.exists())
                && mKernelImageAltPath != null) {
            // Then check within alternative path.
            File imageDir = new File(mKernelImageAltPath);
            if (imageDir.isDirectory()) {
                mKernelAbiWhitelist = new File(imageDir, "abi_whitelist");
            }
        }
    }

    /** Test that kernel ABI is not different from the given ABI representation */
    @Test
    public void test_stable_abi() throws Exception {
        // Figure out location of Linux source tree by inspecting vmlinux.
        String[] cmd = new String[] {"strings", mKernelImageFile.getPath(), "-d"};
        CommandResult result = RunUtil.getDefault().runTimedCmd(CMD_TIMEOUT, cmd);
        assertEquals(CommandStatus.SUCCESS, result.getStatus());

        String vmlinuxStrings = result.getStdout();
        File vmlinuxStringsFile = FileUtil.createTempFile("vmlinuxStrings", ".txt");
        FileUtil.writeToFile(vmlinuxStrings, vmlinuxStringsFile);

        cmd = new String[] {"grep", "-m", "1", "init/main.c", vmlinuxStringsFile.getAbsolutePath()};
        result = RunUtil.getDefault().runTimedCmd(CMD_TIMEOUT, cmd);
        assertEquals(CommandStatus.SUCCESS, result.getStatus());

        String repoRootDir = result.getStdout();
        // Source file absolute path is /repoRootDir/KERNEL_IMAGE_REPO/location-in-linux-tree.
        repoRootDir = repoRootDir.split("/" + KERNEL_IMAGE_REPO + "/", 2)[0];

        // Try to set the executable bit for abidw and abidiff
        Stream.of("abidw", "abidiff")
                .forEach(
                        bin -> {
                            String[] chmod =
                                    new String[] {
                                        "chmod",
                                        "u+x",
                                        mKernelImageCheckTool.getAbsolutePath() + "/" + bin
                                    };
                            RunUtil.getDefault().runTimedCmd(CMD_TIMEOUT, chmod);
                        });

        // Generate kernel ABI
        ArrayList<String> abidwCmd = new ArrayList<String>();
        abidwCmd.add(mKernelImageCheckTool.getAbsolutePath() + "/abidw");
        // omit various sources of indeterministic abidw output
        abidwCmd.add("--no-corpus-path");
        abidwCmd.add("--no-comp-dir-path");
        // the path containing vmlinux and *.ko
        abidwCmd.add("--linux-tree");
        abidwCmd.add(mKernelImageFile.getParent());
        abidwCmd.add("--out-file");
        abidwCmd.add("abi-new.xml");
        if (mKernelAbiWhitelist != null && mKernelAbiWhitelist.exists()) {
            abidwCmd.add("--kmi-whitelist");
            abidwCmd.add(mKernelAbiWhitelist.getAbsolutePath());
        }
        result = RunUtil.getDefault().runTimedCmd(CMD_TIMEOUT, abidwCmd.toArray(new String[0]));
        CLog.i("Result stdout: %s", result.getStdout());
        // TODO: differentiate non-zero exit codes.
        if (result.getExitCode() != 0) {
            CLog.e("Result stderr: %s", result.getStderr());
            CLog.e("Result exit code: %d", result.getExitCode());
        }
        assertEquals(CommandStatus.SUCCESS, result.getStatus());

        // Sanitize abi-new.xml by removing any occurences of the kernel path.
        cmd =
                new String[] {
                    "sed",
                    "-i",
                    "s#" + repoRootDir + "/" + KERNEL_IMAGE_REPO + "/##g",
                    "abi-new.xml"
                };
        result = RunUtil.getDefault().runTimedCmd(CMD_TIMEOUT, cmd);
        assertEquals(CommandStatus.SUCCESS, result.getStatus());

        cmd = new String[] {"sed", "-i", "s#" + repoRootDir + "/##g", "abi-new.xml"};
        result = RunUtil.getDefault().runTimedCmd(CMD_TIMEOUT, cmd);
        assertEquals(CommandStatus.SUCCESS, result.getStatus());

        // Diff kernel ABI with the given ABI file
        ArrayList<String> abidiffCmd = new ArrayList<String>();
        abidiffCmd.add(mKernelImageCheckTool.getAbsolutePath() + "/abidiff");
        abidiffCmd.add("--impacted-interfaces");
        abidiffCmd.add("--leaf-changes-only");
        abidiffCmd.add("--dump-diff-tree");
        abidiffCmd.add(mKernelAbiFile.getAbsolutePath());
        abidiffCmd.add("abi-new.xml");
        if (mKernelAbiWhitelist != null && mKernelAbiWhitelist.exists()) {
            abidiffCmd.add("--kmi-whitelist");
            abidiffCmd.add(mKernelAbiWhitelist.getAbsolutePath());
        }
        result = RunUtil.getDefault().runTimedCmd(CMD_TIMEOUT, abidiffCmd.toArray(new String[0]));
        CLog.i("Result stdout: %s", result.getStdout());
        if (result.getExitCode() != 0) {
            CLog.e("Result stderr: %s", result.getStderr());
            CLog.e("Result exit code: %d", result.getExitCode());
        }
        assertEquals(
                "Kernel's ABI has changed. See go/kernel-abi-monitoring for details.\n",
                CommandStatus.SUCCESS,
                result.getStatus());
    }
}
