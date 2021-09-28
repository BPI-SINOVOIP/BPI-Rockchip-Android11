#!/usr/bin/env python3.4
#
#   Copyright 2017 - The Android Open Source Project
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

import unittest
from acts.libs.test_binding.all_tests_decorator import for_all_tests


class AllTestDecoratorTest(unittest.TestCase):

    def test_add_to_all_tests(self):

        def decorator(decorated):
            def inner(*args, **kwargs):
                return 3

            return inner

        @for_all_tests(decorator)
        class TestTest(object):
            def test_a_thing(self):
                return 4

            def not_a_test(self):
                return 4

        test = TestTest()
        self.assertEqual(test.test_a_thing(), 3)
        self.assertEqual(test.not_a_test(), 4)


if __name__ == "__main__":
    unittest.main()
