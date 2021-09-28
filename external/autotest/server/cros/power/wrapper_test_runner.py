# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Class for running test inside wrapper tests."""

import json
import logging

from autotest_lib.client.common_lib import error
from autotest_lib.server import autotest
from autotest_lib.server.cros.dynamic_suite import suite


class WrapperTestRunner(object):
    """A helper class for running test inside wrapper tests.

    This class takes the tagged test name and finds the test in the autotest
    directory. It also runs the test on the given DUT.
    """

    def __init__(self, config, test_dir):
        """Init WrapperTestRunner.

        The test to run inside the wrapper test is usually a client side
        autotest. Look up the control file of the inner test, and then prepend
        the args to to the control file to pass the args to the inner test.

        @param config: the args argument from test_that in a dict, contains the
                       test to look up in the autotest directory and the args to
                       prepend to the control file.
                       required data: {'test': 'test_TestName.tag'}
        @param test_dir: the directory to retrieve the test from.
        """
        if not config:
            msg = 'Wrapper test must run with args input.'
            raise error.TestNAError(msg)
        if 'test' not in config:
            msg = 'User did not specify client side test to run in wrapper.'
            raise error.TestNAError(msg)
        # test_name is tagged test name.
        self._test_name = config['test']

        # Find the test in autotest file system.
        fs_getter = suite.create_fs_getter(test_dir)
        predicate = suite.test_name_equals_predicate(self._test_name)
        test = suite.find_and_parse_tests(fs_getter, predicate)
        if not test:
            msg = '%s is not a valid test name.' % self._test_name
            raise error.TestNAError(msg)

        # If multiple tests with the same name are found, run the first one.
        if len(test) > 1:
            logging.warning('Found %d tests with name %s, running the first '
                            'one.', len(test), self._test_name)
        control_file_no_args = test[0].text

        # Prepend the args.
        args_list = ['='.join((k, str(v))) for k, v in config.iteritems()]
        args_string = 'args = ' + json.dumps(args_list)
        args_dict_string = 'args_dict = ' + json.dumps(config)
        control_file_list = [args_string, args_dict_string]
        control_file_list.extend(
                ['%s = "%s"' % (k, str(v)) for k, v in config.iteritems()])
        control_file_list.append(control_file_no_args)

        self._test_control_file = '\n'.join(control_file_list)

    def run_test(self, host):
        """Run the autotest from its control file on the specified DUT.

        @param host: CrosHost object representing the DUT.
        """
        autotest_client = autotest.Autotest(host)
        autotest_client.run(self._test_control_file)

    def get_tagged_test_name(self):
        """Return the tagged test name to be run inside the wrapper.

        @return the tagged test name to be run inside the wrapper.
        """
        return self._test_name
