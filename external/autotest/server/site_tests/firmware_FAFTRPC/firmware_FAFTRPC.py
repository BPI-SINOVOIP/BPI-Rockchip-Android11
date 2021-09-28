# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import operator
import re
import sys
import xmlrpclib

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.firmware_test import FirmwareTest
from autotest_lib.server.site_tests.firmware_FAFTRPC import config


def get_rpc_category_by_name(name):
    """Find a category from config.RPC_CATEGORIES by its category_name."""
    for rpc_category in config.RPC_CATEGORIES:
        if rpc_category["category_name"] == name:
            return rpc_category
    raise ValueError("No RPC category defined with category_name=%s" % name)


def get_rpc_method_names_from_test_case(test_case):
    """
    Extract the method_name or method_names from a test case configuration.

    @param test_case: An element from a test_cases array,
                      like those in config.RPC_CATEGORIES

    @return: A list of names of RPC methods in that test case.

    """
    if (("method_name" in test_case) ^ ("method_names" in test_case)):
        if "method_name" in test_case:
            return [test_case["method_name"]]
        elif "method_names" in test_case:
            return test_case["method_names"]
        else:
            err_msg = "Something strange happened while parsing RPC methods"
            raise ValueError(err_msg)
    else:
        err_msg = "test_case must contain EITHER method_name OR method_names"
        raise ValueError(err_msg)



class firmware_FAFTRPC(FirmwareTest):
    """
    This test checks that all RPC commands work as intended.

    For now, we only need to verify that the RPC framework is intact,
    so we only verify that all RPCs can be called with the
    expected arguments.

    It would be good to expand this test to verify that all RPCs
    yields the expected results.

    """
    version = 1
    _stored_values = {}


    def initialize(self, host, cmdline_args, dev_mode=False):
        """Runs before test begins."""
        super(firmware_FAFTRPC, self).initialize(host, cmdline_args)
        self.backup_firmware()
        self.faft_client.rpc_settings.enable_test_mode()


    def cleanup(self):
        """Runs after test completion."""
        self.faft_client.rpc_settings.disable_test_mode()
        try:
            if self.is_firmware_saved():
                self.restore_firmware()
            if self.reboot_after_completion:
                logging.info("Rebooting DUT, as specified in control file")
                self.switcher.mode_aware_reboot()
        except Exception as e:
            logging.error("Caught exception: %s", str(e))
        super(firmware_FAFTRPC, self).cleanup()


    def _log_success(self, rpc_name, params, success_message):
        """Report on an info level that a test passed."""
        logging.info("RPC test for %s%s successfully %s",
                     rpc_name, params, success_message)


    def _fail(self, rpc_name, params, error_msg):
        """Raise a TestFail error explaining why a test failed."""
        raise error.TestFail("RPC function %s%s had an unexpected result: %s"
                             % (rpc_name, params, error_msg))


    def _retrieve_stored_values(self, params):
        """
        Replace any operator.itemgetter params with corresponding stored values.

        @param params: A tuple of args that might be passed into an RPC method,
                       some of which might be operator.itemgetter objects.

        @return: A tuple of pargs to be passed into an RPC method,
                 with stored values swapped in for operator.itemgetters.

        """
        new_params = []
        for old_param in params:
            if isinstance(old_param, operator.itemgetter):
                retrieved_value = old_param(self._stored_values)
                new_params.append(retrieved_value)
            else:
                new_params.append(old_param)
        new_params = tuple(new_params)
        return new_params


    def _assert_passes(self, category, method, params, allow_error_msg=None,
                       expected_return_type=None, silence_result=False):
        """
        Check whether an RPC function with given input passes,
        and fail if it does not.

        If an expected_return_type is passed in, then require the RPC function
        to return a value matching that type, or else fail.

        @param category: The RPC subsystem category; ex. kernel, bios
        @param method: The name of the RPC function within the subsystem
        @param params: A tuple containing params to pass into the RPC function
        @param allow_error_msg: If a regexy string is passed in, and the RPC
                                returns an RPC error matching this regex,
                                then the test will pass instead of failing.
        @param expected_return_type: If not None, then the RPC return value
                                     must be this type, else the test fails.
        @param silence_result: If True, then the RPC return value will not be
                               logged.

        @raise error.TestFail: If the RPC raises any error (unless handled by
                               allow_error_msg).
        @raise error.TestFail: If expected_return_type is not None, and the RPC
                               return value is not expected_return_type.

        @return: Not meaningful.

        """
        rpc_function = self.get_rpc_function(category, method)
        rpc_name = ".".join([category, method])
        try:
            result = rpc_function(*params)
        except xmlrpclib.Fault as e:
            if allow_error_msg is not None and \
                    re.search(allow_error_msg, str(e)):
                success_msg = "raised an acceptable error during RPC handling"
                self._log_success(rpc_name, params, success_msg)
                return e
            error_msg = "Unexpected RPC error: %s" % e
            self._fail(rpc_name, params, error_msg)
        except:
            error_msg = "Unexpected misc error: %s" % sys.exc_info()[0]
            self._fail(rpc_name, params, error_msg)
        else:
            if expected_return_type is None:
                if silence_result:
                    success_msg = "passed with a silenced result"
                else:
                    success_msg = "passed with result %s" % result
                self._log_success(rpc_name, params, success_msg)
                return result
            elif isinstance(result, expected_return_type):
                if silence_result:
                    success_msg = "passed with a silenced result of " \
                            "expected type %s" % type(result)
                else:
                    success_msg = "passed with result %s of expected type %s" \
                            % (result, type(result))
                self._log_success(rpc_name, params, success_msg)
                return result
            else:
                error_msg = ("Expected a result of type %s, but got %s " +
                                "of type %s)") \
                            % (expected_return_type, result, type(result))
                self._fail(rpc_name, params, error_msg)


    def _assert_fails(self, category, method, params):
        """
        Check whether an RPC function with given input throws an RPC error,
        and fail if it does not.

        @param category: The RPC subsystem category; ex. kernel, bios
        @param method: The name of the RPC function within the subsystem
        @param params: A tuple containing params to pass into the RPC function

        @raise error.TestFail: If the RPC raises no error, or if it raises any
                               error other than xmlrpclib.Fault

        @return: Not meaningful.

        """
        rpc_function = self.get_rpc_function(category, method)
        rpc_name = ".".join([category, method])
        try:
            result = rpc_function(*params)
        except xmlrpclib.Fault as e:
            self._log_success(rpc_name, params, "raised RPC error")
        except:
            error_msg = "Unexpected misc error: %s" % sys.exc_info()[0]
            self._fail(rpc_name, params, error_msg)
        else:
            error_msg = "Should have raised an RPC error, but did not"
            self._fail(rpc_name, params, error_msg)


    def _assert_output(self, category, method, params, expected_output,
                       allow_error_msg=None):
        """
        Check whether an RPC function with given input
        returns a particular value, and fail if it does not.

        @param category: The RPC subsystem category; ex. kernel, bios
        @param method: The name of the RPC function within the subsystem
        @param params: A tuple containing params to pass into the RPC function
        @param expected_output: The value that the RPC function should return
        @param allow_error_msg: If a regexy string is passed in, and the RPC
                                returns an RPC error containing this string,
                                then the test will pass instead of failing.

        @raise error.TestFail: If self._assert_passes(...) fails, or if the
                               RPC return value does not match expected_output

        @return: Not meaningful.

        """
        rpc_name = ".".join([category, method])
        actual_output = self._assert_passes(category, method, params,
                                            allow_error_msg=allow_error_msg)
        if expected_output == actual_output:
            success_message = "returned the expected value <%s>" \
                              % expected_output
            self._log_success(rpc_name, params, success_message)
        else:
            error_msg = "Expected output <%s>, but actually returned <%s>" \
                        % (expected_output, actual_output)
            self._fail(rpc_name, params, error_msg)


    def get_rpc_function(self, category, method):
        """
        Find a callable RPC function given its name.

        @param category: The name of an RPC subsystem category; ex. kernel, ec
        @param method: The name of an RPC function within the subsystem

        @return: A callable method of the RPC proxy
        """
        rpc_function_handler = getattr(self.faft_client, category)
        rpc_function = getattr(rpc_function_handler, method)
        return rpc_function


    def run_once(self, category_under_test="*", reboot_after_completion=False):
        """
        Main test logic.

        For all RPC categories being tested,
        iterate through all test cases defined in config.py.

        @param category_under_test: The name of an RPC category to be tested,
                                    such as ec, bios, or kernel.
                                    Default is '*', which tests all categories.

        """
        if category_under_test == "*":
            logging.info("Testing all %d RPC categories",
                         len(config.RPC_CATEGORIES))
            rpc_categories_to_test = config.RPC_CATEGORIES
        else:
            rpc_categories_to_test = [
                    get_rpc_category_by_name(category_under_test)]
            logging.info("Testing RPC category '%s'", category_under_test)
        self.reboot_after_completion = reboot_after_completion
        for rpc_category in rpc_categories_to_test:
            category_name = rpc_category["category_name"]
            if category_name == "ec" and not self.check_ec_capability():
                logging.info("No EC found on DUT. Skipping EC category.")
                continue
            test_cases = rpc_category["test_cases"]
            logging.info("Testing %d cases for RPC category '%s'",
                         len(test_cases), category_name)
            for test_case in test_cases:
                method_names = get_rpc_method_names_from_test_case(test_case)
                passing_args = test_case.get("passing_args", [])
                failing_args = test_case.get("failing_args", [])
                allow_error_msg = test_case.get("allow_error_msg", None)
                expected_return_type = test_case.get("expected_return_type",
                                                     None)
                store_result_as = test_case.get("store_result_as", None)
                silence_result = test_case.get("silence_result", False)
                for method_name in method_names:
                    for passing_arg_tuple in passing_args:
                        passing_arg_tuple = self._retrieve_stored_values(
                                passing_arg_tuple)
                        result = self._assert_passes(category_name, method_name,
                                                     passing_arg_tuple,
                                                     allow_error_msg,
                                                     expected_return_type,
                                                     silence_result)
                        if store_result_as is not None:
                            self._stored_values[store_result_as] = result
                    for failing_arg_tuple in failing_args:
                        failing_arg_tuple = self._retrieve_stored_values(
                                failing_arg_tuple)
                        self._assert_fails(category_name, method_name,
                                           failing_arg_tuple)
