<%--
  ~ Copyright (c) 2018 Google Inc. All Rights Reserved.
  ~
  ~ Licensed under the Apache License, Version 2.0 (the "License"); you
  ~ may not use this file except in compliance with the License. You may
  ~ obtain a copy of the License at
  ~
  ~     http://www.apache.org/licenses/LICENSE-2.0
  ~
  ~ Unless required by applicable law or agreed to in writing, software
  ~ distributed under the License is distributed on an "AS IS" BASIS,
  ~ WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
  ~ implied. See the License for the specific language governing
  ~ permissions and limitations under the License.
  --%>
<%@ page contentType='text/html;charset=UTF-8' language='java' %>
<%@ taglib prefix='fn' uri='http://java.sun.com/jsp/jstl/functions' %>
<%@ taglib prefix='c' uri='http://java.sun.com/jsp/jstl/core'%>
<%@ taglib prefix='fmt' uri='http://java.sun.com/jsp/jstl/fmt'%>
<jsp:useBean id="startDateObject" class="java.util.Date"/>
<jsp:useBean id="endDateObject" class="java.util.Date"/>
<c:set var="timeZone" value="America/Los_Angeles"/>

<html>
  <%@ include file='header.jsp' %>
  <link type='text/css' href='/css/show_test_runs_common.css' rel='stylesheet'>
  <link type='text/css' href='/css/test_results.css' rel='stylesheet'>
  <link rel='stylesheet' href='/css/search_header.css'>
  <script type='text/javascript'>
      $(document).ready(function() {

          $.deparam = $.deparam || function(uri){
              if(uri === undefined){
                  uri = window.location.search;
              }
              var queryString = {};
              uri.replace(
                  new RegExp(
                      "([^?=&]+)(=([^&#]*))?", "g"),
                  function($0, $1, $2, $3) { queryString[$1] = $3; }
              );
              return queryString;
          };

          $("li.tab").each(function( index ) {
              $(this).click(function() {
                  window.open($(this).children().attr("href"), '_self');
              });
          });

          $(".search-icon-wrapper").click(function() {
              $(".search-wrapper").slideToggle("fast", function() {
                  // Animation complete.
              });
          });

          <c:if test="${not empty branch or not empty hostName or not empty buildId or not empty deviceName}">
          $(".search-wrapper").slideToggle("fast");
          </c:if>

          $("#searchBtn").click(function(event) {
              event.preventDefault();

              var url = '<c:out value="${requestScope['javax.servlet.forward.servlet_path']}" escapeXml="false"></c:out>';
              var params = $.deparam('<c:out value="${requestScope['javax.servlet.forward.query_string']}" escapeXml="false"></c:out>');

              var branch = $("#deviceBranch").val().trim();
              if (  branch.length > 0 ) {
                  params['branch'] = branch;
              } else {
                  delete params['branch'];
              }
              var host = $("#host").val().trim();
              if ( host.length > 0 ) {
                  params['hostName'] = host;
              } else {
                  delete params['hostName'];
              }
              var buildId = $("#deviceBuildId").val().trim();
              if ( buildId.length > 0 ) {
                  params['buildId'] = buildId;
              } else {
                  delete params['buildId'];
              }
              var deviceName = $("#deviceName").val().trim();
              if ( deviceName.length > 0 ) {
                params['deviceName'] = deviceName;
              } else {
                delete params['deviceName'];
              }

              $(location).prop('href', url + "?" + decodeURIComponent($.param(params)));
              $(this).prop('href', url);
          });

          $("#deviceBranch").autocomplete({
              source: [ "c++", "java", "php", "coldfusion", "javascript", "asp", "ruby" ],
              open: function( event, ui ) {
                  alert("open")
              },
              close: function( event, ui ) {
                  alert("close")
              }
          });

          $( "div.test-case-container > div.input-field > a.btn" ).each(function() {
              $( this ).click(function() {
                $(this).parent().prev().children().select();
                document.execCommand('copy');
                alert("Reproduce Command copied to clipboard.");
              });
          });
      });
  </script>
  <body>
    <div class='wide container'>

        <div class="row card search-bar expanded">
            <div class="header-wrapper">
                <h5 class="section-header">
                    <b>Plan: </b><span><c:out value="${plan}"></c:out></span>
                </h5>
                <div class="search-icon-wrapper">
                    <i class="material-icons">search</i>
                </div>
            </div>
            <div class="search-wrapper" style="display: none">
                <div class="col s12">
                    <div class="input-field col s4">
                        <input id="deviceBranch" type="text" value="<c:out value="${branch}"></c:out>" autocomplete="off" />
                        <label>Device Branch</label>
                    </div>
                    <div class="input-field col s4">
                        <input id="host" type="text" value="<c:out value="${hostName}"></c:out>" autocomplete="off" />
                        <label>Host</label>
                    </div>
                    <div class="input-field col s4">
                        <input id="deviceBuildId" type="text" value="<c:out value="${buildId}"></c:out>" autocomplete="off" />
                        <label>Device Build ID</label>
                    </div>
                </div>
                <div class="col s12">
                    <div class="input-field col s4">
                        <input id="deviceName" type="text" value="<c:out value="${deviceName}"></c:out>" autocomplete="off" />
                        <label>Device Name</label>
                    </div>
                    <div class="input-field col s4">

                    </div>
                    <div class="run-type-wrapper col s4 right-align">
                        <a class="waves-effect waves-light btn" id="searchBtn">
                            <i class="material-icons left">search</i>Apply
                        </a>
                    </div>
                </div>
            </div>
        </div>

        <div class='row'>
            <div class='col s12'>

                <ul class="tabs z-depth-1">
                    <li class="tab col s4" id="totTabLink">
                        <a class="<c:out value="${testCategoryType == '1' ? 'active' : 'inactive'}"></c:out>" href="${requestScope['javax.servlet.forward.servlet_path']}?plan=${plan}&type=${testType}&testCategoryType=1">TOT</a>
                    </li>
                    <li class="tab col s4" id="signedTabLink">
                        <a class="<c:out value="${testCategoryType == '4' ? 'active' : 'inactive'}"></c:out>" href="${requestScope['javax.servlet.forward.servlet_path']}?plan=${plan}&type=${testType}&testCategoryType=4">SIGNED</a>
                    </li>
                    <li class="tab col s4" id="otaTabLink">
                        <a class="<c:out value="${testCategoryType == '2' ? 'active' : 'inactive'}"></c:out>" href="${requestScope['javax.servlet.forward.servlet_path']}?plan=${plan}&type=${testType}&testCategoryType=2">OTA</a>
                    </li>
                </ul>

            </div>
        </div>

      <div class='row' id='test-suite-green-release-container'>
        <div class="col s12">

            <ul class="collapsible popout test-runs">
                <c:forEach var="testSuiteResultEntity" items="${testSuiteResultEntityPagination.list}">
                    <li class="test-run-container">
                    <div class="collapsible-header test-run">
                        <div class="row" style="margin-bottom: 0px; line-height: 30px;">
                            <div class="col s9">
                                <b><c:out value="${testSuiteResultEntity.branch}"></c:out>/<c:out value="${testSuiteResultEntity.target}"></c:out> (<c:out value="${testSuiteResultEntity.buildId}"></c:out>)</b>
                            </div>
                            <div class="col s3">
                                <span class="indicator right center
                                <c:choose>
                                    <c:when test="${testSuiteResultEntity.passedTestCaseCount eq 0 and testSuiteResultEntity.failedTestCaseCount eq 0}">
                                        black
                                    </c:when>
                                    <c:otherwise>
                                        green
                                    </c:otherwise>
                                </c:choose>
                                ">
                                    <c:out value="${testSuiteResultEntity.passedTestCaseCount}"></c:out>/<c:out value="${testSuiteResultEntity.passedTestCaseCount + testSuiteResultEntity.failedTestCaseCount}"></c:out>
                                    (
                                    <c:set var="integerVal" value="${fn:substringBefore(testSuiteResultEntity.passedTestCaseRatio, '.')}" />
                                    <c:choose>
                                        <c:when test="${integerVal eq '100'}">
                                            100%
                                        </c:when>
                                        <c:otherwise>
                                            <c:set var="decimalVal" value="${fn:substring(fn:substringAfter(testSuiteResultEntity.passedTestCaseRatio, '.'), 0, 2)}" />
                                            <c:choose>
                                                <c:when test="${decimalVal eq '00'}">
                                                    <c:out value="${integerVal}"></c:out>%
                                                </c:when>
                                                <c:otherwise>
                                                    <c:out value="${integerVal}"></c:out>.<c:out value="${decimalVal}"></c:out>%
                                                </c:otherwise>
                                            </c:choose>
                                        </c:otherwise>
                                    </c:choose>
                                    )
                                </span>
                                <c:if test="${!testSuiteResultEntity.bootSuccess}">
                                <span class="indicator right center" style="min-width: 0px; padding: 0 2px;"></span>
                                <span class="indicator right center red">Boot Error</span>
                                </c:if>
                            </div>
                            <div class="col s5">
                                <span class="suite-test-run-metadata">
                                    <b>Suite Build Number: </b><c:out value="${testSuiteResultEntity.suiteBuildNumber}"></c:out><br>
                                    <b>Device Name: </b><c:out value="${testSuiteResultEntity.deviceName}"></c:out><br>
                                </span>
                            </div>
                            <div class="col s5">
                                <span class="suite-test-run-metadata">
                                    <b>Host: </b><c:out value="${testSuiteResultEntity.hostName}"></c:out><br>
                                    <b>Modules: </b><c:out value="${testSuiteResultEntity.modulesDone}"></c:out>/<c:out value="${testSuiteResultEntity.modulesTotal}"></c:out><br>
                                </span>
                            </div>
                            <div class="col s2" style="padding: 5px 12px;">
                                <a href="<c:out value="${testSuiteResultEntity.buganizerLink}"></c:out>" class="waves-effect waves-light btn right blue-grey" style="padding: 0 15px;" target="_blank">
                                    Buganizer
                                </a>
                            </div>
                            <div class="col s12">
                                <span class="suite-test-run-metadata">
                                    <b>Result Log Path: </b>
                                    <a href="show_gcs_log?path=${testSuiteResultEntity.screenResultLogPath}">
                                        <c:out value="${testSuiteResultEntity.screenResultLogPath}"></c:out>
                                    </a>
                                    <br>
                                    <b>Infra Log Path: </b>
                                    <a href="show_gcs_log/download?file=${testSuiteResultEntity.screenInfraLogPath}">
                                        <c:out value="${testSuiteResultEntity.screenInfraLogPath}"></c:out>
                                    </a>
                                    <br>
                                </span>
                            </div>
                            <div class="col s10">
                                <span style="font-size: 13px;">
                                <jsp:setProperty name="startDateObject" property="time" value="${testSuiteResultEntity.startTime}"/>
                                <jsp:setProperty name="endDateObject" property="time" value="${testSuiteResultEntity.endTime}"/>
                                <fmt:formatDate value="${startDateObject}" pattern="yyyy-MM-dd HH:mm:ss" timeZone="${timeZone}" /> - <fmt:formatDate value="${endDateObject}" pattern="yyyy-MM-dd HH:mm:ss z" timeZone="${timeZone}" />
                                <c:set var="executionTime" scope="page" value="${(testSuiteResultEntity.endTime - testSuiteResultEntity.startTime) / 1000}"/>
                                (<c:out value="${executionTime}"></c:out>s)
                                </span>
                            </div>
                            <div class="col s2">
                                <i class="material-icons expand-arrow">expand_more</i>
                            </div>
                        </div>
                    </div>
                        <div class="collapsible-body test-results row" style="display: none;">
                            <div class="col test-col grey lighten-5 s12 left-most right-most">
                                <h5 class="test-result-label white" style="text-transform: capitalize;">
                                    Vendor Fingerprint
                                </h5>
                                <div class="test-case-container">
                                    <c:out value="${testSuiteResultEntity.buildVendorFingerprint}"></c:out>
                                </div>
                            </div>
                            <div class="col test-col grey lighten-5 s12 left-most right-most">
                                <h5 class="test-result-label white" style="text-transform: capitalize;">
                                    System Fingerprint
                                </h5>
                                <div class="test-case-container">
                                    <c:out value="${testSuiteResultEntity.buildSystemFingerprint}"></c:out>
                                </div>
                            </div>
                            <div class="col test-col grey lighten-5 s12 left-most right-most">
                                <h5 class="test-result-label white" style="text-transform: capitalize;">
                                    Reproduce Command
                                </h5>
                                <div class="row test-case-container">
                                    <div class="input-field col s9">
                                        <input type="text" class="validate" readonly="true" onclick="this.select()" value="reproduce --report_path=gs://vts-report/<c:out value="${testSuiteResultEntity.getTestSuiteFileEntityKey().getName()}"></c:out> --suite=<c:out value="${fn:toLowerCase(testSuiteResultEntity.getSuiteName())}"></c:out>" />
                                    </div>
                                    <div class="input-field col s3">
                                        <a class="waves-effect waves-light btn right"><i class="material-icons left">content_copy</i>Copy</a>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </li>
                </c:forEach>
            </ul>

        </div>
      </div>

      <div class="row">
          <c:set var="searchQueryString" value="" />
          <c:if test="${not empty branch}">
              <c:set var="searchQueryString" value="${searchQueryString}&branch=${branch}" />
          </c:if>
          <c:if test="${not empty hostName}">
              <c:set var="searchQueryString" value="${searchQueryString}&hostName=${hostName}" />
          </c:if>
          <c:if test="${not empty buildId}">
              <c:set var="searchQueryString" value="${searchQueryString}&buildId=${buildId}" />
          </c:if>
          <c:if test="${not empty deviceName}">
              <c:set var="searchQueryString" value="${searchQueryString}&deviceName=${deviceName}" />
          </c:if>
        <div class="col s12 center-align">
          <ul class="pagination">
            <c:choose>
                <c:when test="${testSuiteResultEntityPagination.minPageRange gt testSuiteResultEntityPagination.pageSize}">
                    <li class="waves-effect">
                        <a href="${requestScope['javax.servlet.forward.servlet_path']}?plan=${plan}&type=${testType}&testCategoryType=${testCategoryType}&page=${testSuiteResultEntityPagination.minPageRange - 1}&nextPageToken=${testSuiteResultEntityPagination.previousPageCountToken}${searchQueryString}">
                            <i class="material-icons">chevron_left</i>
                        </a>
                    </li>
                </c:when>
                <c:otherwise>

                </c:otherwise>
            </c:choose>
            <c:forEach var="pageLoop" begin="${testSuiteResultEntityPagination.minPageRange}" end="${testSuiteResultEntityPagination.maxPageRange}">
              <li class="waves-effect<c:if test="${pageLoop eq page}"> active</c:if>">
                  <a href="${requestScope['javax.servlet.forward.servlet_path']}?plan=${plan}&type=${testType}&testCategoryType=${testCategoryType}&page=${pageLoop}<c:if test="${testSuiteResultEntityPagination.currentPageCountToken ne ''}">&nextPageToken=${testSuiteResultEntityPagination.currentPageCountToken}</c:if>${searchQueryString}">
                      <c:out value="${pageLoop}" />
                  </a>
              </li>
            </c:forEach>
            <c:choose>
                <c:when test="${testSuiteResultEntityPagination.maxPages gt testSuiteResultEntityPagination.pageSize}">
                    <li class="waves-effect">
                        <a href="${requestScope['javax.servlet.forward.servlet_path']}?plan=${plan}&type=${testType}&testCategoryType=${testCategoryType}&page=${testSuiteResultEntityPagination.maxPageRange + 1}&nextPageToken=${testSuiteResultEntityPagination.nextPageCountToken}${searchQueryString}">
                            <i class="material-icons">chevron_right</i>
                        </a>
                    </li>
                </c:when>
                <c:otherwise>

                </c:otherwise>
            </c:choose>
          </ul>
        </div>
      </div>

    </div>
    <%@ include file='footer.jsp' %>
  </body>
</html>
