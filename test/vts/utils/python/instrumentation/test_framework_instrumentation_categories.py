#
# Copyright (C) 2018 The Android Open Source Project
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

import logging


class TestFrameworkInstrumentationCategories(object):
    """Enum values for common category strings."""
    FRAMEWORK_SETUP = 'Framework setUp'
    FRAMEWORK_TEARDOWN = 'Framework tearDown'
    TEST_MODULE_SETUP = 'Test module setUp'
    TEST_MODULE_TEARDOWN = 'Test module tearDown'
    TEST_CLASS_SETUP = 'Test class setUp'
    TEST_CLASS_TEARDOWN = 'Test class tearDown'
    TEST_CASE_SETUP = 'Test case setUp'
    TEST_CASE_TEARDOWN = 'Test case tearDown'
    DEVICE_SETUP = 'Device setUp'
    DEVICE_CLEANUP = 'Device cleanUp'
    FAILED_TEST_CASE_PROCESSING = 'Failed test case processing'
    TEST_CASE_EXECUTION = 'Test case execution'
    RESULT_PROCESSING = 'Result processing'
    WAITING_FOR_DEVICE_RESPOND = 'Waiting for device respond'

    def Add(self, key, value):
        """Add a category key and value to the class attribute.

        Key being empty or starting with non-letter is not allowed.

        Returns:
            bool, whether adding the values is success
        """
        if not key or not key[0].isalpha():
            logging.error('Category name empty or starting with non-letter '
                          'is not allowed. Ignoring key=[%s], value=[%s]',
                          key, value)
            return False

        if hasattr(self, key):
            logging.warn('Categories key %s already exists with value %s. '
                         'Overwriting with %s.', key, getattr(self,key), value)

        setattr(self, key, value)
        return True