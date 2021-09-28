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

import com.android.vts.entity.BranchEntity;
import com.android.vts.entity.BuildTargetEntity;
import com.android.vts.entity.CodeCoverageEntity;
import com.android.vts.entity.CoverageEntity;
import com.android.vts.entity.DeviceInfoEntity;
import com.android.vts.entity.ProfilingPointRunEntity;
import com.android.vts.entity.TestCaseRunEntity;
import com.android.vts.entity.TestEntity;
import com.android.vts.entity.TestPlanEntity;
import com.android.vts.entity.TestPlanRunEntity;
import com.android.vts.entity.TestRunEntity;
import com.android.vts.entity.TestStatusEntity;
import com.android.vts.entity.TestStatusEntity.TestCaseReference;
import com.android.vts.entity.TestSuiteFileEntity;
import com.android.vts.entity.TestSuiteResultEntity;
import com.google.appengine.api.datastore.DatastoreFailureException;
import com.google.appengine.api.datastore.DatastoreService;
import com.google.appengine.api.datastore.DatastoreServiceFactory;
import com.google.appengine.api.datastore.DatastoreTimeoutException;
import com.google.appengine.api.datastore.Entity;
import com.google.appengine.api.datastore.EntityNotFoundException;
import com.google.appengine.api.datastore.Key;
import com.google.appengine.api.datastore.KeyFactory;
import com.google.appengine.api.datastore.Query;
import com.google.appengine.api.datastore.Transaction;
import com.google.appengine.api.users.User;
import com.google.appengine.api.users.UserService;
import com.google.appengine.api.users.UserServiceFactory;
import com.google.appengine.api.utils.SystemProperty;
import com.google.gson.Gson;
import com.google.gson.GsonBuilder;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;
import java.nio.file.FileSystems;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.ConcurrentModificationException;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.Random;
import java.util.Set;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.stream.IntStream;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

/** Servlet for handling requests to add mock data in datastore. */
public class TestDataForDevServlet extends HttpServlet {
    protected static final Logger logger = Logger.getLogger(TestDataForDevServlet.class.getName());

    /** Google Cloud Storage project's default directory name for suite test result files */
    private static String GCS_SUITE_TEST_FOLDER_NAME;

    /** datastore instance to save the test data into datastore through datastore library. */
    private DatastoreService datastore = DatastoreServiceFactory.getDatastoreService();
    /**
     * Gson is a Java library that can be used to convert Java Objects into their JSON
     * representation. It can also be used to convert a JSON string to an equivalent Java object.
     */
    private Gson gson = new GsonBuilder().create();

    /** System Configuration Property class */
    protected Properties systemConfigProp = new Properties();

    @Override
    public void init(ServletConfig cfg) throws ServletException {
        super.init(cfg);

        try {
            InputStream defaultInputStream =
                    TestDataForDevServlet.class
                            .getClassLoader()
                            .getResourceAsStream("config.properties");
            systemConfigProp.load(defaultInputStream);

            GCS_SUITE_TEST_FOLDER_NAME = systemConfigProp.getProperty("gcs.suiteTestFolderName");
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    /**
     * TestReportData class for mapping test-report-data.json. This internal class's each fields
     * will be automatically mapped to test-report-data.json file through Gson
     */
    private class TestReportDataObject {
        private List<Test> testList;

        private class Test {
            private List<TestRun> testRunList;

            private class TestRun {
                private String testName;
                private int type;
                private long startTimestamp;
                private long endTimestamp;
                private String testBuildId;
                private String hostName;
                private long passCount;
                private long failCount;
                private boolean hasCoverage;
                private long coveredLineCount;
                private long totalLineCount;
                private List<Long> testCaseIds;
                private List<Long> failingTestcaseIds;
                private List<Integer> failingTestcaseOffsets;
                private List<String> links;

                private List<Coverage> coverageList;
                private List<Profiling> profilingList;
                private List<TestCaseRun> testCaseRunList;
                private List<DeviceInfo> deviceInfoList;
                private List<BuildTarget> buildTargetList;
                private List<Branch> branchList;

                private class Coverage {
                    private String group;
                    private long coveredLineCount;
                    private long totalLineCount;
                    private String filePath;
                    private String projectName;
                    private String projectVersion;
                    private List<Long> lineCoverage;
                }

                private class Profiling {
                    private String name;
                    private int type;
                    private int regressionMode;
                    private List<String> labels;
                    private List<Long> values;
                    private String xLabel;
                    private String yLabel;
                    private List<String> options;
                }

                private class TestCaseRun {
                    private List<String> testCaseNames;
                    private List<Integer> results;
                }

                private class DeviceInfo {
                    private String branch;
                    private String product;
                    private String buildFlavor;
                    private String buildId;
                    private String abiBitness;
                    private String abiName;
                }

                private class BuildTarget {
                    private String targetName;
                }

                private class Branch {
                    private String branchName;
                }
            }
        }

        @Override
        public String toString() {
            return "(" + testList + ")";
        }
    }

    private class TestPlanReportDataObject {
        private List<TestPlan> testPlanList;

        private class TestPlan {

            private String testPlanName;
            private List<String> testModules;
            private List<Long> testTimes;
        }

        @Override
        public String toString() {
            return "(" + testPlanList + ")";
        }
    }

    private Map<String, Object> generateSuiteTestData(
            HttpServletRequest request, HttpServletResponse response) {
        Map<String, Object> resultMap = new HashMap<>();
        String fileSeparator = FileSystems.getDefault().getSeparator();
        Random rand = new Random();
        List<String> branchList = Arrays.asList("master", "oc_mr", "oc");
        List<String> targetList =
                Arrays.asList(
                        "sailfish-userdebug",
                        "marlin-userdebug",
                        "taimen-userdebug",
                        "walleye-userdebug",
                        "aosp_arm_a-userdebug");
        branchList.forEach(
                branch ->
                        targetList.forEach(
                                target ->
                                        IntStream.range(0, 10)
                                                .forEach(
                                                        idx -> {
                                                            String year =
                                                                    String.format(
                                                                            "%04d", 2010 + idx);
                                                            String month =
                                                                    String.format(
                                                                            "%02d",
                                                                            rand.nextInt(12));
                                                            String day =
                                                                    String.format(
                                                                            "%02d",
                                                                            rand.nextInt(30));
                                                            String fileName =
                                                                    String.format(
                                                                            "%02d%02d%02d.bin",
                                                                            rand.nextInt(23) + 1,
                                                                            rand.nextInt(59) + 1,
                                                                            rand.nextInt(59) + 1);

                                                            List<String> pathList =
                                                                    Arrays.asList(
                                                                            GCS_SUITE_TEST_FOLDER_NAME
                                                                                            == ""
                                                                                    ? "suite_result"
                                                                                    : GCS_SUITE_TEST_FOLDER_NAME,
                                                                            year,
                                                                            month,
                                                                            day,
                                                                            fileName);

                                                            Path pathInfo =
                                                                    Paths.get(
                                                                            String.join(
                                                                                    fileSeparator,
                                                                                    pathList));

                                                            TestSuiteFileEntity
                                                                    newTestSuiteFileEntity =
                                                                            new TestSuiteFileEntity(
                                                                                    pathInfo
                                                                                            .toString());

                                                            com.googlecode.objectify.Key<
                                                                    TestSuiteFileEntity>
                                                                    testSuiteFileParent =
                                                                            com.googlecode.objectify
                                                                                    .Key.create(
                                                                                    TestSuiteFileEntity
                                                                                            .class,
                                                                                    newTestSuiteFileEntity
                                                                                            .getFilePath());


                                                            TestSuiteResultEntity
                                                                    testSuiteResultEntity =
                                                                            new TestSuiteResultEntity(
                                                                                    testSuiteFileParent,
                                                                                    Instant.now()
                                                                                            .minus(
                                                                                                    rand
                                                                                                            .nextInt(
                                                                                                                    100),
                                                                                                    ChronoUnit
                                                                                                            .DAYS)
                                                                                            .getEpochSecond(),
                                                                                    Instant.now()
                                                                                            .minus(
                                                                                                    rand
                                                                                                            .nextInt(
                                                                                                                    100),
                                                                                                    ChronoUnit
                                                                                                            .DAYS)
                                                                                            .getEpochSecond(),
                                                                                    1,
                                                                                    idx / 2 == 0
                                                                                            ? false
                                                                                            : true,
                                                                                    pathInfo
                                                                                            .toString(),
                                                                                    idx / 2 == 0
                                                                                            ? "/error/infra/log"
                                                                                            : "",
                                                                                    "Test Place Name -"
                                                                                            + idx,
                                                                                    "Suite Test Plan",
                                                                                    "Suite Version "
                                                                                            + idx,
                                                                                    "Suite Test Name",
                                                                                    "Suite Build Number "
                                                                                            + idx,
                                                                                    rand.nextInt(),
                                                                                    rand.nextInt(),
                                                                                    branch,
                                                                                    target,
                                                                                    Long.toString(
                                                                                            Math
                                                                                                    .abs(
                                                                                                            rand
                                                                                                                    .nextLong())),
                                                                                    "Build System Fingerprint "
                                                                                            + idx,
                                                                                    "Build Vendor Fingerprint "
                                                                                            + idx,
                                                                                    rand.nextInt(),
                                                                                    rand.nextInt());

                                                            testSuiteResultEntity.save(newTestSuiteFileEntity);
                                                        })));
        resultMap.put("result", "successfully generated!");
        return resultMap;
    }

    @Override
    public void doGet(HttpServletRequest request, HttpServletResponse response) throws IOException {
        String requestUri = request.getRequestURI();
        String requestArgs = request.getQueryString();

        Map<String, Object> resultMap = new HashMap<>();
        String pathInfo = requestUri.replace("/api/test_data/", "");
        switch (pathInfo) {
            case "suite":
                resultMap = this.generateSuiteTestData(request, response);
                break;
            default:
                throw new IllegalArgumentException("Invalid path info of URL");
        }

        String json = new Gson().toJson(resultMap);
        response.setStatus(HttpServletResponse.SC_OK);
        response.setContentType("application/json");
        response.setCharacterEncoding("UTF-8");
        response.getWriter().write(json);
    }

    /** Add mock data to local dev datastore. */
    @Override
    public void doPost(HttpServletRequest request, HttpServletResponse response)
            throws IOException {
        if (SystemProperty.environment.value() == SystemProperty.Environment.Value.Production) {
            response.setStatus(HttpServletResponse.SC_NOT_ACCEPTABLE);
            return;
        }

        UserService userService = UserServiceFactory.getUserService();
        User currentUser = userService.getCurrentUser();

        String pathInfo = request.getPathInfo();
        String[] pathParts = pathInfo.split("/");
        if (pathParts.length > 1) {
            // Read the json output
            Reader postJsonReader = new InputStreamReader(request.getInputStream());
            Gson gson = new GsonBuilder().create();

            String testType = pathParts[1];
            if (testType.equalsIgnoreCase("report")) {
                TestReportDataObject trdObj =
                        gson.fromJson(postJsonReader, TestReportDataObject.class);
                logger.log(Level.INFO, "trdObj => " + trdObj);
                trdObj.testList.forEach(
                        test -> {
                            test.testRunList.forEach(
                                    testRun -> {
                                        TestEntity testEntity = new TestEntity(testRun.testName);

                                        Key testRunKey =
                                                KeyFactory.createKey(
                                                        testEntity.getOldKey(),
                                                        TestRunEntity.KIND,
                                                        testRun.startTimestamp);

                                        List<TestCaseReference> failingTestCases =
                                                new ArrayList<>();
                                        for (int idx = 0;
                                                idx < testRun.failingTestcaseIds.size();
                                                idx++) {
                                            failingTestCases.add(
                                                    new TestCaseReference(
                                                            testRun.failingTestcaseIds.get(idx),
                                                            testRun.failingTestcaseOffsets.get(
                                                                    idx)));
                                        }

                                        TestStatusEntity testStatusEntity =
                                                new TestStatusEntity(
                                                        testRun.testName,
                                                        testRun.startTimestamp,
                                                        (int) testRun.passCount,
                                                        failingTestCases.size(),
                                                        failingTestCases);
                                        datastore.put(testStatusEntity.toEntity());

                                        testRun.coverageList.forEach(
                                                testRunCoverage -> {
                                                    CoverageEntity coverageEntity =
                                                            new CoverageEntity(
                                                                    testRunKey,
                                                                    testRunCoverage.group,
                                                                    testRunCoverage
                                                                            .coveredLineCount,
                                                                    testRunCoverage.totalLineCount,
                                                                    testRunCoverage.filePath,
                                                                    testRunCoverage.projectName,
                                                                    testRunCoverage.projectVersion,
                                                                    testRunCoverage.lineCoverage);
                                                    datastore.put(coverageEntity.toEntity());
                                                });

                                        testRun.profilingList.forEach(
                                                testRunProfile -> {
                                                    ProfilingPointRunEntity profilingEntity =
                                                            new ProfilingPointRunEntity(
                                                                    testRunKey,
                                                                    testRunProfile.name,
                                                                    testRunProfile.type,
                                                                    testRunProfile.regressionMode,
                                                                    testRunProfile.labels,
                                                                    testRunProfile.values,
                                                                    testRunProfile.xLabel,
                                                                    testRunProfile.yLabel,
                                                                    testRunProfile.options);
                                                    datastore.put(profilingEntity.toEntity());
                                                });

                                        TestCaseRunEntity testCaseEntity = new TestCaseRunEntity();
                                        testRun.testCaseRunList.forEach(
                                                testCaseRun -> {
                                                    for (int idx = 0;
                                                            idx < testCaseRun.testCaseNames.size();
                                                            idx++) {
                                                        testCaseEntity.addTestCase(
                                                                testCaseRun.testCaseNames.get(idx),
                                                                testCaseRun.results.get(idx));
                                                    }
                                                });
                                        datastore.put(testCaseEntity.toEntity());

                                        testRun.deviceInfoList.forEach(
                                                deviceInfo -> {
                                                    DeviceInfoEntity deviceInfoEntity =
                                                            new DeviceInfoEntity(
                                                                    testRunKey,
                                                                    deviceInfo.branch,
                                                                    deviceInfo.product,
                                                                    deviceInfo.buildFlavor,
                                                                    deviceInfo.buildId,
                                                                    deviceInfo.abiBitness,
                                                                    deviceInfo.abiName);
                                                    ;
                                                    datastore.put(deviceInfoEntity.toEntity());
                                                });

                                        testRun.buildTargetList.forEach(
                                                buildTarget -> {
                                                    BuildTargetEntity buildTargetEntity =
                                                            new BuildTargetEntity(
                                                                    buildTarget.targetName);
                                                    buildTargetEntity.save();
                                                });

                                        testRun.branchList.forEach(
                                                branch -> {
                                                    BranchEntity branchEntity =
                                                            new BranchEntity(branch.branchName);
                                                    branchEntity.save();
                                                });

                                        boolean hasCodeCoverage =
                                                testRun.totalLineCount > 0
                                                        && testRun.coveredLineCount >= 0;
                                        TestRunEntity testRunEntity =
                                                new TestRunEntity(
                                                        testEntity.getOldKey(),
                                                        testRun.type,
                                                        testRun.startTimestamp,
                                                        testRun.endTimestamp,
                                                        testRun.testBuildId,
                                                        testRun.hostName,
                                                        testRun.passCount,
                                                        testRun.failCount,
                                                        hasCodeCoverage,
                                                        testRun.testCaseIds,
                                                        testRun.links);
                                        datastore.put(testRunEntity.toEntity());

                                        CodeCoverageEntity codeCoverageEntity =
                                                new CodeCoverageEntity(
                                                        testRunEntity.getKey(),
                                                        testRun.coveredLineCount,
                                                        testRun.totalLineCount);
                                        datastore.put(codeCoverageEntity.toEntity());

                                        Entity newTestEntity = testEntity.toEntity();

                                        Transaction txn = datastore.beginTransaction();
                                        try {
                                            // Check if test already exists in the datastore
                                            try {
                                                Entity oldTest =
                                                        datastore.get(testEntity.getOldKey());
                                                TestEntity oldTestEntity =
                                                        TestEntity.fromEntity(oldTest);
                                                if (oldTestEntity == null
                                                        || !oldTestEntity.equals(testEntity)) {
                                                    datastore.put(newTestEntity);
                                                }
                                            } catch (EntityNotFoundException e) {
                                                datastore.put(newTestEntity);
                                            }
                                            txn.commit();

                                        } catch (ConcurrentModificationException
                                                | DatastoreFailureException
                                                | DatastoreTimeoutException e) {
                                            logger.log(
                                                    Level.WARNING,
                                                    "Retrying test run insert: "
                                                            + newTestEntity.getKey());
                                        } finally {
                                            if (txn.isActive()) {
                                                logger.log(
                                                        Level.WARNING,
                                                        "Transaction rollback forced for run: "
                                                                + testRunEntity.getKey());
                                                txn.rollback();
                                            }
                                        }
                                    });
                        });
            } else {
                TestPlanReportDataObject tprdObj =
                        gson.fromJson(postJsonReader, TestPlanReportDataObject.class);
                tprdObj.testPlanList.forEach(
                        testPlan -> {
                            Entity testPlanEntity =
                                    new TestPlanEntity(testPlan.testPlanName).toEntity();
                            List<Key> testRunKeys = new ArrayList<>();
                            for (int idx = 0; idx < testPlan.testModules.size(); idx++) {
                                String test = testPlan.testModules.get(idx);
                                long time = testPlan.testTimes.get(idx);
                                Key parentKey = KeyFactory.createKey(TestEntity.KIND, test);
                                Key testRunKey =
                                        KeyFactory.createKey(parentKey, TestRunEntity.KIND, time);
                                testRunKeys.add(testRunKey);
                            }
                            Map<Key, Entity> testRuns = datastore.get(testRunKeys);
                            long passCount = 0;
                            long failCount = 0;
                            long startTimestamp = -1;
                            long endTimestamp = -1;
                            String testBuildId = null;
                            long type = 0;
                            Set<DeviceInfoEntity> devices = new HashSet<>();
                            for (Key testRunKey : testRuns.keySet()) {
                                TestRunEntity testRun =
                                        TestRunEntity.fromEntity(testRuns.get(testRunKey));
                                if (testRun == null) {
                                    continue; // not a valid test run
                                }
                                passCount += testRun.getPassCount();
                                failCount += testRun.getFailCount();
                                if (startTimestamp < 0 || testRunKey.getId() < startTimestamp) {
                                    startTimestamp = testRunKey.getId();
                                }
                                if (endTimestamp < 0 || testRun.getEndTimestamp() > endTimestamp) {
                                    endTimestamp = testRun.getEndTimestamp();
                                }
                                type = testRun.getType();
                                testBuildId = testRun.getTestBuildId();
                                Query deviceInfoQuery =
                                        new Query(DeviceInfoEntity.KIND).setAncestor(testRunKey);
                                for (Entity deviceInfoEntity :
                                        datastore.prepare(deviceInfoQuery).asIterable()) {
                                    DeviceInfoEntity device =
                                            DeviceInfoEntity.fromEntity(deviceInfoEntity);
                                    if (device == null) {
                                        continue; // invalid entity
                                    }
                                    devices.add(device);
                                }
                            }
                            if (startTimestamp < 0 || testBuildId == null || type == 0) {
                                logger.log(
                                        Level.WARNING,
                                        "Couldn't infer test run information from runs.");
                                return;
                            }
                            TestPlanRunEntity testPlanRun =
                                    new TestPlanRunEntity(
                                            testPlanEntity.getKey(),
                                            testPlan.testPlanName,
                                            type,
                                            startTimestamp,
                                            endTimestamp,
                                            testBuildId,
                                            passCount,
                                            failCount,
                                            0L,
                                            0L,
                                            testRunKeys);

                            // Create the device infos.
                            for (DeviceInfoEntity device : devices) {
                                datastore.put(
                                        device.copyWithParent(testPlanRun.getOfyKey()).toEntity());
                            }
                            datastore.put(testPlanRun.toEntity());

                            Transaction txn = datastore.beginTransaction();
                            try {
                                // Check if test already exists in the database
                                try {
                                    datastore.get(testPlanEntity.getKey());
                                } catch (EntityNotFoundException e) {
                                    datastore.put(testPlanEntity);
                                }
                                txn.commit();
                            } catch (ConcurrentModificationException
                                    | DatastoreFailureException
                                    | DatastoreTimeoutException e) {
                                logger.log(
                                        Level.WARNING,
                                        "Retrying test plan insert: " + testPlanEntity.getKey());
                            } finally {
                                if (txn.isActive()) {
                                    logger.log(
                                            Level.WARNING,
                                            "Transaction rollback forced for plan run: "
                                                    + testPlanRun.key);
                                    txn.rollback();
                                }
                            }
                        });
            }
        } else {
            logger.log(Level.WARNING, "URL path parameter is omitted!");
        }

        response.setStatus(HttpServletResponse.SC_OK);
    }
}
