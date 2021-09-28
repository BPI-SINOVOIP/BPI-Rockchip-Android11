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

import com.android.vts.entity.CodeCoverageEntity;
import com.android.vts.entity.DeviceInfoEntity;
import com.android.vts.entity.TestCoverageStatusEntity;
import com.android.vts.entity.TestEntity;
import com.android.vts.entity.TestRunEntity;

import com.android.vts.proto.VtsReportMessage;
import com.android.vts.util.FilterUtil;
import com.google.cloud.datastore.Datastore;
import com.google.cloud.datastore.DatastoreOptions;
import com.google.cloud.datastore.PathElement;
import com.google.cloud.datastore.StructuredQuery.CompositeFilter;
import com.google.cloud.datastore.StructuredQuery.Filter;
import com.google.cloud.datastore.StructuredQuery.PropertyFilter;
import com.google.gson.Gson;
import com.google.visualization.datasource.DataSourceHelper;
import com.google.visualization.datasource.DataSourceRequest;
import com.google.visualization.datasource.base.DataSourceException;
import com.google.visualization.datasource.base.ReasonType;
import com.google.visualization.datasource.base.ResponseStatus;
import com.google.visualization.datasource.base.StatusType;
import com.google.visualization.datasource.base.TypeMismatchException;
import com.google.visualization.datasource.datatable.ColumnDescription;
import com.google.visualization.datasource.datatable.DataTable;
import com.google.visualization.datasource.datatable.TableRow;
import com.google.visualization.datasource.datatable.value.DateTimeValue;
import com.google.visualization.datasource.datatable.value.NumberValue;
import com.google.visualization.datasource.datatable.value.ValueType;
import com.googlecode.objectify.Key;
import com.ibm.icu.util.GregorianCalendar;
import com.ibm.icu.util.TimeZone;
import java.io.IOException;
import java.math.BigDecimal;
import java.math.RoundingMode;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Calendar;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.function.Predicate;
import java.util.logging.Level;
import java.util.stream.Collectors;
import java.util.stream.Stream;
import javax.servlet.RequestDispatcher;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.joda.time.format.DateTimeFormat;
import org.joda.time.format.DateTimeFormatter;

import static com.googlecode.objectify.ObjectifyService.ofy;

/** Represents the servlet that is invoked on loading the coverage overview page. */
public class ShowCoverageOverviewServlet extends BaseServlet {

    @Override
    public PageType getNavParentType() {
        return PageType.COVERAGE_OVERVIEW;
    }

    @Override
    public List<Page> getBreadcrumbLinks(HttpServletRequest request) {
        return null;
    }

    @Override
    public void doGetHandler(HttpServletRequest request, HttpServletResponse response)
            throws IOException {

        String pageType =
                request.getParameter("pageType") == null
                        ? "html"
                        : request.getParameter("pageType");

        RequestDispatcher dispatcher;
        if (pageType.equalsIgnoreCase("html")) {
            dispatcher = this.getCoverageDispatcher(request, response);
            try {
                request.setAttribute("pageType", pageType);
                response.setStatus(HttpServletResponse.SC_OK);
                dispatcher.forward(request, response);
            } catch (ServletException e) {
                logger.log(Level.SEVERE, "Servlet Exception caught : ", e);
            }
        } else {

            String testName = request.getParameter("testName");

            DataTable data = getCoverageDataTable(testName);
            DataSourceRequest dsRequest = null;

            try {
                // Extract the datasource request parameters.
                dsRequest = new DataSourceRequest(request);

                // NOTE: If you want to work in restricted mode, which means that only
                // requests from the same domain can access the data source, uncomment the following
                // call.
                //
                // DataSourceHelper.verifyAccessApproved(dsRequest);

                // Apply the query to the data table.
                DataTable newData =
                        DataSourceHelper.applyQuery(
                                dsRequest.getQuery(), data, dsRequest.getUserLocale());

                // Set the response.
                DataSourceHelper.setServletResponse(newData, dsRequest, response);
            } catch (RuntimeException rte) {
                logger.log(Level.SEVERE, "A runtime exception has occured", rte);
                ResponseStatus status =
                        new ResponseStatus(
                                StatusType.ERROR, ReasonType.INTERNAL_ERROR, rte.getMessage());
                if (dsRequest == null) {
                    dsRequest = DataSourceRequest.getDefaultDataSourceRequest(request);
                }
                DataSourceHelper.setServletErrorResponse(status, dsRequest, response);
            } catch (DataSourceException e) {
                if (dsRequest != null) {
                    DataSourceHelper.setServletErrorResponse(e, dsRequest, response);
                } else {
                    DataSourceHelper.setServletErrorResponse(e, request, response);
                }
            }
        }
    }

    private List<Key<TestRunEntity>> getTestRunEntityKeyList(
            List<TestCoverageStatusEntity> testCoverageStatusEntityList) {
        return testCoverageStatusEntityList.stream()
                .map(
                        testCoverageStatusEntity -> {
                            com.googlecode.objectify.Key testKey =
                                    com.googlecode.objectify.Key.create(
                                            TestEntity.class,
                                            testCoverageStatusEntity.getTestName());
                            return com.googlecode.objectify.Key.create(
                                    testKey,
                                    TestRunEntity.class,
                                    testCoverageStatusEntity.getUpdatedTimestamp());
                        })
                .collect(Collectors.toList());
    }

    private Predicate<DeviceInfoEntity> isBranchAndDevice(String branch, String device) {
        return d -> d.getBranch().equals(branch) && d.getBuildFlavor().equals(device);
    }

    private Predicate<DeviceInfoEntity> isBranch(String branch) {
        return d -> d.getBranch().equals(branch);
    }

    private Predicate<DeviceInfoEntity> isDevice(String device) {
        return d -> d.getBuildFlavor().equals(device);
    }

    private RequestDispatcher getCoverageDispatcher(
            HttpServletRequest request, HttpServletResponse response) {

        String COVERAGE_OVERVIEW_JSP = "WEB-INF/jsp/show_coverage_overview.jsp";

        RequestDispatcher dispatcher = null;
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

        // Add test names to list
        List<String> resultNames =
                Arrays.stream(VtsReportMessage.TestCaseResult.values())
                        .map(testCaseResult -> testCaseResult.name())
                        .collect(Collectors.toList());

        Map<String, String[]> parameterMap = request.getParameterMap();

        List<TestCoverageStatusEntity> testCoverageStatusEntityList =
                TestCoverageStatusEntity.getAllTestCoverage();

        List<com.googlecode.objectify.Key<TestRunEntity>> testRunEntityKeyList = new ArrayList<>();

        if (Objects.nonNull(parameterMap.get("branch"))
                || Objects.nonNull(parameterMap.get("device"))) {
            List<com.googlecode.objectify.Key<DeviceInfoEntity>> deviceInfoEntityKeyList =
                    TestCoverageStatusEntity.getDeviceInfoEntityKeyList(
                            testCoverageStatusEntityList);

            Collection<DeviceInfoEntity> deviceInfoEntityMap =
                    ofy().load().keys(() -> deviceInfoEntityKeyList.iterator()).values();

            Stream<DeviceInfoEntity> deviceInfoEntityStream = Stream.empty();
            if (Objects.nonNull(parameterMap.get("branch"))
                    && Objects.nonNull(parameterMap.get("device"))) {
                String branch = parameterMap.get("branch")[0];
                String device = parameterMap.get("device")[0];
                deviceInfoEntityStream =
                        deviceInfoEntityMap.stream().filter(isBranchAndDevice(branch, device));
            } else if (Objects.nonNull(parameterMap.get("branch"))) {
                String branch = parameterMap.get("branch")[0];
                deviceInfoEntityStream = deviceInfoEntityMap.stream().filter(isBranch(branch));
            } else if (Objects.nonNull(parameterMap.get("device"))) {
                String device = parameterMap.get("device")[0];
                deviceInfoEntityStream = deviceInfoEntityMap.stream().filter(isDevice(device));
            } else {
                logger.log(Level.WARNING, "unmet search condition!");
            }
            testRunEntityKeyList =
                    deviceInfoEntityStream
                            .map(
                                    deviceInfoEntity -> {
                                        com.googlecode.objectify.Key testKey =
                                                com.googlecode.objectify.Key.create(
                                                        TestEntity.class,
                                                        deviceInfoEntity
                                                                .getParent()
                                                                .getParent()
                                                                .getName());
                                        return com.googlecode.objectify.Key.create(
                                                testKey,
                                                TestRunEntity.class,
                                                deviceInfoEntity.getParent().getId());
                                    })
                            .collect(Collectors.toList());
            logger.log(Level.INFO, "testRunEntityKeyList size => " + testRunEntityKeyList.size());
        } else {
            testRunEntityKeyList = this.getTestRunEntityKeyList(testCoverageStatusEntityList);
        }
        Iterator<Key<TestRunEntity>> testRunEntityKeyIterator = testRunEntityKeyList.iterator();

        Map<Key<TestRunEntity>, TestRunEntity> keyTestRunEntityMap =
                ofy().load().keys(() -> testRunEntityKeyIterator);

        List<com.googlecode.objectify.Key<CodeCoverageEntity>> codeCoverageEntityKeyList =
                new ArrayList<>();
        Map<Long, TestRunEntity> testRunEntityMap = new HashMap<>();
        for (Map.Entry<com.googlecode.objectify.Key<TestRunEntity>, TestRunEntity> entry :
                keyTestRunEntityMap.entrySet()) {
            com.googlecode.objectify.Key codeCoverageEntityKey =
                    com.googlecode.objectify.Key.create(
                            entry.getKey(), CodeCoverageEntity.class, entry.getValue().getId());
            codeCoverageEntityKeyList.add(codeCoverageEntityKey);
            testRunEntityMap.put(entry.getValue().getId(), entry.getValue());
        }

        Map<com.googlecode.objectify.Key<CodeCoverageEntity>, CodeCoverageEntity>
                keyCodeCoverageEntityMap =
                        ofy().load().keys(() -> codeCoverageEntityKeyList.iterator());

        Map<Long, CodeCoverageEntity> codeCoverageEntityMap = new HashMap<>();
        for (Map.Entry<com.googlecode.objectify.Key<CodeCoverageEntity>, CodeCoverageEntity> entry :
                keyCodeCoverageEntityMap.entrySet()) {
            codeCoverageEntityMap.put(entry.getValue().getId(), entry.getValue());
        }

        int coveredLines = 0;
        int uncoveredLines = 0;
        int passCount = 0;
        int failCount = 0;
        for (Map.Entry<Long, CodeCoverageEntity> entry : codeCoverageEntityMap.entrySet()) {
            TestRunEntity testRunEntity = testRunEntityMap.get(entry.getKey());

            CodeCoverageEntity codeCoverageEntity = entry.getValue();

            coveredLines += codeCoverageEntity.getCoveredLineCount();
            uncoveredLines +=
                    codeCoverageEntity.getTotalLineCount()
                            - codeCoverageEntity.getCoveredLineCount();
            passCount += testRunEntity.getPassCount();
            failCount += testRunEntity.getFailCount();
        }

        FilterUtil.setAttributes(request, parameterMap);

        int[] testStats = new int[VtsReportMessage.TestCaseResult.values().length];
        testStats[VtsReportMessage.TestCaseResult.TEST_CASE_RESULT_PASS.getNumber()] = passCount;
        testStats[VtsReportMessage.TestCaseResult.TEST_CASE_RESULT_FAIL.getNumber()] = failCount;

        response.setStatus(HttpServletResponse.SC_OK);
        request.setAttribute("resultNames", resultNames);
        request.setAttribute("resultNamesJson", new Gson().toJson(resultNames));
        request.setAttribute("testRunEntityList", testRunEntityMap.values());
        request.setAttribute("codeCoverageEntityMap", codeCoverageEntityMap);
        request.setAttribute("coveredLines", new Gson().toJson(coveredLines));
        request.setAttribute("uncoveredLines", new Gson().toJson(uncoveredLines));
        request.setAttribute("testStats", new Gson().toJson(testStats));

        request.setAttribute("unfiltered", unfiltered);
        request.setAttribute("showPresubmit", showPresubmit);
        request.setAttribute("showPostsubmit", showPostsubmit);

        request.setAttribute(
                "deviceOptions",
                TestCoverageStatusEntity.getDeviceSet(testCoverageStatusEntityList));
        request.setAttribute(
                "branchOptions",
                TestCoverageStatusEntity.getBranchSet(testCoverageStatusEntityList));
        dispatcher = request.getRequestDispatcher(COVERAGE_OVERVIEW_JSP);
        return dispatcher;
    }

    private DataTable getCoverageDataTable(String testName) {

        Datastore datastore = DatastoreOptions.getDefaultInstance().getService();

        DataTable dataTable = new DataTable();
        ArrayList<ColumnDescription> cd = new ArrayList<>();
        ColumnDescription startDate =
                new ColumnDescription("startDate", ValueType.DATETIME, "Date");
        startDate.setPattern("yyyy-MM-dd");
        cd.add(startDate);
        cd.add(
                new ColumnDescription(
                        "coveredLineCount", ValueType.NUMBER, "Covered Source Code Line Count"));
        cd.add(
                new ColumnDescription(
                        "totalLineCount", ValueType.NUMBER, "Total Source Code Line Count"));
        cd.add(new ColumnDescription("percentage", ValueType.NUMBER, "Coverage Ratio (%)"));

        dataTable.addColumns(cd);

        Calendar cal = Calendar.getInstance();
        cal.add(Calendar.MONTH, -6);
        Long startTime = cal.getTime().getTime() * 1000;
        Long endTime = Calendar.getInstance().getTime().getTime() * 1000;

        com.google.cloud.datastore.Key startKey =
                datastore
                        .newKeyFactory()
                        .setKind(TestRunEntity.KIND)
                        .addAncestors(
                                PathElement.of(TestEntity.KIND, testName),
                                PathElement.of(TestRunEntity.KIND, startTime))
                        .newKey(startTime);

        com.google.cloud.datastore.Key endKey =
                datastore
                        .newKeyFactory()
                        .setKind(TestRunEntity.KIND)
                        .addAncestors(
                                PathElement.of(TestEntity.KIND, testName),
                                PathElement.of(TestRunEntity.KIND, endTime))
                        .newKey(endTime);

        Filter codeCoverageFilter =
                CompositeFilter.and(
                        PropertyFilter.lt("__key__", endKey),
                        PropertyFilter.gt("__key__", startKey));

        List<CodeCoverageEntity> codeCoverageEntityList =
                ofy().load()
                        .type(CodeCoverageEntity.class)
                        .filter(codeCoverageFilter)
                        .limit(10)
                        .list();

        DateTimeFormatter dateTimeFormatter = DateTimeFormat.forPattern("yyyy-MM-dd");
        Map<String, List<CodeCoverageEntity>> codeCoverageEntityListMap =
                codeCoverageEntityList
                        .stream()
                        .collect(
                                Collectors.groupingBy(
                                        v -> dateTimeFormatter.print(v.getId() / 1000)));

        codeCoverageEntityListMap.forEach(
                (key, entityList) -> {
                    GregorianCalendar gCal = new GregorianCalendar();
                    gCal.setTimeZone(TimeZone.getTimeZone("GMT"));
                    gCal.setTimeInMillis(entityList.get(0).getId() / 1000);

                    Long sumCoveredLine =
                            entityList.stream().mapToLong(val -> val.getCoveredLineCount()).sum();
                    Long sumTotalLine =
                            entityList.stream().mapToLong(val -> val.getTotalLineCount()).sum();
                    float percentage = 0;
                    if (sumTotalLine > 0) {
                        BigDecimal coveredLineNum = new BigDecimal(sumCoveredLine);
                        BigDecimal totalLineNum = new BigDecimal(sumTotalLine);
                        BigDecimal totalPercent = new BigDecimal(100);
                        percentage =
                                coveredLineNum
                                        .multiply(totalPercent)
                                        .divide(totalLineNum, 2, RoundingMode.HALF_DOWN)
                                        .floatValue();
                    }

                    TableRow tableRow = new TableRow();
                    tableRow.addCell(new DateTimeValue(gCal));
                    tableRow.addCell(new NumberValue(sumCoveredLine));
                    tableRow.addCell(new NumberValue(sumTotalLine));
                    tableRow.addCell(new NumberValue(percentage));
                    try {
                        dataTable.addRow(tableRow);
                    } catch (TypeMismatchException e) {
                        logger.log(Level.WARNING, "Invalid type! ");
                    }
                });

        return dataTable;
    }
}
