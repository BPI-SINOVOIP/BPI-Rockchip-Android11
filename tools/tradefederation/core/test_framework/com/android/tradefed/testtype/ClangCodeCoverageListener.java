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

package com.android.tradefed.testtype;

import static com.google.common.base.Verify.verify;
import static com.google.common.base.Verify.verifyNotNull;

import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationReceiver;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.ResultForwarder;
import com.android.tradefed.testtype.coverage.CoverageOptions;
import com.android.tradefed.testtype.coverage.CoverageOptions.Toolchain;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.NativeCodeCoverageFlusher;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.TarUtil;
import com.android.tradefed.util.ZipUtil;

import com.google.common.annotations.VisibleForTesting;

import java.io.File;
import java.io.IOException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Set;

/**
 * A {@link ResultForwarder} that will pull Clang coverage measurements off of the device and log
 * them as test artifacts.
 */
public final class ClangCodeCoverageListener extends ResultForwarder
        implements IConfigurationReceiver {

    private static final String NATIVE_COVERAGE_DEVICE_PATH = "/data/misc/trace";
    private static final String COVERAGE_TAR_PATH =
            String.format("%s/coverage.tar", NATIVE_COVERAGE_DEVICE_PATH);

    // Finds .profraw files in /data/misc/trace and compresses those files only. Stores the full
    // path of the file on the device.
    private static final String ZIP_CLANG_FILES_COMMAND =
            String.format(
                    "find %s -name '*.profraw' | tar -cvf %s -T -",
                    NATIVE_COVERAGE_DEVICE_PATH, COVERAGE_TAR_PATH);

    // Deletes .profraw files in /data/misc/trace.
    private static final String DELETE_COVERAGE_FILES_COMMAND =
            String.format("find %s -name '*.profraw' -delete", NATIVE_COVERAGE_DEVICE_PATH);

    private final ITestDevice mDevice;

    private IBuildInfo mBuildInfo;
    private IConfiguration mConfiguration;
    private IRunUtil mRunUtil;

    private NativeCodeCoverageFlusher mFlusher;

    private File mLlvmProfdataTool;
    private String mCurrentRunName;

    public ClangCodeCoverageListener(ITestDevice device, ITestInvocationListener... listeners) {
        super(listeners);
        mDevice = device;
        mRunUtil = RunUtil.getDefault();
    }

    @Override
    public void setConfiguration(IConfiguration configuration) {
        mConfiguration = configuration;
    }

    @VisibleForTesting
    public void setRunUtil(IRunUtil runUtil) {
        mRunUtil = runUtil;
    }

    @Override
    public void testRunStarted(String runName, int testCount) {
        mCurrentRunName = runName;
        super.testRunStarted(runName, testCount);
    }

    @Override
    public void testRunEnded(long elapsedTime, HashMap<String, Metric> runMetrics) {
        CoverageOptions options = mConfiguration.getCoverageOptions();
        try {
            if (options.isCoverageEnabled()
                    && options.getCoverageToolchains().contains(Toolchain.CLANG)) {
                // Enable abd root on the device, otherwise the following commands will fail.
                verify(mDevice.enableAdbRoot(), "Failed to enable adb root.");

                if (options.isCoverageFlushEnabled()) {
                    getCoverageFlusher().forceCoverageFlush();
                }
                logCoverageMeasurement(mCurrentRunName);

                // Delete coverage files on the device.
                mDevice.executeShellCommand(DELETE_COVERAGE_FILES_COMMAND);
            }
        } catch (DeviceNotAvailableException | IOException e) {
            throw new RuntimeException(e);
        } finally {
            super.testRunEnded(elapsedTime, runMetrics);
        }
    }

    @Override
    public void invocationEnded(long elapsedTime) {
        // Clean up the llvm-profdata tool.
        FileUtil.recursiveDelete(mLlvmProfdataTool);
        super.invocationEnded(elapsedTime);
    }

    /**
     * Logs Clang coverage measurements from the device.
     *
     * @param runName name used in the log file
     * @throws DeviceNotAvailableException
     * @throws IOException
     */
    private void logCoverageMeasurement(String runName)
            throws DeviceNotAvailableException, IOException {
        File coverageTarGz = null;
        File untarDir = null;
        File profileTool = null;
        File indexedProfileFile = null;
        try {
            // Compress coverage measurements on the device before pulling.
            mDevice.executeShellCommand(ZIP_CLANG_FILES_COMMAND);
            coverageTarGz = mDevice.pullFile(COVERAGE_TAR_PATH);
            verifyNotNull(
                    coverageTarGz,
                    "Failed to pull the Clang code coverage file %s",
                    COVERAGE_TAR_PATH);
            mDevice.deleteFile(COVERAGE_TAR_PATH);

            untarDir = FileUtil.createTempDir("clang_coverage");
            TarUtil.unTar(coverageTarGz, untarDir);
            Set<String> rawProfileFiles = FileUtil.findFiles(untarDir, ".*\\.profraw");

            if (rawProfileFiles.isEmpty()) {
                CLog.i("No Clang code coverage measurements found.");
                return;
            }

            CLog.i("Received Clang code coverage measurements: %s", rawProfileFiles);

            // Get the llvm-profdata tool from the build. This tool must match the same one used to
            // compile the build, otherwise this action will fail.
            profileTool = getProfileTool();
            Path profileBin = profileTool.toPath().resolve("bin/llvm-profdata");
            profileBin.toFile().setExecutable(true);

            List<String> command = new ArrayList<>();
            command.add(profileBin.toString());
            command.add("merge");
            command.add("-sparse");

            // Add all .profraw files from untarDir.
            command.addAll(rawProfileFiles);

            // Create the output file.
            indexedProfileFile =
                    FileUtil.createTempFile(runName + "_clang_runtime_coverage", ".profdata");
            command.add("-o");
            command.add(indexedProfileFile.getAbsolutePath());

            CommandResult result = mRunUtil.runTimedCmd(0, command.toArray(new String[0]));
            if (result.getStatus() != CommandStatus.SUCCESS) {
                throw new IOException(
                        "Failed to merge Clang profile data in "
                                + command.toString()
                                + " "
                                + result.toString());
            }

            try (FileInputStreamSource source =
                    new FileInputStreamSource(indexedProfileFile, true)) {
                testLog(runName + "_clang_runtime_coverage", LogDataType.CLANG_COVERAGE, source);
            }
        } finally {
            FileUtil.deleteFile(coverageTarGz);
            FileUtil.recursiveDelete(untarDir);
            FileUtil.deleteFile(indexedProfileFile);
        }
    }

    /**
     * Creates a {@link NativeCodeCoverageFlusher} if one does not already exist.
     *
     * @return a NativeCodeCoverageFlusher
     */
    private NativeCodeCoverageFlusher getCoverageFlusher() {
        if (mFlusher == null) {
            verifyNotNull(mConfiguration);
            verifyNotNull(mDevice);
            mFlusher =
                    new NativeCodeCoverageFlusher(
                            mDevice, mConfiguration.getCoverageOptions().getCoverageProcesses());
        }
        return mFlusher;
    }

    /**
     * Retrieves the profile tool and dependencies from the build, and extracts them.
     *
     * @return the directory containing the profile tool and dependencies
     */
    private File getProfileTool() throws IOException {
        // If we have a cached version of the profile tool already, use it.
        if (mLlvmProfdataTool != null) {
            return mLlvmProfdataTool;
        }

        // If llvm-profdata-path was set in the Configuration, pass it through. Don't save the path
        // locally since the parent process is responsible for cleaning it up.
        File configurationTool = mConfiguration.getCoverageOptions().getLlvmProfdataPath();
        if (configurationTool != null) {
            return configurationTool;
        }

        // Otherwise, try to download llvm-profdata.zip from the build and cache it.
        File profileToolZip = null;
        try {
            IBuildInfo buildInfo = mConfiguration.getBuildProvider().getBuild();
            profileToolZip =
                    verifyNotNull(
                            buildInfo.getFile("llvm-profdata.zip"),
                            "Could not get llvm-profdata.zip from the build.");
            mLlvmProfdataTool = ZipUtil.extractZipToTemp(profileToolZip, "llvm-profdata");
            return mLlvmProfdataTool;
        } catch (BuildRetrievalError e) {
            throw new RuntimeException(e);
        } finally {
            FileUtil.deleteFile(profileToolZip);
        }
    }
}
