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

import com.android.vts.entity.BranchEntity;
import com.android.vts.entity.TestCoverageStatusEntity;
import com.google.gson.Gson;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.util.List;
import java.util.Objects;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.android.vts.entity.BuildTargetEntity;

/** REST endpoint for getting all branch and buildFlavors information. */
public class DataRestServlet extends BaseApiServlet {

    private static final Logger logger = Logger.getLogger(DataRestServlet.class.getName());

    @Override
    public void doGet(HttpServletRequest request, HttpServletResponse response) throws IOException {
        String pathInfo = request.getPathInfo();
        String json = "";
        if (Objects.nonNull(pathInfo)) {
            String schKey =
                    Objects.isNull(request.getParameter("schKey"))
                            ? ""
                            : request.getParameter("schKey");
            if (pathInfo.equalsIgnoreCase("/branch")) {
                json = new Gson().toJson(BranchEntity.getByBranch(schKey));
            } else if (pathInfo.equalsIgnoreCase("/device")) {
                json = new Gson().toJson(BuildTargetEntity.getByBuildTarget(schKey));
            } else if (pathInfo.startsWith("/code/coverage/status/")) {
                List<TestCoverageStatusEntity> testCoverageStatusEntityList =
                        TestCoverageStatusEntity.getAllTestCoverage();
                if (pathInfo.endsWith("branch")) {
                    json =
                            new Gson()
                                    .toJson(
                                            TestCoverageStatusEntity.getBranchSet(
                                                    testCoverageStatusEntityList));
                } else {
                    json =
                            new Gson()
                                    .toJson(
                                            TestCoverageStatusEntity.getDeviceSet(
                                                    testCoverageStatusEntityList));
                }
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
}
