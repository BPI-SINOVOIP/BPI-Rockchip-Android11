<%--
  ~ Copyright (c) 2016 Google Inc. All Rights Reserved.
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

<html>
  <%@ include file="header.jsp" %>
  <link rel="stylesheet" href="/css/show_coverage.css">
  <script async defer src="https://apis.google.com/js/api.js"
          onload="this.onload=function(){};handleClientLoad()"
          onreadystatechange="if (this.readyState === 'complete') this.onload()">
  </script>
  <body>
    <script type="text/javascript">
        $(document).ready(function() {
            // Initialize AJAX for CORS
            $.ajaxSetup({
                xhrFields : {
                    withCredentials: true
                }
            });

            $('.collapsible.popout').collapsible({
              accordion : true
            }).find('.collapsible-header').click(onClick);


            $("div.collapsible-header > span.indicator.waves-effect").click(function(evt) {
              evt.preventDefault();

              $("#loader-indicator").show();

              var cmd = $(evt.target).text();
              var testRunId = $(evt.target).data("id");
              var postData = { coverageId: testRunId, testName: "${testName}", testRunId: "${startTime}", cmd: cmd};
              $.post("/api/coverage/data", postData, function() {
                // success
                console.log("success");
                var detachedLi = $(evt.target).parent().parent().detach();
                if (cmd == "enable") {
                  $(evt.target).text("disable");
                  $(evt.target).parent().removeClass("grey");
                  $('ul.collapsible.popout').prepend(detachedLi);
                } else {
                  $(evt.target).text("enable");
                  $(evt.target).parent().addClass("grey");
                  $('ul.collapsible.popout').append(detachedLi);
                }
              })
              .done(function() {
                // Done
                $("#loader-indicator").fadeOut("slow");
              })
              .fail(function() {
                alert( "Error occurred during changing the status" );
              });
            });
        });

        function handleClientLoad() {
          // Load the API client and auth2 library
          gapi.load('client:auth2', initClient);
        }

        function initClient() {
          gapi.client.init({
            client_id: ${clientId},
            scope: ${gerritScope}
          }).then(function () {
            // displayEntries();
          });
        }

        /* Open a window to Gerrit so that user can login.
           Minimize the previously clicked entry.
        */
        var gerritLogin = function(element) {
            window.open(${gerritURI}, "Ratting", "toolbar=0,status=0");
            element.click();
        }

        /* Loads source code for a particular entry and displays it with
           coverage information as the accordion entry expands.
        */
        var onClick = function() {
            // Remove source code from the accordion entry that was open before
            var self = $(this);
            var prev = self.parent().siblings('li.active');
            if (prev.length > 0) {
                prev.find('.table-container').empty();
            }
            var url = self.parent().data('url');
            var container = self.parent().find('.table-container');
            container.html('<div class="center-align">Loading...</div>');
            var coverageVectors = self.parent().data('coverage');
            if (self.parent().hasClass('active')) {
                // Remove the code from display
                container.empty();
            } else {
                /* Fetch and display the code.
                   Note: a coverageVector may be shorter than sourceContents due
                   to non-executable (i.e. comments or language-specific syntax)
                   lines in the code. Trailing source lines that have no
                   coverage information are assumed to be non-executable.
                */
                $.ajax({
                    url: url,
                    dataType: 'text'
                }).promise().done(function(src) {
                    src = atob(src);
                    if (!src) return;
                    srcLines = src.split('\n');
                    covered = 0;
                    total = 0;
                    var table = $('<table class="table"></table>');
                    var rows = srcLines.forEach(function(line, j) {
                        var count = coverageVectors[j];
                        var row = $('<tr></tr>');
                        if (typeof count == 'undefined' || count < 0) {
                            count = "--";
                        } else if (count == 0) {
                            row.addClass('uncovered');
                            total += 1;
                        } else {
                            row.addClass('covered');
                            total += 1;
                        }
                        row.append('<td class="count">' + String(count) + '</td>');
                        row.append('<td class="line_no">' + String(j+1) + '</td>');
                        code = $('<td class="code"></td>');
                        code.text(String(line));
                        code.appendTo(row);
                        row.appendTo(table);
                    });
                    container.empty();
                    container.append(table);
                }).fail(function(error) {
                    if (error.status == 0) {  // origin error, refresh cookie
                        container.empty();
                        container.html('<div class="center-align">' +
                                       '<span class="login-button">' +
                                       'Click to authorize Gerrit access' +
                                       '</span></div>');
                        container.find('.login-button').click(function() {
                            gerritLogin(self);
                        });
                    } else {
                        container.html('<div class="center-align">' +
                                       'Not found.</div>');
                    }
                });
            }
        }
    </script>
    <div id='coverage-container' class='wide container'>
      <h4 class="section-title"><b>Coverage:</b> </h4>
      <ul class="collapsible popout" data-collapsible="accordion">
        <c:forEach var="coverageEntity" items="${coverageEntityList}" varStatus="loop">
          <li data-url="<c:url value="${coverageEntity.gerritUrl}"/>" data-index="${loop.index}" data-coverage="${coverageEntity.lineCoverage}">
          <div class="collapsible-header <c:out value='${coverageEntity.isIgnored ? "grey" : ""}'/>">
            <i class="material-icons">library_books</i>
            <div class="truncate"><b>${coverageEntity.projectName}</b>/${coverageEntity.filePath}</div>
            <div class="right total-count">${coverageEntity.coveredCount}/${coverageEntity.totalCount}</div>
            <div class="indicator ${coverageEntity.percentage >= 70 ? "green" : "red"}">${coverageEntity.percentage}%</div>
            <c:if test="${isModerator}">
              <span data-id="${coverageEntity.id}" class="indicator waves-effect blue lighten-1" style="margin-left: 5px;"><c:out value='${coverageEntity.isIgnored ? "enable" : "disable"}'/></span>
            </c:if>
          </div>
          <div class="collapsible-body row">
            <div class="html-container">
              <div class="table-container"></div>
            </div>
          </div>
          </li>
        </c:forEach>
      </ul>
    </div>

      <div id="loader-indicator" class="loader-background" style="display: none">
          <div class="preloader-wrapper big active">
              <div class="spinner-layer spinner-blue-only">
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
      </div>
    <%@ include file="footer.jsp" %>
  </body>
</html>
