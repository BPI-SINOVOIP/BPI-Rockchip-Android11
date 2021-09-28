#!/usr/bin/env python3
# Copyright 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Wrapper to run pylint with the right settings."""

import argparse
import errno
import os
import shutil
import sys
import subprocess


assert (sys.version_info.major, sys.version_info.minor) >= (3, 6), (
    'Python 3.6 or newer is required; found %s' % (sys.version,))


DEFAULT_PYLINTRC_PATH = os.path.join(
    os.path.dirname(os.path.realpath(__file__)), 'pylintrc')


def is_pylint3(pylint):
    """See whether |pylint| supports Python 3."""
    # Make sure pylint is using Python 3.
    result = subprocess.run([pylint, '--version'], stdout=subprocess.PIPE,
                            check=True)
    if b'Python 3' not in result.stdout:
        print('%s: unable to locate a Python 3 version of pylint; Python 3 '
              'support cannot be guaranteed' % (__file__,), file=sys.stderr)
        return False

    return True


def find_pylint3():
    """Figure out the name of the pylint tool for Python 3.

    It keeps changing with Python 2->3 migrations.  Fun.
    """
    # Prefer pylint3 as that's what we want.
    if shutil.which('pylint3'):
        return 'pylint3'

    # If there's no pylint, give up.
    if not shutil.which('pylint'):
        print('%s: unable to locate pylint; please install:\n'
              'sudo apt-get install pylint' % (__file__,), file=sys.stderr)
        sys.exit(1)

    return 'pylint'


def get_parser():
    """Return a command line parser."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--init-hook', help='Init hook commands to run.')
    parser.add_argument('--py3', action='store_true',
                        help='Force Python 3 mode')
    parser.add_argument('--executable-path',
                        help='The path of the pylint executable.')
    parser.add_argument('--no-rcfile',
                        help='Specify to use the executable\'s default '
                        'configuration.',
                        action='store_true')
    parser.add_argument('files', nargs='+')
    return parser


def main(argv):
    """The main entry."""
    parser = get_parser()
    opts, unknown = parser.parse_known_args(argv)

    pylint = opts.executable_path
    if pylint is None:
        if opts.py3:
            pylint = find_pylint3()
        else:
            pylint = 'pylint'

    # Make sure pylint is using Python 3.
    if opts.py3:
        is_pylint3(pylint)

    cmd = [pylint]
    if not opts.no_rcfile:
        # We assume pylint is running in the top directory of the project,
        # so load the pylintrc file from there if it's available.
        pylintrc = os.path.abspath('pylintrc')
        if not os.path.exists(pylintrc):
            pylintrc = DEFAULT_PYLINTRC_PATH
            # If we pass a non-existent rcfile to pylint, it'll happily ignore
            # it.
            assert os.path.exists(pylintrc), 'Could not find %s' % pylintrc
        cmd += ['--rcfile', pylintrc]

    cmd += unknown + opts.files

    if opts.init_hook:
        cmd += ['--init-hook', opts.init_hook]

    try:
        os.execvp(cmd[0], cmd)
        return 0
    except OSError as e:
        if e.errno == errno.ENOENT:
            print('%s: unable to run `%s`: %s' % (__file__, cmd[0], e),
                  file=sys.stderr)
            print('%s: Try installing pylint: sudo apt-get install %s' %
                  (__file__, os.path.basename(cmd[0])), file=sys.stderr)
            return 1

        raise


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
