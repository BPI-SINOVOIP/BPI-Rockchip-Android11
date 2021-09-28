/**
 * Copyright (c) 2018 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You may
 * obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/**
 * Display the log links in a modal window.
 * @param linkList A list of [name, url] tuples representing log links.
 */
function showLinks(container, linkList) {
  if (!linkList || linkList.length == 0) return;

  var logCollection = $('<ul class="collection"></ul>');
  var entries = linkList.reduce(function(acc, entry) {
    if (!entry || entry.length == 0) return acc;
    var link = '<a href="' + entry[1] + '"';
    link += 'class="collection-item">' + entry[0] + '</li>';
    return acc + link;
  }, '');
  logCollection.html(entries);

  if (container.find('#info-modal').length == 0) {
    var modal =
        $('<div id="info-modal" class="modal modal-fixed-footer"></div>');
    var content = $('<div class="modal-content"></div>');
    content.append('<h4>Links</h4>');
    content.append('<div class="info-container"></div>');
    content.appendTo(modal);
    var footer = $('<div class="modal-footer"></div>');
    footer.append('<a class="btn-flat modal-close">Close</a></div>');
    footer.appendTo(modal);
    modal.appendTo(container);
  }
  var infoContainer = $('#info-modal>.modal-content>.info-container');
  infoContainer.empty();
  logCollection.appendTo(infoContainer);
  $('#info-modal').modal({dismissible: true});
  $('#info-modal').modal('open');
}

/**
 * Get the nickname for a test case result.
 *
 * Removes the result prefix and suffix, extracting only the result name.
 *
 * @param testCaseResult The string name of a VtsReportMessage.TestCaseResult.
 * @returns the string nickname of the result.
 */
function getNickname(testCaseResult) {
  return testCaseResult.replace('TEST_CASE_RESULT_', '')
      .replace('_RESULT', '')
      .trim()
      .toLowerCase();
}

/**
 * Display test data in the body beneath a test run's metadata.
 * @param container The jquery object in which to insert the test metadata.
 * @param data The json object containing the columns to display.
 * @param lineHeight The height of each list element.
 */
function displayTestDetails(container, data, lineHeight) {
  var nCol = data.length;
  var width = 's' + (12 / nCol);
  test = container;
  var maxLines = 0;
  data.forEach(function(column, index) {
    if (column.data == undefined || column.name == undefined) {
      return;
    }
    var classes = 'col test-col grey lighten-5 ' + width;
    if (index != nCol - 1) {
      classes += ' bordered';
    }
    if (index == 0) {
      classes += ' left-most';
    }
    if (index == nCol - 1) {
      classes += ' right-most';
    }
    var colContainer = $('<div class="' + classes + '"></div>');
    var col = $('<div class="test-case-container"></div>');
    colContainer.appendTo(container);
    var count = column.data.length;
    var head = $('<h5 class="test-result-label white"></h5>')
                   .text(getNickname(column.name))
                   .appendTo(colContainer)
                   .css('text-transform', 'capitalize');
    $('<div class="indicator right center"></div>')
        .text(count)
        .addClass(column.name)
        .appendTo(head);
    col.appendTo(colContainer);
    var list = $('<ul></ul>').appendTo(col);
    column.data.forEach(function(testCase) {
      $('<li></li>')
          .text(testCase)
          .addClass('test-case')
          .css('font-size', lineHeight - 2)
          .css('line-height', lineHeight + 'px')
          .appendTo(list);
    });
    if (count > maxLines) {
      maxLines = count;
    }
  });
  var containers = container.find('.test-case-container');
  containers.height(maxLines * lineHeight);
}
