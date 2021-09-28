/*
 * Copyright (c) 2016 Google Inc. All Rights Reserved.
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

import com.android.vts.entity.ApiCoverageEntity;
import com.android.vts.entity.BranchEntity;
import com.android.vts.entity.BuildTargetEntity;
import com.android.vts.entity.CodeCoverageEntity;
import com.android.vts.entity.CoverageEntity;
import com.android.vts.entity.DashboardEntity;
import com.android.vts.entity.DeviceInfoEntity;
import com.android.vts.entity.HalApiEntity;
import com.android.vts.entity.ProfilingPointRunEntity;
import com.android.vts.entity.TestCaseRunEntity;
import com.android.vts.entity.TestEntity;
import com.android.vts.entity.TestPlanEntity;
import com.android.vts.entity.TestPlanRunEntity;
import com.android.vts.entity.TestRunEntity;
import com.android.vts.proto.VtsReportMessage;
import com.android.vts.proto.VtsReportMessage.DashboardPostMessage;
import com.android.vts.proto.VtsReportMessage.TestPlanReportMessage;
import com.android.vts.proto.VtsReportMessage.TestReportMessage;
import com.google.api.client.googleapis.auth.oauth2.GoogleCredential;
import com.google.api.client.http.javanet.NetHttpTransport;
import com.google.api.client.json.jackson.JacksonFactory;
import com.google.api.services.oauth2.Oauth2;
import com.google.api.services.oauth2.model.Tokeninfo;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import com.google.appengine.api.datastore.Key;
import lombok.extern.slf4j.Slf4j;
import org.apache.commons.codec.binary.Base64;

import static com.googlecode.objectify.ObjectifyService.ofy;

@Slf4j
/** REST endpoint for posting data to the Dashboard. */
public class DatastoreRestServlet extends BaseApiServlet {
    private static String SERVICE_CLIENT_ID;
    private static final String SERVICE_NAME = "VTS Dashboard";

    @Override
    public void init(ServletConfig cfg) throws ServletException {
        super.init(cfg);

        SERVICE_CLIENT_ID = this.systemConfigProp.getProperty("appengine.serviceClientID");
    }

    @Override
    public void doPost(HttpServletRequest request, HttpServletResponse response)
            throws IOException {
        // Retrieve the params
        DashboardPostMessage postMessage;
        try {
            String payload = request.getReader().lines().collect(Collectors.joining());
            byte[] value = Base64.decodeBase64(payload);
            postMessage = DashboardPostMessage.parseFrom(value);
        } catch (IOException e) {
            response.setStatus(HttpServletResponse.SC_BAD_REQUEST);
            log.error("Invalid proto: " + e.getLocalizedMessage());
            return;
        }

        String resultMsg = "";
        // Verify service account access token.
        if (postMessage.hasAccessToken()) {
            String accessToken = postMessage.getAccessToken();
            log.debug("accessToken => " + accessToken);

            GoogleCredential credential = new GoogleCredential().setAccessToken(accessToken);
            Oauth2 oauth2 =
                    new Oauth2.Builder(new NetHttpTransport(), new JacksonFactory(), credential)
                            .setApplicationName(SERVICE_NAME)
                            .build();
            Tokeninfo tokenInfo = oauth2.tokeninfo().setAccessToken(accessToken).execute();
            if (tokenInfo.getIssuedTo().equals(SERVICE_CLIENT_ID)) {
                for (TestReportMessage testReportMessage : postMessage.getTestReportList()) {
                    this.insertTestReport(testReportMessage);
                }

                for (TestPlanReportMessage planReportMessage :
                        postMessage.getTestPlanReportList()) {
                    this.insertTestPlanReport(planReportMessage);
                }

                response.setStatus(HttpServletResponse.SC_OK);
                resultMsg = "Success!!";
            } else {
                log.warn("service_client_id didn't match!");
                log.debug("SERVICE_CLIENT_ID => " + tokenInfo.getIssuedTo());
                resultMsg = "Your SERVICE_CLIENT_ID is incorrect!";
                response.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
            }
        } else {
            log.error("postMessage do not contain any accessToken!");
            resultMsg = "Your message do not have access token!";
            response.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
        }
        response.setContentType("application/json");
        response.setCharacterEncoding("UTF-8");
        response.getWriter().write("{'result_msg': " + resultMsg + "}");
    }

    /**
     * Upload data from a test report message
     *
     * @param report The test report containing data to upload.
     */
    private void insertTestReport(TestReportMessage report) {

        if (!report.hasStartTimestamp()
                || !report.hasEndTimestamp()
                || !report.hasTest()
                || !report.hasHostInfo()
                || !report.hasBuildInfo()) {
            // missing information
            log.error("Missing information in report !");
            return;
        }

        List<TestEntity> testEntityList = new ArrayList<>();
        List<TestRunEntity> testRunEntityList = new ArrayList<>();
        List<BranchEntity> branchEntityList = new ArrayList<>();
        List<BuildTargetEntity> buildTargetEntityList = new ArrayList<>();
        List<CoverageEntity> coverageEntityList = new ArrayList<>();
        List<CodeCoverageEntity> codeCoverageEntityList = new ArrayList<>();
        List<DeviceInfoEntity> deviceInfoEntityList = new ArrayList<>();
        List<ProfilingPointRunEntity> profilingPointRunEntityList = new ArrayList<>();
        List<TestCaseRunEntity> testCaseRunEntityList = new ArrayList<>();
        List<ApiCoverageEntity> apiCoverageEntityList = new ArrayList<>();

        List<?> allEntityList =
                Arrays.asList(
                        testEntityList,
                        branchEntityList,
                        buildTargetEntityList,
                        coverageEntityList,
                        codeCoverageEntityList,
                        deviceInfoEntityList,
                        profilingPointRunEntityList,
                        testCaseRunEntityList,
                        apiCoverageEntityList,
                        testRunEntityList);

        long passCount = 0;
        long failCount = 0;
        long coveredLineCount = 0;
        long totalLineCount = 0;

        Set<Key> buildTargetKeys = new HashSet<>();
        Set<Key> branchKeys = new HashSet<>();
        List<Key> profilingPointKeyList = new ArrayList<>();
        List<String> linkList = new ArrayList<>();

        long startTimestamp = report.getStartTimestamp();
        long endTimestamp = report.getEndTimestamp();
        String testName = report.getTest().toStringUtf8();
        String testBuildId = report.getBuildInfo().getId().toStringUtf8();
        String hostName = report.getHostInfo().getHostname().toStringUtf8();

        TestEntity testEntity = new TestEntity(testName);

        com.googlecode.objectify.Key testRunKey =
                testEntity.getTestRunKey(report.getStartTimestamp());

        testEntityList.add(testEntity);

        int testCaseRunEntityIndex = 0;
        testCaseRunEntityList.add(new TestCaseRunEntity());
        // Process test cases
        for (VtsReportMessage.TestCaseReportMessage testCase : report.getTestCaseList()) {
            String testCaseName = testCase.getName().toStringUtf8();
            VtsReportMessage.TestCaseResult result = testCase.getTestResult();
            // Track global pass/fail counts
            if (result == VtsReportMessage.TestCaseResult.TEST_CASE_RESULT_PASS) {
                ++passCount;
            } else if (result != VtsReportMessage.TestCaseResult.TEST_CASE_RESULT_SKIP) {
                ++failCount;
            }
            if (testCase.getSystraceCount() > 0
                    && testCase.getSystraceList().get(0).getUrlCount() > 0) {
                String systraceLink = testCase.getSystraceList().get(0).getUrl(0).toStringUtf8();
                linkList.add(systraceLink);
            }

            // Process coverage data for test case
            for (VtsReportMessage.CoverageReportMessage coverage : testCase.getCoverageList()) {
                CoverageEntity coverageEntity =
                        CoverageEntity.fromCoverageReport(testRunKey, testCaseName, coverage);
                if (coverageEntity == null) {
                    log.warn("Invalid coverage report in test run " + testRunKey);
                } else {
                    coveredLineCount += coverageEntity.getCoveredCount();
                    totalLineCount += coverageEntity.getTotalCount();
                    coverageEntityList.add(coverageEntity);
                }
            }

            // Process profiling data for test case
            for (VtsReportMessage.ProfilingReportMessage profiling : testCase.getProfilingList()) {
                ProfilingPointRunEntity profilingPointRunEntity =
                        ProfilingPointRunEntity.fromProfilingReport(testRunKey, profiling);
                if (profilingPointRunEntity == null) {
                    log.warn("Invalid profiling report in test run " + testRunKey);
                } else {
                    profilingPointRunEntityList.add(profilingPointRunEntity);
                    profilingPointKeyList.add(profilingPointRunEntity.getKey());
                    testEntity.setHasProfilingData(true);
                }
            }

            TestCaseRunEntity testCaseRunEntity = testCaseRunEntityList.get(testCaseRunEntityIndex);
            if (!testCaseRunEntity.addTestCase(testCaseName, result.getNumber())) {
                testCaseRunEntity = new TestCaseRunEntity();
                testCaseRunEntity.addTestCase(testCaseName, result.getNumber());
                testCaseRunEntityList.add(testCaseRunEntity);
                testCaseRunEntityIndex++;
            }
        }

        // Process device information
        long testRunType = 0;
        for (VtsReportMessage.AndroidDeviceInfoMessage device : report.getDeviceInfoList()) {
            DeviceInfoEntity deviceInfoEntity =
                    DeviceInfoEntity.fromDeviceInfoMessage(testRunKey, device);
            if (deviceInfoEntity == null) {
                log.warn("Invalid device info in test run " + testRunKey);
            } else {
                // Run type on devices must be the same, else set to OTHER
                TestRunEntity.TestRunType runType =
                        TestRunEntity.TestRunType.fromBuildId(deviceInfoEntity.getBuildId());
                if (runType == null) {
                    testRunType = TestRunEntity.TestRunType.OTHER.getNumber();
                } else {
                    testRunType = runType.getNumber();
                }
                deviceInfoEntityList.add(deviceInfoEntity);
                BuildTargetEntity target = new BuildTargetEntity(deviceInfoEntity.getBuildFlavor());
                if (buildTargetKeys.add(target.getKey())) {
                    buildTargetEntityList.add(target);
                }
                BranchEntity branch = new BranchEntity(deviceInfoEntity.getBranch());
                if (branchKeys.add(branch.getKey())) {
                    branchEntityList.add(branch);
                }
            }
        }

        // Overall run type should be determined by the device builds unless test build is OTHER
        if (testRunType == TestRunEntity.TestRunType.OTHER.getNumber()) {
            testRunType = TestRunEntity.TestRunType.fromBuildId(testBuildId).getNumber();
        } else if (TestRunEntity.TestRunType.fromBuildId(testBuildId)
                == TestRunEntity.TestRunType.OTHER) {
            testRunType = TestRunEntity.TestRunType.OTHER.getNumber();
        }

        // Process global coverage data
        for (VtsReportMessage.CoverageReportMessage coverage : report.getCoverageList()) {
            CoverageEntity coverageEntity =
                    CoverageEntity.fromCoverageReport(testRunKey, new String(), coverage);
            if (coverageEntity == null) {
                log.warn("Invalid coverage report in test run " + testRunKey);
            } else {
                coveredLineCount += coverageEntity.getCoveredCount();
                totalLineCount += coverageEntity.getTotalCount();
                coverageEntityList.add(coverageEntity);
            }
        }

        // Process global API coverage data
        for (VtsReportMessage.ApiCoverageReportMessage apiCoverage : report.getApiCoverageList()) {
            VtsReportMessage.HalInterfaceMessage halInterfaceMessage =
                    apiCoverage.getHalInterface();
            List<String> halApiList =
                    apiCoverage
                            .getHalApiList()
                            .stream()
                            .map(h -> h.toStringUtf8())
                            .collect(Collectors.toList());
            List<String> coveredHalApiList =
                    apiCoverage
                            .getCoveredHalApiList()
                            .stream()
                            .map(h -> h.toStringUtf8())
                            .collect(Collectors.toList());
            ApiCoverageEntity apiCoverageEntity =
                    new ApiCoverageEntity(
                            testRunKey,
                            halInterfaceMessage.getHalPackageName().toStringUtf8(),
                            halInterfaceMessage.getHalVersionMajor(),
                            halInterfaceMessage.getHalVersionMinor(),
                            halInterfaceMessage.getHalInterfaceName().toStringUtf8(),
                            halApiList,
                            coveredHalApiList);
            apiCoverageEntityList.add(apiCoverageEntity);
        }

        // Process global profiling data
        for (VtsReportMessage.ProfilingReportMessage profiling : report.getProfilingList()) {
            ProfilingPointRunEntity profilingPointRunEntity =
                    ProfilingPointRunEntity.fromProfilingReport(testRunKey, profiling);
            if (profilingPointRunEntity == null) {
                log.warn("Invalid profiling report in test run " + testRunKey);
            } else {
                profilingPointRunEntityList.add(profilingPointRunEntity);
                profilingPointKeyList.add(profilingPointRunEntity.getKey());
                testEntity.setHasProfilingData(true);
            }
        }

        // Process log data
        for (VtsReportMessage.LogMessage log : report.getLogList()) {
            if (log.hasUrl()) {
                linkList.add(log.getUrl().toStringUtf8());
            }
        }
        // Process url resource
        for (VtsReportMessage.UrlResourceMessage resource : report.getLinkResourceList()) {
            if (resource.hasUrl()) {
                linkList.add(resource.getUrl().toStringUtf8());
            }
        }

        boolean hasCodeCoverage = totalLineCount > 0 && coveredLineCount >= 0;
        TestRunEntity testRunEntity =
                new TestRunEntity(
                        testEntity.getOldKey(),
                        testRunType,
                        startTimestamp,
                        endTimestamp,
                        testBuildId,
                        hostName,
                        passCount,
                        failCount,
                        hasCodeCoverage,
                        new ArrayList<>(),
                        linkList);
        testRunEntityList.add(testRunEntity);

        CodeCoverageEntity codeCoverageEntity =
                new CodeCoverageEntity(
                        testRunEntity.getId(),
                        testRunEntity.getKey(),
                        coveredLineCount,
                        totalLineCount);
        codeCoverageEntityList.add(codeCoverageEntity);

        ofy().transact(
                        () -> {
                            List<Long> testCaseIds = new ArrayList<>();
                            for (Object entity : allEntityList) {
                                if (entity instanceof List) {
                                    List listEntity = (List) entity;
                                    if (listEntity.size() > 0
                                            && listEntity.get(0) instanceof TestCaseRunEntity) {
                                        Map<
                                                        com.googlecode.objectify.Key<
                                                                TestCaseRunEntity>,
                                                        TestCaseRunEntity>
                                                testCaseRunEntityMap =
                                                        DashboardEntity.saveAll(
                                                                testCaseRunEntityList,
                                                                this
                                                                        .MAX_ENTITY_SIZE_PER_TRANSACTION);

                                        testCaseIds =
                                                testCaseRunEntityMap
                                                        .values()
                                                        .stream()
                                                        .map(
                                                                testCaseRunEntity ->
                                                                        testCaseRunEntity.getId())
                                                        .collect(Collectors.toList());
                                    } else if (listEntity.size() > 0
                                            && listEntity.get(0) instanceof TestRunEntity) {
                                        testRunEntityList.get(0).setTestCaseIds(testCaseIds);
                                        DashboardEntity.saveAll(
                                                testRunEntityList,
                                                this.MAX_ENTITY_SIZE_PER_TRANSACTION);
                                    } else {
                                        List<DashboardEntity> dashboardEntityList =
                                                (List<DashboardEntity>) entity;
                                        DashboardEntity.saveAll(
                                                dashboardEntityList,
                                                this.MAX_ENTITY_SIZE_PER_TRANSACTION);
                                    }
                                }
                            }
                        });
    }

    /**
     * Upload data from a test plan report message
     *
     * @param report The test plan report containing data to upload.
     */
    private void insertTestPlanReport(TestPlanReportMessage report) {
        List<DeviceInfoEntity> deviceInfoEntityList = new ArrayList<>();
        List<HalApiEntity> halApiEntityList = new ArrayList<>();

        List allEntityList = Arrays.asList(deviceInfoEntityList, halApiEntityList);

        List<String> testModules = report.getTestModuleNameList();
        List<Long> testTimes = report.getTestModuleStartTimestampList();
        if (testModules.size() != testTimes.size() || !report.hasTestPlanName()) {
            log.error("TestPlanReportMessage is missing information.");
            return;
        }

        String testPlanName = report.getTestPlanName();
        TestPlanEntity testPlanEntity = new TestPlanEntity(testPlanName);
        List<com.googlecode.objectify.Key<TestRunEntity>> testRunKeyList = new ArrayList<>();
        for (int index = 0; index < testModules.size(); index++) {
            String test = testModules.get(index);
            long time = testTimes.get(index);
            com.googlecode.objectify.Key testKey =
                    com.googlecode.objectify.Key.create(TestEntity.class, test);
            com.googlecode.objectify.Key testRunKey =
                    com.googlecode.objectify.Key.create(testKey, TestRunEntity.class, time);
            testRunKeyList.add(testRunKey);
        }

        Map<com.googlecode.objectify.Key<TestRunEntity>, TestRunEntity> testRunEntityMap =
                ofy().load().keys(() -> testRunKeyList.iterator());

        long passCount = 0;
        long failCount = 0;
        long startTimestamp = -1;
        long endTimestamp = -1;
        String testBuildId = null;
        long testType = -1;
        Set<DeviceInfoEntity> deviceInfoEntitySet = new HashSet<>();
        for (TestRunEntity testRunEntity : testRunEntityMap.values()) {
            passCount += testRunEntity.getPassCount();
            failCount += testRunEntity.getFailCount();
            if (startTimestamp < 0 || testRunEntity.getStartTimestamp() < startTimestamp) {
                startTimestamp = testRunEntity.getStartTimestamp();
            }
            if (endTimestamp < 0 || testRunEntity.getEndTimestamp() > endTimestamp) {
                endTimestamp = testRunEntity.getEndTimestamp();
            }
            testType = testRunEntity.getType();
            testBuildId = testRunEntity.getTestBuildId();

            List<DeviceInfoEntity> deviceInfoEntityListWithTestRunKey =
                    ofy().load()
                            .type(DeviceInfoEntity.class)
                            .ancestor(testRunEntity.getOfyKey())
                            .list();

            for (DeviceInfoEntity deviceInfoEntity : deviceInfoEntityListWithTestRunKey) {
                deviceInfoEntitySet.add(deviceInfoEntity);
            }
        }

        if (startTimestamp < 0 || testBuildId == null || testType == -1) {
            log.debug("startTimestamp => " + startTimestamp);
            log.debug("testBuildId => " + testBuildId);
            log.debug("type => " + testType);
            log.error("Couldn't infer test run information from runs.");
            return;
        }

        TestPlanRunEntity testPlanRunEntity =
                new TestPlanRunEntity(
                        testPlanEntity.getKey(),
                        testPlanName,
                        testType,
                        startTimestamp,
                        endTimestamp,
                        testBuildId,
                        passCount,
                        failCount,
                        0L,
                        0L,
                        testRunKeyList);

        // Create the device infos.
        for (DeviceInfoEntity device : deviceInfoEntitySet) {
            deviceInfoEntityList.add(device.copyWithParent(testPlanRunEntity.getOfyKey()));
        }

        // Process global HAL API coverage data
        for (VtsReportMessage.ApiCoverageReportMessage apiCoverage : report.getHalApiReportList()) {
            VtsReportMessage.HalInterfaceMessage halInterfaceMessage =
                    apiCoverage.getHalInterface();
            List<String> halApiList =
                    apiCoverage
                            .getHalApiList()
                            .stream()
                            .map(h -> h.toStringUtf8())
                            .collect(Collectors.toList());
            List<String> coveredHalApiList =
                    apiCoverage
                            .getCoveredHalApiList()
                            .stream()
                            .map(h -> h.toStringUtf8())
                            .collect(Collectors.toList());
            HalApiEntity halApiEntity =
                    new HalApiEntity(
                            testPlanRunEntity.getOfyKey(),
                            halInterfaceMessage.getHalReleaseLevel().toStringUtf8(),
                            halInterfaceMessage.getHalPackageName().toStringUtf8(),
                            halInterfaceMessage.getHalVersionMajor(),
                            halInterfaceMessage.getHalVersionMinor(),
                            halInterfaceMessage.getHalInterfaceName().toStringUtf8(),
                            halApiList,
                            coveredHalApiList);
            halApiEntityList.add(halApiEntity);
        }

        ofy().transact(
                        () -> {
                            testPlanEntity.save();
                            testPlanRunEntity.save();
                            for (Object entity : allEntityList) {
                                List<DashboardEntity> dashboardEntityList =
                                        (List<DashboardEntity>) entity;
                                Map<com.googlecode.objectify.Key<DashboardEntity>, DashboardEntity>
                                        mapInfo =
                                                DashboardEntity.saveAll(
                                                        dashboardEntityList,
                                                        this.MAX_ENTITY_SIZE_PER_TRANSACTION);
                            }
                        });

        // Add the task to calculate total number API list.
        testPlanRunEntity.addCoverageApiTask();
    }
}
