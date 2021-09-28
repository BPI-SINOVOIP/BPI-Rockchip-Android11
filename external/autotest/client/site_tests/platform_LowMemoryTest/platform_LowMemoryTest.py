# Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import time

from autotest_lib.client.bin import test
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.cros import cros_logging
from autotest_lib.client.cros.input_playback import input_playback

GB_TO_BYTE = 1024 * 1024 * 1024
MB_TO_BYTE = 1024 * 1024
KB_TO_BYTE = 1024

DEFAULT_RANDOM_RATIO = 0.666

class MemoryKillsMonitor:
    """A util class for reading kill events."""

    _LOG_FILE = '/var/log/chrome/chrome'
    _PATTERN_DISCARD = re.compile(
        'tab_manager_delegate_chromeos.*:(\d+) Killed tab')
    _PATTERN_OOM = re.compile('Tab OOM-Killed Memory details:')

    def __init__(self):
        self._log_reader = cros_logging.ContinuousLogReader(self._LOG_FILE)
        self._oom = False
        self._discarded = False

    @property
    def oom(self):
        """Returns true if oom event is recorded."""
        return self._oom

    @property
    def discarded(self):
        """Returns true if tab discard event is recorded."""
        return self._discarded

    def check_events(self):
        """Checks the events and updates oom and discarded property."""
        for line in self._log_reader.read_all_logs():
            matched = self._PATTERN_DISCARD.search(line)
            if matched:
                logging.info('Matched line %s', line)
                self._discarded = True
            matched = self._PATTERN_OOM.search(line)
            if matched:
                logging.info('Matched line %s', line)
                self._oom = True


def create_pages_and_check_oom(create_page_func, size_mb, bindir):
    """Common code to create pages and to check OOM.

    Args:
        create_page_func: function to create page, it takes 3 arguments,
            cr: chrome wrapper, size_mb: alloc size per page in MB,
            bindir: path to the test directory.
        bindir: path to the test directory.
    Returns:
        Dictionary of test results.
    """
    kills_monitor = MemoryKillsMonitor()

    # The amount of tabs that can trigger OOM consistently if the tabs are not
    # discarded properly.
    tab_count = 1 + (utils.memtotal() * KB_TO_BYTE * 4) / (size_mb * MB_TO_BYTE)

    # The tab count at the first tab discard.
    first_discard = -1
    # The number of tabs actually created.
    tabs_created = tab_count

    # Opens a specific amount of tabs, breaks if the OOM killer is invoked.
    with chrome.Chrome(init_network_controller=True) as cr:
        cr.browser.platform.SetHTTPServerDirectories(bindir)
        for i in range(tab_count):
            create_page_func(cr, size_mb, bindir)
            time.sleep(3)
            kills_monitor.check_events()
            if first_discard == -1 and kills_monitor.discarded:
                first_discard = i + 1
            if kills_monitor.oom:
                tabs_created = i + 1
                break

    # Test is successful if at least one Chrome tab is killed by tab
    # discarder and the kernel OOM killer isn't invoked.
    if kills_monitor.oom:
        raise error.TestFail('OOM Killer invoked')

    if not kills_monitor.discarded:
        raise error.TestFail('No tab discarded')

    # TODO: reports the page loading time.
    return {'NumberOfTabsAtFirstDiscard': first_discard,
            'NumberOfTabsCreated': tabs_created}


def get_alloc_size_per_page():
    """Returns the default alloc size per page in MB."""
    ALLOC_MB_PER_PAGE_DEFAULT = 800
    ALLOC_MB_PER_PAGE_SUB_2GB = 400

    alloc_mb_per_page = ALLOC_MB_PER_PAGE_DEFAULT
    # Allocate less memory per page for devices with 2GB or less memory.
    if utils.memtotal() * KB_TO_BYTE < 2 * GB_TO_BYTE:
        alloc_mb_per_page = ALLOC_MB_PER_PAGE_SUB_2GB
    return alloc_mb_per_page


def create_alloc_page(cr, page_name, size_mb, random_ratio, bindir):
    """The program in alloc.html allocates a large array with random data.

    Args:
        cr: chrome wrapper.
        size_mb: size of the allocated javascript array in the page.
        random_ratio: the ratio of random data size : all data size
        bindir: path to the test directory.
    Returns:
        The created tab.
    """
    url = cr.browser.platform.http_server.UrlOf(
        os.path.join(bindir, page_name))
    url += '?alloc=' + str(size_mb)
    url += '&ratio=' + str(random_ratio)
    tab = cr.browser.tabs.New()
    tab.Navigate(url)
    tab.WaitForDocumentReadyStateToBeComplete()
    tab.WaitForJavaScriptCondition(
        "document.hasOwnProperty('out') == true", timeout=60)
    return tab


def random_pages(bindir, random_ratio):
    """Creates pages with random javascript data and checks OOM.

    Args:
        bindir: path to the test directory.
        random_ratio: the ratio of random data size : all data size
    """
    def create_random_page(cr, size_mb, bindir):
        """Creates a page with random javascript data."""
        create_alloc_page(cr, 'alloc.html', size_mb, random_ratio,
                          bindir)

    return create_pages_and_check_oom(
        create_random_page, get_alloc_size_per_page(), bindir)


def form_pages(bindir, random_ratio):
    """Creates pages with pending form data and checks OOM.

    Args:
        bindir: path to the test directory.
        random_ratio: the ratio of random data size : all data size
    """
    player = input_playback.InputPlayback()
    player.emulate(input_type='keyboard')
    player.find_connected_inputs()

    def create_form_page(cr, size_mb, bindir):
        """Creates a page with pending form data."""
        tab = create_alloc_page(cr, 'form.html', size_mb, random_ratio,
                                bindir)
        # Presses tab to focus on the first interactive element.
        player.blocking_playback_of_default_file(input_type='keyboard',
                                                 filename='keyboard_tab')
        # Fills the form.
        player.blocking_playback_of_default_file(input_type='keyboard',
                                                 filename='keyboard_a')

    ret = create_pages_and_check_oom(
        create_form_page, get_alloc_size_per_page(), bindir)
    player.close()
    return ret


class platform_LowMemoryTest(test.test):
    """Memory pressure test."""
    version = 1

    def run_once(self, flavor='random', random_ratio=DEFAULT_RANDOM_RATIO):
        """Runs the test once."""
        if flavor == 'random':
            perf_results = random_pages(self.bindir, random_ratio)
        elif flavor == 'form':
            perf_results = form_pages(self.bindir, random_ratio)

        self.write_perf_keyval(perf_results)
        for result_key in perf_results:
            self.output_perf_value(description=result_key,
                                   value=perf_results[result_key],
                                   higher_is_better=True)

