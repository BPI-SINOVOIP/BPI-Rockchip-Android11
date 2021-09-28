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

package android.device.collectors;

import android.device.collectors.annotations.OptionClass;
import android.os.Bundle;
import android.util.Log;

import androidx.annotation.Nullable;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Strings;

import org.junit.runner.Description;
import org.junit.runner.Result;
import org.junit.runner.notification.Failure;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;

/**
 * A {@link GcaEventLogCollector} that captures Google Camera App (GCA) on-device event log protos
 * and save them to external storage at run_listeners/camera_events directory. This collector
 * disables selinux at the start of the test run and recover at the end. So if your tests require
 * selinux enforcing, please do not use this collector.
 */
@OptionClass(alias = "gca-proto-log-collector")
public class GcaEventLogCollector extends BaseMetricListener {

    private static final String TAG = GcaEventLogCollector.class.getSimpleName();
    private static final String DEST_DIR = "run_listeners/camera_events";
    private static final String CAMERA_EVENT_LOG_PATTERN =
            "/data/user/0/%s/files/camera_events/session.pb";

    @VisibleForTesting
    static final String COLLECT_TEST_FAILURE_CAMERA_LOGS = "collect_test_failure_camera_logs";

    @VisibleForTesting
    static final String COLLECT_CAMERA_LOGS_PER_RUN = "collect_camera_logs_per_run";

    private static final String GOOGLE_CAMERA_APP_PACKAGE = "google_camera_app_package";

    enum SeLinuxEnforceProperty {
        ENFORCING("1"),
        PERMISSIVE("0");

        private String property;

        public String getProperty() {
            return property;
        }

        private SeLinuxEnforceProperty(String property) {
            this.property = property;
        }
    }

    private String mGcaPkg = "com.google.android.GoogleCamera";
    private boolean mIsCollectPerRun = false;
    private boolean mCollectMetricsForFailedTest = false;
    private boolean mIsTestFailed;
    private File mDestDir;
    private String mEventLogPath;
    private SeLinuxEnforceProperty mOrigSeLinuxEnforceProp;
    private String mOrigCameraEventProp;

    // Map to keep track of test iterations for multiple test iterations.
    private HashMap<Description, Integer> mTestIterations = new HashMap<>();

    public GcaEventLogCollector() {
        super();
    }

    @VisibleForTesting
    GcaEventLogCollector(Bundle args) {
        super(args);
    }

    @Override
    public void onTestRunStart(DataRecord runData, Description description) {
        setupAdditionalArgs();
        mDestDir = createAndEmptyDirectory(DEST_DIR);
        mEventLogPath = String.format(CAMERA_EVENT_LOG_PATTERN, mGcaPkg);
        cleanUpEventLog();
        mOrigSeLinuxEnforceProp = getSeLinuxEnforceProperty();
        mOrigCameraEventProp = getCameraEventLogProperty();
        setSeLinuxEnforceProperty(SeLinuxEnforceProperty.PERMISSIVE);
        setCameraEventLogProperty("1");
        killGoogleCameraApp();
    }

    @Override
    public void onTestRunEnd(DataRecord runData, Result result) {
        try {
            if (mIsCollectPerRun) {
                collectEventLog(runData, null);
            }
            setCameraEventLogProperty(mOrigCameraEventProp);
            killGoogleCameraApp();
        } finally {
            setSeLinuxEnforceProperty(mOrigSeLinuxEnforceProp);
        }
    }

    @Override
    public void onTestStart(DataRecord runData, Description description) {
        mIsTestFailed = false;
        if (!mIsCollectPerRun) {
            killGoogleCameraApp();
            cleanUpEventLog();
        }
        // Keep track of test iterations.
        mTestIterations.computeIfPresent(description, (desc, iteration) -> iteration + 1);
        mTestIterations.computeIfAbsent(description, desc -> 1);
    }

    @Override
    public void onTestEnd(DataRecord testData, Description description) {
        if (mIsCollectPerRun) {
            return;
        }
        if (!mCollectMetricsForFailedTest && mIsTestFailed) {
            return;
        }
        collectEventLog(testData, description);
    }

    @Override
    public void onTestFail(DataRecord runData, Description description, Failure failure) {
        mIsTestFailed = true;
    }

    private void setupAdditionalArgs() {
        Bundle args = getArgsBundle();
        if (!Strings.isNullOrEmpty(args.getString(GOOGLE_CAMERA_APP_PACKAGE))) {
            mGcaPkg = args.getString(GOOGLE_CAMERA_APP_PACKAGE);
        }
        mIsCollectPerRun =
                Boolean.parseBoolean(args.getString(COLLECT_CAMERA_LOGS_PER_RUN, "false"));
        mCollectMetricsForFailedTest =
                Boolean.parseBoolean(args.getString(COLLECT_TEST_FAILURE_CAMERA_LOGS, "false"));
    }

    private void collectEventLog(DataRecord testData, Description description) {
        logListResult(mEventLogPath);
        String fileName = buildCollectedEventLogFileName(description);
        Log.d(TAG, String.format("Destination event log file name: %s", fileName));
        copyEventLogToSharedStorage(fileName);
        testData.addFileMetric(fileName, new File(mDestDir, fileName));
    }

    @VisibleForTesting
    protected void copyEventLogToSharedStorage(String fileName) {
        String cmd =
                String.format("cp %s %s/%s", mEventLogPath, mDestDir.getAbsolutePath(), fileName);
        Log.d(TAG, "Copy command: " + cmd);
        executeCommandBlocking(cmd);
        logListResult(mDestDir.getAbsolutePath());
    }

    private void cleanUpEventLog() {
        executeCommandBlocking("rm " + mEventLogPath);
        logListResult(mEventLogPath);
    }

    private String getCameraEventLogProperty() {
        String res =
                new String(
                                executeCommandBlocking("getprop camera.use_local_logger"),
                                StandardCharsets.UTF_8)
                        .trim();
        return res.isEmpty() ? "0" : res;
    }

    private void setCameraEventLogProperty(String value) {
        if (!getCameraEventLogProperty().equals(value)) {
            Log.d(TAG, "Setting property camera.use_local_logger to " + value);
            executeCommandBlocking("setprop camera.use_local_logger " + value);
        }
    }

    private void killGoogleCameraApp() {
        executeCommandBlocking("am force-stop " + mGcaPkg);
    }

    private SeLinuxEnforceProperty getSeLinuxEnforceProperty() {
        String res =
                new String(executeCommandBlocking("getenforce"), StandardCharsets.UTF_8).trim();
        return SeLinuxEnforceProperty.valueOf(res.toUpperCase());
    }

    private void setSeLinuxEnforceProperty(SeLinuxEnforceProperty value) {
        if (getSeLinuxEnforceProperty() != value) {
            Log.d(TAG, "Setting SeLinux enforcing to " + value.getProperty());
            executeCommandBlocking("setenforce " + value.getProperty());
        }
    }

    private String buildCollectedEventLogFileName(@Nullable Description description) {
        long timestamp = System.currentTimeMillis();
        if (description == null) {
            return String.format("session_run_%d.pb", timestamp);
        }
        int iteration = mTestIterations.get(description);
        return String.format(
                "session_%s_%s%s_%d.pb",
                description.getClassName(),
                description.getMethodName(),
                iteration == 1 ? "" : ("_" + String.valueOf(iteration)),
                timestamp);
    }

    private void logListResult(String targetPath) {
        byte[] lsResult = executeCommandBlocking("ls " + targetPath);
        Log.d(
                TAG,
                String.format(
                        "List path %s result: %s",
                        targetPath, new String(lsResult, StandardCharsets.UTF_8)));
    }
}
