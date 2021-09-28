# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This is a temporary module to help scheduling paygen suites in trampoline.

In trampoline, paygen suites are scheduled via skylab create-test, to schedule
every paygen test independently, instead of creating a paygen suite via
skylab create-suite.
"""

import re

import common

from autotest_lib.server.cros.dynamic_suite import control_file_getter
from autotest_lib.server.cros.dynamic_suite import suite_common


def is_paygen_suite(suite_name):
    """Check if it's to run a paygen suite in trampoline."""
    paygen_au_regexp = 'paygen_au_*'
    return re.match(paygen_au_regexp, suite_name) is not None


def get_paygen_tests(build, suite_name):
    """Parse paygen tests from au control files."""
    if not is_paygen_suite(suite_name):
        raise ValueError('Cannot download paygen test control files for '
                         'non-paygen suite %s' % suite_name)

    ds, _ = suite_common.stage_build_artifacts(
        build, artifacts=['%s_suite' % suite_name])
    cf_getter = control_file_getter.DevServerGetter(build, ds)
    tests = suite_common.retrieve_for_suite(cf_getter, suite_name)
    return suite_common.filter_tests(
            tests, suite_common.name_in_tag_predicate(suite_name))


def paygen_skylab_args(test, suite_name, image, pool, board, model,
                       timeout_mins, qs_account, service_account):
    """Form args for requesting paygen tests in skylab."""
    args = ['-image', image]
    args += ['-pool', pool]
    if board is not None:
        args += ['-board', board]

    if model is not None:
        args += ['-model', model]

    args += ['-timeout-mins', str(timeout_mins)]

    tags = ['skylab:run_suite_trampoline',
            'build:%s' % image,
            'suite:%s' % suite_name]
    for t in tags:
        args += ['-tag', t]

    keyvals = ['build:%s' % image,
               'suite:%s' % suite_name,
               'label:%s/%s/%s' % (image, suite_name, test.name)]
    for k in keyvals:
        args += ['-keyval', k]

    # Paygen test expects a space-separated string of name=value pairs.
    # See http://shortn/_C8r3rC0rOP.
    test_args = ['name=%s' % test.suite,
                 'update_type=%s' % test.update_type,
                 'source_release=%s' % test.source_release,
                 'target_release=%s' % test.target_release,
                 'target_payload_uri=%s' % test.target_payload_uri,
                 'source_payload_uri=%s' % test.source_payload_uri,
                 'suite=%s' % test.suite,
                 'source_archive_uri=%s' % test.source_archive_uri]
    args += ['-test-args', ' '.join(test_args)]

    if qs_account:
        args += ['-qs-account', qs_account]
    args += ['-service-account-json', service_account]

    return args + ['autoupdate_EndToEndTest']
