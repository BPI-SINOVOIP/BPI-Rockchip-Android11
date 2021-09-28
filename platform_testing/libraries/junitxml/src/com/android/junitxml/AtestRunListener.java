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

package com.android.junitxml;

import org.json.JSONObject;
import org.junit.runner.Description;
import org.junit.runner.Result;
import org.junit.runner.notification.Failure;
import org.junit.runner.notification.RunListener;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

/**
 * {@link RunListener} to write JUnit4 test status to File, then atest could use this file to get
 * real time status}.
 */
public class AtestRunListener extends RunListener {

    private static final String CLASSNAME_KEY = "className";
    private static final String TESTNAME_KEY = "testName";
    private static final String TRACE_KEY = "trace";
    private static final String RUNNAME_KEY = "runName";
    private static final String TESTCOUNT_KEY = "testCount";
    private static final String ATTEMPT_KEY = "runAttempt";
    private static final String TIME_KEY = "time";
    private static final String START_TIME_KEY = "start_time";
    private static final String END_TIME_KEY = "end_time";
    private static final String MODULE_NAME_KEY = "moduleName";

    /** Relevant test status keys. */
    public static class StatusKeys {
        public static final String TEST_ENDED = "TEST_ENDED";
        public static final String TEST_FAILED = "TEST_FAILED";
        public static final String TEST_IGNORED = "TEST_IGNORED";
        public static final String TEST_STARTED = "TEST_STARTED";
        public static final String TEST_RUN_ENDED = "TEST_RUN_ENDED";
        public static final String TEST_RUN_STARTED = "TEST_RUN_STARTED";
        public static final String TEST_MODULE_STARTED = "TEST_MODULE_STARTED";
        public static final String TEST_MODULE_ENDED = "TEST_MODULE_ENDED";
    }

    private final int mTotalCount;

    // the file where to log the events.
    private final File mReportFile;

    private final String mSuiteName;

    public AtestRunListener(String suiteName, File reportFile, int totalCount) {
        mSuiteName = suiteName;
        mReportFile = reportFile;
        mTotalCount = totalCount;
    }

    @Override
    public void testRunStarted(Description description) {
        Map<String, Object> moduleStartEventData = new HashMap<>();
        moduleStartEventData.put(MODULE_NAME_KEY, mSuiteName);
        printEvent(StatusKeys.TEST_MODULE_STARTED, moduleStartEventData);

        Map<String, Object> testRunEventData = new HashMap<>();
        testRunEventData.put(TESTCOUNT_KEY, mTotalCount);
        testRunEventData.put(ATTEMPT_KEY, 0);
        testRunEventData.put(RUNNAME_KEY, mSuiteName);
        printEvent(StatusKeys.TEST_RUN_STARTED, testRunEventData);
    }

    @Override
    public void testRunFinished(Result result) {
        Map<String, Object> eventData = new HashMap<>();
        eventData.put(TIME_KEY, result.getRunTime());
        printEvent(StatusKeys.TEST_RUN_ENDED, eventData);
        printEvent(StatusKeys.TEST_MODULE_ENDED, new HashMap<>());
    }

    @Override
    public void testFailure(Failure failure) {
        Description description = failure.getDescription();
        Map<String, Object> eventData = new HashMap<>();
        eventData.put(CLASSNAME_KEY, description.getClassName());
        eventData.put(TESTNAME_KEY, description.getMethodName());
        eventData.put(TRACE_KEY, failure.getTrace());
        printEvent(StatusKeys.TEST_FAILED, eventData);
    }

    @Override
    public void testStarted(Description description) {
        Map<String, Object> eventData = new HashMap<>();
        eventData.put(START_TIME_KEY, System.currentTimeMillis());
        eventData.put(CLASSNAME_KEY, description.getClassName());
        eventData.put(TESTNAME_KEY, description.getMethodName());
        printEvent(StatusKeys.TEST_STARTED, eventData);
    }

    @Override
    public void testFinished(Description description) {
        Map<String, Object> eventData = new HashMap<>();
        eventData.put(END_TIME_KEY, System.currentTimeMillis());
        eventData.put(CLASSNAME_KEY, description.getClassName());
        eventData.put(TESTNAME_KEY, description.getMethodName());
        printEvent(StatusKeys.TEST_ENDED, eventData);
    }

    @Override
    public void testIgnored(Description description) {
        Map<String, Object> eventData = new HashMap<>();
        eventData.put(TESTNAME_KEY, description.getMethodName());
        eventData.put(CLASSNAME_KEY, description.getClassName());
        eventData.put(START_TIME_KEY, System.currentTimeMillis());
        eventData.put(END_TIME_KEY, System.currentTimeMillis());
        printEvent(StatusKeys.TEST_STARTED, eventData);
        printEvent(StatusKeys.TEST_IGNORED, eventData);
        printEvent(StatusKeys.TEST_ENDED, eventData);
    }

    private void printEvent(String key, Map<String, Object> event) {
        if (mReportFile.canWrite()) {
            try {
                try (FileWriter fw = new FileWriter(mReportFile, true)) {
                    String eventLog = String.format("%s %s\n\n", key, new JSONObject(event));
                    fw.append(eventLog);
                }
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        } else {
            throw new RuntimeException(
                    String.format(
                            "report file: %s is not writable", mReportFile.getAbsolutePath()));
        }
    }
}
