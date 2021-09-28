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

package com.android.tradefed.result.ddmlib;

import com.android.commands.am.InstrumentationData.ResultsBundle;
import com.android.commands.am.InstrumentationData.ResultsBundleEntry;
import com.android.commands.am.InstrumentationData.Session;
import com.android.commands.am.InstrumentationData.SessionStatus;
import com.android.commands.am.InstrumentationData.TestStatus;
import com.android.ddmlib.IShellOutputReceiver;
import com.android.ddmlib.testrunner.ITestRunListener;
import com.android.ddmlib.testrunner.InstrumentationResultParser;

import com.google.protobuf.InvalidProtocolBufferException;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.Collection;
import java.util.LinkedList;
import java.util.List;

/**
 * Parses the instrumentation result proto collected during instrumentation test run
 * and informs ITestRunListener of the results.
 */
public class InstrumentationResultProtoParser implements IShellOutputReceiver {

    /** Error message supplied when no test result file is found. */
    public static final String NO_TEST_RESULTS_FILE = "No instrumentation proto test"
            + " results file found";

    /** Error message supplied when no test results are received from test run. */
    public static final String NO_TEST_RESULTS_MSG = "No test results";

    /** Error message supplied when no test result file is found. */
    public static final String INVALID_TEST_RESULTS_FILE = "Invalid instrumentation proto"
            + " test results file";

    private static final String INSTRUMENTATION_STATUS_FORMAT = "INSTRUMENTATION_STATUS: %s=%s";
    private static final String INSTRUMENTATION_STATUS_CODE_FORMAT =
            "INSTRUMENTATION_STATUS_CODE: %d";
    private static final String INSTRUMENTATION_RESULT_FORMAT = "INSTRUMENTATION_RESULT: %s=%s";
    private static final String INSTRUMENTATION_CODE_FORMAT = "INSTRUMENTATION_CODE: %d";

    private InstrumentationResultParser parser;

    public InstrumentationResultProtoParser(String runName,
            Collection<ITestRunListener> listeners) {
        parser = new InstrumentationResultParser(runName, listeners);
    }

    /**
     * Process the instrumentation result proto file collected during the instrumentation test run.
     * Instrumentation proto file consist of test status and instrumentation session status. This
     * method will be used only when complete instrumentation results proto file is available for
     * parsing.
     *
     * @param protoFile that contains the test status and instrumentation session results.
     * @throws IOException
     */
    public void processProtoFile(File protoFile) throws IOException {

        // Report tes run failures in case of null and empty proto file.
        if (protoFile == null) {
            parser.handleTestRunFailed(NO_TEST_RESULTS_FILE);
            return;
        }
        if (protoFile.length() == 0) {
            parser.handleTestRunFailed(NO_TEST_RESULTS_MSG);
            return;
        }

        // Read the input proto file
        byte[] bytesArray = new byte[(int) protoFile.length()];
        FileInputStream fis = new FileInputStream(protoFile);
        fis.read(bytesArray);
        fis.close();

        try {
            // Parse the proto file.
            Session instrumentSession = Session.parseFrom(bytesArray);

            // Process multiple test status.
            List<TestStatus> multipleTestStatus = instrumentSession.getTestStatusList();
            for (TestStatus teststatus : multipleTestStatus) {
                processTestStatus(teststatus);
            }

            // Process instrumentation session status.
            SessionStatus sessionStatus = instrumentSession.getSessionStatus();
            if (sessionStatus.isInitialized()) {
                processSessionStatus(sessionStatus);
            }
        } catch (InvalidProtocolBufferException ex) {
            parser.handleTestRunFailed(INVALID_TEST_RESULTS_FILE);
        }
        parser.done();
    }

    /**
     * Preprocess the single TestStatus proto message which includes the test info or test
     * results and result code in to shell output format for further processing by
     * InstrumentationResultParser.
     *
     * @param testStatus The {@link TestStatus} holding the current test info collected during the
     *            test.
     */
    public void processTestStatus(TestStatus testStatus) {
        // Process the test results.
        ResultsBundle results = testStatus.getResults();
        List<String> preProcessedLines = new LinkedList<>();
        for (ResultsBundleEntry entry : results.getEntriesList()) {
            String currentKey = entry.getKey();
            String currentValue = null;
            if (entry.hasValueString()) {
                currentValue = entry.getValueString().trim();
            } else if (entry.hasValueInt()) {
                currentValue = String.valueOf(entry.getValueInt());
            }
            preProcessedLines.add(String.format(INSTRUMENTATION_STATUS_FORMAT, currentKey,
                    currentValue));
        }
        preProcessedLines.add(String.format(INSTRUMENTATION_STATUS_CODE_FORMAT,
                testStatus.getResultCode()));
        parser.processNewLines(preProcessedLines.toArray(new String[preProcessedLines.size()]));
    }

    /**
     * Preprocess the instrumentation session status which includes the instrumentation test results
     * and the session status code to shell output format for further processing by
     * InstrumentationResultParser.
     *
     * @param sessionStatus The {@link SessionStatus} holding the current instrumentation session
     *     info collected during the test run.
     */
    public void processSessionStatus(SessionStatus sessionStatus) {

        List<String> preProcessedLines = new LinkedList<>();
        ResultsBundle results = sessionStatus.getResults();
        for (ResultsBundleEntry entry : results.getEntriesList()) {
            String currentKey = entry.getKey();
            String currentValue = "";
            if (entry.hasValueString()) {
                currentValue = entry.getValueString();
                String lines[] = currentValue.split("\\r?\\n");
                int lineCount = 1;
                for (String line : lines) {
                    if (lineCount == 1) {
                        // Only first line should have the Result code prefix.
                        preProcessedLines.add(String.format(INSTRUMENTATION_RESULT_FORMAT,
                                currentKey,
                                line));
                        lineCount++;
                        continue;
                    }
                    preProcessedLines.add(line);
                }
            } else if (entry.hasValueInt()) {
                currentValue = String.valueOf(entry.getValueInt());
                preProcessedLines.add(String.format(INSTRUMENTATION_RESULT_FORMAT, currentKey,
                        currentValue));
            }
        }
        if (results.isInitialized()) {
            preProcessedLines.add(String.format(INSTRUMENTATION_CODE_FORMAT,
                    sessionStatus.getResultCode()));
        }

        parser.processNewLines(preProcessedLines.toArray(new String[preProcessedLines.size()]));
    }

    /* (non-Javadoc)
     * @see com.android.ddmlib.IShellOutputReceiver#addOutput(byte[], int, int)
     */
    @Override
    public void addOutput(byte[] protoData, int bytes, int length) {
        // TODO : Process the streaming proto instrumentation results.
    }

    /* (non-Javadoc)
     * @see com.android.ddmlib.IShellOutputReceiver#flush()
     */
    @Override
    public void flush() {
    }

    /* (non-Javadoc)
     * @see com.android.ddmlib.IShellOutputReceiver#isCancelled()
     */
    @Override
    public boolean isCancelled() {
        return false;
    }
}
