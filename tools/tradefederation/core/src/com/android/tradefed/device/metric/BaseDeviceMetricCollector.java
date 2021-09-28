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
package com.android.tradefed.device.metric;

import com.android.tradefed.build.BuildInfoKey.BuildInfoFileKey;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.config.Option;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;
import com.android.tradefed.testtype.suite.ModuleDefinition;
import com.android.tradefed.util.FileUtil;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;

/**
 * Base implementation of {@link IMetricCollector} that allows to start and stop collection on
 * {@link #onTestRunStart(DeviceMetricData)} and {@link #onTestRunEnd(DeviceMetricData, Map)}.
 */
public class BaseDeviceMetricCollector implements IMetricCollector {

    public static final String TEST_CASE_INCLUDE_GROUP_OPTION = "test-case-include-group";
    public static final String TEST_CASE_EXCLUDE_GROUP_OPTION = "test-case-exclude-group";

    @Option(name = "disable", description = "disables the metrics collector")
    private boolean mDisable = false;

    @Option(
        name = TEST_CASE_INCLUDE_GROUP_OPTION,
        description =
                "Specify a group to include as part of the collection,"
                        + "group can be specified via @MetricOption. Can be repeated."
                        + "Usage: @MetricOption(group = \"groupname\") to your test methods, then"
                        + "use --test-case-include-anotation groupename to only run your group."
    )
    private List<String> mTestCaseIncludeAnnotationGroup = new ArrayList<>();

    @Option(
        name = TEST_CASE_EXCLUDE_GROUP_OPTION,
        description =
                "Specify a group to exclude from the metric collection,"
                        + "group can be specified via @MetricOption. Can be repeated."
    )
    private List<String> mTestCaseExcludeAnnotationGroup = new ArrayList<>();

    private IInvocationContext mContext;
    private List<ITestDevice> mRealDeviceList;
    private ITestInvocationListener mForwarder;
    private Map<String, File> mTestArtifactFilePathMap = new HashMap<>();
    private DeviceMetricData mRunData;
    private DeviceMetricData mTestData;
    private String mTag;
    private String mRunName;

    /**
     * Variable for whether or not to skip the collection of one test case because it was filtered.
     */
    private boolean mSkipTestCase = false;
    /** Whether or not the collector was initialized already or not. */
    private boolean mWasInitDone = false;

    @Override
    public ITestInvocationListener init(
            IInvocationContext context, ITestInvocationListener listener) {
        mContext = context;
        mForwarder = listener;
        if (mWasInitDone) {
            throw new IllegalStateException(
                    String.format("init was called a second time on %s", this));
        }
        mWasInitDone = true;
        return this;
    }

    @Override
    public final List<ITestDevice> getDevices() {
        return mContext.getDevices();
    }

    /** Returns all the non-stub devices from the {@link #getDevices()} list. */
    public final List<ITestDevice> getRealDevices() {
        if (mRealDeviceList == null) {
            mRealDeviceList =
                    mContext.getDevices()
                            .stream()
                            .filter(d -> !(d.getIDevice() instanceof StubDevice))
                            .collect(Collectors.toList());
        }
        return mRealDeviceList;
    }

    /**
     * Retrieve the file from the test artifacts or module artifacts and cache
     * it in a map for the subsequent calls.
     *
     * @param fileName name of the file to look up in the artifacts.
     * @return File from the test artifact or module artifact. Returns null
     *         if file is not found.
     */
    public File getFileFromTestArtifacts(String fileName) {
        if (mTestArtifactFilePathMap.containsKey(fileName)) {
            return mTestArtifactFilePathMap.get(fileName);
        }

        File resolvedFile = resolveRelativeFilePath(fileName);
        if (resolvedFile != null) {
            CLog.i("Using file %s from %s", fileName, resolvedFile.getAbsolutePath());
            mTestArtifactFilePathMap.put(fileName, resolvedFile);
        }
        return resolvedFile;
    }

    @Override
    public final List<IBuildInfo> getBuildInfos() {
        return mContext.getBuildInfos();
    }

    @Override
    public final ITestInvocationListener getInvocationListener() {
        return mForwarder;
    }

    @Override
    public void onTestRunStart(DeviceMetricData runData) {
        // Does nothing
    }

    @Override
    public void onTestRunEnd(
            DeviceMetricData runData, final Map<String, Metric> currentRunMetrics) {
        // Does nothing
    }

    @Override
    public void onTestStart(DeviceMetricData testData) {
        // Does nothing
    }

    @Override
    public void onTestFail(DeviceMetricData testData, TestDescription test) {
        // Does nothing
    }

    @Override
    public void onTestAssumptionFailure(DeviceMetricData testData, TestDescription test) {
        // Does nothing
    }

    @Override
    public void onTestEnd(
            DeviceMetricData testData, final Map<String, Metric> currentTestCaseMetrics) {
        // Does nothing
    }

    @Override
    public void onTestEnd(
            DeviceMetricData testData,
            final Map<String, Metric> currentTestCaseMetrics,
            TestDescription test) {
        // Call the default implementation of onTestEnd if not overridden
        onTestEnd(testData, currentTestCaseMetrics);
    }

    /** =================================== */
    /** Invocation Listeners for forwarding */
    @Override
    public final void invocationStarted(IInvocationContext context) {
        mForwarder.invocationStarted(context);
    }

    @Override
    public final void invocationFailed(Throwable cause) {
        mForwarder.invocationFailed(cause);
    }

    @Override
    public final void invocationFailed(FailureDescription failure) {
        mForwarder.invocationFailed(failure);
    }

    @Override
    public final void invocationEnded(long elapsedTime) {
        mForwarder.invocationEnded(elapsedTime);
    }

    @Override
    public final void testLog(String dataName, LogDataType dataType, InputStreamSource dataStream) {
        mForwarder.testLog(dataName, dataType, dataStream);
    }

    @Override
    public final void testModuleStarted(IInvocationContext moduleContext) {
        mForwarder.testModuleStarted(moduleContext);
    }

    @Override
    public final void testModuleEnded() {
        mForwarder.testModuleEnded();
    }

    /** Test run callbacks */
    @Override
    public final void testRunStarted(String runName, int testCount) {
        testRunStarted(runName, testCount, 0);
    }

    @Override
    public final void testRunStarted(String runName, int testCount, int attemptNumber) {
        testRunStarted(runName, testCount, 0, System.currentTimeMillis());
    }

    @Override
    public final void testRunStarted(
            String runName, int testCount, int attemptNumber, long startTime) {
        mRunData = new DeviceMetricData(mContext);
        mRunName = runName;
        try {
            onTestRunStart(mRunData);
        } catch (Throwable t) {
            // Prevent exception from messing up the status reporting.
            CLog.e(t);
        }
        mForwarder.testRunStarted(runName, testCount, attemptNumber, startTime);
    }

    @Override
    public final void testRunFailed(String errorMessage) {
        mForwarder.testRunFailed(errorMessage);
    }

    @Override
    public final void testRunFailed(FailureDescription failure) {
        mForwarder.testRunFailed(failure);
    }

    @Override
    public final void testRunStopped(long elapsedTime) {
        mForwarder.testRunStopped(elapsedTime);
    }

    @Override
    public final void testRunEnded(long elapsedTime, HashMap<String, Metric> runMetrics) {
        try {
            onTestRunEnd(mRunData, runMetrics);
            mRunData.addToMetrics(runMetrics);
        } catch (Throwable t) {
            // Prevent exception from messing up the status reporting.
            CLog.e(t);
        }
        mForwarder.testRunEnded(elapsedTime, runMetrics);
    }

    /** Test cases callbacks */
    @Override
    public final void testStarted(TestDescription test) {
        testStarted(test, System.currentTimeMillis());
    }

    @Override
    public final void testStarted(TestDescription test, long startTime) {
        mTestData = new DeviceMetricData(mContext);
        mSkipTestCase = shouldSkip(test);
        if (!mSkipTestCase) {
            try {
                onTestStart(mTestData);
            } catch (Throwable t) {
                // Prevent exception from messing up the status reporting.
                CLog.e(t);
            }
        }
        mForwarder.testStarted(test, startTime);
    }

    @Override
    public final void testFailed(TestDescription test, String trace) {
        if (!mSkipTestCase) {
            try {
                onTestFail(mTestData, test);
            } catch (Throwable t) {
                // Prevent exception from messing up the status reporting.
                CLog.e(t);
            }
        }
        mForwarder.testFailed(test, trace);
    }

    @Override
    public final void testFailed(TestDescription test, FailureDescription failure) {
        if (!mSkipTestCase) {
            // Don't collect on not_executed test case
            if (failure.getFailureStatus() == null
                    || !FailureStatus.NOT_EXECUTED.equals(failure.getFailureStatus())) {
                try {
                    onTestFail(mTestData, test);
                } catch (Throwable t) {
                    // Prevent exception from messing up the status reporting.
                    CLog.e(t);
                }
            }
        }
        mForwarder.testFailed(test, failure);
    }

    @Override
    public final void testEnded(TestDescription test, HashMap<String, Metric> testMetrics) {
        testEnded(test, System.currentTimeMillis(), testMetrics);
    }

    @Override
    public final void testEnded(
            TestDescription test, long endTime, HashMap<String, Metric> testMetrics) {
        if (!mSkipTestCase) {
            try {
                onTestEnd(mTestData, testMetrics, test);
                mTestData.addToMetrics(testMetrics);
            } catch (Throwable t) {
                // Prevent exception from messing up the status reporting.
                CLog.e(t);
            }
        } else {
            CLog.d("Skipping %s collection for %s.", this.getClass().getName(), test.toString());
        }
        mForwarder.testEnded(test, endTime, testMetrics);
    }

    @Override
    public final void testAssumptionFailure(TestDescription test, String trace) {
        if (!mSkipTestCase) {
            try {
                onTestAssumptionFailure(mTestData, test);
            } catch (Throwable t) {
                // Prevent exception from messing up the status reporting.
                CLog.e(t);
            }
        }
        mForwarder.testAssumptionFailure(test, trace);
    }

    @Override
    public final void testAssumptionFailure(TestDescription test, FailureDescription failure) {
        if (!mSkipTestCase) {
            try {
                onTestAssumptionFailure(mTestData, test);
            } catch (Throwable t) {
                // Prevent exception from messing up the status reporting.
                CLog.e(t);
            }
        }
        mForwarder.testAssumptionFailure(test, failure);
    }

    @Override
    public final void testIgnored(TestDescription test) {
        mForwarder.testIgnored(test);
    }

    /** {@inheritDoc} */
    @Override
    public final boolean isDisabled() {
        return mDisable;
    }

    /** {@inheritDoc} */
    @Override
    public final void setDisable(boolean isDisabled) {
        mDisable = isDisabled;
    }

    /**
     * Sets the {@code mTag} of the collector. It can be used to specify the interval of the
     * collector.
     *
     * @param tag the unique identifier of the collector.
     */
    public void setTag(String tag) {
        mTag = tag;
    }

    /**
     * Returns the identifier {@code mTag} of the collector.
     *
     * @return mTag, the unique identifier of the collector.
     */
    public String getTag() {
        return mTag;
    }

    /**
     * Returns the name of test run {@code mRunName} that triggers the collector.
     *
     * @return mRunName, the current test run name.
     */
    public String getRunName() {
        return mRunName;
    }

    /**
     * Helper to decide if a test case should or not run the collector method associated.
     *
     * @param desc the identifier of the test case.
     * @return True the collector should be skipped. False otherwise.
     */
    private boolean shouldSkip(TestDescription desc) {
        Set<String> testCaseGroups = new HashSet<>();
        if (desc.getAnnotation(MetricOption.class) != null) {
            String groupName = desc.getAnnotation(MetricOption.class).group();
            testCaseGroups.addAll(Arrays.asList(groupName.split(",")));
        } else {
            // Add empty group name for default case.
            testCaseGroups.add("");
        }
        // Exclusion has priority: if any of the groups is excluded, exclude the test case.
        for (String groupName : testCaseGroups) {
            if (mTestCaseExcludeAnnotationGroup.contains(groupName)) {
                return true;
            }
        }
        // Inclusion filter: if any of the group is included, include the test case.
        for (String includeGroupName : mTestCaseIncludeAnnotationGroup) {
            if (testCaseGroups.contains(includeGroupName)) {
                return false;
            }
        }

        // If we had filters and did not match any groups
        if (!mTestCaseIncludeAnnotationGroup.isEmpty()) {
            return true;
        }
        return false;
    }

    /**
     * Resolves the relative path of the file from the test artifacts
     * directory or module directory.
     *
     * @param fileName file name that needs to be resolved.
     * @return File file resolved for the given file name. Returns null
     *         if file not found.
     */
    private File resolveRelativeFilePath(String fileName) {
        IBuildInfo buildInfo = getBuildInfos().get(0);
        String mModuleName = null;
        // Retrieve the module name.
        if (mContext.getAttributes().get(ModuleDefinition.MODULE_NAME) != null) {
            mModuleName = mContext.getAttributes().get(ModuleDefinition.MODULE_NAME)
                    .get(0);
        }

        File src = null;
        if (buildInfo != null) {
            src = buildInfo.getFile(fileName);
            if (src != null && src.exists()) {
                return src;
            }
        }

        if (buildInfo instanceof IDeviceBuildInfo) {
            IDeviceBuildInfo deviceBuild = (IDeviceBuildInfo) buildInfo;
            File testDir = deviceBuild.getTestsDir();
            List<File> scanDirs = new ArrayList<>();
            // If it exists, always look first in the ANDROID_TARGET_OUT_TESTCASES
            File targetTestCases = deviceBuild.getFile(BuildInfoFileKey.TARGET_LINKED_DIR);
            if (targetTestCases != null) {
                scanDirs.add(targetTestCases);
            }
            // If not, look into the test directory.
            if (testDir != null) {
                scanDirs.add(testDir);
            }

            if (mModuleName != null) {
                // Use module name as a discriminant to find some files
                if (testDir != null) {
                    try {
                        File moduleDir = FileUtil.findDirectory(
                                mModuleName, scanDirs.toArray(new File[] {}));
                        if (moduleDir != null) {
                            // If the spec is pushing the module itself
                            if (mModuleName.equals(fileName)) {
                                // If that's the main binary generated by the target, we push the
                                // full directory
                                return moduleDir;
                            }
                            // Search the module directory if it exists use it in priority
                            src = FileUtil.findFile(fileName, null, moduleDir);
                            if (src != null) {
                                CLog.i("Retrieving src file from" + src.getAbsolutePath());
                                return src;
                            }
                        } else {
                            CLog.d("Did not find any module directory for '%s'", mModuleName);
                        }

                    } catch (IOException e) {
                        CLog.w(
                                "Something went wrong while searching for the module '%s' "
                                        + "directory.",
                                mModuleName);
                    }
                }
            }

            for (File searchDir : scanDirs) {
                try {
                    Set<File> allMatch = FileUtil.findFilesObject(searchDir, fileName);
                    if (allMatch.size() > 1) {
                        CLog.d(
                                "Several match for filename '%s', searching for top-level match.",
                                fileName);
                        for (File f : allMatch) {
                            if (f.getParent().equals(searchDir.getAbsolutePath())) {
                                return f;
                            }
                        }
                    } else if (allMatch.size() == 1) {
                        return allMatch.iterator().next();
                    }
                } catch (IOException e) {
                    CLog.w("Failed to find test files from directory.");
                }
            }
        }
        return src;
    }

}
