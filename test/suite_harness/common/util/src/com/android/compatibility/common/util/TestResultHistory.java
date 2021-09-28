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

package com.android.compatibility.common.util;

import org.xmlpull.v1.XmlSerializer;

import java.io.IOException;
import java.io.Serializable;
import java.util.Objects;
import java.util.Set;

/**
 * Utility class to add test case result history to the report. This class records per-case history
 * for CTS Verifier. If this field is used for large test suites like CTS, it may cause performance
 * issues in APFE. Thus please do not use this class in other test suites.
 */
public class TestResultHistory implements Serializable {

    private static final long serialVersionUID = 10L;

    private static final String ENCODING = "UTF-8";
    private static final String TYPE = "org.kxml2.io.KXmlParser,org.kxml2.io.KXmlSerializer";

    // XML constants
    private static final String SUB_TEST_ATTR = "subtest";
    private static final String RUN_HISTORY_TAG = "RunHistory";
    private static final String RUN_TAG = "Run";
    private static final String START_TIME_ATTR = "start";
    private static final String END_TIME_ATTR = "end";
    private static final String IS_AUTOMATED_ATTR = "isAutomated";

    private final String mTestName;
    private final Set<ExecutionRecord> mExecutionRecords;

    /**
     * Constructor of test result history.
     *
     * @param testName a string of test name.
     * @param executionRecords a Set of ExecutionRecords.
     */
    public TestResultHistory(String testName, Set<ExecutionRecord> executionRecords) {
        this.mTestName = testName;
        this.mExecutionRecords = executionRecords;
    }

    /** Get test name */
    public String getTestName() {
        return mTestName;
    }

    /** Get a set of ExecutionRecords. */
    public Set<ExecutionRecord> getExecutionRecords() {
        return mExecutionRecords;
    }

    /** {@inheritDoc} */
    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (o == null || getClass() != o.getClass()) {
            return false;
        }
        TestResultHistory that = (TestResultHistory) o;
        return Objects.equals(mTestName, that.mTestName)
                && Objects.equals(mExecutionRecords, that.mExecutionRecords);
    }

    /** {@inheritDoc} */
    @Override
    public int hashCode() {
        return Objects.hash(mTestName, mExecutionRecords);
    }

    /**
     * Serializes a given {@link TestResultHistory} to XML.
     *
     * @param serializer given serializer.
     * @param resultHistory test result history with test name and execution record.
     * @param testName top-level test name.
     * @throws IOException
     */
    public static void serialize(
            XmlSerializer serializer, TestResultHistory resultHistory, String testName)
            throws IOException {
        if (resultHistory == null) {
            throw new IllegalArgumentException("Test result history was null");
        }

        serializer.startTag(null, RUN_HISTORY_TAG);
        // Only show sub-test names in test attribute in run history node.
        String name = resultHistory.getTestName().replaceFirst(testName + ":", "");
        if (!name.isEmpty() && !name.equalsIgnoreCase(testName)) {
            serializer.attribute(null, SUB_TEST_ATTR, name);
        }

        for (ExecutionRecord execRecord : resultHistory.getExecutionRecords()) {
            serializer.startTag(null, RUN_TAG);
            serializer.attribute(null, START_TIME_ATTR, String.valueOf(execRecord.getStartTime()));
            serializer.attribute(null, END_TIME_ATTR, String.valueOf(execRecord.getEndTime()));
            serializer.attribute(
                    null, IS_AUTOMATED_ATTR, String.valueOf(execRecord.getIsAutomated()));
            serializer.endTag(null, RUN_TAG);
        }
        serializer.endTag(null, RUN_HISTORY_TAG);
    }

    /** Execution Record about start time, end time and isAutomated */
    public static class ExecutionRecord implements Serializable {

        private static final long serialVersionUID = 0L;
        // Start time of test case.
        private final long startTime;
        // End time of test case.
        private final long endTime;
        // Whether test case was executed through automation.
        private final boolean isAutomated;

        public ExecutionRecord(long startTime, long endTime, boolean isAutomated) {
            this.startTime = startTime;
            this.endTime = endTime;
            this.isAutomated = isAutomated;
        }

        public long getStartTime() {
            return startTime;
        }

        public long getEndTime() {
            return endTime;
        }

        public boolean getIsAutomated() {
            return isAutomated;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) {
                return true;
            }
            if (o == null || getClass() != o.getClass()) {
                return false;
            }
            ExecutionRecord that = (ExecutionRecord) o;
            return startTime == that.startTime
                    && endTime == that.endTime
                    && isAutomated == that.isAutomated;
        }

        @Override
        public int hashCode() {
            return Objects.hash(startTime, endTime, isAutomated);
        }
    }
}
