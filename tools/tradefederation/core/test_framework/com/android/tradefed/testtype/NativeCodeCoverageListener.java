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

package com.android.tradefed.testtype;

import static com.google.common.base.Verify.verify;
import static com.google.common.base.Verify.verifyNotNull;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.ResultForwarder;
import com.android.tradefed.testtype.coverage.CoverageOptions;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.NativeCodeCoverageFlusher;
import com.android.tradefed.util.TarUtil;
import com.android.tradefed.util.ZipUtil;

import com.google.common.collect.ImmutableList;

import java.io.File;
import java.io.IOException;
import java.util.Arrays;
import java.util.HashMap;

/**
 * A {@link ResultForwarder} that will pull native coverage measurements off of the device and log
 * them as test artifacts.
 */
public final class NativeCodeCoverageListener extends ResultForwarder {

    private static final String NATIVE_COVERAGE_DEVICE_PATH = "/data/misc/trace";
    private static final String COVERAGE_TAR_PATH =
            String.format("%s/coverage.tar", NATIVE_COVERAGE_DEVICE_PATH);

    // Finds .gcda files in /data/misc/trace and compresses those files only. Stores the full
    // path of the file on the device.
    private static final String ZIP_COVERAGE_FILES_COMMAND =
            String.format(
                    "find %s -name '*.gcda' | tar -cvf %s -T -",
                    NATIVE_COVERAGE_DEVICE_PATH, COVERAGE_TAR_PATH);

    // Deletes .gcda files in /data/misc/trace.
    private static final String DELETE_COVERAGE_FILES_COMMAND =
            String.format("find %s -name '*.gcda' -delete", NATIVE_COVERAGE_DEVICE_PATH);

    private final boolean mFlushCoverage;
    private final ITestDevice mDevice;
    private final NativeCodeCoverageFlusher mFlusher;
    private boolean mCollectCoverageOnTestEnd = true;

    private String mCurrentRunName;

    public NativeCodeCoverageListener(ITestDevice device, ITestInvocationListener... listeners) {
        super(listeners);
        mDevice = device;
        mFlushCoverage = false;
        mFlusher = new NativeCodeCoverageFlusher(mDevice, ImmutableList.of());
    }

    public NativeCodeCoverageListener(
            ITestDevice device,
            CoverageOptions coverageOptions,
            ITestInvocationListener... listeners) {
        super(listeners);
        mDevice = device;
        mFlushCoverage = coverageOptions.isCoverageFlushEnabled();
        mFlusher = new NativeCodeCoverageFlusher(mDevice, coverageOptions.getCoverageProcesses());
    }

    /**
     * Sets whether to collect coverage on testRunEnded.
     *
     * <p>Set this to false during re-runs, otherwise each individual test re-run will collect
     * coverage rather than having a single merged coverage result.
     */
    public void setCollectOnTestEnd(boolean collect) {
        mCollectCoverageOnTestEnd = collect;
    }

    @Override
    public void testRunStarted(String runName, int testCount) {
        super.testRunStarted(runName, testCount);
        mCurrentRunName = runName;
    }

    @Override
    public void testRunEnded(long elapsedTime, HashMap<String, Metric> runMetrics) {
        try {
            if (mCollectCoverageOnTestEnd) {
                logCoverageMeasurements(mCurrentRunName);
            }
        } finally {
            super.testRunEnded(elapsedTime, runMetrics);
        }
    }

    /** Pulls native coverage measurements from the device and logs them. */
    public void logCoverageMeasurements(String runName) {
        File coverageTar = null;
        File coverageZip = null;
        try {
            // Enable abd root on the device, otherwise the following commands will fail.
            verify(mDevice.enableAdbRoot(), "Failed to enable adb root.");

            // Flush cross-process coverage.
            if (mFlushCoverage) {
                mFlusher.forceCoverageFlush();
            }

            // Compress coverage measurements on the device before pulling.
            mDevice.executeShellCommand(ZIP_COVERAGE_FILES_COMMAND);
            coverageTar = mDevice.pullFile(COVERAGE_TAR_PATH);
            verifyNotNull(
                    coverageTar,
                    "Failed to pull the native code coverage file %s",
                    COVERAGE_TAR_PATH);
            mDevice.deleteFile(COVERAGE_TAR_PATH);

            coverageZip = convertTarToZip(coverageTar);

            try (FileInputStreamSource source = new FileInputStreamSource(coverageZip, true)) {
                testLog(runName + "_native_runtime_coverage", LogDataType.NATIVE_COVERAGE, source);
            }

            // Delete coverage files on the device.
            mDevice.executeShellCommand(DELETE_COVERAGE_FILES_COMMAND);
        } catch (DeviceNotAvailableException | IOException e) {
            throw new RuntimeException(e);
        } finally {
            FileUtil.deleteFile(coverageTar);
            FileUtil.deleteFile(coverageZip);
        }
    }

    /**
     * Converts a .tar file to a .zip file.
     *
     * @param tar the .tar file to convert
     * @return a .zip file with the same contents
     * @throws IOException
     */
    private File convertTarToZip(File tar) throws IOException {
        File untarDir = null;
        try {
            untarDir = FileUtil.createTempDir("gcov_coverage");
            TarUtil.unTar(tar, untarDir);
            return ZipUtil.createZip(Arrays.asList(untarDir.listFiles()), "native_coverage");
        } finally {
            FileUtil.recursiveDelete(untarDir);
        }
    }
}
