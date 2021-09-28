#!/usr/bin/env python3
#
# Copyright 2019, The Android Open Source Project
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

import argparse
import sys
from resource_utils import get_all_resources, merge_resources
from git_utils import has_chassis_changes

def main():
    parser = argparse.ArgumentParser(description="Check that an rro does not attempt to overlay any resources that don't exist")
    parser.add_argument('--sha', help='Git hash of current changes. This script will not run if this is provided and there are no chassis changes.')
    parser.add_argument('-r', '--rro', action='append', nargs=1, help='res folder of an RRO')
    parser.add_argument('-b', '--base', action='append', nargs=1, help='res folder of what is being RROd')
    args = parser.parse_args()

    if not has_chassis_changes(args.sha):
        # Don't run because there were no chassis changes
        return

    if args.rro is None or args.base is None:
        parser.print_help()
        sys.exit(1)

    rro_resources = set()
    for resDir in args.rro:
        merge_resources(rro_resources, get_all_resources(resDir[0]))

    base_resources = set()
    for resDir in args.base:
        merge_resources(base_resources, get_all_resources(resDir[0]))

    extras = rro_resources.difference(base_resources)
    if len(extras) > 0:
        print("RRO attempting to override resources that don't exist:\n"
              + '\n'.join(map(lambda x: str(x), extras)))
        sys.exit(1)

if __name__ == "__main__":
    main()
