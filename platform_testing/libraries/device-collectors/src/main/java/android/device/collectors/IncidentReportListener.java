/*
 * Copyright (C) 2018 The Android Open Source Project
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
import android.os.Environment;
import android.util.Log;
import androidx.test.InstrumentationRegistry;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;

import org.junit.runner.Description;
import org.junit.runner.Result;

/**
 * A {@link BaseMetricListener} that captures a full incident report at the end of a test run in
 * proto format and records it to a fixed file location.
 *
 * Note: This class requires both {@code READ_EXTERNAL_STORAGE} and {@code WRITE_EXTERNAL_STORAGE}
 * permissions.
 */
@OptionClass(alias = "incident-collector")
public class IncidentReportListener extends BaseMetricListener {
    private static final String LOG_TAG = IncidentReportListener.class.getSimpleName();
    private static final String REDIRECT_INCIDENT_REPORT_CMD = "incident -b -p %s";
    // Separate directory name for conveniently calling {@code this#createAndEmptyDirectory}, which
    // is already conditioned on the external storage directory path. If improved, remove constant.
    private static final String DIRECTORY_NAME = "incidents";
    static final Path REPORT_DIRECTORY =
            Environment.getExternalStorageDirectory().toPath().resolve(DIRECTORY_NAME);
    static final Path FINAL_REPORT_PATH = REPORT_DIRECTORY.resolve("final.pb");
    static final String FINAL_REPORT_KEY = "incident-report-final";

    public IncidentReportListener() {
        super();
    }

    @Override
    public void onTestRunStart(DataRecord runData, Description description) {
        Log.i(LOG_TAG, "Removing all contents from report directory.");
        File result = createAndEmptyDirectory(DIRECTORY_NAME);
        if (result == null) {
            throw new RuntimeException(String.format(
                    "Couldn't create destination folder: %s.", REPORT_DIRECTORY.toString()));
        }
    }

    @Override
    public void onTestEnd(DataRecord testData, Description description) {
        // TODO(b/119120269): Collect per-test incidents for granular debugging.
    }

    @Override
    public void onTestRunEnd(DataRecord runData, Result result) {
        // Fail fast if the parent directory was not successfully created.
        if (!REPORT_DIRECTORY.toFile().exists()) {
            throw new IllegalStateException(String.format(
                    "Report destination folder does not exist: %s", REPORT_DIRECTORY.toString()));
        }
        // Construct and execute the incident report command with output.
        String fullReportCmd = String.format(REDIRECT_INCIDENT_REPORT_CMD, "EXPLICIT");
        Log.v(LOG_TAG, String.format("Collecting full incident report: %s.", fullReportCmd));
        // Adopt shell permissions for just the report collection command.
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity();
        byte[] output = executeCommandBlocking(fullReportCmd);
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .dropShellPermissionIdentity();
        if (output == null) {
            // Null output signals the command did not successfully execute.
            throw new RuntimeException("Failed to run the incident report command.");
        } if (output.length == 0) {
            // Empty output signals the command did not return any content.
            throw new RuntimeException("The incident report command returned no data.");
        } else {
            // Write the report bytes to the destination and fail if it already exists.
            try {
                Files.write(FINAL_REPORT_PATH, output, StandardOpenOption.CREATE_NEW);
            } catch (IOException e) {
                throw new RuntimeException("Unable to write full incident report.", e);
            }
            // Ensure the newly created report exists at the expected destination.
            File destination = FINAL_REPORT_PATH.toFile();
            if (!destination.exists()) {
                throw new RuntimeException("Unable to find the full incident report available.");
            }
            // Declare success given the file exists and is non-empty.
            runData.addFileMetric(FINAL_REPORT_KEY, destination);
        }
    }
}
