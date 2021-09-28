#!/usr/bin/python

#
# Copyright 2020, The Android Open Source Project
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

from __future__ import print_function

from argparse import ArgumentParser
import subprocess
import sys


def is_in_aosp():
    branches = subprocess.check_output(['git', 'branch', '-vv']).splitlines()

    for branch in branches:
        # current branch starts with a '*'
        if branch.startswith('*'):
            return '[aosp/' in branch

    # otherwise assume in AOSP
    return True


def is_commit_msg_valid(commit_msg):
    for line in commit_msg.splitlines():
        line = line.strip().lower()
        if line.startswith('updated-pdd'):
            return True

    return False


def main():
    parser = ArgumentParser(description='Check if the Privacy Design Doc (PDD) has been updated')
    parser.add_argument('metrics_file', type=str, help='path to the metrics Protobuf file')
    parser.add_argument('commit_msg', type=str, help='commit message')
    parser.add_argument('commit_files', type=str, nargs='*', help='files changed in the commit')
    args = parser.parse_args()

    metrics_file = args.metrics_file
    commit_msg = args.commit_msg
    commit_files = args.commit_files

    if is_in_aosp():
        return 0

    if metrics_file not in commit_files:
        return 0

    if is_commit_msg_valid(commit_msg):
        return 0

    print('This commit has changed {metrics_file}.'.format(metrics_file=metrics_file))
    print('If this change added/changed/removed metrics collected from the device,')
    print('please update the Wifi Metrics Privacy Design Doc (PDD) at go/wifi-metrics-pdd')
    print('and acknowledge you have done so by adding this line to your commit message:')
    print()
    print('Updated-PDD: TRUE')
    print()
    print('Otherwise, please explain why the PDD does not need to be updated:')
    print()
    print('Updated-PDD: Not applicable - reformatted file')
    print()
    print('Please reach out to the OWNERS for more information about the Wifi Metrics PDD.')
    return 1


if __name__ == '__main__':
    exit_code = main()
    sys.exit(exit_code)
