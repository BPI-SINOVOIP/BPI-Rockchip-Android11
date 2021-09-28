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
package android.device.collectors;

import android.device.collectors.annotations.OptionClass;
import android.os.Bundle;
import android.util.Log;

import androidx.annotation.VisibleForTesting;

import org.junit.runner.Description;
import org.junit.runner.notification.Failure;
import org.junit.runner.Result;

import java.io.File;
import java.io.IOException;
import java.util.Arrays;
import java.util.Date;
import java.util.HashMap;
import java.text.SimpleDateFormat;

/**
 * A {@link LogcatCollector} that captures logcat after each test.
 *
 * This class needs external storage permission. See {@link BaseMetricListener} how to grant
 * external storage permission, especially at install time.
 *
 */
@OptionClass(alias = "logcat-collector")
public class LogcatCollector extends BaseMetricListener {
    @VisibleForTesting
    static final SimpleDateFormat DATE_FORMATTER = new SimpleDateFormat("MM-dd HH:mm:ss.SSS");

    @VisibleForTesting static final String METRIC_SEP = "-";
    @VisibleForTesting static final String FILENAME_SUFFIX = "logcat";
    @VisibleForTesting static final String BEFORE_LOGCAT_DURATION_SECS =
            "before-logcat-duration-secs";
    @VisibleForTesting static final String COLLECT_ON_FAILURE_ONLY = "collect-on-failure-only";
    @VisibleForTesting static final String RETURN_LOGCAT_DIR = "return-logcat-directory";
    @VisibleForTesting static final String DEFAULT_DIR = "run_listeners/logcats";

    private static final int BUFFER_SIZE = 16 * 1024;


    private File mDestDir;
    private String mStartTime = null;
    private boolean mTestFailed = false;
    // Logcat duration to include before the test starts.
    private long mBeforeLogcatDurationInSecs = 0;
    // Use this flag to enable logcat collection only when the test fails.
    private boolean mCollectOnlyTestFailed = false;
    // Use this flag to return the root directory of the logcat files in run metrics
    // otherwise individual logcat file will be reported associated with the test.
    // The final directory which contains all the logcat files will be <DEFAULT_DIR>_all.
    private boolean mReturnLogcatDir = false;

    // Map to keep track of test iterations for multiple test iterations.
    private HashMap<Description, Integer> mTestIterations = new HashMap<>();

    public LogcatCollector() {
        super();
    }

    /**
     * Constructor to simulate receiving the instrumentation arguments. Should not be used except
     * for testing.
     */
    @VisibleForTesting
    LogcatCollector(Bundle args) {
        super(args);
    }

    @Override
    public void onTestRunStart(DataRecord runData, Description description) {
        setupAdditionalArgs();
        mDestDir = createAndEmptyDirectory(DEFAULT_DIR);
        // Capture the start time in case onTestStart() is never called due to failure during
        // @BeforeClass.
        mStartTime = getLogcatStartTime();
    }

    @Override
    public void onTestStart(DataRecord testData, Description description) {
        // Capture the start time for logcat purpose.
        // Overwrites any start time set prior to the test and adds custom
        // duration to capture before current start time.
        mStartTime = getLogcatStartTime();
        // Keep track of test iterations.
        mTestIterations.computeIfPresent(description, (desc, iteration) -> iteration + 1);
        mTestIterations.computeIfAbsent(description, desc -> 1);
    }

    /**
     * Mark the test as failed if this is called. The actual collection will be done in {@link
     * onTestEnd} to ensure that all actions around a test failure end up in the logcat.
     */
    @Override
    public void onTestFail(DataRecord testData, Description description, Failure failure) {
        mTestFailed = true;
    }

    /**
     * Collect the logcat at the end of each test or collect the logcat only on test
     * failed if the flag is enabled.
     */
    @Override
    public void onTestEnd(DataRecord testData, Description description) {
        if (!mCollectOnlyTestFailed || (mCollectOnlyTestFailed && mTestFailed)) {
            // Capture logcat from start time
            if (mDestDir == null) {
                return;
            }
            try {
                int iteration = mTestIterations.get(description);
                final String fileName =
                        String.format(
                                "%s.%s%s%s-logcat.txt",
                                description.getClassName(),
                                description.getMethodName(),
                                iteration == 1 ? "" : (METRIC_SEP + String.valueOf(iteration)),
                                METRIC_SEP + FILENAME_SUFFIX);
                File logcat = new File(mDestDir, fileName);
                getLogcatSince(mStartTime, logcat);
                if (!mReturnLogcatDir) {
                    // Do not return individual logcat file path if the logcat directory
                    // option is enabled. Logcat root directory path will be returned in the
                    // test run status.
                    testData.addFileMetric(String.format("%s_%s", getTag(), logcat.getName()),
                            logcat);
                }
            } catch (IOException | InterruptedException e) {
                Log.e(getTag(), "Error trying to retrieve logcat.", e);
            }
        }
        // Reset the flag here, as onTestStart might not have been called if a @BeforeClass method
        // fails.
        mTestFailed = false;
        // Update the start time here in case onTestStart() is not called for the next test. If it
        // is called, the start time will be overwritten.
        mStartTime = getLogcatStartTime();
    }

    @Override
    public void onTestRunEnd(DataRecord runData, Result result) {
        if (mReturnLogcatDir) {
            runData.addStringMetric(getTag(), mDestDir.getAbsolutePath().toString());
        }
    }

    /** @hide */
    @VisibleForTesting
    protected void getLogcatSince(String startTime, File saveTo)
            throws IOException, InterruptedException {
        // ProcessBuilder is used here in favor of UiAutomation.executeShellCommand() because the
        // logcat command requires the timestamp to be quoted which in Java requires
        // Runtime.exec(String[]) or ProcessBuilder to work properly, and UiAutomation does not
        // support this for now.
        ProcessBuilder pb = new ProcessBuilder(Arrays.asList("logcat", "-t", startTime));
        pb.redirectOutput(saveTo);
        Process proc = pb.start();
        // Make the process blocking to ensure consistent behavior.
        proc.waitFor();
    }

    @VisibleForTesting
    protected String getLogcatStartTime() {
        Date date = new Date(System.currentTimeMillis());
        Log.i(getTag(), "Current Date:" + DATE_FORMATTER.format(date));
        if (mBeforeLogcatDurationInSecs > 0) {
            date = new Date(System.currentTimeMillis() - (mBeforeLogcatDurationInSecs * 1000));
            Log.i(getTag(), "Date including the before duration:" + DATE_FORMATTER.format(date));
        }
        return DATE_FORMATTER.format(date);
    }

    /**
     * Add custom options if available.
     */
    private void setupAdditionalArgs() {
        Bundle args = getArgsBundle();

        if (args.getString(BEFORE_LOGCAT_DURATION_SECS) != null) {
            mBeforeLogcatDurationInSecs = Long
                    .parseLong(args.getString(BEFORE_LOGCAT_DURATION_SECS));
        }

        if (args.getString(COLLECT_ON_FAILURE_ONLY) != null) {
            mCollectOnlyTestFailed = Boolean.parseBoolean(args.getString(COLLECT_ON_FAILURE_ONLY));
        }

        if (args.getString(RETURN_LOGCAT_DIR) != null) {
            mReturnLogcatDir = Boolean
                    .parseBoolean(args.getString(RETURN_LOGCAT_DIR));
        }

    }
}
