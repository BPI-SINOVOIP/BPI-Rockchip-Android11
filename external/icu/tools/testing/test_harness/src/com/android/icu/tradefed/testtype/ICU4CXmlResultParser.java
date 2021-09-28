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
package com.android.icu.tradefed.testtype;

import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.CommandResult;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXException;
import org.xml.sax.helpers.DefaultHandler;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;

/** Parses the XUnit results of ICU4C tests and informs a ITestInvocationListener of the results. */
public class ICU4CXmlResultParser {

    private static final String TEST_CASE_TAG = "testcase";

    private final String mTestRunName;
    private final String mModuleName;
    private int mNumTestsRun = 0;
    private int mNumTestsExpected = 0;
    private long mTotalRunTime = 0;
    private final Collection<ITestInvocationListener> mTestListeners;

    /**
     * Creates the ICU4CXmlResultParser.
     *
     * @param moduleName module name
     * @param testRunName the test run name to provide to {@link
     *     ITestInvocationListener#testRunStarted(String, int)}
     * @param listeners informed of test results as the tests are executing
     */
    public ICU4CXmlResultParser(String moduleName, String testRunName,
        Collection<ITestInvocationListener> listeners) {
        mModuleName = moduleName;
        mTestRunName = testRunName;
        mTestListeners = new ArrayList<>(listeners);
    }

    /**
     * Creates the ICU4CXmlResultParser for a single listener.
     *
     * @param moduleName module name
     * @param testRunName the test run name to provide to {@link
     *     ITestInvocationListener#testRunStarted(String, int)}
     * @param listener informed of test results as the tests are executing
     */
    public ICU4CXmlResultParser(String moduleName, String testRunName,
        ITestInvocationListener listener) {
        mModuleName = moduleName;
        mTestRunName = testRunName;
        mTestListeners = new ArrayList<>();
        if (listener != null) {
            mTestListeners.add(listener);
        }
    }

    /**
     * Parse the xml results
     *
     * @param f {@link File} containing the outputed xml
     * @param output The output collected from the execution run to complete the logs if necessary
     */
    public void parseResult(File f, CommandResult commandResult) {
        DocumentBuilderFactory dbf = DocumentBuilderFactory.newInstance();
        Document result = null;
        try {
            DocumentBuilder db = dbf.newDocumentBuilder();
            db.setErrorHandler(new DefaultHandler());
            result = db.parse(f);
        } catch (SAXException | IOException | ParserConfigurationException e) {
            reportTestRunStarted();
            for (ITestInvocationListener listener : mTestListeners) {
                String errorMessage =
                        String.format(
                                "Failed to get an xml output from tests," + " it probably crashed");
                errorMessage += "\nstdout:\n" + commandResult.getStdout();
                errorMessage += "\nstderr:\n" + commandResult.getStderr();
                CLog.e(errorMessage);
                listener.testRunFailed(errorMessage);
                listener.testRunEnded(mTotalRunTime, Collections.emptyMap());
            }
            return;
        }
        Element rootNode = result.getDocumentElement();

        getTestSuitesInfo(rootNode);
        reportTestRunStarted();

        // The ICU4C test runner doesn't put out the same format as GTest.
        // There's no root <testsuites> node. The root node is <testsuite> and
        // its children are <testcase>.
        NodeList testcasesList = rootNode.getElementsByTagName(TEST_CASE_TAG);
        // Iterate other the test cases in the test suite.
        if (testcasesList != null && testcasesList.getLength() > 0) {
            for (int i = 0; i < testcasesList.getLength(); i++) {
                processTestResult((Element) testcasesList.item(i));
            }
        }

        if (mNumTestsExpected > mNumTestsRun) {
            for (ITestInvocationListener listener : mTestListeners) {
                listener.testRunFailed(
                        String.format(
                                "Test run incomplete. Expected %d tests, received %d",
                                mNumTestsExpected, mNumTestsRun));
            }
        }
        for (ITestInvocationListener listener : mTestListeners) {
            listener.testRunEnded(mTotalRunTime, Collections.emptyMap());
        }
    }

    private void getTestSuitesInfo(Element rootNode) {
        mNumTestsExpected = rootNode.getElementsByTagName(TEST_CASE_TAG).getLength();
        // TODO(danalbert): Teach the ICU4C test runner to output this.
        mTotalRunTime = 0L;
    }

    /**
     * Reports the start of a test run, and the total test count, if it has not been previously
     * reported.
     */
    private void reportTestRunStarted() {
        for (ITestInvocationListener listener : mTestListeners) {
            listener.testRunStarted(mTestRunName, mNumTestsExpected);
        }
    }

    /**
     * Processes and informs listener when we encounter a tag indicating that a test has started.
     *
     * @param testcase Raw log output of the form classname.testname, with an optional time (x ms)
     */
    private void processTestResult(Element testcase) {
        String classname = testcase.getAttribute("classname");
        String testname = testcase.getAttribute("name");
        String runtime = testcase.getAttribute("time");

        // Remove extra leading '/' character
        if (classname.startsWith("/")) {
            classname = classname.substring(1);
        }

        // For test reporting on Android, prefix module name to the class name
        // and replace '/' with '.'
        classname = mModuleName + '.' + classname.replace('/', '.');

        // TODO: Fix the duplicate test name in the testId
        // Currently, testId is like spoof#spoof or spoof/testBug8654#testBug8654
        // in order to avoid empty test name in the case of spoof#spoof. We should remove
        // spoof#spoof from the test report because it's not a test case.
        TestDescription testId = new TestDescription(classname, testname);
        mNumTestsRun++;
        for (ITestInvocationListener listener : mTestListeners) {
            listener.testStarted(testId);
        }

        // If there is a failure tag report failure
        if (testcase.getElementsByTagName("failure").getLength() != 0) {
            String trace =
                    ((Element) testcase.getElementsByTagName("failure").item(0))
                            .getAttribute("message");
            if (!trace.contains("Failed")) {
                // For some reason, the alternative ICU4C format doesn't specify Failed in the
                // trace and error doesn't show properly in reporter, so adding it here.
                trace += "\nFailed";
            }
            for (ITestInvocationListener listener : mTestListeners) {
                listener.testFailed(testId, trace);
            }
        }

        Map<String, String> map = new HashMap<>();
        map.put("runtime", runtime);
        for (ITestInvocationListener listener : mTestListeners) {
            listener.testEnded(testId, map);
        }
    }
}
