# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tast-specific logics of TKO parser."""

import json
import os

import common

from autotest_lib.client.common_lib import utils

# Name of the Autotest test case that runs Tast tests.
_TAST_AUTOTEST_NAME = 'tast'

# Prefix added to Tast test names when writing their results to TKO.
# This should match with _TEST_NAME_PREFIX in server/site_tests/tast/tast.py.
_TAST_TEST_NAME_PREFIX = 'tast.'


def is_tast_test(test_name):
    """Checks if a test is a Tast test."""
    return test_name.startswith(_TAST_TEST_NAME_PREFIX)


def load_tast_test_aux_results(job, test_name):
    """Loads auxiliary results of a Tast test.

    @param job: A job object.
    @param test_name: The name of the test.
    @return (attributes, perf_values) where
        attributes: A str-to-str dict of attribute keyvals
        perf_values: A dict loaded from a chromeperf JSON
    """
    assert is_tast_test(test_name)

    test_dir = os.path.join(job.dir, _TAST_AUTOTEST_NAME)

    case_name = test_name[len(_TAST_TEST_NAME_PREFIX):]
    case_dir = os.path.join(test_dir, 'results', 'tests', case_name)

    # Load attribute keyvals.
    attributes_path = os.path.join(test_dir, 'keyval')
    if os.path.exists(attributes_path):
        attributes = utils.read_keyval(attributes_path)
    else:
        attributes = {}

    # Load a chromeperf JSON.
    perf_values_path = os.path.join(case_dir, 'results-chart.json')
    if os.path.exists(perf_values_path):
        with open(perf_values_path) as fp:
            perf_values = json.load(fp)
    else:
        perf_values = {}

    return attributes, perf_values
