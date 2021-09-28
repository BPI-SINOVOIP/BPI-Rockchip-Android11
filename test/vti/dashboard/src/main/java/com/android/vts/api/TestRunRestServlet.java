/*
 * Copyright (c) 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You may
 * obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

package com.android.vts.api;

import com.android.vts.entity.TestCaseRunEntity;
import com.android.vts.entity.TestEntity;
import com.android.vts.entity.TestRunEntity;
import com.android.vts.util.TestRunDetails;
import com.google.gson.Gson;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.logging.Logger;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import static com.googlecode.objectify.ObjectifyService.ofy;

/** Servlet for handling requests to fetch test case results. */
public class TestRunRestServlet extends BaseApiServlet {
    private static final String LATEST = "latest";
    protected static final Logger logger = Logger.getLogger(TestRunRestServlet.class.getName());

    /**
     * Get the test case results for the specified run of the specified test.
     *
     * @param test The test whose test cases to get.
     * @param timeString The string representation of the test run timestamp (in microseconds).
     * @return A TestRunDetails object with the test case details for the specified run.
     */
    private TestRunDetails getTestRunDetails(String test, String timeString) {
        long timestamp;
        try {
            timestamp = Long.parseLong(timeString);
            if (timestamp <= 0) throw new NumberFormatException();
            timestamp = timestamp > 0 ? timestamp : null;
        } catch (NumberFormatException e) {
            return null;
        }

        TestRunEntity testRunEntity = TestRunEntity.getByTestNameId(test, timestamp);

        return getTestRunDetails(testRunEntity);
    }

    /**
     * Get the test case results for the latest run of the specified test.
     *
     * @param testName The test whose test cases to get.
     * @return A TestRunDetails object with the test case details for the latest run.
     */
    private TestRunDetails getLatestTestRunDetails(String testName) {
        com.googlecode.objectify.Key testKey =
                com.googlecode.objectify.Key.create(
                        TestEntity.class, testName);

        TestRunEntity testRun = ofy().load().type(TestRunEntity.class).ancestor(testKey)
                .filter("type", 2).orderKey(true).first().now();

        if (testRun == null) return null;

        return getTestRunDetails(testRun);
    }

    /**
     * Get TestRunDetails instance from codeCoverageEntity instance.
     *
     * @param testRunEntity The TestRunEntity to access testCaseId.
     * @return A TestRunDetails object with the test case details for the latest run.
     */
    private TestRunDetails getTestRunDetails(TestRunEntity testRunEntity) {
        TestRunDetails details = new TestRunDetails();
        List<com.googlecode.objectify.Key<TestCaseRunEntity>> testCaseKeyList = new ArrayList<>();
        if ( Objects.isNull(testRunEntity.getTestCaseIds()) ) {
            return details;
        } else {
            for (long testCaseId : testRunEntity.getTestCaseIds()) {
                testCaseKeyList.add(
                        com.googlecode.objectify.Key.create(TestCaseRunEntity.class, testCaseId));
            }
            Map<com.googlecode.objectify.Key<TestCaseRunEntity>, TestCaseRunEntity>
                    testCaseRunEntityKeyMap = ofy().load().keys(() -> testCaseKeyList.iterator());
            testCaseRunEntityKeyMap.forEach((key, value) -> details.addTestCase(value));
        }
        return details;
    }

    /**
     * Get the test case details for a test run.
     *
     * Expected format: (1) /api/test_run?test=[test name]&timestamp=[timestamp] to the details
     * for a specific run, or (2) /api/test_run?test=[test name]&timestamp=latest -- the details for
     * the latest test run.
     */
    @Override
    public void doGet(HttpServletRequest request, HttpServletResponse response) throws IOException {
        String test = request.getParameter("test");
        String timeString = request.getParameter("timestamp");
        TestRunDetails details = null;

        if (timeString != null && timeString.equals(LATEST)) {
            details = getLatestTestRunDetails(test);
        } else if (timeString != null) {
            details = getTestRunDetails(test, timeString);
        }

        if (details == null) {
            response.setStatus(HttpServletResponse.SC_BAD_REQUEST);
        } else {
            response.setStatus(HttpServletResponse.SC_OK);
            response.setContentType("application/json");
            PrintWriter writer = response.getWriter();
            writer.print(new Gson().toJson(details.toJson()));
            writer.flush();
        }
    }
}
