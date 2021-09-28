#!/usr/bin/python2
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Fake implementation of tast executable used by tast_unittest.py.

In unit tests, this file is executed instead of the real tast executable by the
tast.py server test. It:

- reads a config.json file installed alongside this script,
- parses command-line arguments passed by tast.py,
- checks that the arguments included all required args specified by the config
  file for the given command (e.g. 'list', 'run' etc.), and
- performs actions specified by the config file.
"""

import argparse
import json
import os
import sys


def main():
    # pylint: disable=missing-docstring

    # The config file is written by TastTest._run_test in tast_unittest.py and
    # consists of a dict from command names (e.g. 'list', 'run', etc.) to
    # command definition dicts (see TastCommand in tast_unittest.py).
    path = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                        'config.json')
    with open(path) as f:
        cfg = json.load(f)

    args = parse_args()
    cmd = cfg.get(args.command)
    if not cmd:
        raise RuntimeError('Unexpected command "%s"' % args.command)

    for arg in cmd.get('required_args', []):
        name, expected_value = arg.split('=', 1)
        # argparse puts the repeated "pattern" args into a list of lists
        # instead of a single list. Pull the args back out in this case.
        val = getattr(args, name)
        if isinstance(val, list) and len(val) == 1 and isinstance(val[0], list):
            val = val[0]
        actual_value = str(val)
        if actual_value != expected_value:
            raise RuntimeError('Got arg %s with value "%s"; want "%s"' %
                               (name, actual_value, expected_value))

    if cmd.get('stdout'):
        sys.stdout.write(cmd['stdout'])
    if cmd.get('stderr'):
        sys.stderr.write(cmd['stderr'])

    if cmd.get('files_to_write'):
        for path, data in cmd['files_to_write'].iteritems():
            dirname = os.path.dirname(path)
            if not os.path.exists(dirname):
                os.makedirs(dirname, 0755)
            with open(path, 'w') as f:
                f.write(data)

    sys.exit(cmd.get('status'))


def parse_args():
    """Parses command-line arguments.

    @returns: argparse.Namespace object containing parsed attributes.
    """
    def to_bool(v):
        """Converts a boolean flag's value to a Python bool value."""
        if v == 'true' or v == '':
            return True
        if v == 'false':
            return False
        raise argparse.ArgumentTypeError('"true" or "false" expected')

    parser = argparse.ArgumentParser()
    parser.add_argument('-logtime', type=to_bool, default=False, nargs='?')
    parser.add_argument('-verbose', type=to_bool, default=False, nargs='?')
    parser.add_argument('-version', action='version', version='1.0')

    subparsers = parser.add_subparsers(dest='command')

    def add_common_args(subparser):
        """Adds arguments shared between tast's 'list' and 'run' subcommands."""
        subparser.add_argument('-build', type=to_bool, default=True, nargs='?')
        subparser.add_argument('-downloadprivatebundles', type=to_bool,
                               default=False, nargs='?')
        subparser.add_argument('-devservers')
        subparser.add_argument('-remotebundledir')
        subparser.add_argument('-remotedatadir')
        subparser.add_argument('-remoterunner')
        subparser.add_argument('-sshretries')
        subparser.add_argument('target')
        subparser.add_argument('patterns', action='append', nargs='*')

    list_parser = subparsers.add_parser('list')
    add_common_args(list_parser)
    list_parser.add_argument('-json', type=to_bool, default=False, nargs='?')

    run_parser = subparsers.add_parser('run')
    add_common_args(run_parser)
    run_parser.add_argument('-resultsdir')
    run_parser.add_argument('-waituntilready')
    run_parser.add_argument('-timeout')
    run_parser.add_argument('-continueafterfailure', type=to_bool,
                            default=False, nargs='?')
    run_parser.add_argument('-var', action='append', default=[])
    run_parser.add_argument('-defaultvarsdir')
    run_parser.add_argument('-varsfile', action='append', default=[])

    return parser.parse_args()


if __name__ == '__main__':
    main()
