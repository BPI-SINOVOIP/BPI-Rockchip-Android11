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

package com.android.vts.servlet;

import static com.googlecode.objectify.ObjectifyService.ofy;

import com.android.vts.entity.CoverageEntity;
import com.android.vts.entity.RoleEntity;
import com.android.vts.entity.TestEntity;
import com.android.vts.entity.TestRunEntity;
import com.android.vts.entity.UserEntity;
import com.google.appengine.api.datastore.DatastoreService;
import com.google.appengine.api.datastore.DatastoreServiceFactory;
import com.google.appengine.api.datastore.Entity;
import com.google.appengine.api.datastore.Key;
import com.google.appengine.api.datastore.KeyFactory;
import com.google.appengine.api.datastore.Query;
import com.google.gson.Gson;
import com.googlecode.objectify.Ref;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.logging.Level;
import javax.servlet.RequestDispatcher;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

/**
 * Servlet for handling requests to show code coverage.
 */
public class ShowCoverageServlet extends BaseServlet {

  private static final String COVERAGE_JSP = "WEB-INF/jsp/show_coverage.jsp";
  private static final String TREE_JSP = "WEB-INF/jsp/show_tree.jsp";

  @Override
  public PageType getNavParentType() {
    return PageType.TOT;
  }

  @Override
  public List<Page> getBreadcrumbLinks(HttpServletRequest request) {
    List<Page> links = new ArrayList<>();
    String testName = request.getParameter("testName");
    links.add(new Page(PageType.TABLE, testName, "?testName=" + testName));

    String startTime = request.getParameter("startTime");
    links.add(new Page(PageType.COVERAGE, "?testName=" + testName + "&startTime=" + startTime));
    return links;
  }

  @Override
  public void doGetHandler(HttpServletRequest request, HttpServletResponse response)
      throws IOException {
    RequestDispatcher dispatcher = null;

    String test = request.getParameter("testName");
    String timeString = request.getParameter("startTime");

    Boolean isModerator = false;
    String currentUserEmail = request.getAttribute("email").toString();
    Optional<UserEntity> userEntityOptional = Optional
        .ofNullable(UserEntity.getUser(currentUserEmail));
    if (userEntityOptional.isPresent()) {
      Ref refRole = Ref.create(RoleEntity.getRole("coverage-moderator"));
      isModerator = userEntityOptional.get().getRoles().contains(refRole);
    }

    // Process the time key requested
    long time = -1;
    try {
      time = Long.parseLong(timeString);
    } catch (NumberFormatException e) {
      request.setAttribute("testName", test);
      dispatcher = request.getRequestDispatcher(TREE_JSP);
      return;
    }

    com.googlecode.objectify.Key testKey = com.googlecode.objectify.Key
        .create(TestEntity.class, test);
    com.googlecode.objectify.Key testRunKey = com.googlecode.objectify.Key
        .create(testKey, TestRunEntity.class, time);

    List<CoverageEntity> coverageEntityList = ofy().load().type(CoverageEntity.class)
        .ancestor(testRunKey).list();

    Collections.sort(coverageEntityList, CoverageEntity.isIgnoredComparator);

    request.setAttribute("isModerator", isModerator);
    request.setAttribute("testName", request.getParameter("testName"));
    request.setAttribute("gerritURI", new Gson().toJson(GERRIT_URI));
    request.setAttribute("gerritScope", new Gson().toJson(GERRIT_SCOPE));
    request.setAttribute("clientId", new Gson().toJson(CLIENT_ID));
    request.setAttribute("startTime", request.getParameter("startTime"));
    request.setAttribute("coverageEntityList", coverageEntityList);

    dispatcher = request.getRequestDispatcher(COVERAGE_JSP);
    try {
      dispatcher.forward(request, response);
    } catch (ServletException e) {
      logger.log(Level.SEVERE, "Servlet Exception caught : ", e);
    }
  }
}
