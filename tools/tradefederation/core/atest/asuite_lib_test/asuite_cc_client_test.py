#!/usr/bin/env python
#
# Copyright 2019, The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Unittest for atest_execution_info."""

import unittest


class AsuiteCCLibTest(unittest.TestCase):
    """Tests for verify asuite_metrics libs"""

    def test_import_asuite_cc_lib(self):
        """Test asuite_cc_lib."""
        # pylint: disable=import-error, unused-variable
        from asuite.metrics import metrics
        from asuite.metrics import metrics_base
        from asuite.metrics import metrics_utils

        # TODO (b/132602907): Add the real usage for checking if metrics pass or fail.

if __name__ == "__main__":
    unittest.main()
