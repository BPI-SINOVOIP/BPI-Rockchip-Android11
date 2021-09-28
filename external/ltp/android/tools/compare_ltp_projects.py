#! /usr/bin/env python
#
# Copyright 2018 - The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Parses the output of parse_ltp_{make,make_install} and generates a
# corresponding Android.mk.
#
# This process is split into two steps so this second step can later be replaced
# with an Android.bp generator.

from __future__ import print_function

"""Tool for comparing 2 LTP projects to find added / removed tests & testsuites"""

import os
import argparse
import os.path
import sys

def scan_tests(ltp_root, suite):
    ''' Find all tests that are run as part of given test suite in LTP.

    Args:
        ltp_root: Path ot the LTP project.
        suite: Name of testsuite.

    Returns:
        List of tests names that are run as part of the given test suite.
    '''

    tests = []
    if not suite:
        return tests

    runtest_dir = ltp_root + os.path.sep + 'runtest'
    test_suiteFile = runtest_dir + os.path.sep + suite
    if not os.path.isfile(test_suiteFile):
        print ('No tests defined for suite {}',format(suite))
        return tests

    with open(test_suiteFile) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#'):
                tests.append(line.split()[0])
    f.close()
    return tests

def scan_test_suites(ltp_root, scenario):
    ''' Find all testuites and tests run as part of given LTP scenario

    Args:
        ltp_root: Path to the LTP project.
        scenario: name of the scenario (found in ltp_root/scenario_groups). E.g. "vts"

    Returns:
        List of testsuite names that are run as part of given scenario (e.g. vts).
        If scenario is not specified, return all testsuites.
    '''

    runtest_dir = ltp_root + os.path.sep + 'runtest'
    if not os.path.isdir(runtest_dir):
        print ('Invalid ltp_root {}, runtest directory doesnt exist'.format(ltp_root))
        sys.exit(2)

    test_suites = []
    if scenario:
        scenarioFile = ltp_root + os.path.sep + 'scenario_groups' + os.path.sep + scenario
        if not os.path.isfile(scenarioFile):
            return test_suites
        with open(scenarioFile) as f:
            for line in f:
                line = line.strip()
                if not line.startswith('#'):
                    test_suites.append(line)
        return test_suites

    runtest_dir = ltp_root + os.path.sep + 'runtest'
    test_suites = [f for f in os.listdir(runtest_dir) if os.path.isfile(runtest_dir + os.path.sep + f)]

    return test_suites

def scan_ltp(ltp_root, scenario):
    ''' scan LTP project and return all tests and testuites present.

    Args:
        ltp_root: Path to the LTP project.
        scenario: specific scenario we want to scan. e.g. vts

    Returns:
        Dictionary of all LTP test names keyed by testsuite they are run as part of.
        E.g.
             {
                 'mem': ['oom01', 'mem02',...],
                 'syscalls': ['open01', 'read02',...],
                 ...
             }
    '''

    if not os.path.isdir(ltp_root):
        print ('ltp_root {} does not exist'.format(ltp_root))
        sys.exit(1)

    test_suites = scan_test_suites(ltp_root, scenario)
    if not test_suites:
        print ('No Testsuites found for scenario {}'.format(scenario))
        sys.exit(3)

    ltp_tests = {}
    for suite in test_suites:
        ltp_tests[suite] = scan_tests(ltp_root, suite)
    return ltp_tests

def show_diff(ltp_tests_1, ltp_tests_2):
    ''' Find and print diff between testcases in 2 LTP project checkouts.

    Args:
        ltp_tests_1: dictionary of tests keyed by test suite names
        ltp_tests_2: dictionary of tests keyed by test suite names
    '''
    DEFAULT_WIDTH = 8
    test_suites1 = set(sorted(ltp_tests_1.keys()))
    test_suites2 = set(sorted(ltp_tests_2.keys()))

    # Generate lists of deleted, added and common test suites between
    # LTP1 & LTP2
    deleted_test_suites = sorted(test_suites1.difference(test_suites2))
    added_test_suites = sorted(test_suites2.difference(test_suites1))
    common_test_suites = sorted(test_suites1.intersection(test_suites2))

    deleted_tests = []
    added_tests = []
    for suite in common_test_suites:
        tests1 = set(sorted(ltp_tests_1[suite]))
        tests2 = set(sorted(ltp_tests_2[suite]))

        exclusive_test1 = tests1.difference(tests2)
        exclusive_test2 = tests2.difference(tests1)
        for e in exclusive_test1:
            deleted_tests.append(suite + '.' + e)
        for e in exclusive_test2:
            added_tests.append(suite + '.' + e)

    # find the maximum width of a test suite name or test name
    # we have to print to decide the alignment.
    if not deleted_test_suites:
        width_suites = DEFAULT_WIDTH
    else:
        width_suites = max([len(x) for x in deleted_test_suites])

    if not deleted_tests:
        width_tests = DEFAULT_WIDTH
    else:
        width_tests = max([len(x) for x in deleted_tests])
    width = max(width_suites, width_tests);

    # total rows we have to print
    total_rows = max(len(deleted_test_suites), len(added_test_suites))
    if total_rows > 0:
        print ('{:*^{len}}'.format(' Tests Suites ', len=width*2+2))
        print ('{:>{len}} {:>{len}}'.format('Deleted', 'Added', len=width))
        for i in range(total_rows):
            print ('{:>{len}} {:>{len}}'.format('' if i >= len(deleted_test_suites) else str(deleted_test_suites[i]),
                                 '' if i >= len(added_test_suites) else str(added_test_suites[i]), len=width))

    print('')
    # total rows we have to print
    total_rows = max(len(deleted_tests), len(added_tests))
    if total_rows:
        print ('{:*^{len}}'.format(' Tests ', len=width*2+2))
        print ('{:>{len}} {:>{len}}'.format('Deleted', 'Added', len=width))
        for i in range(total_rows):
            print ('{:>{len}} {:>{len}}'.format('' if i >= len(deleted_tests) else str(deleted_tests[i]),
                                 '' if i >= len(added_tests) else str(added_tests[i]), len=width))
def main():
    arg_parser = argparse.ArgumentParser(
        description='Diff 2 LTP projects for supported test cases')
    arg_parser.add_argument('--ltp-root1',
                            dest='ltp_root1',
                            required=True,
                            help="LTP Root Directory before merge")
    arg_parser.add_argument('--ltp-root2',
                            dest='ltp_root2',
                            required=True,
                            help="LTP Root Directory after merge")
    arg_parser.add_argument('--scenario', default=None,
                            dest='scenario',
                            help="LTP scenario to list tests for")
    args = arg_parser.parse_args()

    ltp_tests1 = scan_ltp(args.ltp_root1, args.scenario)
    ltp_tests2 = scan_ltp(args.ltp_root2, args.scenario)
    show_diff(ltp_tests1, ltp_tests2)

if __name__ == '__main__':
    main()
