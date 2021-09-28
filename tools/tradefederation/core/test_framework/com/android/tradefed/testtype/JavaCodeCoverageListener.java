/*
 * Copyright (C) 2017 The Android Open Source Project
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

import static com.google.common.base.Verify.verifyNotNull;
import static com.google.common.io.Files.getNameWithoutExtension;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.ResultForwarder;
import com.android.tradefed.testtype.coverage.CoverageOptions;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.JavaCodeCoverageFlusher;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Splitter;
import com.google.common.collect.ImmutableList;

import org.jacoco.core.tools.ExecFileLoader;

import java.io.File;
import java.io.IOException;
import java.util.HashMap;
import java.util.List;

/**
 * A {@link ResultForwarder} that will pull Java coverage measurements off of the device and log
 * them as test artifacts.
 */
final class JavaCodeCoverageListener extends ResultForwarder {

    public static final String MERGE_COVERAGE_MEASUREMENTS_TEST_NAME = "mergeCoverageMeasurements";
    public static final String COVERAGE_MEASUREMENT_KEY = "coverageFilePath";
    public static final String COVERAGE_DIRECTORY = "/data/misc/trace";
    public static final String FIND_COVERAGE_FILES =
            String.format("find %s -name '*.ec'", COVERAGE_DIRECTORY);

    private final ITestDevice mDevice;
    private final CoverageOptions mCoverageOptions;

    private final boolean mMergeCoverageMeasurements;

    private final ExecFileLoader mExecFileLoader = new ExecFileLoader();

    private JavaCodeCoverageFlusher mFlusher;
    private String mCurrentRunName;

    public JavaCodeCoverageListener(
            ITestDevice device,
            CoverageOptions options,
            boolean mergeMeasurements,
            ITestInvocationListener... listeners) {
        super(listeners);
        mDevice = device;
        mCoverageOptions = options;
        mMergeCoverageMeasurements = mergeMeasurements;

        mFlusher = new JavaCodeCoverageFlusher(device, options.getCoverageProcesses());
    }

    @VisibleForTesting
    public void setCoverageFlusher(JavaCodeCoverageFlusher flusher) {
        mFlusher = flusher;
    }

    @Override
    public void testRunStarted(String runName, int testCount) {
        super.testRunStarted(runName, testCount);
        mCurrentRunName = runName;
    }

    @Override
    public void testRunEnded(long elapsedTime, HashMap<String, Metric> runMetrics) {
        if (MERGE_COVERAGE_MEASUREMENTS_TEST_NAME.equals(mCurrentRunName)) {
            // Log the merged runtime coverage measurement.
            try {
                File mergedMeasurements =
                        FileUtil.createTempFile(
                                "merged_runtime_coverage_",
                                "." + LogDataType.COVERAGE.getFileExt());

                mExecFileLoader.save(mergedMeasurements, false);

                // Save the merged measurement as a test log.
                logCoverageMeasurement("merged_runtime_coverage", mergedMeasurements);
            } catch (IOException e) {
                throw new RuntimeException(e);
            } finally {
                super.testRunEnded(elapsedTime, runMetrics);
            }
        } else {
            // Get the path of the coverage measurement on the device.
            Metric devicePathMetric = runMetrics.get(COVERAGE_MEASUREMENT_KEY);
            if (devicePathMetric == null) {
                super.testRunFailed("No Java code coverage measurement.");
                super.testRunEnded(elapsedTime, runMetrics);
                return;
            }
            String testCoveragePath = devicePathMetric.getMeasurements().getSingleString();
            if (testCoveragePath == null) {
                super.testRunFailed("No Java code coverage measurement.");
                super.testRunEnded(elapsedTime, runMetrics);
                return;
            }

            ImmutableList.Builder<String> devicePaths = ImmutableList.builder();
            devicePaths.add(testCoveragePath);

            try {
                if (mCoverageOptions.isCoverageFlushEnabled()) {
                    mFlusher.forceCoverageFlush();
                }

                // Find all .ec files in /data/misc/trace and pull them from the device as well.
                String fileList = mDevice.executeShellCommand(FIND_COVERAGE_FILES);
                devicePaths.addAll(Splitter.on('\n').omitEmptyStrings().split(fileList));

                collectAndLogCoverageMeasurementsAsRoot(devicePaths.build());

            } catch (DeviceNotAvailableException | IOException e) {
                throw new RuntimeException(e);
            } finally {
                super.testRunEnded(elapsedTime, runMetrics);
            }
        }
    }

    private void logCoverageMeasurement(String name, File coverageFile) {
        try (FileInputStreamSource source = new FileInputStreamSource(coverageFile, true)) {
            testLog(name, LogDataType.COVERAGE, source);
        }
    }

    private void collectAndLogCoverageMeasurementsAsRoot(List<String> devicePaths)
            throws IOException, DeviceNotAvailableException {

        // We enable root before pulling files off the device since the coverage file of the test
        // process is written in its private directory which is otherwise inaccessible. Coverage
        // files of other processes should be accessible without root. Note that we also restore
        // root status to what it was after we're done to not interfere with subsequent tests that
        // run on the device.
        boolean wasRoot = mDevice.isAdbRoot();
        if (!wasRoot && !mDevice.enableAdbRoot()) {
            throw new RuntimeException(
                    "Failed to enable root before pulling Java code coverage files off device");
        }

        try {
            collectAndLogCoverageMeasurements(devicePaths);
        } finally {
            for (String devicePath : devicePaths) {
                mDevice.deleteFile(devicePath);
            }

            if (!wasRoot && !mDevice.disableAdbRoot()) {
                throw new RuntimeException(
                        "Failed to disable root after pulling Java code coverage files off device");
            }
        }
    }

    private void collectAndLogCoverageMeasurements(List<String> devicePaths)
            throws IOException, DeviceNotAvailableException {

        for (String devicePath : devicePaths) {
            File coverageFile = mDevice.pullFile(devicePath);
            verifyNotNull(
                    coverageFile, "Failed to pull the Java code coverage file from %s", devicePath);

            // When merging, load the measurement data. Otherwise log the measurement
            // immediately.
            try {
                if (mMergeCoverageMeasurements) {
                    mExecFileLoader.load(coverageFile);
                } else {
                    logCoverageMeasurement(
                            mCurrentRunName
                                    + "_"
                                    + getNameWithoutExtension(devicePath)
                                    + "_runtime_coverage",
                            coverageFile);
                }
            } finally {
                FileUtil.deleteFile(coverageFile);
            }
        }
    }
}
