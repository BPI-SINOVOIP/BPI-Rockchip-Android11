#!/usr/bin/python2
#
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import common
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.client.common_lib.cros import cros_config

# Lots of command-line mocking in this file.
# Mock cros_config results are based on the path and property provided.
# (Remember, cros_config's syntax is `cros_config path property`.)
# The path determines whether cros_config fails or succeeds.
# The property determines whether there is a fallback command, and if so,
# whether the fallback fails or succeeds.

SUCCEEDS = 0
FAILS = 1
DOES_NOT_EXIST = 2

# cros_config path determines the mock behavior of cros_config.
CC_PATHS = {SUCCEEDS: '/success', FAILS: '/error'}

# cros_config property determines the mock behavior of the fallback command.
CC_PROPERTIES = {
        SUCCEEDS: 'fallback_succeeds',
        FAILS: 'fallback_fails',
        DOES_NOT_EXIST: 'no_fallback'
}

CROS_CONFIG_SUCCESS_RESPONSE = 'cros_config succeeded'
CROS_CONFIG_FALLBACK_RESPONSE = 'fallback succeeded'


def get_cros_config_args(cros_config_result, fallback_result):
    """Build cros_config_args based on the desired outcome."""
    cros_config_path = CC_PATHS[cros_config_result]
    cros_config_property = CC_PROPERTIES[fallback_result]
    return '%s %s' % (cros_config_path, cros_config_property)


class _CrosConfigBaseTestCase(unittest.TestCase):
    """Base class which sets up mock fallback commands"""

    def setUp(self):
        """Add mock fallback command(s) to cros_config.FALLBACKS"""
        for path in CC_PATHS.values():
            pass_args = '%s %s' % (path, CC_PROPERTIES[SUCCEEDS])
            fail_args = '%s %s' % (path, CC_PROPERTIES[FAILS])
            cros_config.FALLBACKS[pass_args] = \
                    'echo %s' % CROS_CONFIG_FALLBACK_RESPONSE
            cros_config.FALLBACKS[fail_args] = 'this command does nothing'

    def tearDown(self):
        """Remove mock fallback command(s) from cros_config.FALLBACKS"""
        for path in CC_PATHS.values():
            pass_args = '%s %s' % (path, CC_PROPERTIES[SUCCEEDS])
            fail_args = '%s %s' % (path, CC_PROPERTIES[FAILS])
            del cros_config.FALLBACKS[pass_args]
            del cros_config.FALLBACKS[fail_args]


class GetFallbackTestCase(_CrosConfigBaseTestCase):
    """Verify cros_config.get_fallback"""

    def runTest(self):
        """Check handling for commands with and without fallbacks"""
        self.assertFalse(
                cros_config.get_fallback(
                        '%s %s' % (CC_PATHS[SUCCEEDS],
                                   CC_PROPERTIES[DOES_NOT_EXIST])))
        self.assertEqual(
                cros_config.get_fallback('%s %s' % (CC_PATHS[SUCCEEDS],
                                                    CC_PROPERTIES[SUCCEEDS])),
                'echo %s' % CROS_CONFIG_FALLBACK_RESPONSE)


def _mock_cmd_runner(cmd, **kwargs):
    """
    Mock running a DUT command, returning a CmdResult.

    We handle a few mock functions here:
    * cros_config $path $property: $path determines error or success.
                                   $property is not used here.
    * echo $text: Returns $text with a trailing newline.

    Additionally, if the kwarg `ignore_status` is passed in as True,
    then when cros_config would raise an error, it instead returns a
    CmdResult with an exit_status of 1.

    @param cmd: A command, as would be run on the DUT
    @param **kwargs: Kwargs that might be passed into, say, utils.run()
    @return: A mock response from the DUT

    @type cmd: string
    @rtype: client.common_lib.utils.CmdResult

    @raise error.CmdError if cros_config should raise an exception.
    @raise NotImplementedError if cros_config has an unexpected path

    """
    result = utils.CmdResult(cmd)
    if cmd.startswith('cros_config '):
        _, path, _ = cmd.split()
        if path == CC_PATHS[SUCCEEDS]:
            result.stdout = CROS_CONFIG_SUCCESS_RESPONSE
        elif path == CC_PATHS[FAILS]:
            result.exit_status = 1
            if not kwargs.get('ignore_status'):
                raise error.CmdError(cmd, result)
        else:
            raise NotImplementedError('Bad cros_config path: %s' % path)
    elif cmd.startswith('echo '):
        result.stdout = cmd.lstrip('echo ') + '\n'
    else:
        result.exit_status = 2
        if not kwargs.get('ignore_status'):
            raise error.CmdError(cmd, result)
    return result


class CallCrosConfigWithFallbackTestCase(_CrosConfigBaseTestCase):
    """Verify cros_config.call_cros_config_with_fallback"""

    def run_cc_w_fallback(self, cros_config_result, fallback_result,
                          ignore_status=False):
        """
        Helper function to call
        cros_config.call_cros_config_with_fallback()

        """
        cc_args = get_cros_config_args(cros_config_result, fallback_result)
        if ignore_status:
            return cros_config.call_cros_config_with_fallback(
                    cc_args, _mock_cmd_runner, ignore_status=True)
        else:
            return cros_config.call_cros_config_with_fallback(
                    cc_args, _mock_cmd_runner)

    def test_cros_config_success(self):
        """
        Verify that if cros_config is defined, we get the cros_config
        result, regardless of whether there is a fallback command.

        """
        for fallback_status in (SUCCEEDS, FAILS, DOES_NOT_EXIST):
            for ignore_status in (True, False):
                output = self.run_cc_w_fallback(SUCCEEDS, fallback_status,
                        ignore_status)
                self.assertEqual(output.stdout, CROS_CONFIG_SUCCESS_RESPONSE)
                self.assertFalse(output.exit_status)

    def test_fallback_success(self):
        """
        Verify that if cros_config is not defined but a fallback is,
        we get the fallback result.

        """
        for ignore_status in (True, False):
            output = self.run_cc_w_fallback(FAILS, SUCCEEDS, ignore_status)
            self.assertEqual(output.stdout, CROS_CONFIG_FALLBACK_RESPONSE)
            self.assertFalse(output.exit_status)

    def test_fallback_fails(self):
        """
        Verify that if both cros_config and the fallback fail, a
        CmdError is raised.

        """
        with self.assertRaises(error.CmdError):
            self.run_cc_w_fallback(FAILS, FAILS)

    def test_fallback_dne(self):
        """
        Verify that if cros_config fails and the fallback does not
        exist, a CmdError is raised.

        """
        with self.assertRaises(error.CmdError):
            self.run_cc_w_fallback(FAILS, DOES_NOT_EXIST)

    def test_fallback_fails_ignore_status(self):
        """
        Verify that if both cros_config and the fallback fail, and the
        ignore_status kwarg is passed in, we get a CmdResult with a
        non-zero exit status.

        """
        output = self.run_cc_w_fallback(FAILS, FAILS, True)
        self.assertTrue(output.exit_status)

    def test_fallback_dne_ignore_status(self):
        """
        Verify that if cros_config fails and the fallback does not
        exist, and the ignore_status kwarg is passed in, we get a
        CmdResult with a non-zero exit status.

        """
        output = self.run_cc_w_fallback(FAILS, DOES_NOT_EXIST, True)
        self.assertTrue(output.exit_status)


class CallCrosConfigGetOutputTestCase(_CrosConfigBaseTestCase):
    """
    Verify cros_config.call_cros_config_get_output.
    Basically the same as CallCrosConfigWithFallbackTestCase, except
    that the expected result is a string instead of a CmdResult, and
    it shouldn't raise exceptions.

    """

    def run_cc_get_output(self, cros_config_result, fallback_result,
                          ignore_status=False):
        """
        Helper function to call
        cros_config.call_cros_config_get_output()

        """
        cc_args = get_cros_config_args(cros_config_result, fallback_result)
        if ignore_status:
            return cros_config.call_cros_config_get_output(
                    cc_args, _mock_cmd_runner, ignore_status=True)
        else:
            return cros_config.call_cros_config_get_output(
                    cc_args, _mock_cmd_runner)

    def test_cros_config_success(self):
        """
        Verify that if cros_config is defined, we get the cros_config
        result, regardless of whether there is a fallback command.

        """
        for fallback_status in (SUCCEEDS, FAILS, DOES_NOT_EXIST):
            output = self.run_cc_get_output(SUCCEEDS, fallback_status)
            self.assertEqual(output, CROS_CONFIG_SUCCESS_RESPONSE)

    def test_fallback_success(self):
        """
        Verify that if cros_config is not defined but a fallback is,
        we get the fallback result.

        """
        output = self.run_cc_get_output(FAILS, SUCCEEDS)
        self.assertEqual(output, CROS_CONFIG_FALLBACK_RESPONSE)

    def test_fallback_fails(self):
        """
        Verify that if both cros_config and the fallback fail, we get
        a falsey value.

        """
        output = self.run_cc_get_output(FAILS, FAILS)
        self.assertFalse(output)

    def test_fallback_dne(self):
        """
        Verify that if cros_config fails and the fallback does not
        exist, we get a falsey value.

        """
        output = self.run_cc_get_output(FAILS, DOES_NOT_EXIST)
        self.assertFalse(output)

    def test_fallback_fails_ignore_status(self):
        """
        Verify that if both cros_config and the fallback fail, and the
        ignore_status kwarg is passed in, we get a falsey value.

        """
        output = self.run_cc_get_output(FAILS, FAILS, True)
        self.assertFalse(output)

    def test_fallback_dne_ignore_status(self):
        """
        Verify that if cros_config fails and the fallback does not
        exist, and the ignore_status kwarg is passed in, we get a
        falsey value.

        """
        output = self.run_cc_get_output(FAILS, DOES_NOT_EXIST, True)
        self.assertFalse(output)


if __name__ == "__main__":
    unittest.main()
