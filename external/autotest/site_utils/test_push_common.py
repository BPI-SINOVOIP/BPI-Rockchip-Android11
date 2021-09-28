# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common file shared by test_push of autotest and skylab.

autotest: site_utils/test_push.py
skylab: venv/skylab_staging/test_push.py
"""

import collections
import re

# Dictionary of test results keyed by test name regular expression.
EXPECTED_TEST_RESULTS = {'^SERVER_JOB$':                 ['GOOD'],
                         # This is related to dummy_Fail/control.dependency.
                         'dummy_Fail.dependency$':       ['TEST_NA'],
                         'login_LoginSuccess.*':         ['GOOD'],
                         'provision_AutoUpdate.double':  ['GOOD'],
                         'dummy_Pass.*':                 ['GOOD'],
                         'dummy_Fail.Fail$':             ['FAIL'],
                         'dummy_Fail.Error$':            ['ERROR'],
                         'dummy_Fail.Warn$':             ['WARN'],
                         'dummy_Fail.NAError$':          ['TEST_NA'],
                         'dummy_Fail.Crash$':            ['GOOD'],
                         'autotest_SyncCount$':          ['GOOD'],
                         }

EXPECTED_TEST_RESULTS_DUMMY = {'^SERVER_JOB$':       ['GOOD'],
                               'dummy_Pass.*':       ['GOOD'],
                               'dummy_Fail.Fail':    ['FAIL'],
                               'dummy_Fail.Warn':    ['WARN'],
                               'dummy_Fail.Crash':   ['GOOD'],
                               'dummy_Fail.Error':   ['ERROR'],
                               'dummy_Fail.NAError': ['TEST_NA'],
                               }

EXPECTED_TEST_RESULTS_POWERWASH = {'platform_Powerwash': ['GOOD'],
                                   'SERVER_JOB':         ['GOOD'],
                                   }

_TestPushErrors = collections.namedtuple(
        '_TestPushErrors',
        [
                'mismatch_errors',
                'unknown_tests',
                'missing_tests',
        ]
)


def summarize_push(test_views, expected_results, ignored_tests=[]):
    """Summarize the test push errors."""
    test_push_errors = _match_test_results(test_views, expected_results,
                                           ignored_tests)
    return _generate_push_summary(test_push_errors)


def _match_test_results(test_views, expected_results, ignored_tests):
    """Match test results with expected results.

    @param test_views: A defaultdict where keys are test names and values are
                       lists of test statuses, e.g.,
                       {'dummy_Fail.Error': ['ERROR', 'ERROR],
                        'dummy_Fail.NAError': ['TEST_NA'],
                        'dummy_Fail.RetrySuccess': ['ERROR', 'GOOD'],
                        }
    @param expected_results: A dictionary of test name to expected test result.
                             Has the same format as test_views.
    @param ignored_tests: A list of test name patterns. Any mismatch between
                          test results and expected test results that matches
                          one these patterns is ignored.

    @return: A _TestPushErrors tuple.
    """
    mismatch_errors = []
    unknown_tests = []
    found_keys = set()
    for test_name, test_status_list in test_views.iteritems():
        test_found = False
        for test_name_pattern, expected_result in expected_results.items():
            if re.search(test_name_pattern, test_name):
                test_found = True
                found_keys.add(test_name_pattern)
                if (sorted(expected_result) != sorted(test_status_list) and
                    _is_significant(test_name, ignored_tests)):
                    error = ('%s Expected: %s, Actual: %s' %
                             (test_name, expected_result, test_status_list))
                    mismatch_errors.append(error)

        if not test_found and _is_significant(test_name, ignored_tests):
            unknown_tests.append(test_name)

    missing_tests = set(expected_results.keys()) - found_keys
    missing_tests = [t for t in missing_tests
                     if _is_significant(t, ignored_tests)]
    return _TestPushErrors(mismatch_errors=mismatch_errors,
                           unknown_tests=unknown_tests,
                           missing_tests=missing_tests)


def _is_significant(test, ignored_tests_patterns):
    return all([test not in m for m in ignored_tests_patterns])


def _generate_push_summary(test_push_errors):
    """Generate a list of summary based on the test_push results."""
    summary = []
    if test_push_errors.mismatch_errors:
        summary.append(('Results of %d test(s) do not match expected '
                        'values:') % len(test_push_errors.mismatch_errors))
        summary.extend(test_push_errors.mismatch_errors)
        summary.append('\n')

    if test_push_errors.unknown_tests:
        summary.append('%d test(s) are not expected to be run:' %
                       len(test_push_errors.unknown_tests))
        summary.extend(test_push_errors.unknown_tests)
        summary.append('\n')

    if test_push_errors.missing_tests:
        summary.append('%d test(s) are missing from the results:' %
                       len(test_push_errors.missing_tests))
        summary.extend(test_push_errors.missing_tests)
        summary.append('\n')

    return summary
