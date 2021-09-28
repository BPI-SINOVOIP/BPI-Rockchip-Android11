<%--
  ~ Copyright (c) 2017 Google Inc. All Rights Reserved.
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
<%@ taglib prefix='c' uri='http://java.sun.com/jsp/jstl/core' %>
<%@ taglib prefix='fmt' uri='http://java.sun.com/jsp/jstl/fmt' %>

<html>
  <!-- <link rel='stylesheet' href='/css/dashboard_main.css'> -->
  <%@ include file='header.jsp' %>
  <link type='text/css' href='/css/show_test_runs_common.css' rel='stylesheet'>
  <link type='text/css' href='/css/test_results.css' rel='stylesheet'>
  <link rel='stylesheet' href='/css/search_header.css'>
  <script type='text/javascript' src='https://www.gstatic.com/charts/loader.js'></script>
  <script src='https://www.gstatic.com/external_hosted/moment/min/moment-with-locales.min.js'></script>
  <script src='js/common.js'></script>
  <script src='js/time.js'></script>
  <script type='text/javascript'>
      google.charts.load('current', {'packages':['table', 'corechart']});
      google.charts.setOnLoadCallback(drawStatsChart);
      google.charts.setOnLoadCallback(drawCoverageCharts);

      $(document).ready(function() {

          $('select').material_select();

          $(".search-icon-wrapper").click(function() {
              $(".search-wrapper").toggle();
          });

          var inputIdList = ["device", "branch"];

          $("#schBtn").click(function (evt) {
              if($(this).hasClass('disabled')) return;
              var queryParam = "?";
              $.each(inputIdList, function( index, value ) {
                  var selectId = value.charAt(0).toUpperCase() + value.slice(1)
                  var result = $("#search" + selectId).val();
                  if ( !$.isEmptyObject(result) ) {
                      queryParam += value + "=" + result.trim() + "&";
                  }
              });
              var link = '${pageContext.request.contextPath}' + '/show_coverage_overview' + queryParam;
              window.open(link, '_self');
          });

          var no_data_msg = "NO DATA";

          $('#coverageModalGraph').modal({
              width: '75%',
              dismissible: true, // Modal can be dismissed by clicking outside of the modal
              opacity: .5, // Opacity of modal background
              inDuration: 300, // Transition in duration
              outDuration: 200, // Transition out duration
              startingTop: '4%', // Starting top style attribute
              endingTop: '10%', // Ending top style attribute
              ready: function(modal, trigger) { // Callback for Modal open. Modal and trigger parameters available.
                  var testname = modal.data('testname');
                  $('#coverageModalTitle').text("Code Coverage Chart : " + testname);
                  var query = new google.visualization.Query('show_coverage_overview?pageType=datatable&testName=' + testname);
                  // Send the query with a callback function.
                  query.send(handleQueryResponse);
              },
              complete: function() {
                  $('#coverage_combo_chart_div').empty();
                  $('#coverage_line_chart_div').empty();
                  $('#coverage_table_chart_div').empty();

                  $("div.valign-wrapper > h2.center-align:contains('" + no_data_msg + "')").each(function(index){
                    $(this).parent().remove();
                  });
                  $("span.indicator.badge.blue:contains('Graph')").each(function( index ) {
                    $(this).removeClass('blue');
                    $(this).addClass('grey');
                  });

                  $('#dataTableLoading').show("slow");
              } // Callback for Modal close
          });

          // Handle the query response.
          function handleQueryResponse(response) {
            $('#dataTableLoading').hide("slow");
            if (response.isError()) {
              alert('Error in query: ' + response.getMessage() + ' ' + response.getDetailedMessage());
              return;
            }
            // Draw the visualization.
            var data = response.getDataTable();
            if (data.getNumberOfRows() == 0) {
              var blankData = '<div class="valign-wrapper" style="height: 90%;">'
                            + '<h2 class="center-align" style="width: 100%;">' + no_data_msg + '</h2>'
                            + '</div>';
              $('#coverageModalTitle').after(blankData);
              return;
            }
            data.sort([{column: 0}]);

            var date_formatter = new google.visualization.DateFormat({ pattern: "yyyy-MM-dd" });
            date_formatter.format(data, 0);

            var dataView = new google.visualization.DataView(data);

            // Disable coveredLine and totalLine
            dataView.hideColumns([1,2]);

            var lineOptions = {
              title: 'Source Code Line Coverage',
              width: '100%',
              height: 450,
              curveType: 'function',
              intervals: { 'color' : 'series-color' },
              interval: {
                'fill': {
                  'style': 'area',
                  'curveType': 'function',
                  'fillOpacity': 0.2
                },
                'bar': {
                  'style': 'bars',
                  'barWidth': 0,
                  'lineWidth': 1,
                  'pointSize': 3,
                  'fillOpacity': 1
                }},
              legend: { position: 'bottom' },
              tooltip: { isHtml: true },
              fontName: 'Roboto',
              titleTextStyle: {
                color: '#757575',
                fontSize: 16,
                bold: false
              },
              pointsVisible: true,
              vAxis:{
                title: 'Code Coverage Ratio (%)',
                titleTextStyle: {
                  color: '#424242',
                  fontSize: 12,
                  italic: false
                },
                textStyle: {
                  fontSize: 12,
                  color: '#757575'
                },
              },
              hAxis: {
                title: 'Date',
                format: 'yyyy-MM-dd',
                minTextSpacing: 0,
                showTextEvery: 1,
                slantedText: true,
                slantedTextAngle: 45,
                textStyle: {
                  fontSize: 12,
                  color: '#757575'
                },
                titleTextStyle: {
                  color: '#424242',
                  fontSize: 12,
                  italic: false
                }
              },
            };
            var lineChart = new google.visualization.LineChart(document.getElementById('coverage_line_chart_div'));
            lineChart.draw(dataView, lineOptions);

            var tableOptions = {
              title: 'Covered/Total Source Code Line Count (SLOC)',
              width: '95%',
              // height: 350,
              is3D: true
            };
            var tableChart = new google.visualization.Table(document.getElementById('coverage_table_chart_div'));
            tableChart.draw(data, tableOptions);
          }

          $('.collapsible').collapsible({
              accordion: false,
              onOpen: function(el) {
                  var header = $( el[0].children[0] );
                  var body = $( el[0].children[1] );
                  var icon = header.children('.material-icons.expand-arrow');
                  var testName = header.data('test');
                  var timestamp = header.data('time');
                  var url = '/api/test_run?test=' + testName + '&timestamp=' + timestamp;
                  $.get(url).done(function(data) {
                      displayTestDetails(body, data, 16);
                  }).fail(function() {
                      icon.removeClass('rotate');
                  }).always(function() {
                      header.removeClass('disabled');
                  });
              }, // Callback for Collapsible open
              onClose: function(el) {
                  console.log(el);
                  var body = $( el[0].children[1] );
                  body.empty();
              } // Callback for Collapsible close
          });

          $( "span.indicator.badge:contains('Coverage')" ).click(function (evt) {
              var header = $(evt.currentTarget.parentElement);
              var testName = header.data('test');
              var startTime = header.data('time');
              var url = '/show_coverage?testName=' + testName + '&startTime=' + startTime;
              window.location.href = url;
              return false;
          });

          $( "span.indicator.badge:contains('Links')" ).click(function (evt) {
              var header = $(evt.currentTarget.parentElement);
              var logLinks = header.data('links');
              showLinks($('body'), logLinks);
              return false;
          });

          $( "span.indicator.badge:contains('Graph')" ).click(function (evt) {
              var header = $(evt.currentTarget.parentElement);
              var testname = header.data('test');
              $('#coverageModalGraph').data("testname", testname);
              $('#coverageModalGraph').modal('open');
              $(evt.target).removeClass("grey");
              $(evt.target).addClass("blue");
              return false;
          });
      });

      // draw test statistics chart
      function drawStatsChart() {
          var testStats = ${testStats};
          if (testStats.length < 1) {
              return;
          }
          var resultNames = ${resultNamesJson};
          var rows = resultNames.map(function(res, i) {
              nickname = res.replace('TEST_CASE_RESULT_', '').replace('_', ' ')
                         .trim().toLowerCase();
              return [nickname, parseInt(testStats[i])];
          });
          rows.unshift(['Result', 'Count']);

          // Get CSS color definitions (or default to white)
          var colors = resultNames.map(function(res) {
              return $('.' + res).css('background-color') || 'white';
          });

          var data = google.visualization.arrayToDataTable(rows);
          var options = {
              is3D: false,
              colors: colors,
              fontName: 'Roboto',
              fontSize: '14px',
              legend: {position: 'labeled'},
              tooltip: {showColorCode: true, ignoreBounds: false},
              chartArea: {height: '80%', width: '90%'},
              pieHole: 0.4
          };

          var chart = new google.visualization.PieChart(document.getElementById('pie-chart-stats'));
          chart.draw(data, options);
      }

      // draw the coverage pie charts
      function drawCoverageCharts() {
          var coveredLines = ${coveredLines};
          var uncoveredLines = ${uncoveredLines};
          var rows = [
              ["Result", "Count"],
              ["Covered Lines", coveredLines],
              ["Uncovered Lines", uncoveredLines]
          ];

          // Get CSS color definitions (or default to white)
          var colors = [
              $('.TEST_CASE_RESULT_PASS').css('background-color') || 'white',
              $('.TEST_CASE_RESULT_FAIL').css('background-color') || 'white'
          ]

          var data = google.visualization.arrayToDataTable(rows);

          var optionsRaw = {
              is3D: false,
              colors: colors,
              fontName: 'Roboto',
              fontSize: '14px',
              pieSliceText: 'value',
              legend: {position: 'bottom'},
              chartArea: {height: '80%', width: '90%'},
              tooltip: {showColorCode: true, ignoreBounds: false, text: 'value'},
              pieHole: 0.4
          };

          var optionsNormalized = {
              is3D: false,
              colors: colors,
              fontName: 'Roboto',
              fontSize: '14px',
              legend: {position: 'bottom'},
              tooltip: {showColorCode: true, ignoreBounds: false, text: 'percentage'},
              chartArea: {height: '80%', width: '90%'},
              pieHole: 0.4
          };

          var chart = new google.visualization.PieChart(document.getElementById('pie-chart-coverage-raw'));
          chart.draw(data, optionsRaw);

          chart = new google.visualization.PieChart(document.getElementById('pie-chart-coverage-normalized'));
          chart.draw(data, optionsNormalized);
      }

      // refresh the page to see the runs matching the specified filter
      function refresh() {
        var link = '${pageContext.request.contextPath}' +
            '/show_coverage_overview?' + search.args();
        if (${unfiltered}) {
          link += '&unfiltered=';
        }
        window.open(link,'_self');
      }

  </script>

  <body>
    <div class='wide container'>
      <div id="filter-bar">
        <div class="row card search-bar expanded">
          <div class="header-wrapper">
            <h5 class="section-header">
              <b>Code Coverage</b>
            </h5>
            <div class="search-icon-wrapper">
              <i class="material-icons">search</i>
            </div>
          </div>
          <div class="search-wrapper" ${empty branch and empty device ? 'style="display: none"' : ''}>
            <div class="row">
              <div class="col s9">
                <div class="input-field col s4">
                  <c:set var="branchVal" value='${fn:replace(branch, "\\\"", "")}'></c:set>
                  <select id="searchBranch">
                      <option value="" <c:if test="${empty branch}">disabled selected</c:if> >Choose your branch</option>
                      <c:forEach items='${branchOptions}' var='branchOption'>
                        <option value="${branchOption}" ${branchVal == branchOption ? 'selected' : ''}>${branchOption}</option>
                      </c:forEach>
                  </select>
                  <label>Branch Select</label>
                </div>
                <div class="input-field col s4">
                  <c:set var="deviceVal" value='${fn:replace(device, "\\\"", "")}'></c:set>
                  <select id="searchDevice">
                    <option value="" <c:if test="${empty device}">disabled selected</c:if> >Choose your device</option>
                    <c:forEach items='${deviceOptions}' var='deviceOption'>
                      <option value="${deviceOption}" ${deviceVal == deviceOption ? 'selected' : ''}>${deviceOption}</option>
                    </c:forEach>
                  </select>
                  <label>Device Select</label>
                </div>
                <div class="col s4"></div>
              </div>
              <div class="refresh-wrapper col s3">
                <a id="schBtn" class="btn-floating btn-medium red waves-effect waves-light" style="margin-right: 30px;">
                  <i class="medium material-icons">cached</i>
                </a>
              </div>
            </div>
          </div>
        </div>
      </div>
      <div class='row'>
        <div class='col s12'>
          <div class='col s12 card center-align'>
            <div id='legend-wrapper'>
              <c:forEach items='${resultNames}' var='res'>
                <div class='center-align legend-entry'>
                  <c:set var='trimmed' value='${fn:replace(res, "TEST_CASE_RESULT_", "")}'/>
                  <c:set var='nickname' value='${fn:replace(trimmed, "_", " ")}'/>
                  <label for='${res}'>${nickname}</label>
                  <div id='${res}' class='${res} legend-bubble'></div>
                </div>
              </c:forEach>
            </div>
          </div>
        </div>
        <div class='col s4 valign-wrapper'>
          <!-- pie chart -->
          <div class='pie-chart-wrapper col s12 valign center-align card'>
            <h6 class='pie-chart-title'>Test Statistics</h6>
            <div id='pie-chart-stats' class='pie-chart-div'></div>
          </div>
        </div>
        <div class='col s4 valign-wrapper'>
          <!-- pie chart -->
          <div class='pie-chart-wrapper col s12 valign center-align card'>
            <h6 class='pie-chart-title'>Line Coverage (Raw)</h6>
            <div id='pie-chart-coverage-raw' class='pie-chart-div'></div>
          </div>
        </div>
        <div class='col s4 valign-wrapper'>
          <!-- pie chart -->
          <div class='pie-chart-wrapper col s12 valign center-align card'>
            <h6 class='pie-chart-title'>Line Coverage (Normalized)</h6>
            <div id='pie-chart-coverage-normalized' class='pie-chart-div'></div>
          </div>
        </div>
      </div>
      <div class='col s12' id='test-results-container'>
        <ul class="collapsible popout test-runs" data-collapsible="expandable">
          <c:forEach var="testRunEntity" items="${testRunEntityList}" varStatus="loop">
            <li class="test-run-container">
              <div data-test="<c:out value="${testRunEntity.testName}" />" data-time="<c:out value="${testRunEntity.startTimestamp}" />" data-links='${testRunEntity.jsonLogLinks}' class="collapsible-header test-run">
                <span class="test-run-metadata">
                  <span class="test-run-label">
                    <c:out value="${testRunEntity.testName}" />
                  </span>
                  <br />
                  <b>VTS Build: </b><c:out value="${testRunEntity.testBuildId}" />
                  <br />
                  <b>Host: </b><c:out value="${testRunEntity.hostName}" />
                  <br />
                  <c:out value="${testRunEntity.startDateTime}" /> - <c:out value="${testRunEntity.endDateTime}" />+0900 (<c:out value="${(testRunEntity.endTimestamp - testRunEntity.startTimestamp) / 1000}" />s)
                </span>
                <span class="indicator badge green" style="color: white;">
                  <c:out value="${testRunEntity.passCount}" />/<c:out value="${testRunEntity.passCount + testRunEntity.passCount}" />
                </span>

                <c:set var="coveredLineCnt" value="${codeCoverageEntityMap[testRunEntity.id].coveredLineCount}" />
                <c:set var="totalLineCnt" value="${codeCoverageEntityMap[testRunEntity.id].totalLineCount}" />
                <c:set var="covPct"
                       value="${(coveredLineCnt / totalLineCnt * 1000) / 10}"/>

                <c:choose>
                  <c:when test = "${covPct <= 20}">
                    <c:set var="badgeColor" value="red" />
                  </c:when>
                  <c:when test = "${covPct >= 70}">
                    <c:set var="badgeColor" value="green" />
                  </c:when>
                  <c:otherwise>
                    <c:set var="badgeColor" value="orange" />
                  </c:otherwise>
                </c:choose>

                <span class="indicator badge padded hoverable waves-effect <c:out value="${badgeColor}" />" style="color: white; margin-left: 1px;">
                  Coverage: <c:out value="${coveredLineCnt}" />/<c:out value="${totalLineCnt}" />
                  (<fmt:formatNumber value="${(coveredLineCnt / totalLineCnt * 1000) / 10}" type="number" pattern="#.##"/>%)
                </span>
                <span class="indicator badge padded hoverable waves-effect grey lighten-1" style="color: white; margin-left: 1px;">Links</span>
                <span class="indicator badge padded hoverable waves-effect grey lighten-1" style="color: white; margin-left: 1px;">Graph</span>
                <i class="material-icons expand-arrow">expand_more</i>
              </div>
              <div class="collapsible-body test-results row"></div>
            </li>
          </c:forEach>
        </ul>

      </div>
    </div>

    <!-- Coverage Graph Modal Structure -->
    <div id="coverageModalGraph" class="modal modal-fixed-footer" style="width: 75%;">
      <div class="modal-content">
        <h4 id="coverageModalTitle">Code Coverage Chart</h4>

        <div class="preloader-wrapper big active loaders">
          <div id="dataTableLoading" class="spinner-layer spinner-blue-only">
            <div class="circle-clipper left">
              <div class="circle"></div>
            </div>
            <div class="gap-patch">
              <div class="circle"></div>
            </div>
            <div class="circle-clipper right">
              <div class="circle"></div>
            </div>
          </div>
        </div>

        <!--Div that will hold the visualization graph -->
        <div id="coverage_line_chart_div"></div>
        <p></p>
        <p></p>
        <div id="coverage_table_chart_div" class="center-align"></div>
      </div>
      <div class="modal-footer">
        <a href="#!" class="modal-action modal-close waves-effect waves-green btn-flat ">Close</a>
      </div>
    </div>

    <%@ include file="footer.jsp" %>
  </body>
</html>
