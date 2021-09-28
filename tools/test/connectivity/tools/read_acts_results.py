#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import argparse
import json


def read_acts_results(acts_summary_filename):
    """Opens the ACTS results file and returns the pass/fail/unknown counters.

    Args:
        acts_summary_filename: The ACTS results file name.

    Returns:
        master_results_data: A list of all the test results, including the
            result of each test.
        master_results_pass: A list of all pass test results.
        master_results_fail: A list of all fail test results.
        master_results_unknown: A list of all unknown test results.
        pass_counter: A counter of how many tests pass.
        fail_counter: A counter of how many tests fail.
        unknown_counter: A counter of how many tests are unknown.
    """
    with open(acts_summary_filename) as json_file:
        data = json.load(json_file)

    master_results_data = [['Test', 'Result']]
    master_results_pass = [['Test', 'Result']]
    master_results_fail = [['Test', 'Result']]
    master_results_unknown = [['Test', 'Result']]
    pass_counter = 0
    fail_counter = 0
    unknown_counter = 0

    for result in data['Results']:
        results_data = []
        results_pass = []
        results_fail = []
        results_unknown = []
        if result['Result'] == 'PASS':
            results_pass.append(result['Test Name'])
            results_pass.append(result['Result'])
            master_results_pass.append(results_pass)
            pass_counter += 1
        if result['Result'] == 'FAIL':
            results_fail.append(result['Test Name'])
            results_fail.append(result['Result'])
            master_results_fail.append(results_fail)
            fail_counter += 1
        if result['Result'] == 'UNKNOWN':
            results_unknown.append(result['Test Name'])
            results_unknown.append(result['Result'])
            master_results_unknown.append(results_unknown)
            unknown_counter += 1
        results_data.append(result['Test Name'])
        results_data.append(result['Result'])
        master_results_data.append(results_data)
    return (master_results_data,
            master_results_pass,
            master_results_fail,
            master_results_unknown,
            pass_counter,
            fail_counter,
            unknown_counter)


def print_acts_summary(master_results_data,
                       master_results_pass,
                       master_results_fail,
                       master_results_unknown,
                       pass_counter,
                       fail_counter,
                       unknown_counter,
                       split_results=False,
                       ):
    """Prints the ACTS test results as either all of the tests together, or
    split into pass, fail, and unknown.

    Args:
        master_results_data: A list of all the test results, including the
            result of each test.
        master_results_pass: A list of all pass test results.
        master_results_fail: A list of all fail test results.
        master_results_unknown: A list of all unknown test results.
        pass_counter: A counter of how many tests pass.
        fail_counter: A counter of how many tests fail.
        unknown_counter: A counter of how many tests are unknown.
        split_results: Whether to split the results into pass/fail/unknown or
            display the results in the order the tests were run.
    """
    widths = [max(map(len, col)) for col in zip(*master_results_data)]
    if not split_results:
        for row in master_results_data:
            print('  '.join((val.ljust(width) for val, width in zip(row,
                                                                    widths))))
        print('')
        print('Pass: %s     '
              'Fail: %s     '
              'Unknown: %s  '
              'Total: %s' % (pass_counter,
                             fail_counter,
                             unknown_counter,
                             pass_counter+fail_counter+unknown_counter))
    else:
        print('')
        for row in master_results_pass:
            print('  '.join((val.ljust(width) for val, width in zip(row,
                                                                    widths))))
        print('Pass: %s' % pass_counter)

        print('')
        for row in master_results_fail:
            print('  '.join((val.ljust(width) for val, width in zip(row,
                                                                    widths))))
        print('Fail: %s' % fail_counter)
        if unknown_counter is not 0:
            print('')
            for row in master_results_unknown:
                print('  '.join((val.ljust(width)
                                 for val, width in zip(row, widths))))
            print('Unknown: %s' % unknown_counter)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-f',
                        '--results_file',
                        help='ACTS results file')
    parser.add_argument('--split_results',
                        help='separate passing and failing results',
                        action='store_true')

    args = parser.parse_args()
    if not args.results_file:
        parser.error('Use --results_file or -f to specify the ACTS '
                     'results file you want to parse.')

    (master_results_data,
     master_results_pass,
     master_results_fail,
     master_results_unknown,
     pass_counter,
     fail_counter,
     unknown_counter) = read_acts_results(args.results_file)
    print_acts_summary(master_results_data,
                       master_results_pass,
                       master_results_fail,
                       master_results_unknown,
                       pass_counter,
                       fail_counter,
                       unknown_counter,
                       split_results=args.split_results)


if __name__ == '__main__':
    main()
