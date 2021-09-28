// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var cycle_tabs = {};
var cycles = {};
var time_ratio = 3600 * 1000 / test_time_ms; // default test time is 1 hour
var preexisting_windows = [];
var log_lines = [];
var error_codes = {}; //for each active tabId
var page_timestamps = [];
var page_timestamps_recorder = {};
var keys_values = [];

function setupTest() {
  //adding these listeners to track request failure codes
  chrome.webRequest.onCompleted.addListener(capture_completed_status,
                                            {urls: ["<all_urls>"]});
  task_monitor.bind();

  chrome.windows.getAll(null, function(windows) {
    preexisting_windows = windows;
    for (var i = 0; i < tasks.length; i++) {
      setTimeout(launch_task, tasks[i].start / time_ratio, tasks[i]);
    }
    var end = 3600 * 1000 / time_ratio;
    log_lines = [];
    page_timestamps = [];
    page_timestamps_recorder = {};
    keys_values = [];
    record_log_entry(dateToString(new Date()) + " Loop started");
    setTimeout(send_summary, end);
  });
}

function close_preexisting_windows() {
  for (var i = 0; i < preexisting_windows.length; i++) {
    chrome.windows.remove(preexisting_windows[i].id);
  }
  preexisting_windows.length = 0;
}

function get_active_url(cycle) {
  active_idx = cycle.idx == 0 ? cycle.urls.length - 1 : cycle.idx - 1;
  return cycle.urls[active_idx];
}

function testListener(request, sender, sendResponse) {
  end = Date.now()
  page = page_timestamps_recorder[sender.tab.id];
  page['end_load_time'] = end;
  console.log("page_timestamps_recorder:");
  console.log(JSON.stringify(page_timestamps_recorder));
  if (sender.tab.id in cycle_tabs) {
    cycle = cycle_tabs[sender.tab.id];
    cycle.successful_loads++;
    url = get_active_url(cycle);
    record_log_entry(dateToString(new Date()) + " [load success] " + url);
    if (request.action == "should_scroll" && cycle.focus) {
      sendResponse({"should_scroll": should_scroll,
                    "should_scroll_up": should_scroll_up,
                    "scroll_loop": scroll_loop,
                    "scroll_interval": scroll_interval_ms,
                    "scroll_by": scroll_by_pixels});
    }
    delete cycle_tabs[sender.tab.id];
  }
}

function report_page_nav_to_test() {
  //Sends message to PLT informing that user is navigating to new page.
  var ping_url = 'http://localhost:8001/pagenav';
  var req = new XMLHttpRequest();
  req.open('GET', ping_url, true);
  req.send("");
}

function capture_completed_status(details) {
  var tabId = details.tabId;
  if (!(details.tabId in error_codes)) {
    error_codes[tabId] = [];
  }
  var report = {
    'url':details.url,
    'code': details.statusCode,
    'status': details.statusLine,
    'time': new Date(details.timeStamp)
  }
  error_codes[tabId].push(report);
}


function cycle_navigate(cycle) {
  cycle_tabs[cycle.id] = cycle;
  var url = cycle.urls[cycle.idx];
  // Resetting the error codes.
  // TODO(coconutruben) Verify if reseting here might give us
  // garbage data since some requests/responses might still come
  // in before we update the tab, but we'll register them as
  // information about the subsequent url
  error_codes[cycle.id] = [];
  record_log_entry(dateToString(new Date()) + " [load start] " + url)
  var start = Date.now();
  // start_time of next page is end_browse_time of previous page
  if (cycle.id in page_timestamps_recorder) {
    page = page_timestamps_recorder[cycle.id];
    page['end_browse_time'] = start;
    page_timestamps.push(page);
    console.log(JSON.stringify(page_timestamps));
  }
  page_timestamps_new_record(cycle.id, url, start);
  chrome.tabs.update(cycle.id, {'url': url, 'selected': true});
  report_page_nav_to_test()
  cycle.idx = (cycle.idx + 1) % cycle.urls.length;
  if (cycle.timeout < cycle.delay / time_ratio && cycle.timeout > 0) {
    cycle.timer = setTimeout(cycle_check_timeout, cycle.timeout, cycle);
  } else {
    cycle.timer = setTimeout(cycle_navigate, cycle.delay / time_ratio, cycle);
  }
}

function record_error_codes(cycle) {
  var error_report = dateToString(new Date()) + " [load failure details] "
                     + get_active_url(cycle) + "\n";
  var reports = error_codes[cycle.id];
  for (var i = 0; i < reports.length; i++) {
    report = reports[i];
    error_report = error_report + "\t\t" +
    dateToString(report.time) + " | " +
    "[response code] " + report.code + " | " +
    "[url] " + report.url + " | " +
    "[status line] " + report.status + "\n";
  }
  log_lines.push(error_report);
  console.log(error_report);
}

function cycle_check_timeout(cycle) {
  if (cycle.id in cycle_tabs) {
    cycle.failed_loads++;
    record_error_codes(cycle);
    record_log_entry(dateToString(new Date()) + " [load failure] " +
                                  get_active_url(cycle));
    cycle_navigate(cycle);
  } else {
    cycle.timer = setTimeout(cycle_navigate,
                             cycle.delay / time_ratio - cycle.timeout,
                             cycle);
  }
}

function launch_task(task) {
  if (task.type == 'window' && task.tabs) {
    chrome.windows.create({'url': '/focus.html'}, function (win) {
      close_preexisting_windows();
      chrome.tabs.getSelected(win.id, function(tab) {
        for (var i = 1; i < task.tabs.length; i++) {
          chrome.tabs.create({'windowId':win.id, 'url': '/focus.html'});
        }
        chrome.tabs.getAllInWindow(win.id, function(tabs) {
          for (var i = 0; i < tabs.length; i++) {
            tab = tabs[i];
            url = task.tabs[i];
            start = Date.now();
            page_timestamps_new_record(tab.id, url, start);
            chrome.tabs.update(tab.id, {'url': url, 'selected': true});
          }
          console.log(JSON.stringify(page_timestamps_recorder));
        });
        setTimeout(function(win_id) {
          record_end_browse_time_for_window(win_id);
          chrome.windows.remove(win_id);
        }, (task.duration / time_ratio), win.id);
      });
    });
  } else if (task.type == 'cycle' && task.urls) {
    chrome.windows.create({'url': '/focus.html'}, function (win) {
      close_preexisting_windows();
      chrome.tabs.getSelected(win.id, function(tab) {
        var cycle = {
           'timeout': task.timeout,
           'name': task.name,
           'delay': task.delay,
           'urls': task.urls,
           'id': tab.id,
           'idx': 0,
           'timer': null,
           'focus': !!task.focus,
           'successful_loads': 0,
           'failed_loads': 0
        };
        cycles[task.name] = cycle;
        cycle_navigate(cycle);
        setTimeout(function(cycle, win_id) {
          clearTimeout(cycle.timer);
          record_end_browse_time_for_window(win_id);
          chrome.windows.remove(win_id);
        }, task.duration / time_ratio, cycle, win.id);
      });
    });
  }
}

function page_timestamps_new_record(tab_id, url, start) {
  // sanitize url, make http(s)://www.abc.com/d/e/f into www.abc.com
  sanitized_url = url.replace(/https?:\/\//, '').split('/')[0];
  page_timestamps_recorder[tab_id] = {
    'url': sanitized_url,
    'start_time': start,
    'end_load_time': null,
    'end_browse_time': null
  }
}

function record_end_browse_time_for_window(win_id) {
  chrome.tabs.getAllInWindow(win_id, function(tabs) {
    end = Date.now();
    console.log("page_timestamps_recorder:");
    console.log(JSON.stringify(page_timestamps_recorder));
    tabs.forEach(function (tab) {
      if (tab.id in page_timestamps_recorder) {
        page = page_timestamps_recorder[tab.id];
        page['end_browse_time'] = end;
        page_timestamps.push(page);
      }
    });
    console.log(JSON.stringify("page_timestamps:"));
    console.log(JSON.stringify(page_timestamps));
  });
}

function record_log_entry(entry) {
  log_lines.push(entry);
}

function record_key_values(dictionary) {
  keys_values.push(dictionary);
}

function send_log_entries() {
  var post = [];
  log_lines.forEach(function (item, index) {
    var entry = encodeURIComponent(item);
    post.push('log'+ index + '=' + entry);
  });

  var log_url = 'http://localhost:8001/log';
  send_post_data(post, log_url)
}

function send_status() {
  var post = ["status=good"];

  for (var name in cycles) {
    var cycle = cycles[name];
    post.push(name + "_successful_loads=" + cycle.successful_loads);
    post.push(name + "_failed_loads=" + cycle.failed_loads);
  }

  chrome.runtime.onMessage.removeListener(testListener);

  var status_url = 'http://localhost:8001/status';
  send_post_data(post, status_url)
}

function send_raw_page_time_info() {
  var post = [];
  page_timestamps.forEach(function (item, index) {
    post.push('page_time_data'+ index + "=" + JSON.stringify(item));
  })

  var pagetime_info_url = 'http://localhost:8001/pagetime';
  send_post_data(post, pagetime_info_url)
}

function send_key_values() {
  var post = [];
  keys_values.forEach(function (item, index) {
    post.push("keyval" + index + "=" + JSON.stringify(item));
  })
  var key_values_info_url = 'http://localhost:8001/keyvalues';
  send_post_data(post, key_values_info_url)
}

function send_post_data(post, url) {
  var req = new XMLHttpRequest();
  req.open('POST', url, true);
  req.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
  req.send(post.join("&"));
  console.log(post.join("&"));
}

function send_summary() {
  send_raw_page_time_info();
  task_monitor.unbind();
  send_key_values();
  send_status();
  send_log_entries();
}

function startTest() {
  time_ratio = 3600 * 1000 / test_time_ms; // default test time is 1 hour
  chrome.runtime.onMessage.addListener(testListener);
  setTimeout(setupTest, 1000);
}

function initialize() {
  // Called when the user clicks on the browser action.
  chrome.browserAction.onClicked.addListener(function(tab) {
    // Start the test with default settings.
    chrome.runtime.onMessage.addListener(testListener);
    setTimeout(setupTest, 1000);
  });
}

window.addEventListener("load", initialize);
