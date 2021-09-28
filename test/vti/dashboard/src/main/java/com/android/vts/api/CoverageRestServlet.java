/*
 * Copyright (c) 2018 Google Inc. All Rights Reserved.
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
import com.android.vts.entity.CoverageEntity;
import com.android.vts.entity.TestCoverageStatusEntity;
import com.android.vts.entity.TestPlanRunEntity;
import com.android.vts.entity.TestRunEntity;
import com.google.gson.Gson;
import com.googlecode.objectify.Key;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import static com.googlecode.objectify.ObjectifyService.ofy;

/** REST endpoint for posting test suite data to the Dashboard. */
public class CoverageRestServlet extends BaseApiServlet {

    private static final Logger logger = Logger.getLogger(CoverageRestServlet.class.getName());

    @Override
    public void doGet(HttpServletRequest request, HttpServletResponse response) throws IOException {
        String pathInfo = request.getPathInfo();
        String json = "";
        if (Objects.nonNull(pathInfo)) {
            if (pathInfo.equalsIgnoreCase("/api/data")) {
                String key = request.getParameter("key");
                json = apiCoverageData(key);
            } else {
                json = "{error: 'true', message: 'unexpected path!!!'}";
                logger.log(Level.INFO, "Path Info => " + pathInfo);
                logger.log(Level.WARNING, "Unknown path access!");
            }
        } else {
            json = "{error: 'true', message: 'the path info is not existed!!!'}";
        }

        response.setStatus(HttpServletResponse.SC_OK);
        response.setContentType("application/json");
        response.setCharacterEncoding("UTF-8");
        response.getWriter().write(json);
    }

    private String apiCoverageData(String key) {
        ApiCoverageEntity apiCoverageEntity = ApiCoverageEntity.getByUrlSafeKey(key);
        String apiCoverageEntityJson = new Gson().toJson(apiCoverageEntity);
        return apiCoverageEntityJson;
    }

    @Override
    public void doPost(HttpServletRequest request, HttpServletResponse response)
            throws IOException {

        String pathInfo = request.getPathInfo();
        String json = "";
        if (Objects.nonNull(pathInfo)) {
            if (pathInfo.equalsIgnoreCase("/api/data")) {
                String cmd = request.getParameter("cmd");
                String coverageId = request.getParameter("coverageId");
                String testName = request.getParameter("testName");
                String testRunId = request.getParameter("testRunId");
                json = postCoverageData(cmd, coverageId, testName, testRunId);
            } else if (pathInfo.equalsIgnoreCase("/api/sum")) {
                String urlSafeKey = request.getParameter("urlSafeKey");
                json = postCoverageDataSum(urlSafeKey);
            } else {
                json = "{error: 'true', message: 'unexpected path!!!'}";
            }
        } else {
            json = "{error: 'true', message: 'the path info is not existed!!!'}";
        }

        response.setStatus(HttpServletResponse.SC_OK);
        response.setContentType("application/json");
        response.setCharacterEncoding("UTF-8");
        response.getWriter().write(json);
    }

    private String postCoverageDataSum(String urlSafeKey) {
        List<List<String>> allHalApiList = new ArrayList();
        List<List<String>> allCoveredHalApiList = new ArrayList();

        Key<TestPlanRunEntity> key = Key.create(urlSafeKey);
        TestPlanRunEntity testPlanRunEntity = ofy().load().key(key).safe();

        for (Key<TestRunEntity> testRunKey : testPlanRunEntity.getTestRuns()) {
            List<ApiCoverageEntity> apiCoverageEntityList =
                    ofy().load().type(ApiCoverageEntity.class).ancestor(testRunKey).list();
            for (ApiCoverageEntity apiCoverageEntity : apiCoverageEntityList) {
                allHalApiList.add(apiCoverageEntity.getHalApi());
                allCoveredHalApiList.add(apiCoverageEntity.getCoveredHalApi());
            }
        }
        long totalHalApiNum = allHalApiList.stream().flatMap(Collection::stream).distinct().count();
        long totalCoveredHalApiNum =
                allCoveredHalApiList.stream().flatMap(Collection::stream).distinct().count();
        if (totalHalApiNum > 0) {
            testPlanRunEntity.setTotalApiCount(totalHalApiNum);
            if (totalCoveredHalApiNum > 0) {
                testPlanRunEntity.setCoveredApiCount(totalCoveredHalApiNum);
            }
            testPlanRunEntity.save();
        }

        Map<String, Long> halApiNumMap =
                new HashMap<String, Long>() {
                    {
                        put("totalHalApiNum", totalHalApiNum);
                        put("totalCoveredHalApiNum", totalCoveredHalApiNum);
                    }
                };
        String json = new Gson().toJson(halApiNumMap);
        return json;
    }

    /**
     * The API to ignore the irrelevant code for calculating ratio
     *
     * @param cmd disable or enable command to ignore the code.
     * @param coverageId the datastore ID for code coverage.
     * @param testName the test name.
     * @param testRunId the test run ID from datastore.
     * @return success json.
     */
    private String postCoverageData(
            String cmd, String coverageId, String testName, String testRunId) {

        Boolean isIgnored = false;
        if (cmd.equals("disable")) {
            isIgnored = true;
        }
        CoverageEntity coverageEntity = CoverageEntity.findById(testName, testRunId, coverageId);
        coverageEntity.setIsIgnored(isIgnored);
        coverageEntity.save();

        TestCoverageStatusEntity testCoverageStatusEntity =
                TestCoverageStatusEntity.findById(testName);
        Long newCoveredLineCount =
                cmd.equals("disable")
                        ? testCoverageStatusEntity.getUpdatedCoveredLineCount()
                                - coverageEntity.getCoveredCount()
                        : testCoverageStatusEntity.getUpdatedCoveredLineCount()
                                + coverageEntity.getCoveredCount();
        Long newTotalLineCount =
                cmd.equals("disable")
                        ? testCoverageStatusEntity.getUpdatedTotalLineCount()
                                - coverageEntity.getTotalCount()
                        : testCoverageStatusEntity.getUpdatedTotalLineCount()
                                + coverageEntity.getTotalCount();
        testCoverageStatusEntity.setUpdatedCoveredLineCount(newCoveredLineCount);
        testCoverageStatusEntity.setUpdatedTotalLineCount(newTotalLineCount);
        testCoverageStatusEntity.save();

        String json = new Gson().toJson("Success!");
        return json;
    }
}
