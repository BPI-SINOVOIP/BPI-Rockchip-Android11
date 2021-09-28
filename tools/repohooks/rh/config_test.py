#!/usr/bin/env python3
# Copyright 2019 The Android Open Source Project
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

"""Integration tests for the config module (via PREUPLOAD.cfg)."""

import argparse
import os
import re
import sys


REPOTOOLS = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
REPO_ROOT = os.path.dirname(os.path.dirname(REPOTOOLS))


def assertEqual(msg, exp, actual):
    """Assert |exp| equals |actual|."""
    assert exp == actual, '%s: expected "%s" but got "%s"' % (msg, exp, actual)


def assertEnv(var, value):
    """Assert |var| is set in the environment as |value|."""
    assert var in os.environ, '$%s missing in environment' % (var,)
    assertEqual('env[%s]' % (var,), value, os.environ[var])


def check_commit_id(commit):
    """Check |commit| looks like a git commit id."""
    assert len(commit) == 40, 'commit "%s" must be 40 chars' % (commit,)
    assert re.match(r'^[a-f0-9]+$', commit), \
        'commit "%s" must be all hex' % (commit,)


def check_commit_msg(msg):
    """Check the ${PREUPLOAD_COMMIT_MESSAGE} setting."""
    assert len(msg) > 1, 'commit message must be at least 2 bytes: %s'


def check_repo_root(root):
    """Check the ${REPO_ROOT} setting."""
    assertEqual('REPO_ROOT', REPO_ROOT, root)


def check_files(files):
    """Check the ${PREUPLOAD_FILES} setting."""
    assert files


def check_env():
    """Verify all exported env vars look sane."""
    assertEnv('REPO_PROJECT', 'platform/tools/repohooks')
    assertEnv('REPO_PATH', 'tools/repohooks')
    assertEnv('REPO_REMOTE', 'aosp')
    check_commit_id(os.environ['REPO_LREV'])
    print(os.environ['REPO_RREV'])
    check_commit_id(os.environ['PREUPLOAD_COMMIT'])


def get_parser():
    """Return a command line parser."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check-env', action='store_true',
                        help='Check all exported env vars.')
    parser.add_argument('--commit-id',
                        help='${PREUPLOAD_COMMIT} setting.')
    parser.add_argument('--commit-msg',
                        help='${PREUPLOAD_COMMIT_MESSAGE} setting.')
    parser.add_argument('--repo-root',
                        help='${REPO_ROOT} setting.')
    parser.add_argument('files', nargs='+',
                        help='${PREUPLOAD_FILES} paths.')
    return parser


def main(argv):
    """The main entry."""
    parser = get_parser()
    opts = parser.parse_args(argv)

    try:
        if opts.check_env:
            check_env()
        if opts.commit_id is not None:
            check_commit_id(opts.commit_id)
        if opts.commit_msg is not None:
            check_commit_msg(opts.commit_msg)
        if opts.repo_root is not None:
            check_repo_root(opts.repo_root)
        check_files(opts.files)
    except AssertionError as e:
        print('error: %s' % (e,), file=sys.stderr)
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
