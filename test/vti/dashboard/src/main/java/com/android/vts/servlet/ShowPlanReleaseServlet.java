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

package com.android.vts.servlet;

import com.android.vts.entity.DeviceInfoEntity;
import com.android.vts.entity.TestPlanEntity;
import com.android.vts.entity.TestPlanRunEntity;
import com.android.vts.entity.TestSuiteResultEntity;
import com.android.vts.util.DatastoreHelper;
import com.android.vts.util.FilterUtil;
import com.android.vts.util.Pagination;

import com.google.appengine.api.datastore.DatastoreService;
import com.google.appengine.api.datastore.DatastoreServiceFactory;
import com.google.appengine.api.datastore.Entity;
import com.google.appengine.api.datastore.Key;
import com.google.appengine.api.datastore.KeyFactory;
import com.google.appengine.api.datastore.Query;
import com.google.appengine.api.datastore.Query.Filter;
import com.google.appengine.api.datastore.Query.SortDirection;
import com.google.gson.Gson;
import com.google.gson.JsonObject;
import com.google.gson.JsonPrimitive;
import org.apache.commons.lang.StringUtils;

import javax.servlet.RequestDispatcher;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.IOException;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.logging.Level;
import java.util.stream.Collectors;
import java.util.stream.IntStream;

import static com.googlecode.objectify.ObjectifyService.ofy;

public class ShowPlanReleaseServlet extends BaseServlet {
    private static final int MAX_RUNS_PER_PAGE = 90;

    /** the previous cursor string token list where to start */
    private static final LinkedHashSet<String> pageCountTokenSet = new LinkedHashSet<>();

    @Override
    public PageType getNavParentType() {
        return PageType.RELEASE;
    }

    @Override
    public List<Page> getBreadcrumbLinks(HttpServletRequest request) {
        String testType =
                request.getParameter("type") == null ? "plan" : request.getParameter("type");
        List<Page> links = new ArrayList<>();
        String planName = request.getParameter("plan");
        if (testType.equals("plan")) {
            links.add(new Page(PageType.RELEASE, "TEST PLANS", "?type=" + testType, true));
            links.add(new Page(PageType.PLAN_RELEASE, planName, "?plan=" + planName));
        } else {
            links.add(new Page(PageType.RELEASE, "TEST SUITES", "?type=" + testType, true));
            links.add(
                    new Page(
                            PageType.PLAN_RELEASE,
                            planName,
                            "?plan=" + planName + "&type=" + testType));
        }
        return links;
    }

    /** Model to describe each test plan run . */
    private class TestPlanRunMetadata implements Comparable<TestPlanRunMetadata> {
        public final TestPlanRunEntity testPlanRun;
        public final List<String> devices;
        public final Set<DeviceInfoEntity> deviceSet;

        public TestPlanRunMetadata(TestPlanRunEntity testPlanRun) {
            this.testPlanRun = testPlanRun;
            this.devices = new ArrayList<>();
            this.deviceSet = new HashSet<>();
        }

        public void addDevice(DeviceInfoEntity device) {
            if (device == null || deviceSet.contains(device)) return;
            devices.add(
                    device.getBranch()
                            + "/"
                            + device.getBuildFlavor()
                            + " ("
                            + device.getBuildId()
                            + ")");
            deviceSet.add(device);
        }

        public JsonObject toJson() {
            JsonObject obj = new JsonObject();
            obj.add("testPlanRun", testPlanRun.toJson());
            obj.add("deviceInfo", new JsonPrimitive(StringUtils.join(devices, ", ")));
            return obj;
        }

        @Override
        public int compareTo(TestPlanRunMetadata o) {
            return new Long(o.testPlanRun.getStartTimestamp())
                    .compareTo(this.testPlanRun.getStartTimestamp());
        }
    }

    @Override
    public void doGetHandler(HttpServletRequest request, HttpServletResponse response)
            throws IOException {
        String testType =
                request.getParameter("type") == null ? "plan" : request.getParameter("type");

        RequestDispatcher dispatcher;
        if (testType.equalsIgnoreCase("plan")) {
            dispatcher = this.getTestPlanDispatcher(request, response);
        } else {
            dispatcher = this.getTestSuiteDispatcher(request, response);
        }

        try {
            request.setAttribute("testType", testType);
            response.setStatus(HttpServletResponse.SC_OK);
            dispatcher.forward(request, response);
        } catch (ServletException e) {
            logger.log(Level.SEVERE, "Servlet Excpetion caught : ", e);
        }
    }

    private RequestDispatcher getTestPlanDispatcher(
            HttpServletRequest request, HttpServletResponse response) {
        String PLAN_RELEASE_JSP = "WEB-INF/jsp/show_plan_release.jsp";

        DatastoreService datastore = DatastoreServiceFactory.getDatastoreService();

        Long startTime = null; // time in microseconds
        Long endTime = null; // time in microseconds
        if (request.getParameter("startTime") != null) {
            String time = request.getParameter("startTime");
            try {
                startTime = Long.parseLong(time);
                startTime = startTime > 0 ? startTime : null;
            } catch (NumberFormatException e) {
                startTime = null;
            }
        }
        if (request.getParameter("endTime") != null) {
            String time = request.getParameter("endTime");
            try {
                endTime = Long.parseLong(time);
                endTime = endTime > 0 ? endTime : null;
            } catch (NumberFormatException e) {
                endTime = null;
            }
        }
        SortDirection dir = SortDirection.DESCENDING;
        if (startTime != null && endTime == null) {
            dir = SortDirection.ASCENDING;
        }
        boolean unfiltered = request.getParameter("unfiltered") != null;
        boolean showPresubmit = request.getParameter("showPresubmit") != null;
        boolean showPostsubmit = request.getParameter("showPostsubmit") != null;
        // If no params are specified, set to default of postsubmit-only.
        if (!(showPresubmit || showPostsubmit)) {
            showPostsubmit = true;
        }

        // If unfiltered, set showPre- and Post-submit to true for accurate UI.
        if (unfiltered) {
            showPostsubmit = true;
            showPresubmit = true;
        }
        Filter typeFilter = FilterUtil.getTestTypeFilter(showPresubmit, showPostsubmit, unfiltered);
        String testPlan = request.getParameter("plan");
        Key testPlanKey = KeyFactory.createKey(TestPlanEntity.KIND, testPlan);
        Filter testPlanRunFilter =
                FilterUtil.getTimeFilter(
                        testPlanKey, TestPlanRunEntity.KIND, startTime, endTime, typeFilter);
        Map<String, String[]> parameterMap = request.getParameterMap();
        List<Filter> userTestFilters = FilterUtil.getUserTestFilters(parameterMap);
        userTestFilters.add(0, testPlanRunFilter);
        Filter userDeviceFilter = FilterUtil.getUserDeviceFilter(parameterMap);

        List<TestPlanRunMetadata> testPlanRuns = new ArrayList<>();
        Map<Key, TestPlanRunMetadata> testPlanMap = new HashMap<>();
        Key minKey = null;
        Key maxKey = null;
        List<Key> gets =
                FilterUtil.getMatchingKeys(
                        testPlanKey,
                        TestPlanRunEntity.KIND,
                        userTestFilters,
                        userDeviceFilter,
                        dir,
                        MAX_RUNS_PER_PAGE);
        Map<Key, Entity> entityMap = datastore.get(gets);
        logger.log(Level.INFO, "entityMap => " + entityMap);
        for (Key key : gets) {
            if (!entityMap.containsKey(key)) {
                continue;
            }
            TestPlanRunEntity testPlanRun = TestPlanRunEntity.fromEntity(entityMap.get(key));
            if (testPlanRun == null) {
                continue;
            }
            TestPlanRunMetadata metadata = new TestPlanRunMetadata(testPlanRun);
            testPlanRuns.add(metadata);
            testPlanMap.put(key, metadata);
            if (minKey == null || key.compareTo(minKey) < 0) {
                minKey = key;
            }
            if (maxKey == null || key.compareTo(maxKey) > 0) {
                maxKey = key;
            }
        }
        if (minKey != null && maxKey != null) {
            Filter deviceFilter =
                    FilterUtil.getDeviceTimeFilter(
                            testPlanKey, TestPlanRunEntity.KIND, minKey.getId(), maxKey.getId());
            Query deviceQuery =
                    new Query(DeviceInfoEntity.KIND)
                            .setAncestor(testPlanKey)
                            .setFilter(deviceFilter)
                            .setKeysOnly();
            List<Key> deviceGets = new ArrayList<>();
            for (Entity device :
                    datastore
                            .prepare(deviceQuery)
                            .asIterable(DatastoreHelper.getLargeBatchOptions())) {
                if (testPlanMap.containsKey(device.getParent())) {
                    deviceGets.add(device.getKey());
                }
            }
            logger.log(Level.INFO, "deviceGets => " + deviceGets);
            Map<Key, Entity> devices = datastore.get(deviceGets);
            for (Key key : devices.keySet()) {
                if (!testPlanMap.containsKey(key.getParent())) continue;
                DeviceInfoEntity device = DeviceInfoEntity.fromEntity(devices.get(key));
                if (device == null) continue;
                TestPlanRunMetadata metadata = testPlanMap.get(key.getParent());
                metadata.addDevice(device);
            }
        }

        testPlanRuns.sort(Comparator.naturalOrder());
        logger.log(Level.INFO, "testPlanRuns => " + testPlanRuns);

        if (testPlanRuns.size() > 0) {
            TestPlanRunMetadata firstRun = testPlanRuns.get(0);
            endTime = firstRun.testPlanRun.getStartTimestamp();

            TestPlanRunMetadata lastRun = testPlanRuns.get(testPlanRuns.size() - 1);
            startTime = lastRun.testPlanRun.getStartTimestamp();
        }

        List<JsonObject> testPlanRunObjects = new ArrayList<>();
        for (TestPlanRunMetadata metadata : testPlanRuns) {
            testPlanRunObjects.add(metadata.toJson());
        }

        FilterUtil.setAttributes(request, parameterMap);

        request.setAttribute("plan", request.getParameter("plan"));
        request.setAttribute(
                "hasNewer",
                new Gson()
                        .toJson(
                                DatastoreHelper.hasNewer(
                                        testPlanKey, TestPlanRunEntity.KIND, endTime)));
        request.setAttribute(
                "hasOlder",
                new Gson()
                        .toJson(
                                DatastoreHelper.hasOlder(
                                        testPlanKey, TestPlanRunEntity.KIND, startTime)));
        request.setAttribute("planRuns", new Gson().toJson(testPlanRunObjects));

        request.setAttribute("unfiltered", unfiltered);
        request.setAttribute("showPresubmit", showPresubmit);
        request.setAttribute("showPostsubmit", showPostsubmit);
        request.setAttribute("startTime", new Gson().toJson(startTime));
        request.setAttribute("endTime", new Gson().toJson(endTime));
        request.setAttribute("branches", new Gson().toJson(DatastoreHelper.getAllBranches()));
        request.setAttribute("devices", new Gson().toJson(DatastoreHelper.getAllBuildFlavors()));

        RequestDispatcher dispatcher = request.getRequestDispatcher(PLAN_RELEASE_JSP);
        return dispatcher;
    }

    private RequestDispatcher getTestSuiteDispatcher(
            HttpServletRequest request, HttpServletResponse response) {
        String PLAN_RELEASE_JSP = "WEB-INF/jsp/show_suite_release.jsp";

        String testPlan = request.getParameter("plan");
        String testCategoryType =
                Objects.isNull(request.getParameter("testCategoryType"))
                        ? "1"
                        : request.getParameter("testCategoryType");
        int page =
                Objects.isNull(request.getParameter("page"))
                        ? 1
                        : Integer.valueOf(request.getParameter("page"));
        String nextPageToken =
                Objects.isNull(request.getParameter("nextPageToken"))
                        ? ""
                        : request.getParameter("nextPageToken");

        com.googlecode.objectify.cmd.Query<TestSuiteResultEntity> testSuiteResultEntityQuery =
                ofy().load()
                        .type(TestSuiteResultEntity.class)
                        .filter("suitePlan", testPlan)
                        .filter(this.getTestTypeFieldName(testCategoryType), true);

        if (Objects.nonNull(request.getParameter("branch"))) {
            request.setAttribute("branch", request.getParameter("branch"));
            testSuiteResultEntityQuery =
                    testSuiteResultEntityQuery.filter("branch", request.getParameter("branch"));
        }
        if (Objects.nonNull(request.getParameter("hostName"))) {
            request.setAttribute("hostName", request.getParameter("hostName"));
            testSuiteResultEntityQuery =
                    testSuiteResultEntityQuery.filter("hostName", request.getParameter("hostName"));
        }
        if (Objects.nonNull(request.getParameter("buildId"))) {
            request.setAttribute("buildId", request.getParameter("buildId"));
            testSuiteResultEntityQuery =
                    testSuiteResultEntityQuery.filter("buildId", request.getParameter("buildId"));
        }
        if (Objects.nonNull(request.getParameter("deviceName"))) {
            request.setAttribute("deviceName", request.getParameter("deviceName"));
            testSuiteResultEntityQuery =
                    testSuiteResultEntityQuery.filter(
                            "deviceName", request.getParameter("deviceName"));
        }
        testSuiteResultEntityQuery = testSuiteResultEntityQuery.orderKey(true);

        Pagination<TestSuiteResultEntity> testSuiteResultEntityPagination =
                new Pagination(
                        testSuiteResultEntityQuery,
                        page,
                        Pagination.DEFAULT_PAGE_SIZE,
                        nextPageToken,
                        pageCountTokenSet);

        String nextPageTokenPagination = testSuiteResultEntityPagination.getNextPageCountToken();
        if (!nextPageTokenPagination.trim().isEmpty()) {
            this.pageCountTokenSet.add(nextPageTokenPagination);
        }

        logger.log(Level.INFO, "pageCountTokenSet => " + pageCountTokenSet);

        logger.log(Level.INFO, "list => " + testSuiteResultEntityPagination.getList());
        logger.log(
                Level.INFO,
                "next page count token => "
                        + testSuiteResultEntityPagination.getNextPageCountToken());
        logger.log(
                Level.INFO,
                "page min range => " + testSuiteResultEntityPagination.getMinPageRange());
        logger.log(
                Level.INFO,
                "page max range => " + testSuiteResultEntityPagination.getMaxPageRange());
        logger.log(Level.INFO, "page size => " + testSuiteResultEntityPagination.getPageSize());
        logger.log(Level.INFO, "total count => " + testSuiteResultEntityPagination.getTotalCount());

        request.setAttribute("plan", testPlan);
        request.setAttribute("page", page);
        request.setAttribute("testType", "suite");
        request.setAttribute("testCategoryType", testCategoryType);
        request.setAttribute("testSuiteResultEntityPagination", testSuiteResultEntityPagination);
        RequestDispatcher dispatcher = request.getRequestDispatcher(PLAN_RELEASE_JSP);
        return dispatcher;
    }

    private String getTestTypeFieldName(String testCategoryType) {
        String fieldName;
        switch (testCategoryType) {
            case "1": // TOT
                fieldName = "testTypeIndex.TOT";
                break;
            case "2": // OTA
                fieldName = "testTypeIndex.OTA";
                break;
            case "4": // SIGNED
                fieldName = "testTypeIndex.SIGNED";
                break;
            default:
                fieldName = "testTypeIndex.TOT";
                break;
        }
        return fieldName;
    }
}
