#!/usr/bin/env python3
#
#   Copyright 2018 - The Android Open Source Project
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
from __future__ import absolute_import

import os
import sys

# A temporary hack to prevent tests/libs/logging from being selected as the
# python default logging module.
sys.path[0] = os.path.join(sys.path[0], '../')
import unittest
import mock

from acts import base_test
from acts.libs import version_selector
from acts.test_decorators import test_tracker_info

from mobly.config_parser import TestRunConfig


def versioning_decorator(min_sdk, max_sdk):
    return version_selector.set_version(lambda ret, *_, **__: ret, min_sdk,
                                        max_sdk)


def versioning_decorator2(min_sdk, max_sdk):
    return version_selector.set_version(lambda ret, *_, **__: ret, min_sdk,
                                        max_sdk)


def test_versioning(min_sdk, max_sdk):
    return version_selector.set_version(lambda *_, **__: 1, min_sdk, max_sdk)


@versioning_decorator(1, 10)
def versioned_func(arg1, arg2):
    return 'function 1', arg1, arg2


@versioning_decorator(11, 11)
def versioned_func(arg1, arg2):
    return 'function 2', arg1, arg2


@versioning_decorator(12, 20)
def versioned_func(arg1, arg2):
    return 'function 3', arg1, arg2


@versioning_decorator(1, 20)
def versioned_func_with_kwargs(_, asdf='jkl'):
    return asdf


def class_versioning_decorator(min_sdk, max_sdk):
    return version_selector.set_version(lambda _, ret, *__, **___: ret,
                                        min_sdk, max_sdk)


class VersionedClass(object):
    @classmethod
    @class_versioning_decorator(1, 99999999)
    def class_func(cls, arg1):
        return cls, arg1

    @staticmethod
    @versioning_decorator(1, 99999999)
    def static_func(arg1):
        return arg1

    @class_versioning_decorator(1, 99999999)
    def instance_func(self, arg1):
        return self, arg1


class VersionedTestClass(base_test.BaseTestClass):
    @test_tracker_info('UUID_1')
    @test_versioning(1, 1)
    def test_1(self):
        pass

    @test_versioning(1, 1)
    @test_tracker_info('UUID_2')
    def test_2(self):
        pass


class VersionSelectorIntegrationTest(unittest.TestCase):
    """Tests the acts.libs.version_selector module."""
    def test_versioned_test_class_calls_both_functions(self):
        """Tests that VersionedTestClass (above) can be called with
        test_tracker_info."""
        test_run_config = TestRunConfig()
        test_run_config.log_path = ''
        test_run_config.summary_writer = mock.MagicMock()

        test_class = VersionedTestClass(test_run_config)
        test_class.run(['test_1', 'test_2'])

        self.assertIn('Executed 2', test_class.results.summary_str(),
                      'One or more of the test cases did not execute.')
        self.assertEqual(
            'UUID_1',
            test_class.results.executed[0].extras['test_tracker_uuid'],
            'The test_tracker_uuid was not found for test_1.')
        self.assertEqual(
            'UUID_2',
            test_class.results.executed[1].extras['test_tracker_uuid'],
            'The test_tracker_uuid was not found for test_2.')

    def test_raises_syntax_error_if_decorated_with_staticmethod_first(self):
        try:

            class SomeClass(object):
                @versioning_decorator(1, 1)
                @staticmethod
                def test_1():
                    pass
        except SyntaxError:
            pass
        else:
            self.fail('Placing the @staticmethod decorator after the '
                      'versioning decorator should cause a SyntaxError.')

    def test_raises_syntax_error_if_decorated_with_classmethod_first(self):
        try:

            class SomeClass(object):
                @versioning_decorator(1, 1)
                @classmethod
                def test_1(cls):
                    pass
        except SyntaxError:
            pass
        else:
            self.fail('Placing the @classmethod decorator after the '
                      'versioning decorator should cause a SyntaxError.')

    def test_overriding_an_undecorated_func_raises_a_syntax_error(self):
        try:

            class SomeClass(object):
                def test_1(self):
                    pass

                @versioning_decorator(1, 1)
                def test_1(self):
                    pass
        except SyntaxError:
            pass
        else:
            self.fail('Overwriting a function that already exists without a '
                      'versioning decorator should raise a SyntaxError.')

    def test_func_decorated_with_2_different_versioning_decorators_causes_error(
            self):
        try:

            class SomeClass(object):
                @versioning_decorator(1, 1)
                def test_1(self):
                    pass

                @versioning_decorator2(2, 2)
                def test_1(self):
                    pass
        except SyntaxError:
            pass
        else:
            self.fail('Using two different versioning decorators to version a '
                      'single function should raise a SyntaxError.')

    def test_func_decorated_with_overlapping_ranges_causes_value_error(self):
        try:

            class SomeClass(object):
                @versioning_decorator(1, 2)
                def test_1(self):
                    pass

                @versioning_decorator(2, 2)
                def test_1(self):
                    pass
        except ValueError:
            pass
        else:
            self.fail('Decorated functions with overlapping version ranges '
                      'should raise a ValueError.')

    def test_func_decorated_with_min_gt_max_causes_value_error(self):
        try:

            class SomeClass(object):
                @versioning_decorator(2, 1)
                def test_1(self):
                    pass
        except ValueError:
            pass
        else:
            self.fail(
                'If the min_version level is higher than the max_version '
                'level, a ValueError should be raised.')

    def test_calling_versioned_func_on_min_version_level_is_inclusive(self):
        """Tests that calling some versioned function with the minimum version
        level of the decorated function will call that function."""
        ret = versioned_func(1, 'some_value')
        self.assertEqual(
            ret, ('function 1', 1, 'some_value'),
            'Calling versioned_func(1, ...) did not return the '
            'versioned function for the correct range.')

    def test_calling_versioned_func_on_middle_level_works(self):
        """Tests that calling some versioned function a version value within the
        range of the decorated function will call that function."""
        ret = versioned_func(16, 'some_value')
        self.assertEqual(
            ret, ('function 3', 16, 'some_value'),
            'Calling versioned_func(16, ...) did not return the '
            'versioned function for the correct range.')

    def test_calling_versioned_func_on_max_version_level_is_inclusive(self):
        """Tests that calling some versioned function with the maximum version
        level of the decorated function will call that function."""
        ret = versioned_func(10, 'some_value')
        self.assertEqual(
            ret, ('function 1', 10, 'some_value'),
            'Calling versioned_func(10, ...) did not return the '
            'versioned function for the correct range.')

    def test_calling_versioned_func_on_min_equals_max_level_works(self):
        """Tests that calling some versioned function with the maximum version
        level of the decorated function will call that function."""
        ret = versioned_func(11, 'some_value')
        self.assertEqual(
            ret, ('function 2', 11, 'some_value'),
            'Calling versioned_func(10, ...) did not return the '
            'versioned function for the correct range.')

    def test_sending_kwargs_through_decorated_functions_works(self):
        """Tests that calling some versioned function with the maximum version
        level of the decorated function will call that function."""
        ret = versioned_func_with_kwargs(1, asdf='some_value')
        self.assertEqual(
            ret, 'some_value',
            'Calling versioned_func_with_kwargs(1, ...) did not'
            'return the kwarg value properly.')

    def test_kwargs_can_default_through_decorated_functions(self):
        """Tests that calling some versioned function with the maximum version
        level of the decorated function will call that function."""
        ret = versioned_func_with_kwargs(1)
        self.assertEqual(
            ret, 'jkl', 'Calling versioned_func_with_kwargs(1) did not'
            'return the default kwarg value properly.')

    def test_staticmethod_can_be_called_properly(self):
        """Tests that decorating a staticmethod will properly send the arguments
        in the correct order.

        i.e., we want to make sure self or cls do not get sent as the first
        argument to the decorated staticmethod.
        """
        versioned_class = VersionedClass()
        ret = versioned_class.static_func(123456)
        self.assertEqual(
            ret, 123456, 'The first argument was not set properly for calling '
            'a staticmethod.')

    def test_instance_method_can_be_called_properly(self):
        """Tests that decorating a method will properly send the arguments
        in the correct order.

        i.e., we want to make sure self is the first argument returned.
        """
        versioned_class = VersionedClass()
        ret = versioned_class.instance_func(123456)
        self.assertEqual(
            ret, (versioned_class, 123456),
            'The arguments were not set properly for an instance '
            'method.')

    def test_classmethod_can_be_called_properly(self):
        """Tests that decorating a classmethod will properly send the arguments
        in the correct order.

        i.e., we want to make sure cls is the first argument returned.
        """
        versioned_class = VersionedClass()
        ret = versioned_class.class_func(123456)
        self.assertEqual(
            ret, (VersionedClass, 123456),
            'The arguments were not set properly for a '
            'classmethod.')


if __name__ == '__main__':
    unittest.main()
